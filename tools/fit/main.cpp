// Measures how well the eight-axis control space can reproduce the factory presets.
//
// For each preset it renders a held note, then searches the eight axes for the point whose sound
// is closest, and reports how close that is next to two yardsticks: the same search budget spent
// on random axis positions, and the distance from the preset to the most similar other preset.
#include "dsp/Axes.h"
#include "dsp/Engine.h"
#include "dsp/Fft.h"
#include "offline/OfflineRenderer.h"
#include "offline/WavWriter.h"
#include "presets/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

using namespace polylogue;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kNote = 55;
constexpr double kHoldSeconds = 2.0;
constexpr double kRenderSeconds = 4.0;
constexpr int kFftSize = 2048;
constexpr int kBands = 40;
constexpr float kFloorDb = -70.0f;
constexpr float kActiveDb = -55.0f;

constexpr std::array<double, 16> kFrameTimes = {0.01, 0.03, 0.06, 0.1,  0.16, 0.25, 0.4, 0.6,
                                                0.85, 1.15, 1.5,  1.95, 2.3,  2.8,  3.3, 3.9};
constexpr std::size_t kCells = kFrameTimes.size() * kBands;

// A log-spectrogram, in dB relative to its loudest cell.
using Features = std::array<float, kCells>;

struct Options {
    int budget = 600;
    unsigned seed = 1;
    std::string wavDir;
    std::string emitPath;
};

offline::StereoBuffer render(const dsp::ParamValues& values)
{
    dsp::Engine engine;
    engine.setSettings(dsp::toSettings(values));
    engine.prepare(kSampleRate);
    const std::array<offline::TimedEvent, 2> events = {
        offline::at(0.0, dsp::MidiEvent::noteOn(0, kNote, 0.8f)),
        offline::at(kHoldSeconds, dsp::MidiEvent::noteOff(0, kNote))};
    return offline::renderOffline(engine, events, kRenderSeconds, kSampleRate);
}

Features analyse(const offline::StereoBuffer& audio)
{
    std::vector<float> mono(audio.left.size());
    for (std::size_t i = 0; i < mono.size(); ++i)
        mono[i] = 0.5f * (audio.left[i] + audio.right[i]);

    dsp::Fft fft(kFftSize);
    std::vector<float> spectrum(kFftSize / 2);
    std::vector<float> frame(kFftSize);
    const double lowHz = 80.0;
    const double highHz = 16000.0;
    const double binHz = kSampleRate / kFftSize;

    Features cells{};
    float loudest = -200.0f;
    for (std::size_t f = 0; f < kFrameTimes.size(); ++f) {
        const long start = static_cast<long>(kFrameTimes[f] * kSampleRate) - kFftSize / 2;
        for (int i = 0; i < kFftSize; ++i) {
            const long at = start + i;
            frame[static_cast<std::size_t>(i)] = at >= 0 && at < static_cast<long>(mono.size())
                                                     ? mono[static_cast<std::size_t>(at)]
                                                     : 0.0f;
        }
        fft.magnitudes(frame, spectrum);
        for (int b = 0; b < kBands; ++b) {
            const double from = lowHz * std::pow(highHz / lowHz, static_cast<double>(b) / kBands);
            const double to = lowHz * std::pow(highHz / lowHz, static_cast<double>(b + 1) / kBands);
            const int first = std::max(1, static_cast<int>(from / binHz));
            const int last = std::max(first + 1, static_cast<int>(to / binHz));
            double power = 0.0;
            for (int bin = first; bin < last && bin < kFftSize / 2; ++bin) {
                const double m = static_cast<double>(spectrum[static_cast<std::size_t>(bin)]);
                power += m * m;
            }
            const float db = static_cast<float>(10.0 * std::log10(power + 1e-12));
            cells[f * kBands + static_cast<std::size_t>(b)] = db;
            loudest = std::max(loudest, db);
        }
    }
    for (float& cell : cells)
        cell = std::max(cell - loudest, kFloorDb);
    return cells;
}

// Root-mean-square difference in dB over the cells where either sound is audible.
double distance(const Features& a, const Features& b)
{
    double sum = 0.0;
    int counted = 0;
    for (std::size_t i = 0; i < kCells; ++i) {
        if (std::max(a[i], b[i]) < kActiveDb)
            continue;
        const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        sum += d * d;
        ++counted;
    }
    return counted > 0 ? std::sqrt(sum / counted) : 0.0;
}

// What the axes do not describe is taken from the preset: it is the keyboard setup, not the sound.
dsp::ParamValues performanceContext(const dsp::ParamValues& preset)
{
    dsp::ParamValues base = dsp::ParamValues::defaults();
    for (dsp::Param p :
         {dsp::Param::Level, dsp::Param::Polyphony, dsp::Param::KeyMode, dsp::Param::Octave,
          dsp::Param::Tune, dsp::Param::BendRange, dsp::Param::GlideTime, dsp::Param::GlideMode})
        base[p] = preset[p];
    return base;
}

struct Fit {
    dsp::Axes axes;
    double error = 1e9;
    int evaluations = 0;
};

class Searcher {
public:
    Searcher(const Features& target, const dsp::ParamValues& base, unsigned seed)
        : target_(target), base_(base), rng_(seed)
    {}

    double score(const dsp::Axes& axes)
    {
        ++evaluations_;
        return distance(target_, analyse(render(dsp::fromAxes(axes, base_))));
    }

    dsp::Axes randomAxes()
    {
        std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
        dsp::Axes axes;
        for (float& v : axes.values)
            v = uniform(rng_);
        return axes;
    }

    // The best of `budget` random positions.
    Fit random(int budget)
    {
        Fit best;
        for (int i = 0; i < budget; ++i) {
            const dsp::Axes axes = randomAxes();
            const double error = score(axes);
            if (error < best.error)
                best = {axes, error, 0};
        }
        best.evaluations = budget;
        return best;
    }

    // An evolution strategy with a self-adjusting step, restarted from the best of a few random
    // points, keeping the best result overall.
    Fit search(int budget)
    {
        constexpr int kRestarts = 4;
        constexpr int kSeeds = 10;
        constexpr int kOffspring = 10;
        Fit best;
        const int perRestart = budget / kRestarts;
        for (int r = 0; r < kRestarts; ++r) {
            Fit mean;
            for (int i = 0; i < kSeeds; ++i) {
                const dsp::Axes axes = randomAxes();
                const double error = score(axes);
                if (error < mean.error)
                    mean = {axes, error, 0};
            }
            float sigma = 0.25f;
            std::normal_distribution<float> normal(0.0f, 1.0f);
            for (int used = kSeeds; used + kOffspring <= perRestart; used += kOffspring) {
                Fit champion = mean;
                for (int k = 0; k < kOffspring; ++k) {
                    dsp::Axes child = mean.axes;
                    for (float& v : child.values)
                        v = std::clamp(v + sigma * normal(rng_), 0.0f, 1.0f);
                    const double error = score(child);
                    if (error < champion.error)
                        champion = {child, error, 0};
                }
                if (champion.error < mean.error) {
                    mean = champion;
                    sigma = std::min(0.4f, sigma * 1.15f);
                } else {
                    sigma = std::max(0.02f, sigma * 0.85f);
                }
            }
            if (mean.error < best.error)
                best = mean;
        }
        best.evaluations = evaluations_;
        return best;
    }

private:
    const Features& target_;
    dsp::ParamValues base_;
    std::mt19937 rng_;
    int evaluations_ = 0;
};

struct Row {
    Fit fit;
    double random = 0.0;
    double nearest = 0.0;
    std::string nearestName;
};

bool parse(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; i += 2) {
        if (i + 1 >= argc)
            return false;
        const std::string_view flag = argv[i];
        const std::string value = argv[i + 1];
        if (flag == "--budget")
            options.budget = std::atoi(value.c_str());
        else if (flag == "--seed")
            options.seed = static_cast<unsigned>(std::atoi(value.c_str()));
        else if (flag == "--wav")
            options.wavDir = value;
        else if (flag == "--emit")
            options.emitPath = value;
        else
            return false;
    }
    return options.budget >= 60;
}

std::string fileName(std::string name)
{
    std::replace(name.begin(), name.end(), ' ', '_');
    return name;
}

void writeWav(const std::filesystem::path& path, const offline::StereoBuffer& audio)
{
    std::vector<float> interleaved(audio.left.size() * 2);
    for (std::size_t i = 0; i < audio.left.size(); ++i) {
        interleaved[2 * i] = audio.left[i];
        interleaved[2 * i + 1] = audio.right[i];
    }
    offline::writeWav(path, interleaved, 2, static_cast<int>(kSampleRate));
}

}  // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parse(argc, argv, options)) {
        std::fprintf(
            stderr, "usage: polylogue-fit [--budget N>=60] [--seed N] [--wav DIR] [--emit FILE]\n");
        return 2;
    }

    const auto presets = presets::factoryPresets();
    std::vector<dsp::ParamValues> targets;
    std::vector<Features> features;
    for (const presets::FactoryPreset& preset : presets) {
        targets.push_back(presets::resolve(preset));
        features.push_back(analyse(render(targets.back())));
    }

    std::vector<Row> rows(presets.size());
    std::atomic<std::size_t> next{0};
    std::vector<std::thread> workers;
    const unsigned threads = std::max(1u, std::thread::hardware_concurrency());
    for (unsigned t = 0; t < threads; ++t) {
        workers.emplace_back([&] {
            for (std::size_t i = next++; i < presets.size(); i = next++) {
                Searcher searcher(features[i], performanceContext(targets[i]),
                                  options.seed + 17 * static_cast<unsigned>(i));
                rows[i].fit = searcher.search(options.budget);
                rows[i].random = searcher.random(rows[i].fit.evaluations).error;
                rows[i].nearest = 1e9;
                for (std::size_t j = 0; j < presets.size(); ++j) {
                    if (j == i)
                        continue;
                    const double d = distance(features[i], features[j]);
                    if (d < rows[i].nearest) {
                        rows[i].nearest = d;
                        rows[i].nearestName = presets[j].name;
                    }
                }
            }
        });
    }
    for (std::thread& worker : workers)
        worker.join();

    std::printf("%-22s %-9s %6s %7s %8s  %-22s  %s\n", "preset", "category", "fit", "random",
                "nearest", "(nearest other preset)", "axes");
    int covered = 0;
    std::vector<double> errors;
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const Row& row = rows[i];
        const bool ok = row.fit.error < row.nearest;
        covered += ok ? 1 : 0;
        errors.push_back(row.fit.error);
        std::printf("%-22s %-9s %5.1f%c %7.1f %8.1f  %-22s ", presets[i].name, presets[i].category,
                    row.fit.error, ok ? '*' : ' ', row.random, row.nearest,
                    row.nearestName.c_str());
        for (std::size_t a = 0; a < dsp::kAxisCount; ++a)
            std::printf(" %.2f", static_cast<double>(row.fit.axes.values[a]));
        std::printf("\n");

        if (!options.wavDir.empty()) {
            std::filesystem::create_directories(options.wavDir);
            const std::filesystem::path dir = options.wavDir;
            const std::string stem = fileName(presets[i].name);
            writeWav(dir / (stem + "_1_original.wav"), render(targets[i]));
            writeWav(dir / (stem + "_2_eight_knobs.wav"),
                     render(dsp::fromAxes(row.fit.axes, performanceContext(targets[i]))));
        }
    }
    if (!options.emitPath.empty()) {
        // One row per preset, in factory order: the eight-knob position that sounds closest.
        std::FILE* out = std::fopen(options.emitPath.c_str(), "w");
        if (out == nullptr)
            return 1;
        std::fprintf(out, "// Generated by polylogue-fit: %s\n",
                     "WAVE METAL GRIT BRIGHT ATTACK SUSTAIN EVOLVE MOTION");
        for (std::size_t i = 0; i < presets.size(); ++i) {
            std::fprintf(out, "{");
            for (std::size_t a = 0; a < dsp::kAxisCount; ++a)
                std::fprintf(out, "%s%.3ff", a == 0 ? "" : ", ",
                             static_cast<double>(rows[i].fit.axes.values[a]));
            std::fprintf(out, "},  // %s\n", presets[i].name);
        }
        std::fclose(out);
    }
    std::sort(errors.begin(), errors.end());
    std::printf("\naxes: ");
    for (std::size_t a = 0; a < dsp::kAxisCount; ++a)
        std::printf("%s ", dsp::axisName(static_cast<dsp::Axis>(a)));
    std::printf("\n* = closer to the original than the most similar other preset is\n");
    std::printf("covered %d of %zu; median fit %.1f dB, worst %.1f dB\n", covered, presets.size(),
                errors[errors.size() / 2], errors.back());
    return 0;
}

// Renders presets to WAV files and reports how they sound in numbers, so the DSP and the factory
// sounds can be checked without a plugin host.
#include "dsp/Engine.h"
#include "dsp/Parameters.h"
#include "offline/OfflineRenderer.h"
#include "offline/WavWriter.h"
#include "presets/FactoryPresets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

using namespace polylogue;

namespace {

constexpr double kSampleRate = 48000.0;

struct Options {
    std::string preset;
    std::vector<int> notes = {48};
    double hold = 2.0;
    double seconds = 6.0;
    float velocity = 0.8f;
    std::string out;
    bool list = false;
};

void usage()
{
    std::cerr
        << "usage: polylogue-render --list\n"
           "       polylogue-render --preset NAME [--notes 48,55,60] [--hold S] [--seconds S]\n"
           "                        [--velocity V] [--out FILE.wav]\n";
}

std::vector<int> parseNotes(const std::string& text)
{
    std::vector<int> notes;
    std::size_t start = 0;
    while (start < text.size()) {
        const std::size_t comma = text.find(',', start);
        notes.push_back(std::atoi(text.substr(start, comma - start).c_str()));
        if (comma == std::string::npos)
            break;
        start = comma + 1;
    }
    return notes;
}

bool parse(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "--list") {
            options.list = true;
            continue;
        }
        if (i + 1 >= argc)
            return false;
        const std::string value = argv[++i];
        if (flag == "--preset")
            options.preset = value;
        else if (flag == "--notes")
            options.notes = parseNotes(value);
        else if (flag == "--hold")
            options.hold = std::atof(value.c_str());
        else if (flag == "--seconds")
            options.seconds = std::atof(value.c_str());
        else if (flag == "--velocity")
            options.velocity = std::strtof(value.c_str(), nullptr);
        else if (flag == "--out")
            options.out = value;
        else
            return false;
    }
    return options.list || !options.preset.empty();
}

const presets::FactoryPreset* findPreset(const std::string& name)
{
    for (const presets::FactoryPreset& preset : presets::factoryPresets()) {
        if (name == preset.name)
            return &preset;
    }
    return nullptr;
}

offline::StereoBuffer render(const presets::FactoryPreset& preset, const Options& options)
{
    dsp::Engine engine;
    engine.setSettings(dsp::toSettings(presets::resolve(preset)));
    engine.prepare(kSampleRate);

    std::vector<offline::TimedEvent> events;
    for (int note : options.notes) {
        events.push_back(offline::at(0.0, dsp::MidiEvent::noteOn(0, note, options.velocity)));
        events.push_back(offline::at(options.hold, dsp::MidiEvent::noteOff(0, note)));
    }
    return offline::renderOffline(engine, events, options.seconds, kSampleRate);
}

double toDb(double value)
{
    return 20.0 * std::log10(std::max(value, 1e-9));
}

struct Stats {
    double peak = 0.0;
    double loudestWindowDb = -120.0;
    double tailSeconds = 0.0;  // how long the sound stays within 40 dB of its loudest window
};

Stats measure(const std::vector<float>& signal)
{
    constexpr int kWindow = 4800;  // 100 ms
    Stats stats;
    std::vector<double> windows;
    for (std::size_t start = 0; start + kWindow <= signal.size(); start += kWindow) {
        double sum = 0.0;
        for (std::size_t i = start; i < start + kWindow; ++i) {
            sum += static_cast<double>(signal[i]) * static_cast<double>(signal[i]);
            stats.peak = std::max(stats.peak, static_cast<double>(std::abs(signal[i])));
        }
        windows.push_back(toDb(std::sqrt(sum / kWindow)));
    }
    if (windows.empty())
        return stats;
    stats.loudestWindowDb = *std::max_element(windows.begin(), windows.end());
    for (std::size_t i = 0; i < windows.size(); ++i) {
        if (windows[i] > stats.loudestWindowDb - 40.0)
            stats.tailSeconds = static_cast<double>(i + 1) * 0.1;
    }
    return stats;
}

int listPresets(const Options& options)
{
    std::printf("%-16s %-10s %8s %8s %8s\n", "preset", "category", "peak", "loud dB", "tail s");
    for (const presets::FactoryPreset& preset : presets::factoryPresets()) {
        const Stats stats = measure(render(preset, options).left);
        std::printf("%-16s %-10s %8.3f %8.1f %8.1f\n", preset.name, preset.category, stats.peak,
                    stats.loudestWindowDb, stats.tailSeconds);
    }
    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parse(argc, argv, options)) {
        usage();
        return 2;
    }

    if (options.list)
        return listPresets(options);

    const presets::FactoryPreset* preset = findPreset(options.preset);
    if (preset == nullptr) {
        std::cerr << "unknown preset: " << options.preset << " (try --list)\n";
        return 1;
    }

    const offline::StereoBuffer audio = render(*preset, options);
    const Stats stats = measure(audio.left);
    std::printf("%s: peak %.3f, loudest %.1f dB, tail %.1f s\n", preset->name, stats.peak,
                stats.loudestWindowDb, stats.tailSeconds);

    if (!options.out.empty()) {
        std::vector<float> interleaved(audio.left.size() * 2);
        for (std::size_t i = 0; i < audio.left.size(); ++i) {
            interleaved[2 * i] = audio.left[i];
            interleaved[2 * i + 1] = audio.right[i];
        }
        if (!offline::writeWav(options.out, interleaved, 2, static_cast<int>(kSampleRate))) {
            std::cerr << "cannot write " << options.out << "\n";
            return 1;
        }
        std::cout << "wrote " << options.out << "\n";
    }
    return 0;
}

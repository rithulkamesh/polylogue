#include "dsp/Drive.h"
#include "dsp/Fft.h"
#include "support/Analysis.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <random>
#include <vector>

using namespace polylogue::dsp;
using namespace polylogue::test;

namespace {

constexpr double kFs = 48000.0;

Drive make(float amount)
{
    Drive drive;
    drive.prepare(kFs);
    drive.setAmount(amount);
    return drive;
}

std::vector<float> sine(double hz, float amplitude, int count)
{
    std::vector<float> out(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i)
        out[static_cast<std::size_t>(i)] =
            amplitude * static_cast<float>(std::sin(2.0 * std::numbers::pi * hz * i / kFs));
    return out;
}

std::vector<float> process(Drive& drive, const std::vector<float>& input)
{
    std::vector<float> out(input.size());
    for (std::size_t i = 0; i < input.size(); ++i)
        out[i] = drive.process(input[i]);
    return out;
}

double harmonicDb(const std::vector<float>& signal, int fftSize, int fundamentalBin, int harmonic)
{
    Fft fft(fftSize);
    std::vector<float> spectrum(static_cast<std::size_t>(fftSize / 2));
    fft.magnitudes(signal, spectrum);
    return decibels(
        static_cast<double>(spectrum[static_cast<std::size_t>(fundamentalBin * harmonic)]) /
        static_cast<double>(spectrum[static_cast<std::size_t>(fundamentalBin)]));
}

}  // namespace

TEST_CASE("zero drive is bit-exact")
{
    Drive drive = make(0.0f);
    std::mt19937 rng(9);
    std::uniform_real_distribution<float> dist(-2.0f, 2.0f);
    for (int i = 0; i < 5000; ++i) {
        const float x = dist(rng);
        CHECK(drive.process(x) == x);
    }
}

TEST_CASE("light drive stays close to transparent")
{
    Drive drive = make(0.03f);
    const auto in = sine(200.0, 0.5f, 4800);
    const auto out = process(drive, in);
    double worst = 0.0;
    for (std::size_t i = 1000; i < in.size(); ++i)
        worst = std::max(worst, static_cast<double>(std::abs(out[i] - in[i])));
    CHECK(worst < 0.01);
}

TEST_CASE("heavy drive keeps the peak near the reference level")
{
    for (float amplitude : {0.7f, 1.0f, 2.0f}) {
        Drive drive = make(1.0f);
        const auto out = process(drive, sine(200.0, amplitude, 9600));
        INFO("amplitude " << amplitude);
        CHECK(peak(out) < 0.75f);
        CHECK(peak(out) > 0.55f);
    }
}

TEST_CASE("drive adds odd harmonics in proportion to the amount")
{
    constexpr int kFftSize = 8192;
    constexpr int kBin = 41;  // about 240 Hz, bin-centred
    const double hz = kBin * kFs / kFftSize;
    const auto in = sine(hz, 0.7f, kFftSize);

    Drive clean = make(0.0f);
    Drive mild = make(0.5f);
    Drive hard = make(1.0f);
    const double cleanDb = harmonicDb(process(clean, in), kFftSize, kBin, 3);
    const double mildDb = harmonicDb(process(mild, in), kFftSize, kBin, 3);
    const double hardDb = harmonicDb(process(hard, in), kFftSize, kBin, 3);
    CHECK(cleanDb < -90.0);
    CHECK(mildDb > cleanDb + 30.0);
    CHECK(hardDb > mildDb + 3.0);
}

TEST_CASE("the shaper is odd-symmetric")
{
    Drive positive = make(0.7f);
    Drive negative = make(0.7f);
    std::mt19937 rng(4);
    std::uniform_real_distribution<float> dist(-1.5f, 1.5f);
    for (int i = 0; i < 4000; ++i) {
        const float x = dist(rng);
        CHECK(std::abs(positive.process(x) + negative.process(-x)) < 1e-6f);
    }
}

TEST_CASE("antiderivative anti-aliasing cuts aliased energy")
{
    constexpr int kFftSize = 8192;
    constexpr int kBin = 350;
    const double hz = kBin * kFs / kFftSize;
    const auto in = sine(hz, 0.9f, kFftSize);

    Drive drive = make(1.0f);
    const auto adaa = process(drive, in);

    // The same curve applied per sample, followed by the same tone filter.
    const double gain = 12.0;
    const double scale = 0.7 / std::tanh(0.7 * gain);
    const double coefficient = 1.0 - std::exp(-2.0 * std::numbers::pi * 5000.0 / kFs);
    std::vector<float> naive(in.size());
    double state = 0.0;
    for (std::size_t i = 0; i < in.size(); ++i) {
        state += coefficient * (scale * std::tanh(gain * static_cast<double>(in[i])) - state);
        naive[i] = static_cast<float>(state);
    }

    const double cleaner = nonHarmonicEnergyDb(adaa, kFftSize, kBin, 4);
    const double dirtier = nonHarmonicEnergyDb(naive, kFftSize, kBin, 4);
    INFO("adaa " << cleaner << " dB, naive " << dirtier << " dB");
    CHECK(cleaner < dirtier - 5.0);
}

TEST_CASE("extreme input stays finite")
{
    Drive drive = make(1.0f);
    for (float x : {1e6f, -1e6f, 1e-30f, 0.0f, 1e6f, -1e6f}) {
        const float out = drive.process(x);
        CHECK(std::isfinite(out));
    }
}

TEST_CASE("sweeping the amount while running never clicks")
{
    for (float level : {0.3f, 0.5f, 0.9f}) {
        Drive drive = make(0.0f);
        float previous = level;
        float worst = 0.0f;
        for (int i = 0; i < 48000; ++i) {
            drive.setAmount(static_cast<float>(i) / 48000.0f);
            const float out = drive.process(level);
            worst = std::max(worst, std::abs(out - previous));
            previous = out;
        }
        INFO("level " << level);
        CHECK(worst < 0.002f);
    }
}

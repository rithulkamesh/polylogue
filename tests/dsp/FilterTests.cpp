#include "dsp/Filter.h"
#include "dsp/Noise.h"
#include "support/Analysis.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <numbers>
#include <vector>

using namespace polylogue::dsp;
using namespace polylogue::test;

namespace {

constexpr double kFs = 48000.0;

Filter make(float cutoff, float resonance)
{
    Filter filter;
    filter.prepare(kFs);
    filter.setParameters(cutoff, resonance);
    return filter;
}

// Steady-state gain of a sine through the filter, in dB.
double gainDb(float cutoff, float resonance, double hz, float amplitude = 0.05f)
{
    Filter filter = make(cutoff, resonance);
    const int total = 48000;
    std::vector<float> out(static_cast<std::size_t>(total));
    for (int i = 0; i < total; ++i) {
        const double phase = 2.0 * std::numbers::pi * hz * i / kFs;
        out[static_cast<std::size_t>(i)] =
            filter.process(amplitude * static_cast<float>(std::sin(phase)));
    }
    const auto tail = std::span<const float>(out).subspan(24000);
    return decibels(rms(tail) * std::numbers::sqrt2 / static_cast<double>(amplitude));
}

}  // namespace

TEST_CASE("filter passes DC at unity")
{
    Filter filter = make(1000.0f, 0.0f);
    float out = 0.0f;
    for (int i = 0; i < 48000; ++i)
        out = filter.process(0.5f);
    CHECK(std::abs(out - 0.5f) < 1e-3f);
}

TEST_CASE("filter follows the two-pole response, warped by the trapezoidal transform")
{
    // Critically damped two-pole: |H| = 1 / (1 + w^2) with w = tan(pi f / fs) / tan(pi fc / fs).
    constexpr double kCutoff = 1000.0;
    const double g = std::tan(std::numbers::pi * kCutoff / kFs);
    for (double hz : {250.0, 1000.0, 2000.0, 4000.0, 8000.0, 16000.0}) {
        const double w = std::tan(std::numbers::pi * hz / kFs) / g;
        const double expected = decibels(1.0 / (1.0 + w * w));
        INFO("hz " << hz);
        CHECK(std::abs(gainDb(static_cast<float>(kCutoff), 0.0f, hz) - expected) < 0.15);
    }
}

TEST_CASE("filter attenuates 12 dB per octave well below Nyquist")
{
    const double slope = gainDb(500.0f, 0.0f, 4000.0) - gainDb(500.0f, 0.0f, 2000.0);
    CHECK(std::abs(slope - -12.0) < 1.0);
}

TEST_CASE("resonance raises a peak at the cutoff")
{
    const double flat = gainDb(1000.0f, 0.0f, 1000.0);
    const double peaked = gainDb(1000.0f, 0.9f, 1000.0);
    CHECK(flat < -5.0);
    CHECK(peaked > 10.0);
}

TEST_CASE("resonance keeps the low end")
{
    CHECK(std::abs(gainDb(2000.0f, 0.8f, 100.0)) < 1.0);
}

TEST_CASE("filter stays finite and bounded across the whole parameter range")
{
    Noise noise(7);
    for (float cutoff : {5.0f, 20.0f, 200.0f, 2000.0f, 12000.0f, 21000.0f, 30000.0f}) {
        for (float resonance : {0.0f, 0.5f, 0.99f, 1.0f, 1.5f}) {
            Filter filter = make(cutoff, resonance);
            float worst = 0.0f;
            bool finite = true;
            for (int i = 0; i < 48000; ++i) {
                const float out = filter.process(noise.next());
                finite = finite && std::isfinite(out);
                worst = std::max(worst, std::abs(out));
            }
            INFO("cutoff " << cutoff << " resonance " << resonance);
            REQUIRE(finite);
            CHECK(worst < 3.0f);
        }
    }
}

TEST_CASE("filter survives per-sample cutoff sweeps at audio rate")
{
    Noise noise(3);
    Filter filter = make(1000.0f, 0.9f);
    bool finite = true;
    for (int i = 0; i < 96000; ++i) {
        const double lfo = std::sin(2.0 * std::numbers::pi * 2500.0 * i / kFs);
        filter.setParameters(static_cast<float>(1000.0 * std::exp2(4.0 * lfo)), 0.9f);
        finite = finite && std::isfinite(filter.process(noise.next()));
    }
    CHECK(finite);
}

TEST_CASE("filter rings down below self-oscillation and sustains above it")
{
    auto tailRms = [](float resonance) {
        Filter filter = make(1000.0f, resonance);
        filter.excite(0.05f);
        std::vector<float> out(48000);
        for (float& sample : out)
            sample = filter.process(0.0f);
        return std::pair{rms(std::span<const float>(out).subspan(36000)),
                         zeroCrossingFrequency(std::span<const float>(out).subspan(24000), kFs)};
    };

    CHECK(tailRms(0.7f).first < 1e-4);

    const auto [level, frequency] = tailRms(1.0f);
    CHECK(level > 0.1);
    REQUIRE(frequency.has_value());
    CHECK(std::abs(centsBetween(*frequency, 1000.0)) < 150.0);
}

TEST_CASE("reset clears the filter state")
{
    Filter filter = make(500.0f, 0.5f);
    for (int i = 0; i < 100; ++i)
        filter.process(1.0f);
    filter.reset();
    CHECK(filter.process(0.0f) == 0.0f);
}

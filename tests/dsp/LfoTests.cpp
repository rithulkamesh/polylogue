#include "dsp/Lfo.h"
#include "support/Analysis.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace polylogue::dsp;
using namespace polylogue::test;

namespace {

constexpr double kFs = 48000.0;

Lfo make(LfoWave wave, LfoMode mode, float rate)
{
    Lfo lfo;
    lfo.prepare(kFs);
    lfo.setParameters({wave, mode, rate, 1.0f, LfoTarget::Pitch});
    lfo.restart();
    return lfo;
}

std::vector<float> run(Lfo& lfo, int count)
{
    std::vector<float> out(static_cast<std::size_t>(count));
    for (float& sample : out)
        sample = lfo.next();
    return out;
}

// The knob position that produces `hz` in a mode.
float rateFor(LfoMode mode, double hz)
{
    const double low = Lfo::rateToHz(mode, 0.0f);
    const double high = Lfo::rateToHz(mode, 1.0f);
    return static_cast<float>(std::log(hz / low) / std::log(high / low));
}

}  // namespace

TEST_CASE("rate ranges match the documented limits")
{
    CHECK(std::abs(Lfo::rateToHz(LfoMode::Fast, 0.0f) - 0.5) < 1e-9);
    CHECK(std::abs(Lfo::rateToHz(LfoMode::Fast, 1.0f) - 2800.0) < 1e-6);
    CHECK(std::abs(Lfo::rateToHz(LfoMode::Slow, 0.0f) - 0.05) < 1e-9);
    CHECK(std::abs(Lfo::rateToHz(LfoMode::Slow, 1.0f) - 28.0) < 1e-9);
    CHECK(Lfo::rateToHz(LfoMode::OneShot, 1.0f) == Lfo::rateToHz(LfoMode::Slow, 1.0f));
}

TEST_CASE("rate is exponential across the knob")
{
    CHECK(std::abs(Lfo::rateToHz(LfoMode::Slow, 0.5f) - std::sqrt(0.05 * 28.0)) < 1e-9);
    double previous = 0.0;
    for (float r = 0.0f; r <= 1.0f; r += 0.05f) {
        const double hz = Lfo::rateToHz(LfoMode::Fast, r);
        CHECK(hz > previous);
        previous = hz;
    }
}

TEST_CASE("rate is clamped to the knob range")
{
    CHECK(Lfo::rateToHz(LfoMode::Fast, -3.0f) == Lfo::rateToHz(LfoMode::Fast, 0.0f));
    CHECK(Lfo::rateToHz(LfoMode::Fast, 5.0f) == Lfo::rateToHz(LfoMode::Fast, 1.0f));
}

TEST_CASE("the LFO runs at the requested frequency")
{
    for (auto [mode, hz] : {std::pair{LfoMode::Slow, 7.0}, std::pair{LfoMode::Fast, 100.0},
                            std::pair{LfoMode::Fast, 1500.0}}) {
        Lfo lfo = make(LfoWave::Triangle, mode, rateFor(mode, hz));
        const auto signal = run(lfo, 96000);
        const auto measured = zeroCrossingFrequency(signal, kFs);
        REQUIRE(measured.has_value());
        INFO("target " << hz);
        CHECK(std::abs(centsBetween(*measured, hz)) < 1.0);
    }
}

TEST_CASE("waves are bipolar and start where they should")
{
    for (LfoWave wave : {LfoWave::Saw, LfoWave::Triangle, LfoWave::Square}) {
        Lfo lfo = make(wave, LfoMode::Slow, rateFor(LfoMode::Slow, 3.0));
        const auto signal = run(lfo, 48000);
        REQUIRE(allFinite(signal));
        CHECK(peak(signal) <= 1.1f);
        CHECK(*std::min_element(signal.begin(), signal.end()) < -0.9f);
        CHECK(*std::max_element(signal.begin(), signal.end()) > 0.9f);
    }

    Lfo square = make(LfoWave::Square, LfoMode::Slow, 0.5f);
    run(square, 2);
    CHECK(square.next() > 0.9f);
}

TEST_CASE("one-shot stops after half a cycle and holds")
{
    constexpr double kHz = 10.0;  // half a cycle is 50 ms, 2400 samples
    Lfo lfo = make(LfoWave::Triangle, LfoMode::OneShot, rateFor(LfoMode::OneShot, kHz));
    const auto out = run(lfo, 9600);

    const float held = out.back();
    CHECK(std::abs(out[4000] - held) < 1e-6f);
    CHECK(std::abs(out[9000] - held) < 1e-6f);
    CHECK(std::abs(out[1000] - held) > 0.1f);
    // A triangle sweeps from -1 to +1 across the half cycle.
    CHECK(out[10] < -0.9f);
    CHECK(held > 0.9f);
}

TEST_CASE("free-running modes keep cycling")
{
    Lfo lfo = make(LfoWave::Saw, LfoMode::Slow, rateFor(LfoMode::Slow, 10.0));
    const auto out = run(lfo, 96000);
    CHECK(std::abs(out[90000] - out[89000]) > 1e-3f);
}

TEST_CASE("restart replays the same output")
{
    Lfo lfo = make(LfoWave::Triangle, LfoMode::OneShot, 0.6f);
    const auto first = run(lfo, 5000);
    lfo.restart();
    CHECK(run(lfo, 5000) == first);
}

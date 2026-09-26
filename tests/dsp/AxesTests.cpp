#include "dsp/Axes.h"
#include "presets/FactoryPresets.h"
#include "support/Analysis.h"
#include "support/EngineRender.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

using namespace polylogue;
using namespace polylogue::dsp;
using namespace polylogue::test;
using Catch::Approx;

namespace {

Axes withAxis(Axes axes, Axis axis, float value)
{
    axes[axis] = value;
    return axes;
}

// Every parameter that differs between two sets.
std::vector<Param> differing(const ParamValues& a, const ParamValues& b)
{
    std::vector<Param> result;
    for (std::size_t i = 0; i < kParamCount; ++i) {
        if (a.values[i] != b.values[i])
            result.push_back(static_cast<Param>(i));
    }
    return result;
}

}  // namespace

TEST_CASE("every axis position produces legal parameter values")
{
    std::mt19937 rng(7);
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
    for (int trial = 0; trial < 500; ++trial) {
        Axes axes;
        for (float& v : axes.values)
            v = uniform(rng);
        const ParamValues values = fromAxes(axes);
        for (std::size_t i = 0; i < kParamCount; ++i) {
            const ParamSpec& spec = paramSpecs()[i];
            INFO(spec.id);
            CHECK(std::isfinite(values.values[i]));
            CHECK(values.values[i] == clampToLegal(spec, values.values[i]));
        }
    }
}

TEST_CASE("the axes move the sound in the direction their names say")
{
    const Axes base = Axes::neutral();
    float previousCutoff = 0.0f;
    float previousAttack = 0.0f;
    float previousDecay = 0.0f;
    for (int step = 0; step <= 10; ++step) {
        const float t = static_cast<float>(step) / 10.0f;
        const float cutoff = fromAxes(withAxis(base, Axis::Bright, t))[Param::Cutoff];
        const float attack = fromAxes(withAxis(base, Axis::Attack, t))[Param::AmpAttack];
        CHECK(cutoff >= previousCutoff);
        CHECK(attack >= previousAttack);
        previousCutoff = cutoff;
        previousAttack = attack;
    }
    // Decay grows along the struck part of SUSTAIN; past it the sound is held instead.
    for (int step = 0; step <= 10; ++step) {
        const float decay = fromAxes(withAxis(
            base, Axis::Sustain, 0.69f * static_cast<float>(step) / 10.0f))[Param::AmpDecay];
        CHECK(decay >= previousDecay);
        previousDecay = decay;
    }
    CHECK(fromAxes(withAxis(base, Axis::Metal, 1.0f))[Param::Osc2SyncRing] ==
          static_cast<float>(SyncRing::Fm));
    CHECK(fromAxes(withAxis(base, Axis::Metal, 0.0f))[Param::Osc2SyncRing] ==
          static_cast<float>(SyncRing::Off));
}

TEST_CASE("with every knob at home the sound is exactly the stored sound")
{
    for (const presets::FactoryPreset& preset : presets::factoryPresets()) {
        INFO(preset.name);
        const ParamValues stored = presets::resolve(preset);
        REQUIRE(knobsOf(stored).values == homeOf(stored).values);
        CHECK(effectiveValues(stored).values == stored.values);
    }
}

TEST_CASE("one knob moves only what it drives")
{
    ParamValues stored = ParamValues::defaults();
    setKnobs(stored, withAxis(Axes::neutral(), Axis::Bright, 0.2f));
    const std::vector<Param> changed = differing(effectiveValues(stored), stored);
    REQUIRE(changed.size() == 1);
    CHECK(changed.front() == Param::Cutoff);
}

TEST_CASE("a knob moves a hand-tuned sound by the same amount as an untouched one")
{
    ParamValues tuned = ParamValues::defaults();
    tuned[Param::Cutoff] = 1000.0f;  // fine-tuned away from what the axes would set
    const float lowered = 0.3f;
    setKnobs(tuned, withAxis(Axes::neutral(), Axis::Bright, lowered));

    const ParamValues from = fromAxes(Axes::neutral());
    const ParamValues to = fromAxes(withAxis(Axes::neutral(), Axis::Bright, lowered));
    const float axisRatio = to[Param::Cutoff] / from[Param::Cutoff];
    CHECK(effectiveValues(tuned)[Param::Cutoff] / 1000.0f == Approx(axisRatio).epsilon(0.02));
}

TEST_CASE("oscillator 2's pitch moves as one value across its range switch")
{
    ParamValues stored = ParamValues::defaults();
    stored[Param::Osc2Octave] = 2.0f;   // 4'
    stored[Param::Osc2Pitch] = 300.0f;  // 1500 cents in all
    const Axes from = Axes::neutral();
    setHome(stored, from);
    setKnobs(stored, withAxis(from, Axis::Metal, 0.45f));

    const auto total = [](const ParamValues& v) {
        constexpr std::array<float, 4> kCents = {-1200.0f, 0.0f, 1200.0f, 2400.0f};
        return kCents[static_cast<std::size_t>(v[Param::Osc2Octave])] + v[Param::Osc2Pitch];
    };
    const float wanted =
        total(stored) + total(fromAxes(withAxis(from, Axis::Metal, 0.45f))) - total(fromAxes(from));
    CHECK(total(effectiveValues(stored)) == Approx(wanted).margin(1.0));
}

TEST_CASE("baking keeps the sound and makes the knobs the new home")
{
    ParamValues stored = presets::resolve(presets::factoryPresets()[3]);
    Axes moved = knobsOf(stored);
    moved[Axis::Bright] = 0.9f;
    moved[Axis::Metal] = 0.5f;
    moved[Axis::Motion] = 0.8f;
    setKnobs(stored, moved);

    const ParamValues playing = effectiveValues(stored);
    const ParamValues baked = bake(stored);
    CHECK(homeOf(baked).values == knobsOf(baked).values);
    CHECK(knobsOf(baked).values == moved.values);
    CHECK(effectiveValues(baked).values == baked.values);
    for (std::size_t i = 0; i < index(Param::AxisWave); ++i) {
        INFO(paramSpecs()[i].id);
        CHECK(baked.values[i] == playing.values[i]);
    }
    CHECK(bake(baked).values == baked.values);
}

TEST_CASE("every knob position is a playable sound")
{
    // Sweep each axis with the others at a neutral spot: never silent, never loud enough to clip.
    for (std::size_t a = 0; a < kAxisCount; ++a) {
        for (int step = 0; step <= 10; ++step) {
            Axes axes = Axes::neutral();
            axes.values[a] = static_cast<float>(step) / 10.0f;
            ParamValues values = fromAxes(axes);
            values[Param::Level] = -6.0f;
            const std::vector<TimedEvent> events = {at(0.0, MidiEvent::noteOn(0, 55, 0.8f)),
                                                    at(0.6, MidiEvent::noteOff(0, 55))};
            const std::vector<float> audio = renderEngine(toSettings(values), events, 1.2);
            INFO(axisName(static_cast<Axis>(a)) << " at " << axes.values[a]);
            CHECK(allFinite(audio));
            CHECK(peak(audio) > 0.005f);
            CHECK(peak(audio) < 2.5f);
        }
    }
}

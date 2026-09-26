#include "dsp/ModMatrix.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace polylogue::dsp;

namespace {

constexpr double kFs = 48000.0;

ModMatrix make(const SynthSettings& settings)
{
    ModMatrix matrix;
    matrix.prepare(kFs);
    matrix.configure(settings);
    matrix.snap();
    return matrix;
}

float at(const ModOffsets& offsets, ModDestination destination)
{
    return offsets[index(destination)];
}

}  // namespace

TEST_CASE("with no modulation every destination is zero")
{
    ModMatrix matrix = make(SynthSettings{});
    const ModOffsets offsets = matrix.evaluate({1.0f, 1.0f});
    for (float offset : offsets)
        CHECK(offset == 0.0f);
}

TEST_CASE("the envelope target selects where it goes")
{
    SynthSettings s;
    s.modEnvAmount = 0.5f;

    s.modEnvTarget = EnvelopeTarget::Cutoff;
    ModOffsets o = make(s).evaluate({1.0f, 0.0f});
    CHECK(std::abs(at(o, ModDestination::Cutoff) - 4.0f) < 1e-5f);
    CHECK(at(o, ModDestination::Osc1Pitch) == 0.0f);
    CHECK(at(o, ModDestination::Osc2Pitch) == 0.0f);

    s.modEnvTarget = EnvelopeTarget::Pitch;
    o = make(s).evaluate({1.0f, 0.0f});
    CHECK(std::abs(at(o, ModDestination::Osc1Pitch) - 12.0f) < 1e-4f);
    CHECK(std::abs(at(o, ModDestination::Osc2Pitch) - 12.0f) < 1e-4f);
    CHECK(at(o, ModDestination::Cutoff) == 0.0f);

    s.modEnvTarget = EnvelopeTarget::Pitch2;
    o = make(s).evaluate({1.0f, 0.0f});
    CHECK(at(o, ModDestination::Osc1Pitch) == 0.0f);
    CHECK(std::abs(at(o, ModDestination::Osc2Pitch) - 12.0f) < 1e-4f);
}

TEST_CASE("the LFO target selects where it goes")
{
    SynthSettings s;
    s.lfo.amount = 1.0f;

    s.lfo.target = LfoTarget::Pitch;
    ModOffsets o = make(s).evaluate({0.0f, 1.0f});
    CHECK(std::abs(at(o, ModDestination::Osc1Pitch) - 12.0f) < 1e-4f);
    CHECK(std::abs(at(o, ModDestination::Osc2Pitch) - 12.0f) < 1e-4f);

    s.lfo.target = LfoTarget::Shape;
    o = make(s).evaluate({0.0f, 1.0f});
    CHECK(std::abs(at(o, ModDestination::Osc1Shape) - 0.5f) < 1e-5f);
    CHECK(std::abs(at(o, ModDestination::Osc2Shape) - 0.5f) < 1e-5f);

    s.lfo.target = LfoTarget::Cutoff;
    o = make(s).evaluate({0.0f, 1.0f});
    CHECK(std::abs(at(o, ModDestination::Cutoff) - 6.0f) < 1e-5f);
}

TEST_CASE("negative amounts invert, and the curve is fine near zero")
{
    SynthSettings s;
    s.lfo.amount = -1.0f;
    CHECK(std::abs(at(make(s).evaluate({0.0f, 1.0f}), ModDestination::Osc1Pitch) + 12.0f) < 1e-4f);

    s.lfo.amount = 0.1f;
    CHECK(std::abs(at(make(s).evaluate({0.0f, 1.0f}), ModDestination::Osc1Pitch) - 0.12f) < 1e-5f);
}

TEST_CASE("sources add together at a shared destination")
{
    SynthSettings s;
    s.modEnvAmount = 0.5f;
    s.lfo.target = LfoTarget::Cutoff;
    s.lfo.amount = 1.0f;
    const ModOffsets o = make(s).evaluate({1.0f, 0.5f});
    CHECK(std::abs(at(o, ModDestination::Cutoff) - (4.0f + 3.0f)) < 1e-4f);
}

TEST_CASE("switching a target crossfades instead of jumping")
{
    SynthSettings s;
    s.lfo.amount = 1.0f;
    s.lfo.target = LfoTarget::Pitch;
    ModMatrix matrix = make(s);

    s.lfo.target = LfoTarget::Cutoff;
    matrix.configure(s);
    const ModOffsets first = matrix.evaluate({0.0f, 1.0f});
    CHECK(at(first, ModDestination::Osc1Pitch) > 11.0f);
    CHECK(at(first, ModDestination::Cutoff) < 0.1f);

    ModOffsets last{};
    for (int i = 0; i < 4800; ++i)
        last = matrix.evaluate({0.0f, 1.0f});
    CHECK(std::abs(at(last, ModDestination::Osc1Pitch)) < 0.01f);
    CHECK(std::abs(at(last, ModDestination::Cutoff) - 6.0f) < 0.01f);
}

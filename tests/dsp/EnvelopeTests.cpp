#include "dsp/Envelope.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>

using namespace polylogue::dsp;

namespace {

constexpr double kFs = 48000.0;

int samples(double seconds)
{
    return static_cast<int>(std::lround(seconds * kFs));
}

float run(Envelope& env, int count)
{
    float level = 0.0f;
    for (int i = 0; i < count; ++i)
        level = env.next();
    return level;
}

Envelope make(EnvelopeType type, float attack, float decay)
{
    Envelope env;
    env.setParameters({type, attack, decay}, kFs);
    return env;
}

}  // namespace

TEST_CASE("attack reaches full level in the requested time")
{
    Envelope env = make(EnvelopeType::AGD, 0.010f, 0.2f);
    env.noteOn();

    int reached = -1;
    for (int i = 0; i < samples(0.05); ++i) {
        if (env.next() >= 1.0f) {
            reached = i;
            break;
        }
    }
    REQUIRE(reached >= 0);
    CHECK(std::abs(reached - samples(0.010)) <= 2);
}

TEST_CASE("attack rises monotonically")
{
    Envelope env = make(EnvelopeType::AGD, 0.05f, 0.2f);
    env.noteOn();
    float previous = 0.0f;
    for (int i = 0; i < samples(0.05); ++i) {
        const float level = env.next();
        CHECK(level >= previous);
        previous = level;
    }
}

TEST_CASE("A/G/D holds while the gate is open and decays after note-off")
{
    Envelope env = make(EnvelopeType::AGD, 0.002f, 0.100f);
    env.noteOn();
    CHECK(run(env, samples(1.0)) == 1.0f);

    env.noteOff();
    const float atDecayTime = run(env, samples(0.100));
    CHECK(atDecayTime > 0.0008f);
    CHECK(atDecayTime < 0.0012f);

    run(env, samples(0.05));
    CHECK(env.isIdle());
}

TEST_CASE("release from mid-attack decays from the current level")
{
    Envelope env = make(EnvelopeType::AGD, 0.100f, 0.100f);
    env.noteOn();
    const float atRelease = run(env, samples(0.030));
    REQUIRE(atRelease > 0.05f);
    REQUIRE(atRelease < 0.95f);

    env.noteOff();
    CHECK(env.next() < atRelease);
}

TEST_CASE("A/D ignores note-off and always completes both stages")
{
    Envelope env = make(EnvelopeType::AD, 0.005f, 0.200f);
    env.noteOn();
    run(env, samples(0.001));
    env.noteOff();

    CHECK(run(env, samples(0.005)) > 0.5f);
    CHECK(run(env, samples(0.150)) > 0.001f);
    run(env, samples(0.300));
    CHECK(env.isIdle());
}

TEST_CASE("gate type switches quickly without clicking")
{
    Envelope env = make(EnvelopeType::Gate, 1.0f, 1.0f);
    env.noteOn();
    CHECK(run(env, samples(0.004)) == 1.0f);

    env.noteOff();
    run(env, samples(0.010));
    CHECK(env.isIdle());
}

TEST_CASE("note-on restarts from zero")
{
    Envelope env = make(EnvelopeType::AGD, 0.050f, 0.5f);
    env.noteOn();
    run(env, samples(0.2));
    REQUIRE(env.next() == 1.0f);

    env.noteOn();
    CHECK(env.next() < 0.05f);
}

TEST_CASE("idle envelope outputs silence and ignores note-off")
{
    Envelope env = make(EnvelopeType::AGD, 0.01f, 0.1f);
    env.noteOff();
    CHECK(env.isIdle());
    CHECK(env.next() == 0.0f);
}

#include "dsp/Tuning.h"
#include "dsp/Voice.h"
#include "support/Analysis.h"
#include "support/VoiceRender.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <random>
#include <span>
#include <vector>

using namespace polylogue::dsp;
using namespace polylogue::test;

namespace {

std::span<const float> slice(const std::vector<float>& v, double fromSeconds, double toSeconds)
{
    const auto from = static_cast<std::size_t>(fromSeconds * kSampleRate);
    const auto to = static_cast<std::size_t>(toSeconds * kSampleRate);
    return std::span<const float>(v).subspan(from, to - from);
}

}  // namespace

TEST_CASE("an idle voice is silent and inactive")
{
    Voice voice;
    voice.prepare(kSampleRate);
    std::vector<float> out(256, 0.0f);
    voice.render(out.data(), 256, plainSaw());
    CHECK(peak(out) == 0.0f);
    CHECK_FALSE(voice.isActive());
}

TEST_CASE("a voice plays the requested pitch")
{
    for (int note : {33, 45, 60, 72, 84}) {
        const auto out = renderNote(plainSaw(), {note, 1.0f, 1.5}, 1.5);
        const auto hz = zeroCrossingFrequency(slice(out, 0.2, 1.4), kSampleRate);
        REQUIRE(hz.has_value());
        INFO("note " << note);
        CHECK(std::abs(centsBetween(*hz, midiToHz(note))) < 0.5);
    }
}

TEST_CASE("octave, tune and bend offset the pitch")
{
    SynthSettings s = plainSaw();
    s.octave = 1;
    s.tuneCents = 50.0f;
    s.pitchBendSemitones = -2.0f;
    const auto out = renderNote(s, {48, 1.0f, 1.5}, 1.5);
    const auto hz = zeroCrossingFrequency(slice(out, 0.2, 1.4), kSampleRate);
    REQUIRE(hz.has_value());
    CHECK(std::abs(centsBetween(*hz, midiToHz(48 + 12 + 0.5 - 2.0))) < 1.0);
}

TEST_CASE("oscillator 2 follows its range and cents")
{
    SynthSettings s = plainSaw();
    s.osc1Level = 0.0f;
    s.osc2Level = 1.0f;
    s.osc2Range = Osc2Range::Feet4;
    s.osc2Cents = 700.0f;
    const auto out = renderNote(s, {45, 1.0f, 1.5}, 1.5);
    const auto hz = zeroCrossingFrequency(slice(out, 0.2, 1.4), kSampleRate);
    REQUIRE(hz.has_value());
    CHECK(std::abs(centsBetween(*hz, midiToHz(45 + 12 + 7))) < 1.0);
}

TEST_CASE("hard sync keeps the pitch of oscillator 1")
{
    SynthSettings s = plainSaw();
    s.osc1Level = 0.0f;
    s.osc2Level = 1.0f;
    s.osc2Cents = 900.0f;
    s.syncRing = SyncRing::Sync;
    const auto out = renderNote(s, {40, 1.0f, 1.5}, 1.5);
    REQUIRE(allFinite(out));

    // The synced waveform repeats at oscillator 1's period, so one period apart matches.
    const double period = kSampleRate / midiToHz(40);
    double error = 0.0;
    const auto from = static_cast<std::size_t>(kSampleRate * 0.5);
    for (std::size_t i = from; i < from + 4000; ++i) {
        const auto lag = static_cast<std::size_t>(std::llround(period * 20.0));
        error += std::abs(static_cast<double>(out[i + lag] - out[i]));
    }
    CHECK(error / 4000.0 < 0.05);
}

TEST_CASE("ring modulation and noise produce sound")
{
    SynthSettings ring = plainSaw();
    ring.osc2Level = 1.0f;
    ring.osc2Cents = 350.0f;
    ring.syncRing = SyncRing::Ring;
    const auto ringOut = renderNote(ring, {50, 1.0f, 1.0}, 1.0);
    REQUIRE(allFinite(ringOut));
    CHECK(rms(slice(ringOut, 0.1, 0.9)) > 0.05);

    SynthSettings noisy = plainSaw();
    noisy.osc1Level = 0.0f;
    noisy.osc2Level = 1.0f;
    noisy.osc2Wave = Osc2Wave::Noise;
    const auto noiseOut = renderNote(noisy, {50, 1.0f, 1.0}, 1.0);
    CHECK(rms(slice(noiseOut, 0.1, 0.9)) > 0.2);
    CHECK(std::abs(mean(slice(noiseOut, 0.1, 0.9))) < 0.02);
}

TEST_CASE("note-off fades the voice to silence and deactivates it")
{
    Voice voice;
    voice.prepare(kSampleRate);
    SynthSettings s = plainSaw();
    s.ampEnv = {EnvelopeType::AGD, 0.001f, 0.1f};

    voice.noteOn(60, 1.0f);
    std::vector<float> block(kBlock, 0.0f);
    for (int i = 0; i < 100; ++i)
        voice.render(block.data(), kBlock, s);
    REQUIRE(voice.isActive());

    voice.noteOff();
    for (int i = 0; i < 400; ++i) {
        std::fill(block.begin(), block.end(), 0.0f);
        voice.render(block.data(), kBlock, s);
    }
    CHECK_FALSE(voice.isActive());
    CHECK(peak(block) == 0.0f);
}

TEST_CASE("velocity scales the level by the velocity-to-amp amount")
{
    SynthSettings s = plainSaw();
    s.velocityToAmp = 1.0f;
    const auto loud = renderNote(s, {50, 1.0f, 1.0}, 1.0);
    const auto soft = renderNote(s, {50, 0.5f, 1.0}, 1.0);
    const double ratio = rms(slice(soft, 0.2, 0.9)) / rms(slice(loud, 0.2, 0.9));
    CHECK(std::abs(ratio - 0.5) < 0.02);

    s.velocityToAmp = 0.0f;
    const auto flat = renderNote(s, {50, 0.5f, 1.0}, 1.0);
    CHECK(std::abs(rms(slice(flat, 0.2, 0.9)) / rms(slice(loud, 0.2, 0.9)) - 1.0) < 0.01);
}

TEST_CASE("the mod envelope opens the filter and then closes it")
{
    SynthSettings s = plainSaw();
    s.cutoffHz = 200.0f;
    s.resonance = 0.2f;
    s.ampEnv = {EnvelopeType::AGD, 0.001f, 0.1f};
    s.modEnv = {EnvelopeType::AD, 0.001f, 0.25f};
    s.modEnvAmount = 1.0f;

    const auto out = renderNote(s, {40, 1.0f, 1.5}, 1.5);
    auto brightness = [&](double from, double to) {
        const auto part = slice(out, from, to);
        std::vector<float> diff(part.size() - 1);
        for (std::size_t i = 0; i + 1 < part.size(); ++i)
            diff[i] = part[i + 1] - part[i];
        return rms(diff) / rms(part);
    };
    CHECK(brightness(0.005, 0.05) > 2.0 * brightness(1.0, 1.4));
}

TEST_CASE("key tracking raises the cutoff with pitch")
{
    SynthSettings s = plainSaw();
    s.cutoffHz = 400.0f;
    auto brightnessAt = [&](float keyTrack) {
        s.keyTrack = keyTrack;
        const auto out = renderNote(s, {72, 1.0f, 1.0}, 1.0);
        return rms(slice(out, 0.1, 0.9));
    };
    CHECK(brightnessAt(1.0f) > 1.5 * brightnessAt(0.0f));
}

TEST_CASE("voices with the same seed and settings are deterministic")
{
    SynthSettings s = plainSaw();
    s.osc2Wave = Osc2Wave::Noise;
    s.osc2Level = 0.5f;
    CHECK(renderNote(s, {55, 0.8f, 0.5}, 0.6) == renderNote(s, {55, 0.8f, 0.5}, 0.6));
}

TEST_CASE("random settings never produce non-finite or runaway output")
{
    std::mt19937 rng(1234);
    auto uniform = [&](float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng);
    };
    auto pick = [&](int count) { return static_cast<int>(rng() % static_cast<unsigned>(count)); };

    for (int trial = 0; trial < 150; ++trial) {
        SynthSettings s;
        s.octave = pick(5) - 2;
        s.tuneCents = uniform(-50.0f, 50.0f);
        s.pitchBendSemitones = uniform(-12.0f, 12.0f);
        s.osc1Wave = static_cast<Osc1Wave>(pick(3));
        s.osc1Shape = uniform(0.0f, 1.0f);
        s.osc1Level = uniform(0.0f, 1.0f);
        s.osc2Wave = static_cast<Osc2Wave>(pick(3));
        s.osc2Range = static_cast<Osc2Range>(pick(4));
        s.osc2Cents = uniform(-1200.0f, 1200.0f);
        s.syncRing = static_cast<SyncRing>(pick(3));
        s.osc2Shape = uniform(0.0f, 1.0f);
        s.osc2Level = uniform(0.0f, 1.0f);
        s.cutoffHz = std::exp2(uniform(std::log2(20.0f), std::log2(20000.0f)));
        s.resonance = uniform(0.0f, 1.0f);
        s.keyTrack = static_cast<float>(pick(3)) * 0.5f;
        s.ampEnv = {static_cast<EnvelopeType>(pick(3)), uniform(0.001f, 0.5f),
                    uniform(0.005f, 0.5f)};
        s.modEnv = {static_cast<EnvelopeType>(pick(3)), uniform(0.001f, 0.5f),
                    uniform(0.005f, 0.5f)};
        s.modEnvAmount = uniform(-1.0f, 1.0f);

        const auto out = renderNote(s, {pick(100) + 10, uniform(0.05f, 1.0f), 0.3}, 0.6);
        INFO("trial " << trial);
        REQUIRE(allFinite(out));
        CHECK(peak(out) < 8.0f);
    }
}

TEST_CASE("an LFO on pitch produces vibrato of the expected depth")
{
    SynthSettings s = plainSaw();
    s.lfo = {LfoWave::Square, LfoMode::Slow, 0.583f, 0.5f,
             LfoTarget::Pitch};  // about 2 Hz, +/-3 st
    const auto out = renderNote(s, {50, 1.0f, 1.5}, 1.5);

    const auto up = zeroCrossingFrequency(slice(out, 0.05, 0.2), kSampleRate);
    const auto down = zeroCrossingFrequency(slice(out, 0.3, 0.45), kSampleRate);
    REQUIRE(up.has_value());
    REQUIRE(down.has_value());
    CHECK(std::abs(centsBetween(*up, midiToHz(50 + 3))) < 5.0);
    CHECK(std::abs(centsBetween(*down, midiToHz(50 - 3))) < 5.0);
}

TEST_CASE("a negative LFO amount inverts the modulation")
{
    SynthSettings s = plainSaw();
    s.lfo = {LfoWave::Square, LfoMode::Slow, 0.583f, -0.5f, LfoTarget::Pitch};
    const auto out = renderNote(s, {50, 1.0f, 1.0}, 1.0);
    const auto first = zeroCrossingFrequency(slice(out, 0.05, 0.2), kSampleRate);
    REQUIRE(first.has_value());
    CHECK(std::abs(centsBetween(*first, midiToHz(50 - 3))) < 5.0);
}

TEST_CASE("the mod envelope on pitch drops from above and settles on the note")
{
    SynthSettings s = plainSaw();
    s.modEnv = {EnvelopeType::AD, 0.001f, 0.3f};
    s.modEnvAmount = 0.5f;
    s.modEnvTarget = EnvelopeTarget::Pitch;
    s.ampEnv = {EnvelopeType::AGD, 0.001f, 0.1f};
    const auto out = renderNote(s, {45, 1.0f, 1.6}, 1.6);

    const auto early = zeroCrossingFrequency(slice(out, 0.01, 0.04), kSampleRate);
    const auto late = zeroCrossingFrequency(slice(out, 1.0, 1.5), kSampleRate);
    REQUIRE(early.has_value());
    REQUIRE(late.has_value());
    CHECK(centsBetween(*early, midiToHz(45)) > 500.0);
    CHECK(std::abs(centsBetween(*late, midiToHz(45))) < 5.0);
}

TEST_CASE("pitch 2 leaves oscillator 1 alone")
{
    SynthSettings s = plainSaw();
    s.modEnv = {EnvelopeType::AD, 0.001f, 0.3f};
    s.modEnvAmount = 0.5f;
    s.modEnvTarget = EnvelopeTarget::Pitch2;
    s.osc2Level = 0.0f;
    const auto out = renderNote(s, {45, 1.0f, 1.0}, 1.0);
    const auto early = zeroCrossingFrequency(slice(out, 0.01, 0.1), kSampleRate);
    REQUIRE(early.has_value());
    CHECK(std::abs(centsBetween(*early, midiToHz(45))) < 1.0);
}

TEST_CASE("an LFO on shape changes the tone while the pitch stays put")
{
    SynthSettings plain = plainSaw();
    plain.osc1Wave = Osc1Wave::Square;

    SynthSettings modulated = plain;
    modulated.lfo = {LfoWave::Triangle, LfoMode::Slow, 0.6f, 1.0f, LfoTarget::Shape};

    const auto still = renderNote(plain, {50, 1.0f, 1.0}, 1.0);
    const auto moving = renderNote(modulated, {50, 1.0f, 1.0}, 1.0);
    CHECK(std::abs(rms(slice(still, 0.2, 0.9)) - rms(slice(moving, 0.2, 0.9))) > 0.02);
}

TEST_CASE("a fast LFO on cutoff runs at audio rate without instability")
{
    SynthSettings s = plainSaw();
    s.cutoffHz = 800.0f;
    s.resonance = 0.8f;
    s.lfo = {LfoWave::Triangle, LfoMode::Fast, 0.9f, 1.0f, LfoTarget::Cutoff};
    const auto out = renderNote(s, {40, 1.0f, 1.0}, 1.0);
    REQUIRE(allFinite(out));
    CHECK(peak(out) < 6.0f);
    CHECK(rms(slice(out, 0.1, 0.9)) > 0.01);
}

TEST_CASE("drive brightens the voice and keeps its level")
{
    SynthSettings clean = plainSaw();
    clean.osc1Wave = Osc1Wave::Triangle;
    clean.cutoffHz = 3000.0f;
    SynthSettings driven = clean;
    driven.drive = 0.8f;

    auto brightness = [](const std::vector<float>& out) {
        const auto part = slice(out, 0.2, 0.9);
        std::vector<float> diff(part.size() - 1);
        for (std::size_t i = 0; i + 1 < part.size(); ++i)
            diff[i] = part[i + 1] - part[i];
        return rms(diff) / rms(part);
    };
    const auto cleanOut = renderNote(clean, {45, 1.0f, 1.0}, 1.0);
    const auto drivenOut = renderNote(driven, {45, 1.0f, 1.0}, 1.0);
    CHECK(brightness(drivenOut) > 1.2 * brightness(cleanOut));
    CHECK(rms(slice(drivenOut, 0.2, 0.9)) < 2.0 * rms(slice(cleanOut, 0.2, 0.9)));
    CHECK(peak(drivenOut) < 1.0f);
}

TEST_CASE("a note-on after a note fades cleanly and starts the new pitch")
{
    Voice voice;
    voice.prepare(kSampleRate);
    SynthSettings s = plainSaw();
    s.ampEnv = {EnvelopeType::AGD, 0.001f, 0.2f};

    std::vector<float> out(static_cast<std::size_t>(kSampleRate), 0.0f);
    voice.noteOn(45, 1.0f);
    for (std::size_t pos = 0; pos < out.size(); pos += kBlock) {
        if (pos == 24000)
            voice.noteOn(57, 1.0f);
        voice.render(out.data() + pos, kBlock, s);
    }
    const auto after = zeroCrossingFrequency(slice(out, 0.6, 0.95), kSampleRate);
    REQUIRE(after.has_value());
    CHECK(std::abs(centsBetween(*after, midiToHz(57))) < 1.0);
}

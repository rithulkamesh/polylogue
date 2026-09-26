#include "dsp/Tuning.h"
#include "dsp/VoiceManager.h"
#include "support/Analysis.h"
#include "support/EngineRender.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
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

SynthSettings monoSettings()
{
    SynthSettings s = plainSaw();
    s.keyMode = KeyMode::Mono;
    s.ampEnv = {EnvelopeType::AGD, 0.001f, 0.05f};
    return s;
}

double frequencyIn(const std::vector<float>& out, double from, double to)
{
    return zeroCrossingFrequency(slice(out, from, to), kSampleRate).value_or(0.0);
}

}  // namespace

TEST_CASE("mono plays one voice with last-note priority")
{
    VoiceManager manager;
    manager.prepare(kSampleRate);
    manager.setPlayStyle(KeyMode::Mono, 0.0f, GlideMode::Auto);

    manager.noteOn(60, 0.8f);
    manager.noteOn(64, 0.8f);
    manager.noteOn(67, 0.8f);
    CHECK(manager.info(0).note == 67);
    CHECK(manager.info(1).active == false);
}

TEST_CASE("releasing the top key returns to the one below")
{
    VoiceManager manager;
    manager.prepare(kSampleRate);
    manager.setPlayStyle(KeyMode::Mono, 0.0f, GlideMode::Auto);

    manager.noteOn(60, 0.8f);
    manager.noteOn(64, 0.8f);
    manager.noteOn(67, 0.8f);

    manager.noteOff(67);
    CHECK(manager.info(0).note == 64);
    CHECK(manager.info(0).held);

    manager.noteOff(60);  // not the sounding key: nothing changes
    CHECK(manager.info(0).note == 64);

    manager.noteOff(64);
    CHECK_FALSE(manager.info(0).held);
}

TEST_CASE("mono retriggers the envelope on every note without glide")
{
    SynthSettings s = monoSettings();
    s.ampEnv = {EnvelopeType::AGD, 0.3f, 0.1f};
    const auto out = renderEngine(
        s, {at(0.0, MidiEvent::noteOn(0, 50, 0.9f)), at(1.0, MidiEvent::noteOn(0, 55, 0.9f))}, 1.5);
    CHECK(rms(slice(out, 1.03, 1.06)) < 0.3 * rms(slice(out, 0.8, 0.9)));
}

TEST_CASE("mono with glide and overlapping notes is single-trigger")
{
    SynthSettings s = monoSettings();
    s.ampEnv = {EnvelopeType::AGD, 0.3f, 0.1f};
    s.glideSeconds = 0.1f;
    const auto out = renderEngine(
        s, {at(0.0, MidiEvent::noteOn(0, 50, 0.9f)), at(1.0, MidiEvent::noteOn(0, 55, 0.9f))}, 1.5);
    CHECK(rms(slice(out, 1.03, 1.06)) > 0.9 * rms(slice(out, 0.8, 0.9)));
}

TEST_CASE("mono glide slides the pitch between overlapping notes")
{
    SynthSettings s = monoSettings();
    s.glideSeconds = 0.3f;
    const auto out = renderEngine(
        s, {at(0.0, MidiEvent::noteOn(0, 48, 0.9f)), at(0.5, MidiEvent::noteOn(0, 60, 0.9f))}, 1.5);

    CHECK(std::abs(centsBetween(frequencyIn(out, 0.3, 0.45), midiToHz(48))) < 1.0);
    const double early = frequencyIn(out, 0.52, 0.56);
    CHECK(early > midiToHz(48.5));
    CHECK(early < midiToHz(58.0));
    CHECK(std::abs(centsBetween(frequencyIn(out, 1.2, 1.45), midiToHz(60))) < 5.0);
}

TEST_CASE("glide mode Auto ignores separated notes, On glides them")
{
    auto startPitch = [](GlideMode mode) {
        SynthSettings s = monoSettings();
        s.glideSeconds = 0.4f;
        s.glideMode = mode;
        const auto out = renderEngine(s,
                                      {at(0.0, MidiEvent::noteOn(0, 48, 0.9f)),
                                       at(0.4, MidiEvent::noteOff(0, 48)),
                                       at(0.8, MidiEvent::noteOn(0, 72, 0.9f))},
                                      1.6);
        return frequencyIn(out, 0.81, 0.85);
    };

    CHECK(std::abs(centsBetween(startPitch(GlideMode::Auto), midiToHz(72))) < 20.0);
    CHECK(startPitch(GlideMode::On) < midiToHz(66));
}

TEST_CASE("poly glide starts each note from the previous one")
{
    SynthSettings s = plainSaw();
    s.glideSeconds = 0.3f;
    s.glideMode = GlideMode::On;
    const auto out =
        renderEngine(s,
                     {at(0.0, MidiEvent::noteOn(0, 48, 0.9f)), at(0.3, MidiEvent::noteOff(0, 48)),
                      at(0.6, MidiEvent::noteOn(0, 60, 0.9f))},
                     1.5);
    CHECK(frequencyIn(out, 0.61, 0.66) < midiToHz(56));
    CHECK(std::abs(centsBetween(frequencyIn(out, 1.2, 1.45), midiToHz(60))) < 5.0);
}

TEST_CASE("mono honours the sustain pedal")
{
    SynthSettings s = monoSettings();
    const auto out = renderEngine(
        s,
        {at(0.0, MidiEvent::sustain(0, true)), at(0.01, MidiEvent::noteOn(0, 50, 0.9f)),
         at(0.1, MidiEvent::noteOff(0, 50)), at(0.8, MidiEvent::sustain(0, false))},
        1.5);
    CHECK(rms(slice(out, 0.4, 0.7)) > 0.05);
    CHECK(peak(slice(out, 1.2, 1.5)) == 0.0f);
}

TEST_CASE("switching key mode releases held notes")
{
    VoiceManager manager;
    manager.prepare(kSampleRate);
    manager.noteOn(60, 0.8f);
    manager.noteOn(64, 0.8f);
    manager.setPlayStyle(KeyMode::Mono, 0.0f, GlideMode::Auto);
    CHECK_FALSE(manager.info(0).held);
    CHECK_FALSE(manager.info(1).held);
}

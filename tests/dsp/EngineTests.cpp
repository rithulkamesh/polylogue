#include "dsp/Engine.h"
#include "dsp/Tuning.h"
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

SynthSettings engineSettings()
{
    SynthSettings s = plainSaw();
    s.ampEnv = {EnvelopeType::AGD, 0.002f, 0.1f};
    return s;
}

}  // namespace

TEST_CASE("an idle engine is silent")
{
    const auto out = renderEngine(engineSettings(), {}, 0.2);
    CHECK(peak(out) == 0.0f);
}

TEST_CASE("note-on lands on its exact sample")
{
    constexpr int kOffset = 1000;
    const auto out = renderEngine(engineSettings(),
                                  {at(kOffset / kSampleRate, MidiEvent::noteOn(0, 57, 0.9f))}, 0.2);

    for (int i = 0; i < kOffset; ++i)
        REQUIRE(out[static_cast<std::size_t>(i)] == 0.0f);
    CHECK(peak(std::span<const float>(out).subspan(kOffset, 400)) > 0.05f);
}

TEST_CASE("output does not depend on how the host splits the blocks")
{
    const std::vector<TimedEvent> events = {
        at(0.010, MidiEvent::noteOn(0, 48, 0.8f)), at(0.033, MidiEvent::noteOn(0, 55, 0.6f)),
        at(0.120, MidiEvent::pitchBend(0, 0.5f)),  at(0.200, MidiEvent::noteOff(0, 48)),
        at(0.250, MidiEvent::sustain(0, true)),    at(0.260, MidiEvent::noteOff(0, 55)),
        at(0.400, MidiEvent::sustain(0, false)),
    };
    SynthSettings s = engineSettings();
    s.cutoffHz = 900.0f;
    s.resonance = 0.5f;
    s.osc2Level = 0.6f;
    s.osc2Cents = 9.0f;

    const auto reference = renderEngine(s, events, 0.7, 512);
    for (int blockSize : {1, 7, 64, 100, 480, 4096})
        CHECK(renderEngine(s, events, 0.7, blockSize) == reference);
}

TEST_CASE("note-off releases the sound to silence")
{
    const auto out = renderEngine(
        engineSettings(),
        {at(0.0, MidiEvent::noteOn(0, 50, 0.9f)), at(0.3, MidiEvent::noteOff(0, 50))}, 1.0);
    CHECK(rms(slice(out, 0.2, 0.29)) > 0.05);
    CHECK(peak(slice(out, 0.6, 1.0)) == 0.0f);
}

TEST_CASE("note-on with zero velocity acts as note-off")
{
    const auto out = renderEngine(
        engineSettings(),
        {at(0.0, MidiEvent::noteOn(0, 50, 0.9f)), at(0.3, MidiEvent::noteOn(0, 50, 0.0f))}, 1.0);
    CHECK(peak(slice(out, 0.6, 1.0)) == 0.0f);
}

TEST_CASE("the sustain pedal keeps a released note sounding")
{
    const auto out = renderEngine(
        engineSettings(),
        {at(0.0, MidiEvent::sustain(0, true)), at(0.01, MidiEvent::noteOn(0, 50, 0.9f)),
         at(0.1, MidiEvent::noteOff(0, 50)), at(0.8, MidiEvent::sustain(0, false))},
        1.6);
    CHECK(rms(slice(out, 0.5, 0.75)) > 0.05);
    CHECK(peak(slice(out, 1.3, 1.6)) == 0.0f);
}

TEST_CASE("pitch bend shifts pitch by the bend range")
{
    SynthSettings s = engineSettings();
    s.bendRangeSemitones = 12;
    const auto out = renderEngine(
        s, {at(0.0, MidiEvent::noteOn(0, 45, 0.9f)), at(0.5, MidiEvent::pitchBend(0, 1.0f))}, 1.5);

    const auto before = zeroCrossingFrequency(slice(out, 0.2, 0.45), kSampleRate);
    const auto after = zeroCrossingFrequency(slice(out, 0.9, 1.4), kSampleRate);
    REQUIRE(before.has_value());
    REQUIRE(after.has_value());
    CHECK(std::abs(centsBetween(*before, midiToHz(45))) < 2.0);
    CHECK(std::abs(centsBetween(*after, midiToHz(57))) < 2.0);
}

TEST_CASE("all notes off releases and all sound off cuts")
{
    const std::vector<TimedEvent> chord = {at(0.0, MidiEvent::noteOn(0, 48, 0.8f)),
                                           at(0.0, MidiEvent::noteOn(0, 55, 0.8f)),
                                           at(0.0, MidiEvent::noteOn(0, 60, 0.8f))};
    auto notesOff = chord;
    notesOff.push_back(at(0.3, MidiEvent::allNotesOff(0)));
    CHECK(peak(slice(renderEngine(engineSettings(), notesOff, 1.0), 0.7, 1.0)) == 0.0f);

    auto soundOff = chord;
    soundOff.push_back(at(0.3, MidiEvent::allSoundOff(0)));
    const auto cut = renderEngine(engineSettings(), soundOff, 1.0);
    CHECK(peak(slice(cut, 0.31, 1.0)) == 0.0f);
}

TEST_CASE("a full pool of loud notes stays inside full scale")
{
    SynthSettings s = plainSaw();
    s.osc2Level = 1.0f;
    s.ampEnv = {EnvelopeType::AGD, 0.001f, 0.2f};
    s.polyphony = 16;

    std::vector<TimedEvent> events;
    for (int i = 0; i < 16; ++i)
        events.push_back(at(0.0, MidiEvent::noteOn(0, 36 + i * 3, 1.0f)));
    const auto out = renderEngine(s, events, 0.5);

    REQUIRE(allFinite(out));
    CHECK(peak(out) <= 1.0f);
    CHECK(rms(slice(out, 0.1, 0.4)) > 0.1);
}

TEST_CASE("polyphony setting limits the sounding voices")
{
    SynthSettings s = engineSettings();
    s.polyphony = 3;
    Engine engine;
    engine.setSettings(s);
    engine.prepare(kSampleRate);

    std::vector<MidiEvent> events;
    for (int i = 0; i < 6; ++i)
        events.push_back(MidiEvent::noteOn(0, 50 + i, 0.8f));
    std::vector<float> left(512), right(512);
    engine.process(events, left.data(), right.data(), 512);
    engine.process({}, left.data(), right.data(), 512);

    CHECK(engine.voices().activeVoiceCount() == 3);
}

TEST_CASE("both channels carry the same signal")
{
    Engine engine;
    engine.setSettings(engineSettings());
    engine.prepare(kSampleRate);
    std::vector<MidiEvent> events = {MidiEvent::noteOn(0, 50, 0.9f)};
    std::vector<float> left(1024), right(1024);
    engine.process(events, left.data(), right.data(), 1024);
    CHECK(left == right);
}

#include "host/MidiTranslator.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cmath>

using namespace polylogue;
using namespace polylogue::host;
using dsp::MidiEventType;

namespace {

struct Result {
    std::array<dsp::MidiEvent, 64> events{};
    std::array<ControlChange, 16> controls{};
    TranslatedMidi info;
};

Result translate(const juce::MidiBuffer& midi)
{
    Result r;
    r.info = translateMidi(midi, r.events, r.controls);
    return r;
}

}  // namespace

TEST_CASE("notes keep their offsets, numbers and velocities")
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.5f), 10);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 300);

    const Result r = translate(midi);
    REQUIRE(r.info.eventCount == 2);
    CHECK(r.events[0].type == MidiEventType::NoteOn);
    CHECK(r.events[0].offset == 10);
    CHECK(r.events[0].note == 60);
    CHECK(std::abs(r.events[0].value - 0.5f) < 0.01f);
    CHECK(r.events[1].type == MidiEventType::NoteOff);
    CHECK(r.events[1].offset == 300);
    CHECK(r.events[1].note == 60);
}

TEST_CASE("a note-on with zero velocity is a note-off")
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 64, static_cast<juce::uint8>(0)), 5);

    const Result r = translate(midi);
    REQUIRE(r.info.eventCount == 1);
    CHECK(r.events[0].type == MidiEventType::NoteOff);
    CHECK(r.events[0].note == 64);
}

TEST_CASE("every channel is treated alike")
{
    juce::MidiBuffer midi;
    for (int channel : {1, 5, 16})
        midi.addEvent(juce::MidiMessage::noteOn(channel, 40 + channel, 0.8f), 0);
    CHECK(translate(midi).info.eventCount == 3);
}

TEST_CASE("pitch wheel maps to -1 .. +1 around its centre")
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 0), 0);
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 8192), 1);
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 16383), 2);

    const Result r = translate(midi);
    REQUIRE(r.info.eventCount == 3);
    CHECK(r.events[0].type == MidiEventType::PitchBend);
    CHECK(r.events[0].value == -1.0f);
    CHECK(r.events[1].value == 0.0f);
    CHECK(r.events[2].value > 0.999f);
    CHECK(r.events[2].value <= 1.0f);
}

TEST_CASE("the sustain pedal switches at the midpoint")
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 127), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 63), 1);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 64), 2);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 64, 0), 3);

    const Result r = translate(midi);
    REQUIRE(r.info.eventCount == 4);
    CHECK(r.events[0].value == 1.0f);
    CHECK(r.events[1].value == 0.0f);
    CHECK(r.events[2].value == 1.0f);
    CHECK(r.events[3].value == 0.0f);
    CHECK(r.info.controlCount == 0);
}

TEST_CASE("channel mode messages become engine events")
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
    midi.addEvent(juce::MidiMessage::allSoundOff(1), 1);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 121, 0), 2);

    const Result r = translate(midi);
    REQUIRE(r.info.eventCount == 4);
    CHECK(r.events[0].type == MidiEventType::AllNotesOff);
    CHECK(r.events[1].type == MidiEventType::AllSoundOff);
    CHECK(r.events[2].type == MidiEventType::Sustain);
    CHECK(r.events[2].value == 0.0f);
    CHECK(r.events[3].type == MidiEventType::PitchBend);
    CHECK(r.events[3].value == 0.0f);
}

TEST_CASE("other controllers are handed back for routing")
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, 100), 0);
    midi.addEvent(juce::MidiMessage::controllerEvent(1, 1, 5), 1);

    const Result r = translate(midi);
    CHECK(r.info.eventCount == 0);
    REQUIRE(r.info.controlCount == 2);
    CHECK(r.controls[0].controller == 74);
    CHECK(r.controls[0].value == 100);
    CHECK(r.controls[1].controller == 1);
}

TEST_CASE("order is preserved across message kinds")
{
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.7f), 4);
    midi.addEvent(juce::MidiMessage::pitchWheel(1, 9000), 20);
    midi.addEvent(juce::MidiMessage::noteOff(1, 60), 40);

    const Result r = translate(midi);
    REQUIRE(r.info.eventCount == 3);
    CHECK(r.events[0].offset == 4);
    CHECK(r.events[1].offset == 20);
    CHECK(r.events[2].offset == 40);
}

TEST_CASE("empty and irrelevant input produces nothing")
{
    CHECK(translate(juce::MidiBuffer()).info.sawAnyMessage == false);

    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::midiClock(), 0);
    const Result r = translate(midi);
    CHECK(r.info.sawAnyMessage);
    CHECK(r.info.eventCount == 0);
    CHECK(r.info.controlCount == 0);
}

TEST_CASE("output beyond capacity is dropped without overflow")
{
    juce::MidiBuffer midi;
    for (int i = 0; i < 200; ++i)
        midi.addEvent(juce::MidiMessage::noteOn(1, i % 100, 0.5f), i);
    for (int i = 0; i < 40; ++i)
        midi.addEvent(juce::MidiMessage::controllerEvent(1, 74, i), 300 + i);

    std::array<dsp::MidiEvent, 16> events{};
    std::array<ControlChange, 4> controls{};
    const TranslatedMidi info = translateMidi(midi, events, controls);
    CHECK(info.eventCount == 16);
    CHECK(info.controlCount == 4);
}

#pragma once

#include <cstdint>

namespace polylogue::dsp {

enum class MidiEventType : std::uint8_t {
    NoteOn,
    NoteOff,
    Sustain,
    PitchBend,
    AllNotesOff,
    AllSoundOff,
};

// A host-independent MIDI event. `offset` is in samples from the start of the block being
// rendered; `value` is velocity (0..1), pedal (0..1), or bend (-1..1) depending on the type.
struct MidiEvent {
    MidiEventType type = MidiEventType::NoteOn;
    int offset = 0;
    int note = 0;
    float value = 0.0f;

    static MidiEvent noteOn(int offset, int note, float velocity)
    {
        return {MidiEventType::NoteOn, offset, note, velocity};
    }
    static MidiEvent noteOff(int offset, int note)
    {
        return {MidiEventType::NoteOff, offset, note, 0.0f};
    }
    static MidiEvent sustain(int offset, bool down)
    {
        return {MidiEventType::Sustain, offset, 0, down ? 1.0f : 0.0f};
    }
    static MidiEvent pitchBend(int offset, float bend)
    {
        return {MidiEventType::PitchBend, offset, 0, bend};
    }
    static MidiEvent allNotesOff(int offset)
    {
        return {MidiEventType::AllNotesOff, offset, 0, 0.0f};
    }
    static MidiEvent allSoundOff(int offset)
    {
        return {MidiEventType::AllSoundOff, offset, 0, 0.0f};
    }
};

}  // namespace polylogue::dsp

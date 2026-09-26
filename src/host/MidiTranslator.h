#pragma once

#include "dsp/MidiEvent.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cstddef>
#include <span>

namespace polylogue::host {

struct ControlChange {
    int controller = 0;
    int value = 0;
};

struct TranslatedMidi {
    std::size_t eventCount = 0;
    std::size_t controlCount = 0;
    bool sawAnyMessage = false;
};

// Converts a block of host MIDI into engine events. Controllers the engine does not handle itself
// are passed back in `controls` for the caller to route. Output beyond the capacity of either span
// is dropped. Allocates nothing.
TranslatedMidi translateMidi(const juce::MidiBuffer& midi, std::span<dsp::MidiEvent> events,
                             std::span<ControlChange> controls);

}  // namespace polylogue::host

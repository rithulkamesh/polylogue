#pragma once

#include "dsp/Engine.h"
#include "dsp/MidiEvent.h"

#include <span>
#include <vector>

namespace polylogue::offline {

struct TimedEvent {
    double seconds = 0.0;
    dsp::MidiEvent event;  // its `offset` is ignored; `seconds` decides the position
};

struct StereoBuffer {
    std::vector<float> left;
    std::vector<float> right;
};

// Runs a prepared engine over `seconds` of audio in host-sized blocks, delivering `events` at their
// sample positions. Lets the DSP be exercised without a plugin host.
StereoBuffer renderOffline(dsp::Engine& engine, std::span<const TimedEvent> events, double seconds,
                           double sampleRate, int blockSize = 512);

inline TimedEvent at(double seconds, const dsp::MidiEvent& event)
{
    return {seconds, event};
}

}  // namespace polylogue::offline

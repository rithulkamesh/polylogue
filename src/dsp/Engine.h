#pragma once

#include "dsp/MidiEvent.h"
#include "dsp/Settings.h"
#include "dsp/Smoother.h"
#include "dsp/VoiceManager.h"

#include <span>

namespace polylogue::dsp {

// The synthesizer: turns settings and sample-accurate MIDI events into stereo audio.
// Allocates nothing after construction.
class Engine {
public:
    static constexpr int kMaxSubBlock = 64;

    void prepare(double sampleRate);

    // Takes effect from the next processed sample.
    void setSettings(const SynthSettings& settings);

    // Renders `count` samples. `events` must be ordered by offset, each within [0, count).
    void process(std::span<const MidiEvent> events, float* left, float* right, int count);

    const VoiceManager& voices() const { return voices_; }

private:
    void handle(const MidiEvent& event);
    void renderSubBlock(float* left, float* right, int count);

    VoiceManager voices_;
    SynthSettings settings_;
    Smoother outputGain_;
    float bend_ = 0.0f;
};

}  // namespace polylogue::dsp

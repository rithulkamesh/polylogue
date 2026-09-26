#pragma once

#include "dsp/Settings.h"
#include "dsp/Voice.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace polylogue::test {

inline constexpr double kSampleRate = 48000.0;
inline constexpr int kBlock = 64;

struct Note {
    int number = 45;
    float velocity = 1.0f;
    double heldSeconds = 1.0;
};

// Plays one note through a fresh voice and returns `seconds` of audio.
inline std::vector<float> renderNote(const dsp::SynthSettings& settings, const Note& note,
                                     double seconds, double sampleRate = kSampleRate)
{
    dsp::Voice voice;
    voice.prepare(sampleRate);
    voice.noteOn(note.number, note.velocity);

    const auto total = static_cast<int>(seconds * sampleRate);
    const auto release = static_cast<int>(note.heldSeconds * sampleRate);
    std::vector<float> out(static_cast<std::size_t>(total), 0.0f);

    bool released = false;
    for (int pos = 0; pos < total; pos += kBlock) {
        if (!released && pos >= release) {
            voice.noteOff();
            released = true;
        }
        const int count = std::min(kBlock, total - pos);
        voice.render(out.data() + pos, count, settings);
    }
    return out;
}

// A neutral patch: one open saw, instant amp envelope, filter wide open.
inline dsp::SynthSettings plainSaw()
{
    dsp::SynthSettings s;
    s.cutoffHz = 20000.0f;
    s.ampEnv = {dsp::EnvelopeType::AGD, 0.001f, 0.05f};
    s.velocityToAmp = 0.0f;
    return s;
}

}  // namespace polylogue::test

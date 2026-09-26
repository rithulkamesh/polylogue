#pragma once

#include "dsp/Engine.h"
#include "dsp/MidiEvent.h"
#include "offline/OfflineRenderer.h"
#include "support/VoiceRender.h"

#include <vector>

namespace polylogue::test {

using dsp::MidiEvent;
using offline::at;
using offline::TimedEvent;

// Left channel of a fresh engine playing `events` under `settings`.
inline std::vector<float> renderEngine(const dsp::SynthSettings& settings,
                                       const std::vector<TimedEvent>& events, double seconds,
                                       int blockSize = 512, double sampleRate = kSampleRate)
{
    dsp::Engine engine;
    engine.setSettings(settings);
    engine.prepare(sampleRate);
    return offline::renderOffline(engine, events, seconds, sampleRate, blockSize).left;
}

}  // namespace polylogue::test

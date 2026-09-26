#pragma once

#include "dsp/Oscillator.h"
#include "dsp/Settings.h"

namespace polylogue::dsp {

// The monologue's LFO: saw, triangle or square, in fast (audio-rate), slow, or one-shot mode.
// Output is bipolar. Every wave starts at phase 0 on `restart`; a one-shot runs half a cycle and
// then holds, which makes it a second envelope.
class Lfo {
public:
    void prepare(double sampleRate) { sampleRate_ = sampleRate; }
    void setParameters(const LfoSettings& settings);
    void restart();
    float next();

    static double rateToHz(LfoMode mode, float rate);

private:
    Oscillator oscillator_;
    LfoMode mode_ = LfoMode::Slow;
    double sampleRate_ = 44100.0;
    bool finished_ = false;
    float held_ = 0.0f;
};

}  // namespace polylogue::dsp

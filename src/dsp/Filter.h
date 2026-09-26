#pragma once

namespace polylogue::dsp {

// Two-pole (12 dB/oct) low-pass: trapezoidal state-variable filter with soft-saturated
// integrators. Resonance reaches self-oscillation at the top of its range.
class Filter {
public:
    void prepare(double sampleRate);
    void reset();

    // Nudges the state so a self-oscillating filter starts ringing without input.
    void excite(float amount) { bandState_ += amount; }

    void setParameters(float cutoffHz, float resonance);
    float process(float input);

private:
    float sampleRate_ = 44100.0f;
    float g_ = 0.0f;
    float a1_ = 1.0f;
    float a2_ = 0.0f;
    float a3_ = 0.0f;
    float bandState_ = 0.0f;
    float lowState_ = 0.0f;
};

}  // namespace polylogue::dsp

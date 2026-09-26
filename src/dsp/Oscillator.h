#pragma once

#include <array>

namespace polylogue::dsp {

enum class Waveform {
    Saw,
    Triangle,
    Square,
    Sine
};

// Band-limited oscillator. `shape` in [0, 1] morphs each waveform: saw blends in a phase-shifted
// second saw, triangle skews toward a ramp, square narrows its pulse.
//
// Each discontinuity is located to sub-sample accuracy and smoothed by adding a short residual
// to the four samples around it, so output lags the phase by one sample.
class Oscillator {
public:
    static constexpr double kNoReset = -1.0;

    void setWaveform(Waveform waveform) { waveform_ = waveform; }
    void setShape(float shape) { shape_ = static_cast<double>(shape); }
    void setFrequency(double hz, double sampleRate);
    void reset(double phase = 0.0);

    double phase() const { return phase_; }

    // True when the phase wraps during the next call to process().
    bool wrapsNextSample() const { return increment_ > 0.0 && phase_ + increment_ >= 1.0; }
    // Time until that wrap, as a fraction of the sample, in (0, 1].
    double timeToWrap() const { return (1.0 - phase_) / increment_; }

    // `resetIn` is when, as a fraction of this sample, a hard-sync master restarts this
    // oscillator, or kNoReset. `phaseModulation` shifts the phase for this sample, in radians;
    // it suits the continuous sine, where it is FM, and is not band-limited.
    float process(double resetIn = kNoReset, double phaseModulation = 0.0);

private:
    double value(double t) const;
    double slope(double t) const;
    void scan(double start, double span, double timeOffset);
    void addStep(double jump, double time);
    void addKink(double slopeChange, double time);

    Waveform waveform_ = Waveform::Saw;
    double shape_ = 0.0;
    double phase_ = 0.0;
    double increment_ = 0.0;
    double previousValue_ = 0.0;
    // Corrections for the previous sample and the next three.
    std::array<double, 4> pending_{};
};

}  // namespace polylogue::dsp

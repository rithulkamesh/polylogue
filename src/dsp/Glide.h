#pragma once

#include <cmath>

namespace polylogue::dsp {

// Pitch slew in semitones. Exponential, like an RC portamento circuit: within about 5% of the
// distance to go after the glide time. State is double so the last fraction of a cent doesn't
// stall below float resolution.
class Glide {
public:
    void prepare(double sampleRate) { sampleRate_ = sampleRate; }

    void setTime(float seconds)
    {
        coefficient_ = seconds <= 0.0f
                           ? 1.0
                           : 1.0 - std::exp(-3.0 / (static_cast<double>(seconds) * sampleRate_));
    }

    void jumpTo(float semitones) { value_ = target_ = static_cast<double>(semitones); }
    void glideTo(float semitones) { target_ = static_cast<double>(semitones); }

    float next()
    {
        value_ += (target_ - value_) * coefficient_;
        if (std::abs(target_ - value_) < 1e-6)
            value_ = target_;
        return static_cast<float>(value_);
    }

private:
    double sampleRate_ = 44100.0;
    double value_ = 60.0;
    double target_ = 60.0;
    double coefficient_ = 1.0;
};

}  // namespace polylogue::dsp

#pragma once

#include <cmath>

namespace polylogue::dsp {

// One-pole parameter smoother. Sample-rate agnostic until `configure` is called.
class Smoother {
public:
    void configure(double seconds, double sampleRate)
    {
        coefficient_ = 1.0f - static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
    }

    void setTarget(float target) { target_ = target; }
    void snap(float value) { value_ = target_ = value; }

    float next()
    {
        value_ += (target_ - value_) * coefficient_;
        return value_;
    }

    float target() const { return target_; }

private:
    float value_ = 0.0f;
    float target_ = 0.0f;
    float coefficient_ = 1.0f;
};

}  // namespace polylogue::dsp

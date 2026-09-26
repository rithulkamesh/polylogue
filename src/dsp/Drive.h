#pragma once

namespace polylogue::dsp {

// Level-compensated saturation with a darkening tone control. A tanh curve is normalised so a
// signal at the reference level keeps its peak however hard it is driven, and first-order
// antiderivative anti-aliasing keeps the added harmonics from folding back. At zero drive the
// stage is transparent.
class Drive {
public:
    void prepare(double sampleRate);
    void reset();

    void setAmount(float amount);
    float process(float input);

private:
    double sampleRate_ = 44100.0;
    double gain_ = 0.0;
    double toneCoefficient_ = 1.0;
    double previousInput_ = 0.0;
    double toneState_ = 0.0;
    bool active_ = false;
};

}  // namespace polylogue::dsp

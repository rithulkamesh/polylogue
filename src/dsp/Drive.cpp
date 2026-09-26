#include "dsp/Drive.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace polylogue::dsp {
namespace {

constexpr double kMaxGain = 12.0;
// Peak of a nominal voice signal; the curve is scaled so this level passes at unity.
constexpr double kReferenceLevel = 0.7;
constexpr double kBrightHz = 20000.0;
constexpr double kDarkRatio = 0.25;
constexpr double kInactiveGain = 1e-3;
constexpr double kAdaaThreshold = 1e-5;

double logCosh(double x)
{
    const double a = std::abs(x);
    return a + std::log1p(std::exp(-2.0 * a)) - std::numbers::ln2;
}

}  // namespace

void Drive::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    reset();
    setAmount(0.0f);
}

void Drive::reset()
{
    previousInput_ = 0.0;
    toneState_ = 0.0;
}

void Drive::setAmount(float amount)
{
    const double a = std::clamp(static_cast<double>(amount), 0.0, 1.0);
    gain_ = kMaxGain * a * a;
    active_ = gain_ > kInactiveGain;

    const double cutoff = std::min(kBrightHz * std::pow(kDarkRatio, a), 0.45 * sampleRate_);
    toneCoefficient_ = 1.0 - std::exp(-2.0 * std::numbers::pi * cutoff / sampleRate_);
}

float Drive::process(float input)
{
    const double x = static_cast<double>(input);
    if (!active_) {
        previousInput_ = x;
        toneState_ = x;
        return input;
    }

    // f(x) = ref * tanh(g x) / tanh(ref g), with antiderivative ref * ln cosh(g x) / (g tanh(ref
    // g)).
    const double scale = kReferenceLevel / std::tanh(kReferenceLevel * gain_);
    double shaped;
    const double delta = x - previousInput_;
    if (std::abs(delta) > kAdaaThreshold) {
        shaped = scale * (logCosh(gain_ * x) - logCosh(gain_ * previousInput_)) / (gain_ * delta);
    } else {
        shaped = scale * std::tanh(gain_ * 0.5 * (x + previousInput_));
    }
    previousInput_ = x;

    toneState_ += toneCoefficient_ * (shaped - toneState_);
    return static_cast<float>(toneState_);
}

}  // namespace polylogue::dsp

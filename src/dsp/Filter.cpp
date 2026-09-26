#include "dsp/Filter.h"

#include "dsp/Saturation.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace polylogue::dsp {
namespace {

constexpr float kMaxCutoffRatio = 0.45f;
constexpr float kMinCutoffHz = 5.0f;
// Integrators pass signal untouched up to kStateKnee and are held below kStateCeiling, which is
// where a self-oscillating filter settles.
constexpr float kStateKnee = 1.0f;
constexpr float kStateCeiling = 2.0f;
// Damping is 2 at resonance 0 and slightly negative at 1, which is what lets the filter ring.
constexpr float kResonanceReach = 1.01f;

}  // namespace

void Filter::prepare(double sampleRate)
{
    sampleRate_ = static_cast<float>(sampleRate);
    reset();
    setParameters(1000.0f, 0.0f);
}

void Filter::reset()
{
    bandState_ = 0.0f;
    lowState_ = 0.0f;
}

void Filter::setParameters(float cutoffHz, float resonance)
{
    const float hz = std::clamp(cutoffHz, kMinCutoffHz, kMaxCutoffRatio * sampleRate_);
    g_ = std::tan(std::numbers::pi_v<float> * hz / sampleRate_);
    const float damping = 2.0f * (1.0f - kResonanceReach * std::clamp(resonance, 0.0f, 1.0f));
    a1_ = 1.0f / (1.0f + g_ * (g_ + damping));
    a2_ = g_ * a1_;
    a3_ = g_ * a2_;
}

float Filter::process(float input)
{
    const float v3 = input - lowState_;
    const float band = a1_ * bandState_ + a2_ * v3;
    const float low = lowState_ + a2_ * bandState_ + a3_ * v3;
    bandState_ = softLimit(2.0f * band - bandState_, kStateKnee, kStateCeiling);
    lowState_ = softLimit(2.0f * low - lowState_, kStateKnee, kStateCeiling);
    return low;
}

}  // namespace polylogue::dsp

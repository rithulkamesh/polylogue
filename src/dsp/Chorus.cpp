#include "dsp/Chorus.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace polylogue::dsp {
namespace {

constexpr double kCentreDelaySeconds = 0.012;
constexpr double kSwingSeconds = 0.007;
constexpr double kMixSmoothingSeconds = 0.02;
constexpr float kBypassBelow = 1e-5f;
constexpr float kWetGain = 0.7f;
constexpr float kDryFloor = 0.5f;

// Where each tap sits in the LFO cycle. The right channel is offset from the left so the two
// decorrelate.
constexpr std::array<double, 3> kLeftPhases = {0.0, 1.0 / 3.0, 2.0 / 3.0};
constexpr std::array<double, 3> kRightPhases = {1.0 / 6.0, 0.5, 5.0 / 6.0};

}  // namespace

void Chorus::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    mix_.configure(kMixSmoothingSeconds, sampleRate);
    reset();
}

void Chorus::reset()
{
    buffer_.fill(0.0f);
    writeIndex_ = 0;
    phase_ = 0.0;
    mix_.snap(mix_.target());
}

void Chorus::setParameters(float mix, float rateHz, float depth)
{
    mix_.setTarget(std::clamp(mix, 0.0f, 1.0f));
    phaseIncrement_ = static_cast<double>(rateHz) / sampleRate_;
    depth_ = std::clamp(depth, 0.0f, 1.0f);
}

float Chorus::readTap(double delaySamples) const
{
    const double position = static_cast<double>(writeIndex_) - delaySamples;
    const double wrapped = position < 0.0 ? position + static_cast<double>(kBufferSize) : position;
    const auto whole = static_cast<std::size_t>(wrapped);
    const auto fraction = static_cast<float>(wrapped - static_cast<double>(whole));
    const float a = buffer_[whole % kBufferSize];
    const float b = buffer_[(whole + 1) % kBufferSize];
    return a + (b - a) * fraction;
}

void Chorus::process(float input, float& left, float& right)
{
    buffer_[writeIndex_] = input;

    const float mix = mix_.next();
    if (mix < kBypassBelow && mix_.target() < kBypassBelow) {
        left = right = input;
    } else {
        const double swing = kSwingSeconds * static_cast<double>(depth_) * sampleRate_;
        const double centre = kCentreDelaySeconds * sampleRate_;

        float wetLeft = 0.0f;
        float wetRight = 0.0f;
        for (std::size_t tap = 0; tap < kTaps; ++tap) {
            const double lfoLeft = std::sin(2.0 * std::numbers::pi * (phase_ + kLeftPhases[tap]));
            const double lfoRight = std::sin(2.0 * std::numbers::pi * (phase_ + kRightPhases[tap]));
            wetLeft += readTap(centre + swing * lfoLeft);
            wetRight += readTap(centre + swing * lfoRight);
        }

        const float dry = input * (1.0f - kDryFloor * mix);
        const float wetScale = mix * kWetGain / static_cast<float>(std::sqrt(kTaps));
        left = dry + wetLeft * wetScale;
        right = dry + wetRight * wetScale;
    }

    phase_ += phaseIncrement_;
    phase_ -= std::floor(phase_);
    writeIndex_ = (writeIndex_ + 1) % kBufferSize;
}

}  // namespace polylogue::dsp

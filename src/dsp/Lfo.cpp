#include "dsp/Lfo.h"

#include <algorithm>
#include <cmath>

namespace polylogue::dsp {
namespace {

constexpr double kFastMinHz = 0.5;
constexpr double kFastMaxHz = 2800.0;
constexpr double kSlowMinHz = 0.05;
constexpr double kSlowMaxHz = 28.0;

Waveform toWaveform(LfoWave wave)
{
    switch (wave) {
    case LfoWave::Saw:
        return Waveform::Saw;
    case LfoWave::Triangle:
        return Waveform::Triangle;
    case LfoWave::Square:
        return Waveform::Square;
    }
    return Waveform::Saw;
}

}  // namespace

double Lfo::rateToHz(LfoMode mode, float rate)
{
    const bool fast = mode == LfoMode::Fast;
    const double low = fast ? kFastMinHz : kSlowMinHz;
    const double high = fast ? kFastMaxHz : kSlowMaxHz;
    return low * std::pow(high / low, static_cast<double>(std::clamp(rate, 0.0f, 1.0f)));
}

void Lfo::setParameters(const LfoSettings& settings)
{
    mode_ = settings.mode;
    oscillator_.setWaveform(toWaveform(settings.wave));
    oscillator_.setFrequency(rateToHz(settings.mode, settings.rate), sampleRate_);
}

void Lfo::restart()
{
    oscillator_.reset();
    finished_ = false;
    held_ = 0.0f;
}

float Lfo::next()
{
    if (finished_)
        return held_;

    const float value = oscillator_.process();
    if (mode_ == LfoMode::OneShot && oscillator_.phase() >= 0.5) {
        finished_ = true;
        held_ = value;
    }
    return value;
}

}  // namespace polylogue::dsp

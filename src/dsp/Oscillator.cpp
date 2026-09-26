#include "dsp/Oscillator.h"

#include "dsp/Blep.h"

#include <algorithm>
#include <cmath>

namespace polylogue::dsp {
namespace {

constexpr double kMaxIncrement = 0.45;
constexpr double kSawShapeOffset = 0.5;
constexpr double kTriangleMinRise = 0.05;
constexpr double kPulseMinWidth = 0.02;

double fractionalPart(double x)
{
    return x - std::floor(x);
}

double triangleRise(double shape)
{
    return 0.5 - (0.5 - kTriangleMinRise) * shape;
}

double pulseWidth(double shape)
{
    return 0.5 - (0.5 - kPulseMinWidth) * shape;
}

}  // namespace

void Oscillator::setFrequency(double hz, double sampleRate)
{
    increment_ = std::clamp(hz / sampleRate, 0.0, kMaxIncrement);
}

void Oscillator::reset(double phase)
{
    phase_ = fractionalPart(phase);
    previousValue_ = value(phase_);
    pending_.fill(0.0);
}

float Oscillator::process(double resetIn)
{
    const double current = value(phase_);

    if (increment_ > 0.0) {
        if (resetIn > 0.0) {
            scan(phase_, resetIn * increment_, 0.0);

            const double phaseAtReset = fractionalPart(phase_ + resetIn * increment_);
            addStep(value(0.0) - value(phaseAtReset), resetIn);
            addKink((slope(0.0) - slope(phaseAtReset)) * increment_, resetIn);

            const double remaining = (1.0 - resetIn) * increment_;
            scan(0.0, remaining, resetIn);
            phase_ = remaining;
        } else {
            scan(phase_, increment_, 0.0);
            phase_ = fractionalPart(phase_ + increment_);
        }
    }

    const double out = previousValue_ + pending_[0];
    pending_[0] = pending_[1];
    pending_[1] = pending_[2];
    pending_[2] = pending_[3];
    pending_[3] = 0.0;
    previousValue_ = current;
    return static_cast<float>(out);
}

double Oscillator::value(double t) const
{
    switch (waveform_) {
    case Waveform::Saw: {
        const double offset = kSawShapeOffset * shape_;
        const double shifted = fractionalPart(t + offset);
        return ((2.0 * t - 1.0) + (2.0 * shifted - 1.0)) / (2.0 * (1.0 - offset));
    }
    case Waveform::Triangle: {
        const double rise = triangleRise(shape_);
        return t < rise ? -1.0 + 2.0 * t / rise : 1.0 - 2.0 * (t - rise) / (1.0 - rise);
    }
    case Waveform::Square: {
        // A narrow pulse has DC; remove it and rescale so the peak stays at 1.
        const double width = pulseWidth(shape_);
        const double raw = t < width ? 1.0 : -1.0;
        return (raw - (2.0 * width - 1.0)) / (2.0 * (1.0 - width));
    }
    }
    return 0.0;
}

// Derivative with respect to phase.
double Oscillator::slope(double t) const
{
    switch (waveform_) {
    case Waveform::Saw:
        return 2.0 / (1.0 - kSawShapeOffset * shape_);
    case Waveform::Triangle: {
        const double rise = triangleRise(shape_);
        return t < rise ? 2.0 / rise : -2.0 / (1.0 - rise);
    }
    case Waveform::Square:
        return 0.0;
    }
    return 0.0;
}

// Registers the discontinuities the phase passes while advancing `span` from `start`.
// `timeOffset` is how far into the sample that advance begins.
void Oscillator::scan(double start, double span, double timeOffset)
{
    // Time within the sample at which the phase reaches `position`, or a negative value.
    auto crossing = [&](double position) {
        for (double candidate : {position, position + 1.0}) {
            if (candidate > start && candidate <= start + span)
                return timeOffset + (candidate - start) / increment_;
        }
        return -1.0;
    };

    switch (waveform_) {
    case Waveform::Saw: {
        // Each saw falls by 2 at its wrap; the sum is scaled by 1 / (2 (1 - offset)).
        const double offset = kSawShapeOffset * shape_;
        const double fall = -1.0 / (1.0 - offset);
        if (const double t = crossing(0.0); t > 0.0)
            addStep(fall, t);
        if (const double t = crossing(1.0 - offset); t > 0.0)
            addStep(fall, t);
        break;
    }
    case Waveform::Triangle: {
        const double rise = triangleRise(shape_);
        const double kink = 2.0 / (rise * (1.0 - rise)) * increment_;
        if (const double t = crossing(0.0); t > 0.0)
            addKink(kink, t);
        if (const double t = crossing(rise); t > 0.0)
            addKink(-kink, t);
        break;
    }
    case Waveform::Square: {
        const double width = pulseWidth(shape_);
        const double jump = 1.0 / (1.0 - width);
        if (const double t = crossing(0.0); t > 0.0)
            addStep(jump, t);
        if (const double t = crossing(width); t > 0.0)
            addStep(-jump, t);
        break;
    }
    }
}

void Oscillator::addStep(double jump, double time)
{
    for (std::size_t k = 0; k < pending_.size(); ++k)
        pending_[k] += jump * stepResidual(static_cast<double>(k) - 1.0 - time);
}

void Oscillator::addKink(double slopeChange, double time)
{
    for (std::size_t k = 0; k < pending_.size(); ++k)
        pending_[k] += slopeChange * rampResidual(static_cast<double>(k) - 1.0 - time);
}

}  // namespace polylogue::dsp

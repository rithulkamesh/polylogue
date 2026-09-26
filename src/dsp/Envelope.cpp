#include "dsp/Envelope.h"

#include <cmath>

namespace polylogue::dsp {
namespace {

// The attack aims past 1 so it curves like an RC charge, then clamps.
constexpr float kAttackTarget = 1.2f;
// Decay times are measured to -60 dB.
constexpr double kDecayFloor = 0.001;
constexpr float kIdleLevel = 1e-4f;
constexpr double kGateRampSeconds = 0.001;

float attackCoefficient(double seconds, double sampleRate)
{
    // Time to reach 1.0 from 0 when aiming at kAttackTarget is tau * ln(kAttackTarget /
    // (kAttackTarget - 1)).
    const double tau = seconds / std::log(static_cast<double>(kAttackTarget) /
                                          (static_cast<double>(kAttackTarget) - 1.0));
    return static_cast<float>(1.0 - std::exp(-1.0 / (tau * sampleRate)));
}

float decayCoefficient(double seconds, double sampleRate)
{
    return static_cast<float>(std::pow(kDecayFloor, 1.0 / (seconds * sampleRate)));
}

}  // namespace

void Envelope::setParameters(const EnvelopeSettings& settings, double sampleRate)
{
    type_ = settings.type;
    const bool gate = type_ == EnvelopeType::Gate;
    const double attack = gate ? kGateRampSeconds : static_cast<double>(settings.attack);
    const double decay = gate ? kGateRampSeconds : static_cast<double>(settings.decay);
    attackCoefficient_ = attackCoefficient(attack, sampleRate);
    decayCoefficient_ = decayCoefficient(decay, sampleRate);
}

void Envelope::noteOn()
{
    level_ = 0.0f;
    stage_ = Stage::Attack;
}

void Envelope::noteOff()
{
    if (type_ == EnvelopeType::AD || stage_ == Stage::Idle)
        return;
    stage_ = Stage::Decay;
}

void Envelope::reset()
{
    level_ = 0.0f;
    stage_ = Stage::Idle;
}

float Envelope::next()
{
    switch (stage_) {
    case Stage::Idle:
    case Stage::Hold:
        break;
    case Stage::Attack:
        level_ += (kAttackTarget - level_) * attackCoefficient_;
        if (level_ >= 1.0f) {
            level_ = 1.0f;
            stage_ = type_ == EnvelopeType::AD ? Stage::Decay : Stage::Hold;
        }
        break;
    case Stage::Decay:
        level_ *= decayCoefficient_;
        if (level_ < kIdleLevel) {
            level_ = 0.0f;
            stage_ = Stage::Idle;
        }
        break;
    }
    return level_;
}

}  // namespace polylogue::dsp

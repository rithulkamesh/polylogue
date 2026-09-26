#include "dsp/ModMatrix.h"

#include <cmath>

namespace polylogue::dsp {
namespace {

constexpr double kSmoothingSeconds = 0.01;

// Amounts at full knob travel.
constexpr float kEnvCutoffOctaves = 8.0f;
constexpr float kEnvPitchSemitones = 48.0f;
constexpr float kLfoPitchSemitones = 12.0f;
constexpr float kLfoShape = 0.5f;
constexpr float kLfoCutoffOctaves = 6.0f;

// Squares the knob while keeping its sign: fine control near zero, full range at the ends.
float curved(float knob)
{
    return knob * std::abs(knob);
}

}  // namespace

void ModMatrix::prepare(double sampleRate)
{
    for (auto& row : amounts_) {
        for (Smoother& amount : row)
            amount.configure(kSmoothingSeconds, sampleRate);
    }
    configure(SynthSettings{});
    snap();
}

void ModMatrix::configure(const SynthSettings& s)
{
    for (auto& row : amounts_) {
        for (Smoother& amount : row)
            amount.setTarget(0.0f);
    }

    switch (s.modEnvTarget) {
    case EnvelopeTarget::Cutoff:
        set(ModSource::ModEnv, ModDestination::Cutoff, s.modEnvAmount * kEnvCutoffOctaves);
        break;
    case EnvelopeTarget::Pitch:
        set(ModSource::ModEnv, ModDestination::Osc1Pitch,
            curved(s.modEnvAmount) * kEnvPitchSemitones);
        set(ModSource::ModEnv, ModDestination::Osc2Pitch,
            curved(s.modEnvAmount) * kEnvPitchSemitones);
        break;
    case EnvelopeTarget::Level1:
        set(ModSource::ModEnv, ModDestination::Osc1Level, s.modEnvAmount);
        break;
    case EnvelopeTarget::Level2:
        set(ModSource::ModEnv, ModDestination::Osc2Level, s.modEnvAmount);
        break;
    case EnvelopeTarget::Pitch2:
        set(ModSource::ModEnv, ModDestination::Osc2Pitch,
            curved(s.modEnvAmount) * kEnvPitchSemitones);
        break;
    }

    switch (s.lfo.target) {
    case LfoTarget::Pitch:
        set(ModSource::Lfo, ModDestination::Osc1Pitch, curved(s.lfo.amount) * kLfoPitchSemitones);
        set(ModSource::Lfo, ModDestination::Osc2Pitch, curved(s.lfo.amount) * kLfoPitchSemitones);
        break;
    case LfoTarget::Shape:
        set(ModSource::Lfo, ModDestination::Osc1Shape, s.lfo.amount * kLfoShape);
        set(ModSource::Lfo, ModDestination::Osc2Shape, s.lfo.amount * kLfoShape);
        break;
    case LfoTarget::Cutoff:
        set(ModSource::Lfo, ModDestination::Cutoff, curved(s.lfo.amount) * kLfoCutoffOctaves);
        break;
    }
}

void ModMatrix::snap()
{
    for (auto& row : amounts_) {
        for (Smoother& amount : row)
            amount.snap(amount.target());
    }
}

ModOffsets ModMatrix::evaluate(const ModSourceValues& sources)
{
    ModOffsets offsets{};
    for (std::size_t source = 0; source < kModSourceCount; ++source) {
        for (std::size_t destination = 0; destination < kModDestinationCount; ++destination)
            offsets[destination] += sources[source] * amounts_[source][destination].next();
    }
    return offsets;
}

void ModMatrix::set(ModSource source, ModDestination destination, float amount)
{
    amounts_[index(source)][index(destination)].setTarget(amount);
}

}  // namespace polylogue::dsp

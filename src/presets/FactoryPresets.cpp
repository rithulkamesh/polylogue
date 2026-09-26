#include "presets/FactoryPresets.h"

#include <array>
#include <span>
#include <type_traits>

namespace polylogue::presets {
namespace {

using dsp::Param;

template<typename Enum>
    requires std::is_enum_v<Enum>
constexpr float v(Enum choice)
{
    return static_cast<float>(static_cast<int>(choice));
}

using dsp::EnvelopeTarget;
using dsp::EnvelopeType;
using dsp::GlideMode;
using dsp::KeyMode;
using dsp::LfoMode;
using dsp::LfoTarget;
using dsp::LfoWave;
using dsp::Osc1Wave;
using dsp::Osc2Range;
using dsp::Osc2Wave;
using dsp::SyncRing;

constexpr float kTrack50 = 1.0f;
constexpr float kTrack100 = 2.0f;

// Ring modulation ratios are expressed as oscillator 2 range + cents. For example 3.5:1 is
// 2166 cents, so 4' (+1200) and +966.

// ---- Pads -----------------------------------------------------------------------------------

constexpr PresetValue kWarmPad[] = {
    {Param::Level, -3.5f},       {Param::Osc1Level, 0.8f},
    {Param::Osc2Level, 0.8f},    {Param::Osc2Pitch, 8.0f},
    {Param::Cutoff, 1400.0f},    {Param::Resonance, 0.15f},
    {Param::KeyTrack, kTrack50}, {Param::VelCutoff, 0.3f},
    {Param::AmpAttack, 0.9f},    {Param::AmpDecay, 2.2f},
    {Param::VelAmp, 0.3f},       {Param::EnvDecay, 1.2f},
    {Param::EnvInt, 0.15f},      {Param::LfoRate, 0.35f},
    {Param::LfoInt, 0.12f},      {Param::LfoTarget, v(LfoTarget::Cutoff)},
};

constexpr PresetValue kGlassPad[] = {
    {Param::Level, -1.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc1Level, 0.9f},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},
    {Param::Osc2Pitch, -5.0f},
    {Param::Osc2Level, 0.4f},
    {Param::Cutoff, 5000.0f},
    {Param::Resonance, 0.25f},
    {Param::KeyTrack, kTrack50},
    {Param::AmpAttack, 1.2f},
    {Param::AmpDecay, 3.0f},
    {Param::VelAmp, 0.3f},
    {Param::LfoRate, 0.3f},
    {Param::LfoInt, 0.6f},
    {Param::LfoTarget, v(LfoTarget::Shape)},
};

constexpr PresetValue kChoirDrift[] = {
    {Param::Level, 3.5f},         {Param::Osc1Level, 0.6f}, {Param::Osc2Wave, v(Osc2Wave::Noise)},
    {Param::Osc2Level, 0.12f},    {Param::Cutoff, 900.0f},  {Param::Resonance, 0.55f},
    {Param::KeyTrack, kTrack100}, {Param::AmpAttack, 0.7f}, {Param::AmpDecay, 1.8f},
    {Param::VelAmp, 0.3f},        {Param::LfoRate, 0.74f},  {Param::LfoInt, 0.06f},
};

constexpr PresetValue kSlowSweep[] = {
    {Param::Level, -5.0f},    {Param::Osc2Pitch, 12.0f},
    {Param::Osc2Level, 0.9f}, {Param::Cutoff, 500.0f},
    {Param::Resonance, 0.2f}, {Param::KeyTrack, kTrack50},
    {Param::AmpAttack, 1.0f}, {Param::AmpDecay, 3.0f},
    {Param::VelAmp, 0.3f},    {Param::EnvType, v(EnvelopeType::AD)},
    {Param::EnvAttack, 3.0f}, {Param::EnvDecay, 4.0f},
    {Param::EnvInt, 0.6f},
};

constexpr PresetValue kDarkBloom[] = {
    {Param::Level, -1.5f},
    {Param::Osc1Wave, v(Osc1Wave::Square)},
    {Param::Osc1Shape, 0.3f},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet16)},
    {Param::Osc2Level, 0.6f},
    {Param::Cutoff, 700.0f},
    {Param::Resonance, 0.2f},
    {Param::AmpAttack, 1.6f},
    {Param::AmpDecay, 4.0f},
    {Param::VelAmp, 0.3f},
    {Param::LfoRate, 0.25f},
    {Param::LfoInt, 0.5f},
    {Param::LfoTarget, v(LfoTarget::Shape)},
};

// ---- Bells ----------------------------------------------------------------------------------

constexpr PresetValue kGlassBell[] = {
    {Param::Level, 1.5f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc1Level, 0.6f},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},  // ratio 3.5
    {Param::Osc2Pitch, 966.0f},
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.8f},
    {Param::Cutoff, 2500.0f},
    {Param::VelCutoff, 0.5f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 3.0f},
    {Param::VelAmp, 0.7f},
    {Param::EnvDecay, 1.2f},
    {Param::EnvInt, 0.45f},
};

constexpr PresetValue kTubularBell[] = {
    {Param::Level, 1.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc1Level, 0.7f},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},  // ratio 2.76
    {Param::Osc2Pitch, 551.0f},
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.5f},
    {Param::Cutoff, 4000.0f},
    {Param::VelCutoff, 0.4f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 5.0f},
    {Param::VelAmp, 0.7f},
    {Param::EnvDecay, 2.0f},
    {Param::EnvInt, 0.3f},
};

constexpr PresetValue kMusicBox[] = {
    {Param::Level, 0.5f},
    {Param::Octave, 1.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet2)},
    {Param::Osc2Pitch, 4.0f},
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.35f},
    {Param::Cutoff, 6000.0f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 1.4f},
    {Param::VelAmp, 0.7f},
};

constexpr PresetValue kChurchBell[] = {
    {Param::Level, 0.5f},
    {Param::Octave, -1.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc1Level, 0.7f},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},  // ratio 2.5
    {Param::Osc2Pitch, 386.0f},
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.6f},
    {Param::Drive, 0.15f},
    {Param::Cutoff, 3200.0f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 7.0f},
    {Param::VelAmp, 0.6f},
    {Param::EnvDecay, 3.0f},
    {Param::EnvInt, 0.3f},
};

constexpr PresetValue kCrystal[] = {
    {Param::Level, 0.0f},
    {Param::Osc1Wave, v(Osc1Wave::Square)},
    {Param::Osc1Shape, 0.6f},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet2)},  // ratio 5.19
    {Param::Osc2Pitch, -104.0f},
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.7f},
    {Param::Cutoff, 9000.0f},
    {Param::Resonance, 0.1f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 2.5f},
    {Param::VelAmp, 0.7f},
};

// ---- Gongs ----------------------------------------------------------------------------------

constexpr PresetValue kTempleGong[] = {
    {Param::Level, 2.5f},
    {Param::Octave, -1.0f},
    {Param::Osc1Level, 0.5f},
    {Param::Osc2Pitch, 600.0f},  // ratio 1.41
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.8f},
    {Param::Drive, 0.2f},
    {Param::Cutoff, 1800.0f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpAttack, 0.012f},
    {Param::AmpDecay, 7.5f},
    {Param::VelAmp, 0.6f},
    {Param::EnvDecay, 3.0f},
    {Param::EnvInt, 0.55f},
    {Param::LfoRate, 0.55f},
    {Param::LfoInt, 0.15f},
    {Param::LfoTarget, v(LfoTarget::Cutoff)},
};

constexpr PresetValue kTamTam[] = {
    {Param::Level, 0.0f},
    {Param::Octave, -1.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc1Level, 0.3f},
    {Param::Osc2Wave, v(Osc2Wave::Noise)},
    {Param::Osc2Level, 0.9f},
    {Param::Cutoff, 2500.0f},
    {Param::Resonance, 0.35f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpAttack, 0.02f},
    {Param::AmpDecay, 8.0f},
    {Param::VelAmp, 0.6f},
    {Param::EnvDecay, 2.5f},
    {Param::EnvInt, 0.6f},
    {Param::LfoRate, 0.6f},
    {Param::LfoInt, 0.25f},
    {Param::LfoTarget, v(LfoTarget::Cutoff)},
};

constexpr PresetValue kBronzeGong[] = {
    {Param::Level, -2.5f},
    {Param::Osc1Wave, v(Osc1Wave::Square)},
    {Param::Osc1Shape, 0.5f},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},  // ratio 2.35
    {Param::Osc2Pitch, 280.0f},
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.8f},
    {Param::Cutoff, 3000.0f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpAttack, 0.008f},
    {Param::AmpDecay, 6.0f},
    {Param::VelAmp, 0.6f},
    {Param::EnvDecay, 2.5f},
    {Param::EnvInt, 0.4f},
};

constexpr PresetValue kGhostGong[] = {
    {Param::Level, -3.5f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc2Pitch, 300.0f},  // ratio 1.19
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.9f},
    {Param::Cutoff, 1200.0f},
    {Param::Resonance, 0.3f},
    {Param::AmpAttack, 0.9f},
    {Param::AmpDecay, 8.0f},
    {Param::VelAmp, 0.3f},
    {Param::LfoRate, 0.3f},
    {Param::LfoInt, 0.04f},
};

// The opening gong of a famous 1982 record was a stock digital-FM patch built from partials with
// a non-integer FM ratio, heavy chorusing, and EQ to tame the raw brightness. This is an original
// patch in that spirit, not a copy of it:
//  - two-operator FM at a ratio of the square root of two, so the sidebands are inharmonic;
//  - the mod envelope decays the FM index, so the bright clang collapses into a purer bell tone
//    (what sounds like a pitch bend is the modulator fading);
//  - a one-shot LFO adds a short burst of brightness on the strike and falls away;
//  - a low-pass keeps the body dark, and a wide, slow chorus gives the phased shimmer.
constexpr PresetValue kSynclavierGong[] = {
    {Param::Level, -1.0f},
    {Param::Osc1Level, 1.0f},
    {Param::Osc2Pitch, 600.0f},
    {Param::Osc2SyncRing, v(SyncRing::Fm)},
    {Param::Osc2Level, 0.1f},
    {Param::Drive, 0.12f},
    {Param::Cutoff, 2600.0f},
    {Param::Resonance, 0.05f},
    {Param::KeyTrack, kTrack50},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpAttack, 0.002f},
    {Param::AmpDecay, 8.0f},
    {Param::VelAmp, 0.5f},
    {Param::EnvDecay, 2.6f},
    {Param::EnvInt, 0.8f},
    {Param::EnvTarget, v(EnvelopeTarget::Level2)},
    {Param::LfoWave, v(LfoWave::Saw)},
    {Param::LfoMode, v(LfoMode::OneShot)},
    {Param::LfoRate, 0.47f},
    {Param::LfoInt, -0.5f},
    {Param::LfoTarget, v(LfoTarget::Cutoff)},
    {Param::ChorusMix, 0.7f},
    {Param::ChorusRate, 0.35f},
    {Param::ChorusDepth, 0.85f},
};

// ---- Basses ---------------------------------------------------------------------------------

constexpr PresetValue kSubBass[] = {
    {Param::Level, -0.5f},
    {Param::KeyMode, v(KeyMode::Mono)},
    {Param::Octave, -1.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc1Level, 0.9f},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet16)},
    {Param::Osc2Level, 0.5f},
    {Param::Drive, 0.15f},
    {Param::Cutoff, 500.0f},
    {Param::Resonance, 0.1f},
    {Param::KeyTrack, kTrack50},
    {Param::AmpAttack, 0.003f},
    {Param::AmpDecay, 0.12f},
    {Param::EnvDecay, 0.2f},
    {Param::EnvInt, 0.25f},
};

constexpr PresetValue kAcidBass[] = {
    {Param::Level, -1.5f},       {Param::KeyMode, v(KeyMode::Mono)},
    {Param::GlideTime, 0.07f},   {Param::Drive, 0.35f},
    {Param::Cutoff, 350.0f},     {Param::Resonance, 0.7f},
    {Param::KeyTrack, kTrack50}, {Param::VelCutoff, 0.4f},
    {Param::AmpDecay, 0.08f},    {Param::EnvDecay, 0.22f},
    {Param::EnvInt, 0.55f},
};

constexpr PresetValue kFatMono[] = {
    {Param::Level, -4.5f},     {Param::KeyMode, v(KeyMode::Mono)},
    {Param::GlideTime, 0.05f}, {Param::GlideMode, v(GlideMode::On)},
    {Param::Octave, -1.0f},    {Param::Osc2Pitch, 9.0f},
    {Param::Osc2Level, 0.8f},  {Param::Cutoff, 900.0f},
    {Param::Resonance, 0.25f}, {Param::KeyTrack, kTrack50},
    {Param::AmpDecay, 0.15f},  {Param::EnvDecay, 0.3f},
    {Param::EnvInt, 0.3f},
};

// ---- Leads ----------------------------------------------------------------------------------

constexpr PresetValue kSyncLead[] = {
    {Param::Level, -0.5f},
    {Param::KeyMode, v(KeyMode::Mono)},
    {Param::GlideTime, 0.05f},
    {Param::Osc1Level, 0.3f},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},
    {Param::Osc2Pitch, 200.0f},
    {Param::Osc2SyncRing, v(SyncRing::Sync)},
    {Param::Osc2Level, 0.9f},
    {Param::Cutoff, 6000.0f},
    {Param::AmpDecay, 0.2f},
    {Param::EnvDecay, 0.6f},
    {Param::EnvInt, 0.25f},
    {Param::EnvTarget, v(EnvelopeTarget::Pitch2)},
    {Param::LfoRate, 0.74f},
    {Param::LfoInt, 0.05f},
};

constexpr PresetValue kSoftLead[] = {
    {Param::Level, -3.0f},       {Param::KeyMode, v(KeyMode::Mono)},
    {Param::GlideTime, 0.09f},   {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc2Level, 0.5f},    {Param::Osc2Pitch, 6.0f},
    {Param::Cutoff, 2800.0f},    {Param::Resonance, 0.1f},
    {Param::KeyTrack, kTrack50}, {Param::AmpAttack, 0.01f},
    {Param::AmpDecay, 0.25f},    {Param::LfoRate, 0.74f},
    {Param::LfoInt, 0.09f},
};

constexpr PresetValue kBrightSaw[] = {
    {Param::Level, -4.5f},      {Param::Osc2Pitch, 12.0f}, {Param::Osc2Level, 0.9f},
    {Param::Drive, 0.2f},       {Param::Cutoff, 7000.0f},  {Param::KeyTrack, kTrack50},
    {Param::AmpAttack, 0.004f}, {Param::AmpDecay, 0.25f},  {Param::EnvDecay, 0.4f},
    {Param::EnvInt, 0.2f},
};

// ---- Plucks ---------------------------------------------------------------------------------

constexpr PresetValue kPizzicato[] = {
    {Param::Level, 3.0f},
    {Param::Osc1Level, 1.0f},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},
    {Param::Osc2Level, 0.6f},
    {Param::Cutoff, 600.0f},
    {Param::KeyTrack, kTrack100},
    {Param::VelCutoff, 0.5f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 0.5f},
    {Param::EnvDecay, 0.12f},
    {Param::EnvInt, 0.5f},
};

constexpr PresetValue kKalimba[] = {
    {Param::Level, 1.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet2)},
    {Param::Osc2Pitch, 20.0f},
    {Param::Osc2Level, 0.25f},
    {Param::Cutoff, 3000.0f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 0.9f},
    {Param::VelAmp, 0.7f},
    {Param::EnvDecay, 0.2f},
    {Param::EnvInt, 0.2f},
};

constexpr PresetValue kHarpPluck[] = {
    {Param::Level, -2.5f},   {Param::Osc2Pitch, 5.0f},     {Param::Osc2Level, 0.4f},
    {Param::Cutoff, 900.0f}, {Param::KeyTrack, kTrack100}, {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 1.6f}, {Param::VelAmp, 0.6f},        {Param::EnvDecay, 0.35f},
    {Param::EnvInt, 0.45f},
};

// ---- Ambient --------------------------------------------------------------------------------

constexpr PresetValue kDeepDrone[] = {
    {Param::Level, 2.5f},
    {Param::Polyphony, 4.0f},
    {Param::GlideTime, 0.3f},
    {Param::GlideMode, v(GlideMode::On)},
    {Param::Octave, -1.0f},
    {Param::Osc2Pitch, 7.0f},
    {Param::Osc2Level, 0.9f},
    {Param::Cutoff, 500.0f},
    {Param::Resonance, 0.3f},
    {Param::AmpAttack, 3.5f},
    {Param::AmpDecay, 6.0f},
    {Param::VelAmp, 0.2f},
    {Param::LfoRate, 0.25f},
    {Param::LfoInt, 0.35f},
    {Param::LfoTarget, v(LfoTarget::Cutoff)},
};

constexpr PresetValue kShimmer[] = {
    {Param::Level, -2.0f},
    {Param::Octave, 1.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet2)},
    {Param::Osc2Pitch, 7.0f},
    {Param::Osc2Level, 0.7f},
    {Param::Cutoff, 8000.0f},
    {Param::AmpAttack, 2.0f},
    {Param::AmpDecay, 5.0f},
    {Param::VelAmp, 0.2f},
    {Param::LfoRate, 0.45f},
    {Param::LfoInt, 0.6f},
    {Param::LfoTarget, v(LfoTarget::Shape)},
};

constexpr PresetValue kWind[] = {
    {Param::Level, 6.0f},
    {Param::Osc1Level, 0.0f},
    {Param::Osc2Wave, v(Osc2Wave::Noise)},
    {Param::Osc2Level, 1.0f},
    {Param::Cutoff, 2400.0f},
    {Param::Resonance, 0.45f},
    {Param::KeyTrack, kTrack100},
    {Param::AmpAttack, 2.5f},
    {Param::AmpDecay, 3.0f},
    {Param::VelAmp, 0.2f},
    {Param::LfoRate, 0.3f},
    {Param::LfoInt, 0.45f},
    {Param::LfoTarget, v(LfoTarget::Cutoff)},
};

// ---- Distorted ------------------------------------------------------------------------------

constexpr PresetValue kGrowl[] = {
    {Param::Level, -2.5f},
    {Param::KeyMode, v(KeyMode::Mono)},
    {Param::Octave, -1.0f},
    {Param::Osc2Pitch, 6.0f},
    {Param::Osc2Level, 0.9f},
    {Param::Drive, 0.85f},
    {Param::Cutoff, 700.0f},
    {Param::Resonance, 0.5f},
    {Param::KeyTrack, kTrack50},
    {Param::AmpDecay, 0.2f},
    {Param::EnvDecay, 0.3f},
    {Param::EnvInt, 0.3f},
    {Param::LfoRate, 0.72f},
    {Param::LfoInt, 0.5f},
    {Param::LfoTarget, v(LfoTarget::Cutoff)},
};

constexpr PresetValue kCrunchLead[] = {
    {Param::Level, -2.0f},
    {Param::KeyMode, v(KeyMode::Mono)},
    {Param::GlideTime, 0.04f},
    {Param::Osc1Level, 0.4f},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},
    {Param::Osc2Pitch, 300.0f},
    {Param::Osc2SyncRing, v(SyncRing::Sync)},
    {Param::Osc2Level, 0.9f},
    {Param::Drive, 0.75f},
    {Param::Cutoff, 5000.0f},
    {Param::Resonance, 0.2f},
    {Param::AmpDecay, 0.2f},
    {Param::EnvDecay, 0.5f},
    {Param::EnvInt, 0.2f},
    {Param::EnvTarget, v(EnvelopeTarget::Pitch2)},
};

// ---- Keys -----------------------------------------------------------------------------------

constexpr PresetValue kElectricKeys[] = {
    {Param::Level, 3.0f},
    {Param::Osc1Wave, v(Osc1Wave::Triangle)},
    {Param::Osc1Level, 0.8f},
    {Param::Osc2Wave, v(Osc2Wave::Triangle)},
    {Param::Osc2Octave, v(Osc2Range::Feet4)},
    {Param::Osc2Pitch, 6.0f},
    {Param::Osc2SyncRing, v(SyncRing::Ring)},
    {Param::Osc2Level, 0.25f},
    {Param::Cutoff, 3500.0f},
    {Param::VelCutoff, 0.6f},
    {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 1.8f},
    {Param::VelAmp, 0.7f},
    {Param::EnvDecay, 0.4f},
    {Param::EnvInt, 0.3f},
};

constexpr PresetValue kClav[] = {
    {Param::Level, 4.5f},         {Param::Osc1Wave, v(Osc1Wave::Square)},
    {Param::Osc1Shape, 0.55f},    {Param::Osc2Level, 0.0f},
    {Param::Cutoff, 2200.0f},     {Param::Resonance, 0.2f},
    {Param::KeyTrack, kTrack100}, {Param::AmpType, v(EnvelopeType::AD)},
    {Param::AmpDecay, 0.35f},     {Param::VelAmp, 0.6f},
    {Param::EnvDecay, 0.1f},      {Param::EnvInt, 0.45f},
};

constexpr std::array kPresets = {
    FactoryPreset{"Warm Pad", "Pad", kWarmPad},
    FactoryPreset{"Glass Pad", "Pad", kGlassPad},
    FactoryPreset{"Choir Drift", "Pad", kChoirDrift},
    FactoryPreset{"Slow Sweep", "Pad", kSlowSweep},
    FactoryPreset{"Dark Bloom", "Pad", kDarkBloom},
    FactoryPreset{"Glass Bell", "Bell", kGlassBell},
    FactoryPreset{"Tubular Bell", "Bell", kTubularBell},
    FactoryPreset{"Music Box", "Bell", kMusicBox},
    FactoryPreset{"Church Bell", "Bell", kChurchBell},
    FactoryPreset{"Crystal", "Bell", kCrystal},
    FactoryPreset{"Temple Gong", "Gong", kTempleGong},
    FactoryPreset{"Tam Tam", "Gong", kTamTam},
    FactoryPreset{"Bronze Gong", "Gong", kBronzeGong},
    FactoryPreset{"Ghost Gong", "Gong", kGhostGong},
    FactoryPreset{"Synclavier Gong", "Gong", kSynclavierGong},
    FactoryPreset{"Sub Bass", "Bass", kSubBass},
    FactoryPreset{"Acid Bass", "Bass", kAcidBass},
    FactoryPreset{"Fat Mono", "Bass", kFatMono},
    FactoryPreset{"Sync Lead", "Lead", kSyncLead},
    FactoryPreset{"Soft Lead", "Lead", kSoftLead},
    FactoryPreset{"Bright Saw", "Lead", kBrightSaw},
    FactoryPreset{"Pizzicato", "Pluck", kPizzicato},
    FactoryPreset{"Kalimba", "Pluck", kKalimba},
    FactoryPreset{"Harp Pluck", "Pluck", kHarpPluck},
    FactoryPreset{"Deep Drone", "Ambient", kDeepDrone},
    FactoryPreset{"Shimmer", "Ambient", kShimmer},
    FactoryPreset{"Wind", "Ambient", kWind},
    FactoryPreset{"Growl", "Distorted", kGrowl},
    FactoryPreset{"Crunch Lead", "Distorted", kCrunchLead},
    FactoryPreset{"Electric Keys", "Keys", kElectricKeys},
    FactoryPreset{"Clav", "Keys", kClav},
};

constexpr std::array<const char*, 9> kCategories = {"Pad",   "Bell",    "Gong",      "Bass", "Lead",
                                                    "Pluck", "Ambient", "Distorted", "Keys"};

}  // namespace

std::span<const FactoryPreset> factoryPresets()
{
    return kPresets;
}

std::span<const char* const> presetCategories()
{
    return kCategories;
}

dsp::ParamValues resolve(const FactoryPreset& preset)
{
    dsp::ParamValues values = dsp::ParamValues::defaults();
    apply(preset, values);
    return values;
}

void apply(const FactoryPreset& preset, dsp::ParamValues& values)
{
    for (const PresetValue& entry : preset.values)
        values[entry.param] = dsp::clampToLegal(dsp::paramSpec(entry.param), entry.value);
}

}  // namespace polylogue::presets

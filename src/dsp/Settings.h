#pragma once

#include <cstdint>

namespace polylogue::dsp {

enum class Osc1Wave : std::uint8_t {
    Saw,
    Triangle,
    Square
};
enum class Osc2Wave : std::uint8_t {
    Saw,
    Triangle,
    Noise
};
enum class Osc2Range : std::uint8_t {
    Feet16,
    Feet8,
    Feet4,
    Feet2
};
enum class SyncRing : std::uint8_t {
    Off,
    Sync,
    Ring,
    Fm  // oscillator 2 phase-modulates oscillator 1; both are sines
};
enum class EnvelopeType : std::uint8_t {
    AD,
    AGD,
    Gate
};
enum class EnvelopeTarget : std::uint8_t {
    Cutoff,
    Pitch,
    Pitch2,
    Level1,
    Level2
};
enum class LfoWave : std::uint8_t {
    Saw,
    Triangle,
    Square
};
enum class LfoMode : std::uint8_t {
    Fast,
    Slow,
    OneShot
};
enum class LfoTarget : std::uint8_t {
    Pitch,
    Shape,
    Cutoff
};
enum class KeyMode : std::uint8_t {
    Poly,
    Mono
};
enum class GlideMode : std::uint8_t {
    Auto,
    On
};

struct EnvelopeSettings {
    EnvelopeType type = EnvelopeType::AGD;
    float attack = 0.002f;  // seconds
    float decay = 0.25f;    // seconds; the release time in AGD

    friend bool operator==(const EnvelopeSettings&, const EnvelopeSettings&) = default;
};

struct LfoSettings {
    LfoWave wave = LfoWave::Triangle;
    LfoMode mode = LfoMode::Slow;
    float rate = 0.4f;    // 0..1 across the mode's frequency range
    float amount = 0.0f;  // -1..1
    LfoTarget target = LfoTarget::Pitch;

    friend bool operator==(const LfoSettings&, const LfoSettings&) = default;
};

// Everything a voice needs to render a block, in plain units. Built once per block from the
// parameter values, so DSP modules never index into the parameter table.
struct SynthSettings {
    float outputGain = 1.0f;  // linear
    int polyphony = 8;
    KeyMode keyMode = KeyMode::Poly;
    int bendRangeSemitones = 2;
    float glideSeconds = 0.0f;  // 0 disables glide
    GlideMode glideMode = GlideMode::Auto;

    int octave = 0;
    float tuneCents = 0.0f;
    float pitchBendSemitones = 0.0f;  // set by the engine from the bend wheel

    Osc1Wave osc1Wave = Osc1Wave::Saw;
    float osc1Shape = 0.0f;
    float osc1Level = 1.0f;

    Osc2Wave osc2Wave = Osc2Wave::Saw;
    Osc2Range osc2Range = Osc2Range::Feet8;
    float osc2Cents = 0.0f;
    SyncRing syncRing = SyncRing::Off;
    float osc2Shape = 0.0f;
    float osc2Level = 0.0f;

    float cutoffHz = 8000.0f;
    float resonance = 0.0f;
    float keyTrack = 0.0f;  // 0, 0.5 or 1: fraction of the keyboard interval the cutoff follows
    float velocityToCutoff = 0.0f;

    EnvelopeSettings ampEnv{};
    float velocityToAmp = 0.5f;

    EnvelopeSettings modEnv{EnvelopeType::AD, 0.001f, 0.3f};
    float modEnvAmount = 0.0f;  // -1..1
    EnvelopeTarget modEnvTarget = EnvelopeTarget::Cutoff;

    LfoSettings lfo{};
    float drive = 0.0f;  // 0..1

    float chorusMix = 0.0f;    // 0 bypasses the chorus
    float chorusRate = 0.6f;   // Hz
    float chorusDepth = 0.5f;  // 0..1

    friend bool operator==(const SynthSettings&, const SynthSettings&) = default;
};

constexpr int osc2RangeSemitones(Osc2Range range)
{
    switch (range) {
    case Osc2Range::Feet16:
        return -12;
    case Osc2Range::Feet8:
        return 0;
    case Osc2Range::Feet4:
        return 12;
    case Osc2Range::Feet2:
        return 24;
    }
    return 0;
}

}  // namespace polylogue::dsp

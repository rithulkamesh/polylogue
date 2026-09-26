#pragma once

#include "dsp/Settings.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace polylogue::dsp {

// Every automatable parameter. Append only: reordering or removing an entry would change what
// saved sessions mean. Ids in the table are the stable identity, this order is not.
enum class Param : std::uint8_t {
    Level,
    Polyphony,
    KeyMode,
    Octave,
    Tune,
    BendRange,
    GlideTime,
    GlideMode,
    Drive,
    Osc1Wave,
    Osc1Shape,
    Osc2Wave,
    Osc2Octave,
    Osc2Pitch,
    Osc2SyncRing,
    Osc2Shape,
    Osc1Level,
    Osc2Level,
    Cutoff,
    Resonance,
    KeyTrack,
    VelCutoff,
    AmpType,
    AmpAttack,
    AmpDecay,
    VelAmp,
    EnvType,
    EnvAttack,
    EnvDecay,
    EnvInt,
    EnvTarget,
    LfoWave,
    LfoMode,
    LfoRate,
    LfoInt,
    LfoTarget,
    Count
};

inline constexpr std::size_t kParamCount = static_cast<std::size_t>(Param::Count);

constexpr std::size_t index(Param param)
{
    return static_cast<std::size_t>(param);
}

enum class ParamKind : std::uint8_t {
    Float,
    Int,
    Choice,
    Bool
};

// How a parameter's plain range maps onto a 0..1 control.
enum class Curve : std::uint8_t {
    Linear,
    Exponential,     // equal steps are equal ratios; min must be positive
    Power,           // n^exponent, fine control at the low end
    SymmetricPower,  // fine control around the centre, full range at both ends
};

struct ParamSpec {
    const char* id;
    const char* name;
    ParamKind kind;
    float min;
    float max;
    float defaultValue;
    Curve curve = Curve::Linear;
    float exponent = 1.0f;
    float step = 0.0f;  // Float: snap size, 0 for continuous
    const char* unit = "";
    std::span<const char* const> choices = {};  // Choice: one label per value
};

std::span<const ParamSpec> paramSpecs();
const ParamSpec& paramSpec(Param param);
std::optional<Param> paramFromId(std::string_view id);

// Plain values live in the parameter's own units; `normalized` is the 0..1 control position.
float toNormalized(const ParamSpec& spec, float plain);
float toPlain(const ParamSpec& spec, float normalized);
// Clamps to the range and snaps to whole numbers, choices or steps.
float clampToLegal(const ParamSpec& spec, float plain);
// Text for the display, like "1.20 kHz", "+7 ct", or a choice label.
std::string formatValue(const ParamSpec& spec, float plain);
// Reads text a user typed or a host sent back: a number with an optional unit prefix or suffix
// ("1.5k", "250 ms", "40%"), or a choice label.
std::optional<float> parseValue(const ParamSpec& spec, std::string_view text);

struct ParamValues {
    std::array<float, kParamCount> values{};

    float& operator[](Param param) { return values[index(param)]; }
    const float& operator[](Param param) const { return values[index(param)]; }

    static ParamValues defaults();
};

SynthSettings toSettings(const ParamValues& values);

// The plain value a 7-bit MIDI controller selects. Continuous parameters follow the parameter's
// own curve; integers, choices and switches split the 0..127 range into equal bands.
float plainFromController(const ParamSpec& spec, int value);

}  // namespace polylogue::dsp

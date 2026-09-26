#include "dsp/Parameters.h"

#include "dsp/Tuning.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace polylogue::dsp {
namespace {

constexpr std::array<const char*, 2> kKeyModes = {"Poly", "Mono"};
constexpr std::array<const char*, 2> kGlideModes = {"Auto", "On"};
constexpr std::array<const char*, 3> kOsc1Waves = {"Saw", "Triangle", "Square"};
constexpr std::array<const char*, 3> kOsc2Waves = {"Saw", "Triangle", "Noise"};
constexpr std::array<const char*, 4> kOsc2Ranges = {"16'", "8'", "4'", "2'"};
constexpr std::array<const char*, 3> kSyncRing = {"Off", "Sync", "Ring"};
constexpr std::array<const char*, 3> kKeyTrack = {"0%", "50%", "100%"};
constexpr std::array<const char*, 3> kAmpTypes = {"A/D", "A/G/D", "Gate"};
constexpr std::array<const char*, 2> kEnvTypes = {"A/D", "A/G/D"};
constexpr std::array<const char*, 3> kEnvTargets = {"Cutoff", "Pitch", "Pitch 2"};
constexpr std::array<const char*, 3> kLfoWaves = {"Saw", "Triangle", "Square"};
constexpr std::array<const char*, 3> kLfoModes = {"Fast", "Slow", "1-Shot"};
constexpr std::array<const char*, 3> kLfoTargets = {"Pitch", "Shape", "Cutoff"};

constexpr ParamSpec floatParam(const char* id, const char* name, float min, float max, float def,
                               const char* unit = "", Curve curve = Curve::Linear,
                               float exponent = 1.0f, float step = 0.0f)
{
    return {id, name, ParamKind::Float, min, max, def, curve, exponent, step, unit, {}};
}

constexpr ParamSpec intParam(const char* id, const char* name, int min, int max, int def,
                             const char* unit = "")
{
    return {id,
            name,
            ParamKind::Int,
            static_cast<float>(min),
            static_cast<float>(max),
            static_cast<float>(def),
            Curve::Linear,
            1.0f,
            1.0f,
            unit,
            {}};
}

template<std::size_t N>
constexpr ParamSpec choiceParam(const char* id, const char* name,
                                const std::array<const char*, N>& labels, int def)
{
    return {id,
            name,
            ParamKind::Choice,
            0.0f,
            static_cast<float>(N - 1),
            static_cast<float>(def),
            Curve::Linear,
            1.0f,
            1.0f,
            "",
            labels};
}

// Same order as `Param`.
constexpr std::array<ParamSpec, kParamCount> kSpecs = {{
    floatParam("level", "Level", -60.0f, 6.0f, 0.0f, "dB"),
    intParam("polyphony", "Voices", 1, 16, 8),
    choiceParam("key_mode", "Key Mode", kKeyModes, 0),
    intParam("octave", "Octave", -2, 2, 0),
    floatParam("tune", "Tune", -50.0f, 50.0f, 0.0f, "ct"),
    intParam("bend_range", "Bend Range", 1, 12, 2, "st"),
    floatParam("glide_time", "Glide Time", 0.0f, 2.0f, 0.0f, "s", Curve::Power, 3.0f),
    choiceParam("glide_mode", "Glide Mode", kGlideModes, 0),
    floatParam("drive", "Drive", 0.0f, 1.0f, 0.0f),
    choiceParam("osc1_wave", "Osc 1 Wave", kOsc1Waves, 0),
    floatParam("osc1_shape", "Osc 1 Shape", 0.0f, 1.0f, 0.0f),
    choiceParam("osc2_wave", "Osc 2 Wave", kOsc2Waves, 0),
    choiceParam("osc2_octave", "Osc 2 Octave", kOsc2Ranges, 1),
    floatParam("osc2_pitch", "Osc 2 Pitch", -1200.0f, 1200.0f, 0.0f, "ct", Curve::SymmetricPower,
               3.0f, 1.0f),
    choiceParam("osc2_sync_ring", "Sync / Ring", kSyncRing, 0),
    floatParam("osc2_shape", "Osc 2 Shape", 0.0f, 1.0f, 0.0f),
    floatParam("osc1_level", "Osc 1 Level", 0.0f, 1.0f, 1.0f),
    floatParam("osc2_level", "Osc 2 Level", 0.0f, 1.0f, 0.0f),
    floatParam("cutoff", "Cutoff", 20.0f, 20000.0f, 8000.0f, "Hz", Curve::Exponential),
    floatParam("resonance", "Resonance", 0.0f, 1.0f, 0.0f),
    choiceParam("key_track", "Key Track", kKeyTrack, 0),
    floatParam("vel_cutoff", "Velocity to Cutoff", 0.0f, 1.0f, 0.0f),
    choiceParam("amp_type", "Amp Env Type", kAmpTypes, 1),
    floatParam("amp_attack", "Amp Attack", 0.001f, 4.0f, 0.002f, "s", Curve::Exponential),
    floatParam("amp_decay", "Amp Decay", 0.005f, 8.0f, 0.25f, "s", Curve::Exponential),
    floatParam("vel_amp", "Velocity to Amp", 0.0f, 1.0f, 0.5f),
    choiceParam("env_type", "Mod Env Type", kEnvTypes, 0),
    floatParam("env_attack", "Mod Attack", 0.001f, 4.0f, 0.001f, "s", Curve::Exponential),
    floatParam("env_decay", "Mod Decay", 0.005f, 8.0f, 0.3f, "s", Curve::Exponential),
    floatParam("env_int", "Mod Env Int", -1.0f, 1.0f, 0.0f),
    choiceParam("env_target", "Mod Env Target", kEnvTargets, 0),
    choiceParam("lfo_wave", "LFO Wave", kLfoWaves, 1),
    choiceParam("lfo_mode", "LFO Mode", kLfoModes, 1),
    floatParam("lfo_rate", "LFO Rate", 0.0f, 1.0f, 0.4f),
    floatParam("lfo_int", "LFO Int", -1.0f, 1.0f, 0.0f),
    choiceParam("lfo_target", "LFO Target", kLfoTargets, 0),
}};

float snapToStep(float value, float step)
{
    return step > 0.0f ? std::round(value / step) * step : value;
}

}  // namespace

std::span<const ParamSpec> paramSpecs()
{
    return kSpecs;
}

const ParamSpec& paramSpec(Param param)
{
    return kSpecs[index(param)];
}

std::optional<Param> paramFromId(std::string_view id)
{
    for (std::size_t i = 0; i < kSpecs.size(); ++i) {
        if (id == kSpecs[i].id)
            return static_cast<Param>(i);
    }
    return std::nullopt;
}

float toNormalized(const ParamSpec& spec, float plain)
{
    const float clamped = std::clamp(plain, spec.min, spec.max);
    const float span = spec.max - spec.min;
    switch (spec.curve) {
    case Curve::Linear:
        return (clamped - spec.min) / span;
    case Curve::Exponential:
        return std::log(clamped / spec.min) / std::log(spec.max / spec.min);
    case Curve::Power:
        return std::pow((clamped - spec.min) / span, 1.0f / spec.exponent);
    case Curve::SymmetricPower: {
        const float t = (2.0f * clamped - spec.min - spec.max) / span;
        return 0.5f + 0.5f * std::copysign(std::pow(std::abs(t), 1.0f / spec.exponent), t);
    }
    }
    return 0.0f;
}

float toPlain(const ParamSpec& spec, float normalized)
{
    const float n = std::clamp(normalized, 0.0f, 1.0f);
    const float span = spec.max - spec.min;
    float plain = spec.min;
    switch (spec.curve) {
    case Curve::Linear:
        plain = spec.min + n * span;
        break;
    case Curve::Exponential:
        plain = spec.min * std::pow(spec.max / spec.min, n);
        break;
    case Curve::Power:
        plain = spec.min + span * std::pow(n, spec.exponent);
        break;
    case Curve::SymmetricPower: {
        const float t = 2.0f * n - 1.0f;
        plain = 0.5f * (spec.min + spec.max) +
                0.5f * span * std::copysign(std::pow(std::abs(t), spec.exponent), t);
        break;
    }
    }
    return clampToLegal(spec, plain);
}

float clampToLegal(const ParamSpec& spec, float plain)
{
    const float snapped =
        spec.kind == ParamKind::Float ? snapToStep(plain, spec.step) : std::round(plain);
    return std::clamp(snapped, spec.min, spec.max);
}

float plainFromController(const ParamSpec& spec, int value)
{
    const int clamped = std::clamp(value, 0, 127);
    if (spec.kind == ParamKind::Float)
        return toPlain(spec, static_cast<float>(clamped) / 127.0f);

    const int steps = static_cast<int>(spec.max - spec.min) + 1;
    const int index = std::min(steps - 1, clamped * steps / 128);
    return spec.min + static_cast<float>(index);
}

std::string formatValue(const ParamSpec& spec, float plain)
{
    char text[32];
    const std::string_view unit = spec.unit;

    switch (spec.kind) {
    case ParamKind::Choice:
        return spec.choices[static_cast<std::size_t>(clampToLegal(spec, plain))];
    case ParamKind::Bool:
        return plain >= 0.5f ? "On" : "Off";
    case ParamKind::Int:
        std::snprintf(text, sizeof text, "%d%s%s", static_cast<int>(std::lround(plain)),
                      unit.empty() ? "" : " ", spec.unit);
        return text;
    case ParamKind::Float:
        break;
    }

    if (unit == "Hz") {
        if (plain >= 1000.0f)
            std::snprintf(text, sizeof text, "%.2f kHz", static_cast<double>(plain) / 1000.0);
        else
            std::snprintf(text, sizeof text, "%.0f Hz", static_cast<double>(plain));
    } else if (unit == "s") {
        if (plain < 0.0995f)
            std::snprintf(text, sizeof text, "%.1f ms", static_cast<double>(plain) * 1000.0);
        else if (plain < 1.0f)
            std::snprintf(text, sizeof text, "%.0f ms", static_cast<double>(plain) * 1000.0);
        else
            std::snprintf(text, sizeof text, "%.2f s", static_cast<double>(plain));
    } else if (unit == "ct") {
        std::snprintf(text, sizeof text, "%+.0f ct", static_cast<double>(plain));
    } else if (unit == "dB") {
        std::snprintf(text, sizeof text, "%.1f dB", static_cast<double>(plain));
    } else if (spec.min < 0.0f) {
        std::snprintf(text, sizeof text, "%+.0f%%", static_cast<double>(plain) * 100.0);
    } else {
        std::snprintf(text, sizeof text, "%.0f%%", static_cast<double>(plain) * 100.0);
    }
    return text;
}

std::optional<float> parseValue(const ParamSpec& spec, std::string_view text)
{
    auto lower = [](std::string_view view) {
        std::string out(view);
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return out;
    };
    const std::string typed = lower(text);

    if (spec.kind == ParamKind::Choice) {
        for (std::size_t i = 0; i < spec.choices.size(); ++i) {
            if (typed == lower(spec.choices[i]))
                return static_cast<float>(i);
        }
    }
    if (spec.kind == ParamKind::Bool) {
        if (typed == "on")
            return 1.0f;
        if (typed == "off")
            return 0.0f;
    }

    char* end = nullptr;
    const float number = std::strtof(typed.c_str(), &end);
    if (end == typed.c_str())
        return std::nullopt;

    const std::string rest = typed.substr(static_cast<std::size_t>(end - typed.c_str()));
    const std::string_view unit = spec.unit;
    float value = number;
    if (rest.find("khz") != std::string::npos ||
        (unit == "Hz" && rest.find('k') != std::string::npos))
        value *= 1000.0f;
    else if (rest.find("ms") != std::string::npos)
        value /= 1000.0f;
    else if (rest.find('%') != std::string::npos)
        value /= 100.0f;
    return clampToLegal(spec, value);
}

ParamValues ParamValues::defaults()
{
    ParamValues result;
    for (std::size_t i = 0; i < kSpecs.size(); ++i)
        result.values[i] = kSpecs[i].defaultValue;
    return result;
}

SynthSettings toSettings(const ParamValues& v)
{
    auto choice = [&](Param param) { return static_cast<int>(std::lround(v[param])); };

    SynthSettings s;
    s.outputGain =
        v[Param::Level] <= paramSpec(Param::Level).min + 0.05f ? 0.0f : dbToGain(v[Param::Level]);
    s.polyphony = choice(Param::Polyphony);
    s.keyMode = static_cast<KeyMode>(choice(Param::KeyMode));
    s.bendRangeSemitones = choice(Param::BendRange);
    s.glideSeconds = v[Param::GlideTime];
    s.glideMode = static_cast<GlideMode>(choice(Param::GlideMode));

    s.octave = choice(Param::Octave);
    s.tuneCents = v[Param::Tune];

    s.osc1Wave = static_cast<Osc1Wave>(choice(Param::Osc1Wave));
    s.osc1Shape = v[Param::Osc1Shape];
    s.osc1Level = v[Param::Osc1Level];

    s.osc2Wave = static_cast<Osc2Wave>(choice(Param::Osc2Wave));
    s.osc2Range = static_cast<Osc2Range>(choice(Param::Osc2Octave));
    s.osc2Cents = v[Param::Osc2Pitch];
    s.syncRing = static_cast<SyncRing>(choice(Param::Osc2SyncRing));
    s.osc2Shape = v[Param::Osc2Shape];
    s.osc2Level = v[Param::Osc2Level];

    s.cutoffHz = v[Param::Cutoff];
    s.resonance = v[Param::Resonance];
    s.keyTrack = static_cast<float>(choice(Param::KeyTrack)) * 0.5f;
    s.velocityToCutoff = v[Param::VelCutoff];

    s.ampEnv = {static_cast<EnvelopeType>(choice(Param::AmpType)), v[Param::AmpAttack],
                v[Param::AmpDecay]};
    s.velocityToAmp = v[Param::VelAmp];

    s.modEnv = {static_cast<EnvelopeType>(choice(Param::EnvType)), v[Param::EnvAttack],
                v[Param::EnvDecay]};
    s.modEnvAmount = v[Param::EnvInt];
    s.modEnvTarget = static_cast<EnvelopeTarget>(choice(Param::EnvTarget));

    s.lfo = {static_cast<LfoWave>(choice(Param::LfoWave)),
             static_cast<LfoMode>(choice(Param::LfoMode)), v[Param::LfoRate], v[Param::LfoInt],
             static_cast<LfoTarget>(choice(Param::LfoTarget))};
    s.drive = v[Param::Drive];
    return s;
}

}  // namespace polylogue::dsp

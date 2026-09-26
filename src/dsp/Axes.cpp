#include "dsp/Axes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace polylogue::dsp {
namespace {

constexpr float kPi = 3.14159265358979f;

// Boundaries between the kinds of sound the METAL axis passes through.
constexpr float kThickEnd = 0.35f;
constexpr float kSyncEnd = 0.5f;
constexpr float kRingEnd = 0.65f;

float unit(float x)
{
    return std::clamp(x, 0.0f, 1.0f);
}

// 0 below `from`, 1 above `to`, linear between.
float ramp(float x, float from, float to)
{
    return unit((x - from) / (to - from));
}

float lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

// Equal steps of `t` are equal ratios between `low` and `high`.
float geometric(float low, float high, float t)
{
    return low * std::pow(high / low, unit(t));
}

void set(ParamValues& v, Param param, float plain)
{
    v[param] = clampToLegal(paramSpec(param), plain);
}

// Oscillator 2's pitch above oscillator 1 in cents, spread over its range switch and fine tune.
void setOsc2Cents(ParamValues& v, float cents)
{
    if (cents < 0.0f) {
        set(v, Param::Osc2Octave, 0.0f);
        set(v, Param::Osc2Pitch, cents + 1200.0f);
    } else if (cents <= 1200.0f) {
        set(v, Param::Osc2Octave, 1.0f);
        set(v, Param::Osc2Pitch, cents);
    } else {
        set(v, Param::Osc2Octave, 2.0f);
        set(v, Param::Osc2Pitch, cents - 1200.0f);
    }
}

void applyWave(ParamValues& v, float wave)
{
    if (wave < 0.5f) {
        set(v, Param::Osc1Wave, 1.0f);
        set(v, Param::Osc1Shape, ramp(wave, 0.0f, 0.5f));
    } else if (wave < 0.75f) {
        set(v, Param::Osc1Wave, 0.0f);
        set(v, Param::Osc1Shape, ramp(wave, 0.5f, 0.75f));
    } else {
        set(v, Param::Osc1Wave, 2.0f);
        set(v, Param::Osc1Shape, ramp(wave, 0.75f, 1.0f));
    }
}

void applyMetal(ParamValues& v, float metal)
{
    set(v, Param::Osc1Level, 1.0f);
    set(v, Param::Osc2Wave, 0.0f);
    if (metal < kThickEnd) {
        const float t = ramp(metal, 0.0f, kThickEnd);
        set(v, Param::Osc2SyncRing, 0.0f);
        set(v, Param::Osc2Level, 0.75f * t);
        setOsc2Cents(v, lerp(0.0f, 9.0f, t));
    } else if (metal < kRingEnd) {
        set(v, Param::Osc2Level, 0.75f);
        if (metal < kSyncEnd) {
            set(v, Param::Osc2SyncRing, 1.0f);
            setOsc2Cents(v, lerp(700.0f, 2400.0f, ramp(metal, kThickEnd, kSyncEnd)));
        } else {
            set(v, Param::Osc2SyncRing, 2.0f);
            setOsc2Cents(v, lerp(500.0f, 1900.0f, ramp(metal, kSyncEnd, kRingEnd)));
        }
    } else {
        const float t = ramp(metal, kRingEnd, 1.0f);
        set(v, Param::Osc2SyncRing, 3.0f);
        set(v, Param::Osc2Level, lerp(0.06f, 0.55f, std::pow(t, 0.8f)));
        setOsc2Cents(v, lerp(400.0f, 2200.0f, t));
    }
}

void applyGrit(ParamValues& v, float grit, float metal)
{
    set(v, Param::Drive, 0.85f * std::pow(grit, 1.6f));
    set(v, Param::Resonance, 0.6f * grit * grit);
    // Oscillator 2 is free for noise only while METAL is not using it for something else.
    if (metal < kThickEnd && grit > 0.7f) {
        set(v, Param::Osc2Wave, 2.0f);
        set(v, Param::Osc2Level, std::max(v[Param::Osc2Level], 0.55f * ramp(grit, 0.7f, 1.0f)));
    }
}

void applyBright(ParamValues& v, float bright)
{
    set(v, Param::Cutoff, geometric(70.0f, 20000.0f, std::pow(bright, 1.1f)));
    set(v, Param::KeyTrack, 1.0f);
    set(v, Param::VelCutoff, 0.25f);
}

void applyEnvelopes(ParamValues& v, float attack, float sustain)
{
    const float amp = geometric(0.001f, 2.0f, std::pow(attack, 1.3f));
    set(v, Param::AmpAttack, amp);
    set(v, Param::EnvAttack, std::max(0.001f, 0.5f * amp));
    set(v, Param::EnvType, 0.0f);
    if (sustain < 0.7f) {
        const float decay = geometric(0.05f, 8.0f, sustain / 0.7f);
        set(v, Param::AmpType, 0.0f);
        set(v, Param::AmpDecay, decay);
        set(v, Param::EnvDecay, std::clamp(0.6f * decay, 0.02f, 4.0f));
    } else {
        set(v, Param::AmpType, 1.0f);
        set(v, Param::AmpDecay, geometric(0.25f, 2.5f, ramp(sustain, 0.7f, 1.0f)));
        set(v, Param::EnvDecay, 0.8f);
    }
}

// The modulation envelope sweeps the cutoff, or the FM index once METAL has entered FM.
void applyEvolve(ParamValues& v, float evolve, float metal)
{
    const float signedAmount = (evolve - 0.5f) * 2.0f;
    const float magnitude = ramp(std::abs(signedAmount), 0.08f, 1.0f);
    set(v, Param::EnvInt, std::copysign(0.9f * magnitude, signedAmount));
    set(v, Param::EnvTarget, metal >= kRingEnd ? 4.0f : 0.0f);
}

void applyMotion(ParamValues& v, float motion)
{
    set(v, Param::LfoWave, 1.0f);
    set(v, Param::LfoMode, 1.0f);
    if (motion < 0.35f) {
        set(v, Param::LfoTarget, 0.0f);
        set(v, Param::LfoRate, 0.72f);
        set(v, Param::LfoInt,
            0.16f * ramp(motion, 0.03f, 0.2f) * (1.0f - ramp(motion, 0.25f, 0.35f)));
    } else if (motion < 0.7f) {
        set(v, Param::LfoTarget, 2.0f);
        set(v, Param::LfoRate, 0.55f);
        set(v, Param::LfoInt, 0.65f * std::sin(kPi * ramp(motion, 0.35f, 0.7f)));
    } else {
        set(v, Param::LfoTarget, 1.0f);
        set(v, Param::LfoRate, 0.45f);
        set(v, Param::LfoInt, 0.6f * ramp(motion, 0.7f, 0.8f));
    }
    const float wide = ramp(motion, 0.4f, 1.0f);
    set(v, Param::ChorusMix, 0.85f * wide * wide * (3.0f - 2.0f * wide));
    set(v, Param::ChorusRate, 0.5f);
    set(v, Param::ChorusDepth, 0.6f);
}

constexpr float kSameKnob = 1e-4f;

float osc2Cents(const ParamValues& v)
{
    constexpr std::array<float, 4> kRangeCents = {-1200.0f, 0.0f, 1200.0f, 2400.0f};
    const auto range = static_cast<std::size_t>(std::lround(v[Param::Osc2Octave]));
    return kRangeCents[std::min<std::size_t>(range, 3)] + v[Param::Osc2Pitch];
}

// Oscillator 2's range switch and fine tune together make one pitch; move that as a whole and
// split it again, keeping the stored range switch when the fine tune still fits.
void moveOsc2(ParamValues& out, const ParamValues& stored, float delta)
{
    if (std::abs(delta) < 0.5f)
        return;
    const float total = std::clamp(osc2Cents(stored) + delta, -2400.0f, 3600.0f);
    int range = static_cast<int>(std::lround(stored[Param::Osc2Octave]));
    auto pitchFor = [total](int r) { return total - (static_cast<float>(r) - 1.0f) * 1200.0f; };
    if (std::abs(pitchFor(range)) > 1200.0f)
        range = std::clamp(static_cast<int>(std::lround(total / 1200.0f)) + 1, 0, 3);
    set(out, Param::Osc2Octave, static_cast<float>(range));
    set(out, Param::Osc2Pitch, std::clamp(pitchFor(range), -1200.0f, 1200.0f));
}

}  // namespace

Axes Axes::neutral()
{
    Axes axes;
    axes[Axis::Wave] = 0.6f;
    axes[Axis::Metal] = 0.0f;
    axes[Axis::Grit] = 0.0f;
    axes[Axis::Bright] = 0.75f;
    axes[Axis::Attack] = 0.0f;
    axes[Axis::Sustain] = 0.6f;
    axes[Axis::Evolve] = 0.5f;
    axes[Axis::Motion] = 0.0f;
    return axes;
}

const char* axisName(Axis axis)
{
    static constexpr std::array<const char*, kAxisCount> kNames = {
        "WAVE", "METAL", "GRIT", "BRIGHT", "ATTACK", "SUSTAIN", "EVOLVE", "MOTION"};
    return kNames[static_cast<std::size_t>(axis)];
}

Axes knobsOf(const ParamValues& stored)
{
    Axes axes;
    for (std::size_t a = 0; a < kAxisCount; ++a)
        axes.values[a] = stored.values[index(Param::AxisWave) + a];
    return axes;
}

Axes homeOf(const ParamValues& stored)
{
    Axes axes;
    for (std::size_t a = 0; a < kAxisCount; ++a)
        axes.values[a] = stored.values[index(Param::HomeWave) + a];
    return axes;
}

void setKnobs(ParamValues& stored, const Axes& axes)
{
    for (std::size_t a = 0; a < kAxisCount; ++a)
        stored.values[index(Param::AxisWave) + a] = unit(axes.values[a]);
}

void setHome(ParamValues& stored, const Axes& axes)
{
    for (std::size_t a = 0; a < kAxisCount; ++a)
        stored.values[index(Param::HomeWave) + a] = unit(axes.values[a]);
}

ParamValues effectiveValues(const ParamValues& stored)
{
    const Axes knobs = knobsOf(stored);
    const Axes home = homeOf(stored);
    bool atHome = true;
    for (std::size_t a = 0; a < kAxisCount; ++a)
        atHome = atHome && std::abs(knobs.values[a] - home.values[a]) < kSameKnob;
    if (atHome)
        return stored;

    const ParamValues now = fromAxes(knobs);
    const ParamValues then = fromAxes(home);
    ParamValues out = stored;
    for (std::size_t i = 0; i < index(Param::AxisWave); ++i) {
        const auto param = static_cast<Param>(i);
        if (param == Param::Osc2Octave || param == Param::Osc2Pitch || now[param] == then[param])
            continue;
        const ParamSpec& spec = paramSpec(param);
        if (spec.kind == ParamKind::Float) {
            const float moved = toNormalized(spec, stored[param]) + toNormalized(spec, now[param]) -
                                toNormalized(spec, then[param]);
            out[param] = toPlain(spec, moved);
        } else {
            out[param] = now[param];  // a switch changes to what the knob position calls for
        }
    }
    moveOsc2(out, stored, osc2Cents(now) - osc2Cents(then));
    return out;
}

ParamValues bake(const ParamValues& stored)
{
    ParamValues baked = effectiveValues(stored);
    setHome(baked, knobsOf(stored));
    return baked;
}

ParamValues fromAxes(const Axes& axes, const ParamValues& base)
{
    ParamValues v = base;
    const float metal = unit(axes[Axis::Metal]);
    applyWave(v, unit(axes[Axis::Wave]));
    applyMetal(v, metal);
    applyGrit(v, unit(axes[Axis::Grit]), metal);
    applyBright(v, unit(axes[Axis::Bright]));
    applyEnvelopes(v, unit(axes[Axis::Attack]), unit(axes[Axis::Sustain]));
    applyEvolve(v, unit(axes[Axis::Evolve]), metal);
    applyMotion(v, unit(axes[Axis::Motion]));
    return v;
}

}  // namespace polylogue::dsp

#include "dsp/Voice.h"

#include "dsp/Tuning.h"

#include <algorithm>
#include <cmath>

namespace polylogue::dsp {
namespace {

constexpr double kSmoothingSeconds = 0.008;
constexpr double kPitchSmoothingSeconds = 0.004;
constexpr double kFadeSeconds = 0.002;
constexpr float kMiddleC = 60.0f;
constexpr float kVelocityCutoffOctaves = 4.0f;
// Modulation index, in radians, at full oscillator 2 level in FM mode.
constexpr float kMaxFmIndex = 10.0f;

Waveform toWaveform(Osc1Wave wave)
{
    switch (wave) {
    case Osc1Wave::Saw:
        return Waveform::Saw;
    case Osc1Wave::Triangle:
        return Waveform::Triangle;
    case Osc1Wave::Square:
        return Waveform::Square;
    }
    return Waveform::Saw;
}

Waveform toWaveform(Osc2Wave wave)
{
    return wave == Osc2Wave::Triangle ? Waveform::Triangle : Waveform::Saw;
}

float clampShape(float shape)
{
    return std::clamp(shape, 0.0f, 1.0f);
}

}  // namespace

void Voice::prepare(double sampleRate)
{
    sampleRate_ = sampleRate;
    filter_.prepare(sampleRate);
    driveStage_.prepare(sampleRate);
    lfo_.prepare(sampleRate);
    matrix_.prepare(sampleRate);
    glide_.prepare(sampleRate);
    forEachSmoother([sampleRate](Smoother& s) { s.configure(kSmoothingSeconds, sampleRate); });
    pitchOffset_.configure(kPitchSmoothingSeconds, sampleRate);
    fadeStep_ = static_cast<float>(1.0 / (kFadeSeconds * sampleRate));
    silence();
}

void Voice::noteOn(int note, float velocity, int glideFrom)
{
    if (!isActive()) {
        start(note, velocity, glideFrom);
        return;
    }
    pendingNote_ = note;
    pendingVelocity_ = velocity;
    pendingGlideFrom_ = glideFrom;
    pendingReleased_ = false;
    hasPending_ = true;
    beginFade();
}

void Voice::changeNote(int note, float velocity)
{
    if (!isActive() || fading_) {
        noteOn(note, velocity);
        return;
    }
    note_ = note;
    velocity_ = velocity;
    updateNoteDependentValues();
    glide_.glideTo(static_cast<float>(note));
}

void Voice::noteOff()
{
    if (fading_) {
        pendingReleased_ = hasPending_;
        return;
    }
    ampEnv_.noteOff();
    modEnv_.noteOff();
}

void Voice::fadeOut()
{
    if (!isActive())
        return;
    hasPending_ = false;
    beginFade();
}

void Voice::silence()
{
    ampEnv_.reset();
    modEnv_.reset();
    filter_.reset();
    driveStage_.reset();
    fading_ = false;
    hasPending_ = false;
    fadeGain_ = 1.0f;
    ampLevel_ = 0.0f;
    snapSmoothers_ = true;
}

void Voice::start(int note, float velocity, int glideFrom)
{
    note_ = note;
    velocity_ = velocity;
    updateNoteDependentValues();

    glide_.setTime(settings_.glideSeconds);
    glide_.jumpTo(static_cast<float>(glideFrom == kNoGlide ? note : glideFrom));
    glide_.glideTo(static_cast<float>(note));

    osc1_.reset();
    osc2_.reset();
    filter_.reset();
    driveStage_.reset();
    lfo_.restart();
    ampEnv_.noteOn();
    modEnv_.noteOn();
}

void Voice::updateNoteDependentValues()
{
    noteCutoffOctaves_ = settings_.keyTrack * (static_cast<float>(note_) - kMiddleC) / 12.0f +
                         settings_.velocityToCutoff * (velocity_ - 1.0f) * kVelocityCutoffOctaves;
    velocityGain_ = 1.0f - settings_.velocityToAmp * (1.0f - velocity_);
}

void Voice::beginFade()
{
    if (!fading_) {
        fading_ = true;
        fadeGain_ = 1.0f;
    }
}

void Voice::completeFade()
{
    fading_ = false;
    fadeGain_ = 1.0f;
    if (!hasPending_) {
        ampEnv_.reset();
        modEnv_.reset();
        ampLevel_ = 0.0f;
        return;
    }
    hasPending_ = false;
    start(pendingNote_, pendingVelocity_, pendingGlideFrom_);
    if (pendingReleased_)
        noteOff();
    pendingReleased_ = false;
}

void Voice::render(float* out, int count, const SynthSettings& settings)
{
    applySettings(settings);
    for (int i = 0; i < count; ++i)
        out[i] += nextSample();
}

void Voice::applySettings(const SynthSettings& s)
{
    settings_ = s;

    const bool fm = s.syncRing == SyncRing::Fm;
    osc1_.setWaveform(fm ? Waveform::Sine : toWaveform(s.osc1Wave));
    osc2_.setWaveform(fm ? Waveform::Sine : toWaveform(s.osc2Wave));
    ampEnv_.setParameters(s.ampEnv, sampleRate_);
    modEnv_.setParameters(s.modEnv, sampleRate_);
    lfo_.setParameters(s.lfo);
    matrix_.configure(s);
    glide_.setTime(s.glideSeconds);

    osc2Semitones_ = static_cast<float>(osc2RangeSemitones(s.osc2Range)) + s.osc2Cents / 100.0f;
    updateNoteDependentValues();

    pitchOffset_.setTarget(static_cast<float>(s.octave * 12) + s.tuneCents / 100.0f +
                           s.pitchBendSemitones);
    cutoffOctaves_.setTarget(std::log2(s.cutoffHz));
    resonance_.setTarget(s.resonance);
    osc1Level_.setTarget(s.osc1Level);
    osc2Level_.setTarget(s.osc2Level);
    osc1Shape_.setTarget(s.osc1Shape);
    osc2Shape_.setTarget(s.osc2Shape);
    drive_.setTarget(s.drive);

    if (snapSmoothers_) {
        forEachSmoother([](Smoother& smoother) { smoother.snap(smoother.target()); });
        matrix_.snap();
        snapSmoothers_ = false;
    }
}

float Voice::nextSample()
{
    if (fading_ && fadeGain_ <= 0.0f)
        completeFade();

    const float ampLevel = ampEnv_.next();
    ampLevel_ = ampLevel;

    const ModOffsets mod = matrix_.evaluate({modEnv_.next(), lfo_.next()});

    const double semitones = static_cast<double>(glide_.next() + pitchOffset_.next());
    osc1_.setFrequency(
        midiToHz(semitones + static_cast<double>(mod[index(ModDestination::Osc1Pitch)])),
        sampleRate_);
    osc2_.setFrequency(midiToHz(semitones + static_cast<double>(osc2Semitones_) +
                                static_cast<double>(mod[index(ModDestination::Osc2Pitch)])),
                       sampleRate_);
    osc1_.setShape(clampShape(osc1Shape_.next() + mod[index(ModDestination::Osc1Shape)]));
    osc2_.setShape(clampShape(osc2Shape_.next() + mod[index(ModDestination::Osc2Shape)]));

    const float level1 =
        std::clamp(osc1Level_.next() + mod[index(ModDestination::Osc1Level)], 0.0f, 1.0f);
    const float level2 =
        std::clamp(osc2Level_.next() + mod[index(ModDestination::Osc2Level)], 0.0f, 1.0f);

    float mixed;
    if (settings_.syncRing == SyncRing::Fm) {
        // Two-operator FM. Oscillator 1 is the carrier and sounds at the played pitch; oscillator
        // 2 is the modulator, is not heard, and its level is the modulation index. An envelope on
        // that level turns a bright, inharmonic strike into a pure tone.
        const double modulation =
            static_cast<double>(osc2_.process()) * static_cast<double>(level2 * kMaxFmIndex);
        mixed = level1 * osc1_.process(Oscillator::kNoReset, modulation);
    } else {
        const bool sync = settings_.syncRing == SyncRing::Sync;
        const double resetIn =
            sync && osc1_.wrapsNextSample() ? osc1_.timeToWrap() : Oscillator::kNoReset;

        const float osc1 = osc1_.process();
        const float periodic = osc2_.process(resetIn);
        float osc2 = settings_.osc2Wave == Osc2Wave::Noise ? noise_.next() : periodic;
        if (settings_.syncRing == SyncRing::Ring)
            osc2 *= osc1;
        mixed = level1 * osc1 + level2 * osc2;
    }

    const float octaves =
        cutoffOctaves_.next() + noteCutoffOctaves_ + mod[index(ModDestination::Cutoff)];
    filter_.setParameters(std::exp2(octaves), resonance_.next());

    driveStage_.setAmount(drive_.next());
    float out = driveStage_.process(filter_.process(mixed) * ampLevel * velocityGain_);
    if (fading_) {
        out *= fadeGain_;
        fadeGain_ -= fadeStep_;
    }
    return out;
}

}  // namespace polylogue::dsp

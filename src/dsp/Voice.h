#pragma once

#include "dsp/Drive.h"
#include "dsp/Envelope.h"
#include "dsp/Filter.h"
#include "dsp/Glide.h"
#include "dsp/Lfo.h"
#include "dsp/ModMatrix.h"
#include "dsp/Noise.h"
#include "dsp/Oscillator.h"
#include "dsp/Settings.h"
#include "dsp/Smoother.h"

#include <cstdint>
#include <initializer_list>

namespace polylogue::dsp {

// One monophonic synth voice: two oscillators, mixer, filter, drive, amp and modulation
// envelopes, an LFO, and a modulation matrix tying them together.
class Voice {
public:
    static constexpr int kNoGlide = -1;

    explicit Voice(std::uint32_t noiseSeed = 1) : noise_(noiseSeed) {}

    void prepare(double sampleRate);

    // Starting a note on an audible voice fades the old sound out over a couple of milliseconds
    // first, so retriggering and stealing never click. With `glideFrom` set the pitch slides in
    // from that note over the glide time.
    void noteOn(int note, float velocity, int glideFrom = kNoGlide);
    // Moves a sounding voice to another note without restarting its envelopes, gliding if enabled.
    void changeNote(int note, float velocity);
    void noteOff();
    // Fades out and frees the voice.
    void fadeOut();
    // Frees the voice immediately.
    void silence();

    bool isActive() const { return fading_ || !ampEnv_.isIdle(); }
    // Amp envelope level, for choosing which voice to steal.
    float level() const { return ampLevel_; }

    // Adds `count` samples to `out`.
    void render(float* out, int count, const SynthSettings& settings);

private:
    void start(int note, float velocity, int glideFrom);
    void updateNoteDependentValues();
    void beginFade();
    void completeFade();
    void applySettings(const SynthSettings& settings);
    float nextSample();

    template<typename Fn>
    void forEachSmoother(Fn&& fn)
    {
        for (Smoother* smoother : {&pitchOffset_, &cutoffOctaves_, &resonance_, &osc1Level_,
                                   &osc2Level_, &osc1Shape_, &osc2Shape_, &drive_})
            fn(*smoother);
    }

    double sampleRate_ = 44100.0;
    SynthSettings settings_;

    Oscillator osc1_;
    Oscillator osc2_;
    Noise noise_;
    Filter filter_;
    Drive driveStage_;
    Envelope ampEnv_;
    Envelope modEnv_;
    Lfo lfo_;
    ModMatrix matrix_;
    Glide glide_;

    Smoother pitchOffset_;
    Smoother cutoffOctaves_;
    Smoother resonance_;
    Smoother osc1Level_;
    Smoother osc2Level_;
    Smoother osc1Shape_;
    Smoother osc2Shape_;
    Smoother drive_;

    int note_ = 60;
    float velocity_ = 1.0f;
    float noteCutoffOctaves_ = 0.0f;
    float velocityGain_ = 1.0f;
    float osc2Semitones_ = 0.0f;
    float ampLevel_ = 0.0f;
    bool snapSmoothers_ = true;

    bool fading_ = false;
    float fadeGain_ = 1.0f;
    float fadeStep_ = 0.0f;
    bool hasPending_ = false;
    bool pendingReleased_ = false;
    int pendingNote_ = 0;
    float pendingVelocity_ = 0.0f;
    int pendingGlideFrom_ = kNoGlide;
};

}  // namespace polylogue::dsp

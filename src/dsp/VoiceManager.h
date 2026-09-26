#pragma once

#include "dsp/Settings.h"
#include "dsp/Voice.h"

#include <array>
#include <cstdint>

namespace polylogue::dsp {

// Owns the voice pool and decides which voice plays which note.
//
// Poly: a new note takes, in order, the voice already playing that note, an idle voice, the
// quietest releasing voice, the oldest held voice.
//
// Mono: one voice, last note wins, and releasing a key returns to the previous one still held.
// Envelopes retrigger on every note unless glide is on and the notes overlap, in which case the
// pitch simply slides (single trigger, as on the monologue).
//
// The pool is fixed at kMaxVoices; `setVoiceLimit` chooses how many poly voices are usable.
class VoiceManager {
public:
    static constexpr int kMaxVoices = 16;

    struct VoiceInfo {
        int note = -1;
        bool active = false;
        bool held = false;
    };

    VoiceManager();

    void prepare(double sampleRate);
    void setVoiceLimit(int count);
    int voiceLimit() const { return limit_; }
    // Changing the key mode releases everything that is held.
    void setPlayStyle(KeyMode keyMode, float glideSeconds, GlideMode glideMode);

    void noteOn(int note, float velocity);
    void noteOff(int note);
    void setSustain(bool down);
    void allNotesOff();
    void allSoundOff();

    // Adds `count` samples of every active voice to `out`.
    void render(float* out, int count, const SynthSettings& settings);

    int activeVoiceCount() const;
    VoiceInfo info(int index) const;

private:
    struct Slot {
        Voice voice;
        int note = -1;
        std::uint64_t age = 0;
        bool held = false;
        bool sustained = false;
    };

    struct HeldKey {
        int note = 0;
        float velocity = 0.0f;
        bool sustained = false;
    };

    static constexpr int kMonoStackSize = 32;

    void polyNoteOn(int note, float velocity);
    void polyNoteOff(int note);
    void monoNoteOn(int note, float velocity);
    void monoNoteOff(int note);
    void monoRemoveAt(int position);

    int chooseSlot(int note) const;
    bool glideApplies(bool overlapping) const;
    bool anyHeld() const;
    void release(Slot& slot);

    std::array<Slot, kMaxVoices> slots_;
    std::array<HeldKey, kMonoStackSize> monoStack_{};
    int monoCount_ = 0;

    int limit_ = 8;
    KeyMode keyMode_ = KeyMode::Poly;
    float glideSeconds_ = 0.0f;
    GlideMode glideMode_ = GlideMode::Auto;
    bool sustainDown_ = false;
    int lastNote_ = -1;
    std::uint64_t counter_ = 0;
};

}  // namespace polylogue::dsp

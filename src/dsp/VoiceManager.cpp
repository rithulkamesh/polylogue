#include "dsp/VoiceManager.h"

#include <algorithm>
#include <cstddef>

namespace polylogue::dsp {

VoiceManager::VoiceManager()
{
    for (std::size_t i = 0; i < slots_.size(); ++i)
        slots_[i].voice = Voice(static_cast<std::uint32_t>(i) * 2654435761u + 1u);
}

void VoiceManager::prepare(double sampleRate)
{
    for (Slot& slot : slots_)
        slot.voice.prepare(sampleRate);
}

void VoiceManager::setVoiceLimit(int count)
{
    limit_ = std::clamp(count, 1, kMaxVoices);
    for (std::size_t i = static_cast<std::size_t>(limit_); i < slots_.size(); ++i) {
        Slot& slot = slots_[i];
        slot.held = false;
        slot.sustained = false;
        slot.voice.fadeOut();
    }
}

void VoiceManager::setPlayStyle(KeyMode keyMode, float glideSeconds, GlideMode glideMode)
{
    if (keyMode != keyMode_) {
        allNotesOff();
        keyMode_ = keyMode;
    }
    glideSeconds_ = glideSeconds;
    glideMode_ = glideMode;
}

void VoiceManager::noteOn(int note, float velocity)
{
    if (keyMode_ == KeyMode::Mono)
        monoNoteOn(note, velocity);
    else
        polyNoteOn(note, velocity);
}

void VoiceManager::noteOff(int note)
{
    if (keyMode_ == KeyMode::Mono)
        monoNoteOff(note);
    else
        polyNoteOff(note);
}

void VoiceManager::setSustain(bool down)
{
    sustainDown_ = down;
    if (down)
        return;

    if (keyMode_ == KeyMode::Mono) {
        for (int i = monoCount_ - 1; i >= 0; --i) {
            if (monoStack_[static_cast<std::size_t>(i)].sustained)
                monoRemoveAt(i);
        }
        return;
    }
    for (Slot& slot : slots_) {
        if (slot.sustained)
            release(slot);
    }
}

void VoiceManager::allNotesOff()
{
    monoCount_ = 0;
    for (Slot& slot : slots_) {
        if (slot.held)
            release(slot);
    }
}

void VoiceManager::allSoundOff()
{
    monoCount_ = 0;
    sustainDown_ = false;
    for (Slot& slot : slots_) {
        slot.held = false;
        slot.sustained = false;
        slot.voice.fadeOut();
    }
}

void VoiceManager::render(float* out, int count, const SynthSettings& settings)
{
    for (Slot& slot : slots_) {
        if (slot.voice.isActive())
            slot.voice.render(out, count, settings);
    }
}

int VoiceManager::activeVoiceCount() const
{
    return static_cast<int>(std::count_if(slots_.begin(), slots_.end(),
                                          [](const Slot& slot) { return slot.voice.isActive(); }));
}

VoiceManager::VoiceInfo VoiceManager::info(int index) const
{
    const Slot& slot = slots_[static_cast<std::size_t>(index)];
    return {slot.note, slot.voice.isActive(), slot.held};
}

void VoiceManager::polyNoteOn(int note, float velocity)
{
    const int glideFrom = glideApplies(anyHeld()) ? lastNote_ : Voice::kNoGlide;

    Slot& slot = slots_[static_cast<std::size_t>(chooseSlot(note))];
    slot.voice.noteOn(note, velocity, glideFrom);
    slot.note = note;
    slot.age = ++counter_;
    slot.held = true;
    slot.sustained = false;
    lastNote_ = note;
}

void VoiceManager::polyNoteOff(int note)
{
    for (Slot& slot : slots_) {
        if (slot.held && slot.note == note) {
            if (sustainDown_)
                slot.sustained = true;
            else
                release(slot);
        }
    }
}

void VoiceManager::monoNoteOn(int note, float velocity)
{
    const bool overlapping = monoCount_ > 0;

    for (int i = 0; i < monoCount_; ++i) {
        if (monoStack_[static_cast<std::size_t>(i)].note == note) {
            monoRemoveAt(i);
            break;
        }
    }
    if (monoCount_ == kMonoStackSize) {
        std::copy(monoStack_.begin() + 1, monoStack_.end(), monoStack_.begin());
        --monoCount_;
    }
    monoStack_[static_cast<std::size_t>(monoCount_++)] = {note, velocity, false};

    Slot& slot = slots_[0];
    if (overlapping && glideSeconds_ > 0.0f && slot.voice.isActive() && slot.held) {
        slot.voice.changeNote(note, velocity);
    } else {
        const int glideFrom = glideApplies(overlapping) ? lastNote_ : Voice::kNoGlide;
        slot.voice.noteOn(note, velocity, glideFrom);
    }
    slot.note = note;
    slot.age = ++counter_;
    slot.held = true;
    lastNote_ = note;
}

void VoiceManager::monoNoteOff(int note)
{
    for (int i = monoCount_ - 1; i >= 0; --i) {
        HeldKey& key = monoStack_[static_cast<std::size_t>(i)];
        if (key.note != note || key.sustained)
            continue;
        if (sustainDown_)
            key.sustained = true;
        else
            monoRemoveAt(i);
        return;
    }
}

// Removes a key from the stack. If it was the sounding one, falls back to the key below it, or
// releases when none remain.
void VoiceManager::monoRemoveAt(int position)
{
    const bool wasTop = position == monoCount_ - 1;
    for (int i = position; i + 1 < monoCount_; ++i)
        monoStack_[static_cast<std::size_t>(i)] = monoStack_[static_cast<std::size_t>(i + 1)];
    --monoCount_;

    if (!wasTop)
        return;

    Slot& slot = slots_[0];
    if (monoCount_ == 0) {
        release(slot);
        return;
    }
    const HeldKey& below = monoStack_[static_cast<std::size_t>(monoCount_ - 1)];
    if (glideSeconds_ > 0.0f)
        slot.voice.changeNote(below.note, below.velocity);
    else
        slot.voice.noteOn(below.note, below.velocity);
    slot.note = below.note;
    lastNote_ = below.note;
}

int VoiceManager::chooseSlot(int note) const
{
    const auto usable = static_cast<std::size_t>(limit_);

    for (std::size_t i = 0; i < usable; ++i) {
        if (slots_[i].voice.isActive() && slots_[i].note == note)
            return static_cast<int>(i);
    }
    for (std::size_t i = 0; i < usable; ++i) {
        if (!slots_[i].voice.isActive())
            return static_cast<int>(i);
    }

    std::size_t quietest = usable;
    std::size_t oldest = 0;
    for (std::size_t i = 0; i < usable; ++i) {
        const Slot& slot = slots_[i];
        if (!slot.held &&
            (quietest == usable || slot.voice.level() < slots_[quietest].voice.level()))
            quietest = i;
        if (slot.age < slots_[oldest].age)
            oldest = i;
    }
    return static_cast<int>(quietest != usable ? quietest : oldest);
}

bool VoiceManager::glideApplies(bool overlapping) const
{
    return glideSeconds_ > 0.0f && lastNote_ >= 0 && (glideMode_ == GlideMode::On || overlapping);
}

bool VoiceManager::anyHeld() const
{
    return std::any_of(slots_.begin(), slots_.end(), [](const Slot& slot) { return slot.held; });
}

void VoiceManager::release(Slot& slot)
{
    slot.held = false;
    slot.sustained = false;
    slot.voice.noteOff();
}

}  // namespace polylogue::dsp

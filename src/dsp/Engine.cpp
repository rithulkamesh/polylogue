#include "dsp/Engine.h"

#include "dsp/Saturation.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

namespace polylogue::dsp {
namespace {

// Headroom for chords, then a soft ceiling so a full pool can never overload the output.
constexpr float kVoiceHeadroom = 0.35f;
constexpr float kLimiterKnee = 0.8f;
constexpr float kLimiterCeiling = 1.0f;
constexpr double kGainSmoothingSeconds = 0.01;

}  // namespace

void Engine::prepare(double sampleRate)
{
    voices_.prepare(sampleRate);
    chorus_.prepare(sampleRate);
    chorus_.setParameters(settings_.chorusMix, settings_.chorusRate, settings_.chorusDepth);
    outputGain_.configure(kGainSmoothingSeconds, sampleRate);
    outputGain_.snap(settings_.outputGain);
    voices_.setVoiceLimit(settings_.polyphony);
    voices_.setPlayStyle(settings_.keyMode, settings_.glideSeconds, settings_.glideMode);
}

void Engine::setSettings(const SynthSettings& settings)
{
    if (settings.polyphony != settings_.polyphony)
        voices_.setVoiceLimit(settings.polyphony);
    voices_.setPlayStyle(settings.keyMode, settings.glideSeconds, settings.glideMode);
    settings_ = settings;
    chorus_.setParameters(settings.chorusMix, settings.chorusRate, settings.chorusDepth);
    outputGain_.setTarget(settings.outputGain);
}

void Engine::process(std::span<const MidiEvent> events, float* left, float* right, int count)
{
    std::size_t next = 0;
    int position = 0;
    while (position < count) {
        while (next < events.size() && events[next].offset <= position)
            handle(events[next++]);

        int end = std::min(position + kMaxSubBlock, count);
        if (next < events.size())
            end = std::min(end, events[next].offset);

        renderSubBlock(left + position, right + position, end - position);
        position = end;
    }
}

void Engine::handle(const MidiEvent& event)
{
    switch (event.type) {
    case MidiEventType::NoteOn:
        if (event.value > 0.0f)
            voices_.noteOn(event.note, event.value);
        else
            voices_.noteOff(event.note);
        break;
    case MidiEventType::NoteOff:
        voices_.noteOff(event.note);
        break;
    case MidiEventType::Sustain:
        voices_.setSustain(event.value >= 0.5f);
        break;
    case MidiEventType::PitchBend:
        bend_ = std::clamp(event.value, -1.0f, 1.0f);
        break;
    case MidiEventType::AllNotesOff:
        voices_.allNotesOff();
        break;
    case MidiEventType::AllSoundOff:
        voices_.allSoundOff();
        break;
    }
}

void Engine::renderSubBlock(float* left, float* right, int count)
{
    std::array<float, kMaxSubBlock> mono{};

    SynthSettings block = settings_;
    block.pitchBendSemitones = bend_ * static_cast<float>(settings_.bendRangeSemitones);
    voices_.render(mono.data(), count, block);

    for (int i = 0; i < count; ++i) {
        const float gain = outputGain_.next() * kVoiceHeadroom;
        float wetLeft;
        float wetRight;
        chorus_.process(mono[static_cast<std::size_t>(i)] * gain, wetLeft, wetRight);
        left[i] = softLimit(wetLeft, kLimiterKnee, kLimiterCeiling);
        right[i] = softLimit(wetRight, kLimiterKnee, kLimiterCeiling);
    }
}

}  // namespace polylogue::dsp

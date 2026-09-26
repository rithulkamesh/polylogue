#include "host/MidiTranslator.h"

namespace polylogue::host {
namespace {

constexpr int kSustainPedal = 64;
constexpr int kAllSoundOff = 120;
constexpr int kResetControllers = 121;
constexpr int kAllNotesOff = 123;
constexpr int kLastChannelMode = 127;
constexpr int kPedalDownThreshold = 64;
constexpr float kBendCentre = 8192.0f;

class Sink {
public:
    explicit Sink(std::span<dsp::MidiEvent> events) : events_(events) {}

    void add(const dsp::MidiEvent& event)
    {
        if (count_ < events_.size())
            events_[count_++] = event;
    }
    std::size_t count() const { return count_; }

private:
    std::span<dsp::MidiEvent> events_;
    std::size_t count_ = 0;
};

}  // namespace

TranslatedMidi translateMidi(const juce::MidiBuffer& midi, std::span<dsp::MidiEvent> events,
                             std::span<ControlChange> controls)
{
    using dsp::MidiEvent;

    Sink sink(events);
    TranslatedMidi result;

    for (const juce::MidiMessageMetadata metadata : midi) {
        const juce::MidiMessage message = metadata.getMessage();
        const int offset = metadata.samplePosition;
        result.sawAnyMessage = true;

        if (message.isNoteOn()) {
            sink.add(
                MidiEvent::noteOn(offset, message.getNoteNumber(), message.getFloatVelocity()));
        } else if (message.isNoteOff(true)) {
            sink.add(MidiEvent::noteOff(offset, message.getNoteNumber()));
        } else if (message.isPitchWheel()) {
            const float bend =
                (static_cast<float>(message.getPitchWheelValue()) - kBendCentre) / kBendCentre;
            sink.add(MidiEvent::pitchBend(offset, bend));
        } else if (message.isController()) {
            const int controller = message.getControllerNumber();
            const int value = message.getControllerValue();

            if (controller == kSustainPedal) {
                sink.add(MidiEvent::sustain(offset, value >= kPedalDownThreshold));
            } else if (controller == kAllSoundOff) {
                sink.add(MidiEvent::allSoundOff(offset));
            } else if (controller == kResetControllers) {
                sink.add(MidiEvent::sustain(offset, false));
                sink.add(MidiEvent::pitchBend(offset, 0.0f));
            } else if (controller >= kAllNotesOff && controller <= kLastChannelMode) {
                sink.add(MidiEvent::allNotesOff(offset));
            } else if (result.controlCount < controls.size()) {
                controls[result.controlCount++] = {controller, value};
            }
        }
    }

    result.eventCount = sink.count();
    return result;
}

}  // namespace polylogue::host

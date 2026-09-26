#include "ui/MappableControl.h"

#include "ui/Theme.h"

namespace polylogue::ui {
namespace {

enum MenuItem {
    kLearn = 1,
    kClear,
    kResetValue,
    kResetMap
};

}  // namespace

MappableControl::MappableControl(host::PolylogueProcessor& processor, dsp::Param param,
                                 juce::String label)
    : processor_(processor), param_(param), label_(std::move(label))
{
    refreshMapping();
}

int MappableControl::captionHeight() const
{
    return juce::roundToInt(static_cast<float>(getHeight()) * 0.3f);
}

void MappableControl::setMapMode(bool on)
{
    mapMode_ = on;
    setMouseCursor(on ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    repaint();
}

void MappableControl::refreshMapping()
{
    auto& mapper = processor_.midiMapper();
    const int slot = host::MidiMapper::slotOf(param_);
    const int controller = mapper.controllerFor(slot);
    const bool armed = mapper.learningSlot() == slot;
    if (controller != controller_ || armed != learning_) {
        controller_ = controller;
        learning_ = armed;
        repaint();
    }
}

void MappableControl::paintCaption(juce::Graphics& g) const
{
    auto caption = getLocalBounds().toFloat().removeFromBottom(static_cast<float>(captionHeight()));
    const float row = caption.getHeight() / 2.0f;

    g.setColour(kDim);
    g.setFont(sans(juce::jmin(12.0f, row * 0.92f)));
    g.drawText(label_, caption.removeFromTop(row), juce::Justification::centred);

    juce::String badge = juce::String::charToString(0x2013);
    if (learning_)
        badge = "LEARN";
    else if (controller_ != host::MidiMapper::kUnbound)
        badge = "CC " + juce::String(controller_);

    g.setFont(mono(juce::jmin(10.0f, row * 0.78f)));
    g.setColour(learning_ ? kInk : kDim.withAlpha(mapMode_ ? 0.9f : 0.45f));
    g.drawText(badge, caption, juce::Justification::centred);
}

void MappableControl::startLearning()
{
    processor_.midiMapper().startLearning(host::MidiMapper::slotOf(param_));
    refreshMapping();
}

void MappableControl::showContextMenu()
{
    const int slot = host::MidiMapper::slotOf(param_);

    juce::PopupMenu menu;
    menu.addItem(kLearn, "MIDI Learn");
    menu.addItem(kClear, "Clear MIDI Mapping",
                 processor_.midiMapper().controllerFor(slot) != host::MidiMapper::kUnbound);
    menu.addSeparator();
    menu.addItem(kResetValue, "Reset to Default");
    menu.addSeparator();
    menu.addItem(kResetMap, "Restore Default Controller Map");

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(this),
        [this, slot, safe = juce::Component::SafePointer<MappableControl>(this)](int choice) {
            if (safe == nullptr)
                return;
            auto& mapper = processor_.midiMapper();
            switch (choice) {
            case kLearn:
                startLearning();
                break;
            case kClear:
                mapper.bind(slot, host::MidiMapper::kUnbound);
                break;
            case kResetValue:
                if (auto* parameter = processor_.parameter(param_)) {
                    parameter->beginChangeGesture();
                    parameter->setValueNotifyingHost(parameter->getDefaultValue());
                    parameter->endChangeGesture();
                }
                break;
            case kResetMap:
                mapper.resetToDefaults();
                break;
            default:
                break;
            }
            refreshMapping();
        });
}

}  // namespace polylogue::ui

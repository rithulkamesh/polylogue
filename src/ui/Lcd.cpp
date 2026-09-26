#include "ui/Lcd.h"

#include "ui/PresetMenu.h"
#include "ui/Theme.h"

#include <cmath>

namespace polylogue::ui {
namespace {

constexpr int kPollHz = 20;
constexpr std::int64_t kReadoutMs = 1600;
constexpr std::int64_t kMessageMs = 1800;
constexpr float kCorner = 10.0f;
constexpr float kLeftColumn = 0.4f;
constexpr float kWheelStep = 0.5f;
constexpr std::int64_t kWheelIdleMs = 300;

std::int64_t now()
{
    return static_cast<std::int64_t>(juce::Time::getMillisecondCounter());
}

}  // namespace

Lcd::Lcd(host::PolylogueProcessor& processor) : processor_(processor), scope_(processor)
{
    addAndMakeVisible(scope_);
    startTimerHz(kPollHz);
}

void Lcd::showReadout(const juce::String& label, const juce::String& value,
                      const juce::String& note)
{
    readoutLabel_ = label;
    readoutValue_ = value;
    readoutNote_ = note;
    readoutUntil_ = now() + kReadoutMs;
    repaint();
}

void Lcd::showMessage(const juce::String& text)
{
    message_ = text;
    messageUntil_ = now() + kMessageMs;
    repaint();
}

// Name, category and status on the left; the visualizer takes the right.
Lcd::Layout Lcd::layout() const
{
    Layout l;
    auto area = getLocalBounds().toFloat().reduced(static_cast<float>(getHeight()) * 0.13f);
    auto left = area.removeFromLeft(area.getWidth() * kLeftColumn);
    area.removeFromLeft(area.getHeight() * 0.25f);
    l.scope = area;

    l.nameRow = left.removeFromTop(left.getHeight() * 0.42f);
    l.infoRow = left.removeFromTop(left.getHeight() * 0.3f);
    l.statusRow = left;

    auto row = l.nameRow;
    const float arrow = row.getHeight();
    l.next = row.removeFromRight(arrow);
    l.previous = row.removeFromRight(arrow);
    l.nameArea = row;
    return l;
}

void Lcd::resized()
{
    scope_.setBounds(layout().scope.toNearestInt());
}

void Lcd::timerCallback()
{
    auto& presets = processor_.presets();
    const bool learning = processor_.midiMapper().learningSlot() != host::MidiMapper::kUnbound;
    const int controller = processor_.midiMapper().lastController();
    const bool readout = now() < readoutUntil_;
    const bool message = now() < messageUntil_;

    if (presets.currentName() != shownName_ || presets.isModified() != shownModified_ ||
        learning != shownLearning_ || controller != shownController_ || readout != shownReadout_ ||
        message != shownMessage_)
        repaint();
}

void Lcd::mouseDown(const juce::MouseEvent& event)
{
    const Layout l = layout();
    const auto point = event.position;
    auto status = [safe = juce::Component::SafePointer<Lcd>(this)](const juce::String& text) {
        if (safe != nullptr)
            safe->showMessage(text);
    };

    if (l.previous.contains(point)) {
        preset_menu::step(processor_.presets(), -1);
        repaint();
    } else if (l.next.contains(point)) {
        preset_menu::step(processor_.presets(), +1);
        repaint();
    } else if (l.nameRow.contains(point))
        preset_menu::showBrowser(*this, processor_.presets(), status);
}

// A trackpad sends dozens of tiny scroll events per swipe; stepping once per event would race
// through the whole list. Accumulate the travel instead, and ignore the coasting after a swipe.
void Lcd::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    if (!layout().nameRow.contains(event.position) || wheel.isInertial)
        return;

    if (now() - lastWheelMs_ > kWheelIdleMs)
        wheelTravel_ = 0.0f;
    lastWheelMs_ = now();

    wheelTravel_ += wheel.isReversed ? -wheel.deltaY : wheel.deltaY;
    if (std::abs(wheelTravel_) >= kWheelStep) {
        preset_menu::step(processor_.presets(), wheelTravel_ > 0.0f ? -1 : +1);
        wheelTravel_ = 0.0f;
        repaint();
    }
}

void Lcd::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(juce::Colour(0xff0d0e10));
    g.fillRoundedRectangle(bounds, kCorner);
    g.setColour(kBorder);
    g.drawRoundedRectangle(bounds, kCorner, 1.0f);

    const Layout l = layout();
    paintNameRow(g, l);
    paintStatusRow(g, l);

    shownName_ = processor_.presets().currentName();
    shownModified_ = processor_.presets().isModified();
    shownLearning_ = processor_.midiMapper().learningSlot() != host::MidiMapper::kUnbound;
    shownController_ = processor_.midiMapper().lastController();
    shownReadout_ = now() < readoutUntil_;
    shownMessage_ = now() < messageUntil_;
}

void Lcd::paintNameRow(juce::Graphics& g, const Layout& l) const
{
    auto& presets = processor_.presets();
    const auto& entries = presets.entries();

    int position = 0;
    juce::String category = "SOUND";
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name == presets.currentName()) {
            position = static_cast<int>(i) + 1;
            category = entries[i].category.toUpperCase();
            break;
        }
    }

    const float height = l.nameRow.getHeight();
    const juce::Font nameFont = sans(height * 0.7f, true);
    g.setFont(nameFont);
    g.setColour(kInk);
    g.drawText(presets.currentName(), l.nameArea, juce::Justification::centredLeft, true);

    if (presets.isModified()) {
        const float width = textWidth(nameFont, presets.currentName());
        g.setColour(kAccent);
        g.fillEllipse(
            juce::Rectangle<float>(height * 0.16f, height * 0.16f)
                .withCentre({l.nameArea.getX() + width + height * 0.28f, l.nameArea.getCentreY()}));
    }

    for (const auto& [rect, glyph] : {std::pair{l.previous, 0x2039}, std::pair{l.next, 0x203a}}) {
        g.setColour(kDim);
        g.setFont(sans(height * 0.9f));
        g.drawText(juce::String::charToString(static_cast<juce::juce_wchar>(glyph)), rect,
                   juce::Justification::centred);
    }

    const juce::String index = position > 0 ? juce::String(position).paddedLeft('0', 2) + "/" +
                                                  juce::String(entries.size())
                                            : juce::String("--");
    g.setFont(mono(l.infoRow.getHeight() * 0.62f));
    g.setColour(kDim);
    g.drawText(category + "   " + index, l.infoRow, juce::Justification::centredLeft);
}

void Lcd::paintStatusRow(juce::Graphics& g, const Layout& l) const
{
    const float height = l.statusRow.getHeight();
    const juce::Font font = mono(height * 0.5f);
    g.setFont(font);

    auto row = l.statusRow;
    const auto& mapper = processor_.midiMapper();

    if (mapper.learningSlot() != host::MidiMapper::kUnbound) {
        g.setColour(kInk);
        g.drawText("MOVE A CONTROL...", row, juce::Justification::centredLeft);
    } else if (now() < messageUntil_) {
        g.setColour(kInk);
        g.drawText(message_, row, juce::Justification::centredLeft);
    } else if (now() < readoutUntil_) {
        g.setColour(kDim);
        g.drawText(readoutLabel_, row, juce::Justification::centredLeft);
        row.removeFromLeft(textWidth(font, readoutLabel_) + height * 0.5f);
        g.setColour(kInk);
        g.drawText(readoutValue_, row, juce::Justification::centredLeft);
        row.removeFromLeft(textWidth(font, readoutValue_) + height * 0.5f);
        g.setColour(kDim);
        g.drawText(readoutNote_, row, juce::Justification::centredLeft);
    } else if (mapper.lastController() != host::MidiMapper::kUnbound) {
        g.setColour(kDim);
        g.drawText("LAST CC " + juce::String(mapper.lastController()), row,
                   juce::Justification::centredLeft);
    } else {
        const bool monophonic =
            processor_.parameters().getRawParameterValue("key_mode")->load() > 0.5f;
        const int voices =
            juce::roundToInt(processor_.parameters().getRawParameterValue("polyphony")->load());
        g.setColour(kDim.withAlpha(0.7f));
        g.drawText(monophonic ? juce::String("MONO") : "POLY  " + juce::String(voices) + " VOICES",
                   row, juce::Justification::centredLeft);
    }
}

}  // namespace polylogue::ui

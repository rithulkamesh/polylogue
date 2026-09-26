#include "ui/KeyboardBar.h"

#include "ui/Theme.h"

#include <memory>

namespace polylogue::ui {
namespace {

constexpr int kLowestNote = 36;   // C2
constexpr int kHighestNote = 96;  // C7
constexpr int kWhiteKeys = 36;    // C2 to C7 inclusive
constexpr float kMouseVelocity = 0.8f;

}  // namespace

// Draws the keys in the panel's own dark language instead of JUCE's ivory.
class KeyboardBar::Keys final : public juce::MidiKeyboardComponent {
public:
    explicit Keys(juce::MidiKeyboardState& state)
        : juce::MidiKeyboardComponent(state, juce::MidiKeyboardComponent::horizontalKeyboard)
    {
        setAvailableRange(kLowestNote, kHighestNote);
        setScrollButtonsVisible(false);
        setOctaveForMiddleC(4);
        setBlackNoteLengthProportion(0.62f);
        setVelocity(kMouseVelocity, true);
        setWantsKeyboardFocus(true);
        setColour(keySeparatorLineColourId, juce::Colour(0xffb4bac0));
        setColour(textLabelColourId, juce::Colour(0xff6b7178));
    }

    void drawWhiteNote(int note, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                       bool isOver, juce::Colour line, juce::Colour text) override
    {
        const auto key = area.reduced(0.5f, 0.0f);
        g.setColour(isDown   ? juce::Colour(0xffa6afb8)
                    : isOver ? juce::Colour(0xfff6f7f9)
                             : juce::Colour(0xffe6e9ec));
        g.fillRoundedRectangle(key.withTrimmedTop(-4.0f), 3.0f);

        g.setColour(line);
        g.drawVerticalLine(juce::roundToInt(area.getRight()), area.getY(), area.getBottom());

        if (note % 12 == 0) {
            g.setColour(text);
            g.setFont(mono(juce::jmin(10.0f, area.getWidth() * 0.55f)));
            g.drawText("C" + juce::String(note / 12 - 1), area.withTrimmedBottom(3.0f),
                       juce::Justification::centredBottom);
        }
    }

    void drawBlackNote(int, juce::Graphics& g, juce::Rectangle<float> area, bool isDown,
                       bool isOver, juce::Colour) override
    {
        g.setColour(isDown   ? juce::Colour(0xff5a616a)
                    : isOver ? juce::Colour(0xff2a2d33)
                             : juce::Colour(0xff121316));
        g.fillRoundedRectangle(area, 2.0f);
        g.setColour(juce::Colour(0xff3a3d44));
        g.drawRoundedRectangle(area.reduced(0.5f), 2.0f, 1.0f);
    }
};

KeyboardBar::KeyboardBar(host::PolylogueProcessor& processor)
    : processor_(processor), keys_(std::make_unique<Keys>(state_))
{
    addAndMakeVisible(*keys_);
    state_.addListener(this);
}

KeyboardBar::~KeyboardBar()
{
    state_.removeListener(this);
}

void KeyboardBar::resized()
{
    keys_->setBounds(getLocalBounds());
    keys_->setKeyWidth(static_cast<float>(getWidth()) / static_cast<float>(kWhiteKeys));
}

void KeyboardBar::handleNoteOn(juce::MidiKeyboardState*, int, int note, float velocity)
{
    if (!followingMidi_)
        processor_.queueKeyboardNote(note, velocity);
}

void KeyboardBar::handleNoteOff(juce::MidiKeyboardState*, int, int note, float)
{
    if (!followingMidi_)
        processor_.queueKeyboardNote(note, 0.0f);
}

void KeyboardBar::followMidi()
{
    host::PolylogueProcessor::KeyEvent event;
    followingMidi_ = true;
    while (processor_.popMonitoredNote(event)) {
        if (event.velocity > 0.0f)
            state_.noteOn(1, event.note, event.velocity);
        else
            state_.noteOff(1, event.note, 0.0f);
    }
    followingMidi_ = false;
}

}  // namespace polylogue::ui

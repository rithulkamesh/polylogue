#include "ui/Knob.h"

#include "ui/Theme.h"

#include <cmath>
#include <memory>

namespace polylogue::ui {
namespace {

constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float kEndAngle = juce::MathConstants<float>::pi * 2.75f;
constexpr int kDragPixelsForFullTravel = 260;

}  // namespace

// The slider itself, so right-clicks can open the mapping menu and map mode can intercept clicks.
class Knob::Dial final : public juce::Slider, private juce::Timer {
public:
    explicit Dial(Knob& owner)
        : juce::Slider(juce::Slider::RotaryVerticalDrag, NoTextBox), owner_(owner)
    {
        setRotaryParameters(kStartAngle, kEndAngle, true);
        setMouseDragSensitivity(kDragPixelsForFullTravel);
        setPopupMenuEnabled(false);
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu()) {
            owner_.showContextMenu();
        } else if (owner_.mapMode()) {
            owner_.startLearning();
        } else {
            juce::Slider::mouseDown(event);
        }
    }

    void mouseDrag(const juce::MouseEvent& event) override
    {
        if (!event.mods.isPopupMenu() && !owner_.mapMode())
            juce::Slider::mouseDrag(event);
    }

    void mouseUp(const juce::MouseEvent& event) override
    {
        if (event.mods.isPopupMenu() || owner_.mapMode())
            return;
        juce::Slider::mouseUp(event);
        if (!event.mouseWasDraggedSinceMouseDown())
            owner_.beginTextEntry();
    }

    // A double-click would fight the click that opens the text box; the context menu resets.
    void mouseDoubleClick(const juce::MouseEvent&) override {}

    // Where the ring is drawn. It follows the value at once while a hand is on the knob and glides
    // there when a preset, a controller or the host moves it.
    float shownPosition() const { return shown_; }

    void valueChanged() override
    {
        if (!started_ || isMouseButtonDown()) {
            shown_ = target();
            started_ = true;
            repaint();
        } else {
            startTimerHz(kEaseHz);
        }
    }

    void paint(juce::Graphics& g) override
    {
        if (!started_) {
            shown_ = target();
            started_ = true;
        }
        const auto rotary = getRotaryParameters();
        getLookAndFeel().drawRotarySlider(g, 0, 0, getWidth(), getHeight(), shown_,
                                          rotary.startAngleRadians, rotary.endAngleRadians, *this);
    }

private:
    static constexpr int kEaseHz = 60;
    static constexpr float kEaseStep = 0.3f;
    static constexpr float kEaseDone = 0.002f;

    float target() { return static_cast<float>(valueToProportionOfLength(getValue())); }

    void timerCallback() override
    {
        const float difference = target() - shown_;
        if (std::abs(difference) < kEaseDone) {
            shown_ = target();
            stopTimer();
        } else {
            shown_ += difference * kEaseStep;
        }
        repaint();
        owner_.repaint();
    }

    Knob& owner_;
    float shown_ = 0.0f;
    bool started_ = false;
};

Knob::Knob(host::PolylogueProcessor& processor, dsp::Param param, juce::String label)
    : MappableControl(processor, param, std::move(label)), dial_(std::make_unique<Dial>(*this))
{
    addAndMakeVisible(*dial_);

    entry_.setJustification(juce::Justification::centred);
    entry_.setFont(mono(13.0f));
    entry_.setSelectAllWhenFocused(true);
    entry_.setMultiLine(false);
    entry_.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0d0e10));
    entry_.setColour(juce::TextEditor::outlineColourId, kAccent);
    entry_.setColour(juce::TextEditor::focusedOutlineColourId, kInk);
    entry_.setColour(juce::TextEditor::textColourId, kInk);
    entry_.setColour(juce::TextEditor::highlightColourId, kAccent.withAlpha(0.35f));
    entry_.onReturnKey = [this] {
        applyTypedValue(entry_.getText());
        endTextEntry();
    };
    entry_.onEscapeKey = [this] { endTextEntry(); };
    entry_.onFocusLost = [this] { endTextEntry(); };
    addChildComponent(entry_);

    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters(), dsp::paramSpec(param).id, *dial_);
    dial_->setDoubleClickReturnValue(false, 0.0);
}

Knob::~Knob() = default;

void Knob::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(captionHeight());
    const int side = juce::jmin(area.getWidth(), area.getHeight());
    dial_->setBounds(area.withSizeKeepingCentre(side, side));

    const int height = juce::jmax(20, side / 3);
    entry_.setBounds(dial_->getBounds().getCentre().x - (getWidth() - 4) / 2,
                     dial_->getBounds().getCentreY() - height / 2, getWidth() - 4, height);
    entry_.setFont(mono(static_cast<float>(height) * 0.6f));
}

void Knob::beginTextEntry()
{
    const dsp::ParamSpec& spec = dsp::paramSpec(param());
    const float plain = processor_.parameters().getRawParameterValue(spec.id)->load();
    entry_.setText(dsp::formatValue(spec, plain), juce::dontSendNotification);
    entry_.setVisible(true);
    entry_.toFront(true);
    entry_.grabKeyboardFocus();
    entry_.selectAll();
}

void Knob::endTextEntry()
{
    entry_.setVisible(false);
    if (entry_.hasKeyboardFocus(true))
        giveAwayKeyboardFocus();
}

bool Knob::isEditingText() const
{
    return entry_.isVisible();
}

bool Knob::applyTypedValue(const juce::String& text)
{
    const dsp::ParamSpec& spec = dsp::paramSpec(param());
    const auto plain = dsp::parseValue(spec, text.trim().toStdString());
    if (!plain)
        return false;

    auto* parameter = processor_.parameter(param());
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(parameter->convertTo0to1(*plain));
    parameter->endChangeGesture();
    return true;
}

void Knob::setShowValue(bool show)
{
    showValue_ = show;
    repaint();
}

void Knob::paintOverChildren(juce::Graphics& g)
{
    const auto bounds = dial_->getBounds().toFloat().reduced(2.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
    const float shown = dial_->shownPosition();

    if (showValue_) {
        g.setFont(mono(radius * 0.36f));
        g.setColour(kInk.withAlpha(0.85f));
        g.drawText(juce::String(juce::roundToInt(shown * 100.0f)), dial_->getBounds(),
                   juce::Justification::centred);
    }
}

void Knob::paint(juce::Graphics& g)
{
    paintCaption(g);
    if (mapMode() || learning()) {
        g.setColour(learning() ? kInk : kAccent.withAlpha(0.5f));
        g.drawEllipse(dial_->getBounds().toFloat().reduced(1.0f), 1.0f);
    }
}

}  // namespace polylogue::ui

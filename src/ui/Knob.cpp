#include "ui/Knob.h"

#include "ui/Theme.h"

namespace polylogue::ui {
namespace {

constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float kEndAngle = juce::MathConstants<float>::pi * 2.75f;
constexpr int kDragPixelsForFullTravel = 260;

}  // namespace

// The slider itself, so right-clicks can open the mapping menu and map mode can intercept clicks.
class Knob::Dial final : public juce::Slider {
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
        if (!event.mods.isPopupMenu() && !owner_.mapMode())
            juce::Slider::mouseUp(event);
    }

    void mouseDoubleClick(const juce::MouseEvent& event) override
    {
        if (!owner_.mapMode())
            juce::Slider::mouseDoubleClick(event);
    }

private:
    Knob& owner_;
};

Knob::Knob(host::PolylogueProcessor& processor, dsp::Param param, juce::String label)
    : MappableControl(processor, param, std::move(label)), dial_(std::make_unique<Dial>(*this))
{
    addAndMakeVisible(*dial_);
    attachment_ = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters(), dsp::paramSpec(param).id, *dial_);
}

Knob::~Knob() = default;

void Knob::resized()
{
    auto area = getLocalBounds();
    area.removeFromBottom(captionHeight());
    const int side = juce::jmin(area.getWidth(), area.getHeight());
    dial_->setBounds(area.withSizeKeepingCentre(side, side));
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

#include "ui/LookAndFeel.h"

#include "ui/Theme.h"

#include <cmath>

namespace polylogue::ui {
namespace {

constexpr float kSmallKnobRadius = 17.0f;

}  // namespace

LookAndFeel::LookAndFeel()
{
    setDefaultSansSerifTypeface(sans(12.0f).getTypefacePtr());

    setColour(juce::Slider::rotarySliderFillColourId, kAccent);
    setColour(juce::Label::textColourId, kInk);

    setColour(juce::PopupMenu::backgroundColourId, kPanelRaised);
    setColour(juce::PopupMenu::textColourId, kInk);
    setColour(juce::PopupMenu::headerTextColourId, kDim);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff2e3138));
    setColour(juce::PopupMenu::highlightedTextColourId, kInk);

    setColour(juce::TextButton::textColourOffId, kDim);
    setColour(juce::TextButton::textColourOnId, kBackground);
    setColour(juce::TextButton::buttonOnColourId, kInk);

    setColour(juce::AlertWindow::backgroundColourId, kPanel);
    setColour(juce::AlertWindow::textColourId, kInk);
    setColour(juce::AlertWindow::outlineColourId, kBorder);
    setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff17181c));
    setColour(juce::TextEditor::textColourId, kInk);
    setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff363b42));
    setColour(juce::TextEditor::focusedOutlineColourId, kDim);
    setColour(juce::CaretComponent::caretColourId, kInk);
}

// Dark body with a subtle rim, a pointer that turns with the value, a 1 px track, a 2 px arc from
// the origin to the value, and a dot riding the end of the arc.
void LookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                   float position, float startAngle, float endAngle,
                                   juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(2.0f);
    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + position * (endAngle - startAngle);
    const float origin = slider.getMinimum() < 0.0 ? 0.5f * (startAngle + endAngle) : startAngle;
    const auto accent = slider.findColour(juce::Slider::rotarySliderFillColourId);
    const bool small = radius < kSmallKnobRadius;

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour(kTrack);
    g.strokePath(track, juce::PathStrokeType(1.0f, juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    if (std::abs(angle - origin) > 0.01f) {
        juce::Path active;
        active.addCentredArc(centre.x, centre.y, radius, radius, 0.0f, juce::jmin(origin, angle),
                             juce::jmax(origin, angle), true);
        g.setColour(accent);
        g.strokePath(active, juce::PathStrokeType(small ? 1.5f : 2.0f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    if (!small) {
        const float bodyRadius = radius - 4.0f;
        const auto body =
            juce::Rectangle<float>(bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre(centre);
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xff202226), centre.x,
                                               centre.y - bodyRadius, juce::Colour(0xff141519),
                                               centre.x, centre.y + bodyRadius, false));
        g.fillEllipse(body);
        g.setColour(juce::Colour(0xff33363c));
        g.drawEllipse(body, 1.0f);

        const auto inner = centre.getPointOnCircumference(bodyRadius - 7.0f, angle);
        const auto outer = centre.getPointOnCircumference(bodyRadius - 1.5f, angle);
        g.setColour(kInk.withAlpha(0.9f));
        g.drawLine({inner, outer}, 1.8f);
    }

    const auto tip = centre.getPointOnCircumference(radius, angle);
    g.setColour(kBackground);
    g.fillEllipse(juce::Rectangle<float>(7.0f, 7.0f).withCentre(tip));
    g.setColour(kInk);
    g.fillEllipse(juce::Rectangle<float>(4.5f, 4.5f).withCentre(tip));
}

void LookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                       bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const float corner = bounds.getHeight() / 2.0f;

    if (button.getToggleState()) {
        g.setColour(button.findColour(juce::TextButton::buttonOnColourId));
        g.fillRoundedRectangle(bounds, corner);
        return;
    }
    g.setColour(down ? kPanelRaised : juce::Colour(0xff17181c));
    g.fillRoundedRectangle(bounds, corner);
    g.setColour(highlighted ? kDim : juce::Colour(0xff363b42));
    g.drawRoundedRectangle(bounds, corner, 1.0f);
}

juce::Font LookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return sans(juce::jmin(13.0f, static_cast<float>(buttonHeight) * 0.5f), true);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return sans(14.0f);
}

void LookAndFeel::drawCornerResizer(juce::Graphics& g, int width, int height, bool mouseOver,
                                    bool dragging)
{
    const float w = static_cast<float>(width);
    const float h = static_cast<float>(height);
    g.setColour((mouseOver || dragging ? kDim : kTrack));
    for (float fraction : {0.4f, 0.7f, 1.0f})
        g.drawLine(w * fraction, h, w, h * fraction, 1.0f);
}

}  // namespace polylogue::ui

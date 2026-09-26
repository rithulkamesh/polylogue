#include "ui/ChipGroup.h"

#include "ui/Theme.h"

namespace polylogue::ui {
namespace {

constexpr float kGapRatio = 0.18f;
constexpr float kMaxColumnChip = 20.0f;
constexpr float kMaxRowChip = 24.0f;

}  // namespace

int ChipGroup::positions(const dsp::ParamSpec& spec)
{
    return static_cast<int>(spec.max - spec.min) + 1;
}

ChipGroup::ChipGroup(host::PolylogueProcessor& processor, dsp::Param param, juce::String label,
                     Layout layout)
    : MappableControl(processor, param, std::move(label)),
      layout_(layout),
      attachment_(*processor.parameter(param), [this, spec = dsp::paramSpec(param)](float plain) {
          selected_ = juce::roundToInt(plain - spec.min);
          repaint();
      })
{
    attachment_.sendInitialUpdate();
}

juce::String ChipGroup::chipText(int position) const
{
    const dsp::ParamSpec& spec = dsp::paramSpec(param());
    if (spec.kind == dsp::ParamKind::Choice)
        return juce::String(spec.choices[static_cast<std::size_t>(position)]).toUpperCase();

    const int value = static_cast<int>(spec.min) + position;
    return (value > 0 ? "+" : "") + juce::String(value);
}

juce::Rectangle<float> ChipGroup::chipBounds(int position) const
{
    auto area = getLocalBounds().toFloat();
    area.removeFromBottom(static_cast<float>(captionHeight()));
    const int count = positions(dsp::paramSpec(param()));
    const float scale = static_cast<float>(getHeight()) / 108.0f;

    if (layout_ == Layout::Column) {
        const float slots = static_cast<float>(count) + kGapRatio * static_cast<float>(count - 1);
        const float chip = juce::jmin(area.getHeight() / slots, kMaxColumnChip * scale);
        const float gap = chip * kGapRatio;
        return {area.getX() + 4.0f, area.getY() + static_cast<float>(position) * (chip + gap),
                area.getWidth() - 8.0f, chip};
    }

    const float slots = static_cast<float>(count) + kGapRatio * static_cast<float>(count - 1);
    const float width = area.getWidth() / slots;
    const float height = juce::jmin(area.getHeight(), kMaxRowChip * scale * 1.4f);
    const auto row = area.withSizeKeepingCentre(area.getWidth(), height);
    return {row.getX() + static_cast<float>(position) * width * (1.0f + kGapRatio), row.getY(),
            width, height};
}

int ChipGroup::positionAt(juce::Point<float> point) const
{
    for (int i = 0; i < positions(dsp::paramSpec(param())); ++i) {
        if (chipBounds(i).expanded(2.0f).contains(point))
            return i;
    }
    return -1;
}

void ChipGroup::paint(juce::Graphics& g)
{
    paintCaption(g);

    for (int i = 0; i < positions(dsp::paramSpec(param())); ++i) {
        const auto chip = chipBounds(i);
        const bool on = i == selected_;
        const float corner = chip.getHeight() / 2.0f;

        if (on) {
            g.setColour(kInk);
            g.fillRoundedRectangle(chip, corner);
        } else {
            g.setColour(juce::Colour(0xff17181c));
            g.fillRoundedRectangle(chip, corner);
            g.setColour(juce::Colour(0xff363b42));
            g.drawRoundedRectangle(chip.reduced(0.5f), corner, 1.0f);
        }

        g.setColour(on ? kBackground : kDim);
        g.setFont(sans(juce::jmin(11.0f, chip.getHeight() * 0.6f), on));
        g.drawText(chipText(i), chip.reduced(2.0f, 0.0f), juce::Justification::centred);
    }

    if (mapMode() || learning()) {
        g.setColour(learning() ? kInk : kAccent.withAlpha(0.5f));
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 6.0f, 1.0f);
    }
}

void ChipGroup::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu()) {
        showContextMenu();
        return;
    }
    if (mapMode()) {
        startLearning();
        return;
    }
    const int position = positionAt(event.position);
    if (position >= 0)
        attachment_.setValueAsCompleteGesture(dsp::paramSpec(param()).min +
                                              static_cast<float>(position));
}

}  // namespace polylogue::ui

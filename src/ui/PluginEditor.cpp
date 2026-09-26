#include "ui/PluginEditor.h"

#include "ui/ChipGroup.h"
#include "ui/Knob.h"
#include "ui/PanelLayout.h"
#include "ui/PresetMenu.h"
#include "ui/Theme.h"

#include <algorithm>
#include <cmath>

namespace polylogue::ui {
namespace {

constexpr int kPollHz = 30;
constexpr float kLedDecayPerTick = 0.14f;
constexpr float kPositionEpsilon = 1e-4f;
// More controls than this changing at once means a preset was loaded, not a hand on the panel.
constexpr int kMaxChangesForReadout = 2;

// Geometry in base units (the window is 1200 x 620 at scale 1).
constexpr float kMargin = 24.0f;
constexpr float kHeaderHeight = 46.0f;
constexpr float kLcdHeight = 120.0f;
constexpr float kSectionGap = 20.0f;
constexpr float kTitleHeight = 16.0f;
constexpr float kPanelRowHeight = 108.0f;
constexpr float kStripHeight = 70.0f;
constexpr float kRowGap = 8.0f;
constexpr float kKnobWidth = 70.0f;
constexpr float kColumnChipWidth = 58.0f;
constexpr float kRowChipWidth = 40.0f;
constexpr float kKeyboardHeight = 80.0f;

float cellWidth(const Cell& cell)
{
    switch (cell.kind) {
    case CellKind::Knob:
        return kKnobWidth;
    case CellKind::ChipsColumn:
        return kColumnChipWidth *
               static_cast<float>(ChipGroup::columns(dsp::paramSpec(cell.param)));
    case CellKind::ChipsRow:
        return kRowChipWidth * static_cast<float>(ChipGroup::positions(dsp::paramSpec(cell.param)));
    }
    return kKnobWidth;
}

juce::String choiceText(const host::PolylogueProcessor& processor, dsp::Param param)
{
    const dsp::ParamSpec& spec = dsp::paramSpec(param);
    const float value = processor.parameters().getRawParameterValue(spec.id)->load();
    return juce::String(dsp::formatValue(spec, value)).toUpperCase();
}

}  // namespace

PluginEditor::PluginEditor(host::PolylogueProcessor& processor)
    : juce::AudioProcessorEditor(processor),
      processor_(processor),
      lcd_(processor),
      keyboard_(processor)
{
    setLookAndFeel(&lookAndFeel_.get());
    setOpaque(true);

    addAndMakeVisible(lcd_);
    addAndMakeVisible(keyboard_);

    for (const Section& section : panelSections()) {
        for (const Cell& cell : section.cells) {
            if (cell.kind == CellKind::Knob)
                controls_.push_back(std::make_unique<Knob>(processor, cell.param, cell.label));
            else
                controls_.push_back(std::make_unique<ChipGroup>(processor, cell.param, cell.label,
                                                                cell.kind == CellKind::ChipsColumn
                                                                    ? ChipGroup::Layout::Column
                                                                    : ChipGroup::Layout::Row));
            addAndMakeVisible(*controls_.back());
        }
    }

    for (auto* button : {&saveButton_, &mapButton_})
        addAndMakeVisible(*button);
    saveButton_.onClick = [this] {
        dialog_ =
            preset_menu::promptSave(*this, processor_.presets(),
                                    [this](const juce::String& text) { lcd_.showMessage(text); });
    };
    mapButton_.setClickingTogglesState(true);
    mapButton_.onClick = [this] { setMapMode(mapButton_.getToggleState()); };

    setResizable(true, true);
    setResizeLimits(kBaseWidth * 17 / 20, kBaseHeight * 17 / 20, kBaseWidth * 2, kBaseHeight * 2);
    getConstrainer()->setFixedAspectRatio(static_cast<double>(kBaseWidth) / kBaseHeight);
    setSize(kBaseWidth, kBaseHeight);

    lastMapVersion_ = processor_.midiMapper().version();
    startTimerHz(kPollHz);
}

PluginEditor::~PluginEditor()
{
    stopTimer();
    juce::PopupMenu::dismissAllActiveMenus();
    if (dialog_ != nullptr)
        dialog_->exitModalState(0);
    setLookAndFeel(nullptr);
}

void PluginEditor::resized()
{
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kBaseWidth);
    auto scaled = [scale](juce::Rectangle<float> r) {
        return juce::Rectangle<float>(r.getX() * scale, r.getY() * scale, r.getWidth() * scale,
                                      r.getHeight() * scale)
            .toNearestInt();
    };

    const float contentWidth = static_cast<float>(kBaseWidth) - 2.0f * kMargin;
    float y = kHeaderHeight + 10.0f;

    lcd_.setBounds(scaled({kMargin, y, contentWidth, kLcdHeight}));
    y += kLcdHeight + 12.0f;

    auto sectionWidth = [](const Section& section) {
        float width = 0.0f;
        for (const Cell& cell : section.cells)
            width += cellWidth(cell);
        return width;
    };

    marks_.clear();
    std::size_t next = 0;
    for (int row = 0; row <= 2; ++row) {
        const float cellsHeight = row == 2 ? kStripHeight : kPanelRowHeight;

        // Each row is centred as a whole, so a short one sits in the middle of the panel.
        float rowWidth = -kSectionGap;
        for (const Section& section : panelSections()) {
            if (section.row == row)
                rowWidth += sectionWidth(section) + kSectionGap;
        }

        float x = kMargin + (contentWidth - rowWidth) / 2.0f;
        for (const Section& section : panelSections()) {
            if (section.row != row)
                continue;

            const float width = sectionWidth(section);
            marks_.push_back({section.title, {x, y, width, kTitleHeight}});

            float cellX = x;
            for (const Cell& cell : section.cells) {
                const float w = cellWidth(cell);
                controls_[next++]->setBounds(scaled({cellX, y + kTitleHeight, w, cellsHeight}));
                cellX += w;
            }
            x += width + kSectionGap;
        }
        y += kTitleHeight + cellsHeight + kRowGap;
    }

    keyboard_.setBounds(scaled({kMargin, y + 2.0f, contentWidth, kKeyboardHeight}));

    const float right = static_cast<float>(kBaseWidth) - kMargin - 110.0f;
    mapButton_.setBounds(scaled({right - 64.0f, 11.0f, 64.0f, 24.0f}));
    saveButton_.setBounds(scaled({right - 64.0f - 8.0f - 64.0f, 11.0f, 64.0f, 24.0f}));
}

void PluginEditor::paint(juce::Graphics& g)
{
    const float scale = static_cast<float>(getWidth()) / static_cast<float>(kBaseWidth);
    g.fillAll(kBackground);

    // Wordmark, letter-spaced.
    const juce::String name = "POLYLOGUE";
    const float size = 15.0f * scale;
    const float tracking = 6.0f * scale;
    g.setFont(sans(size));
    g.setColour(kInk);
    float x = kMargin * scale;
    for (auto character : name) {
        const juce::String glyph = juce::String::charToString(character);
        const float advance = textWidth(sans(size), glyph);
        g.drawText(glyph, juce::Rectangle<float>(x, 16.0f * scale, advance + 2.0f, size * 1.4f),
                   juce::Justification::centredLeft);
        x += advance + tracking;
    }

    // MIDI activity light.
    const float ledSize = 7.0f * scale;
    const auto led =
        juce::Rectangle<float>(ledSize, ledSize)
            .withCentre({static_cast<float>(getWidth()) - 30.0f * scale, 24.0f * scale});
    g.setColour(kTrack);
    g.fillEllipse(led);
    g.setColour(kInk.withAlpha(ledLevel_));
    g.fillEllipse(led);
    g.setColour(kDim);
    g.setFont(mono(10.0f * scale));
    g.drawText("MIDI",
               juce::Rectangle<float>(static_cast<float>(getWidth()) - 80.0f * scale, 16.0f * scale,
                                      36.0f * scale, 16.0f * scale),
               juce::Justification::centredRight);

    g.setColour(kBorder);
    g.drawHorizontalLine(juce::roundToInt(kHeaderHeight * scale), 0.0f,
                         static_cast<float>(getWidth()));

    // Section titles, each with a hairline running to the end of its group.
    g.setFont(mono(10.0f * scale));
    for (const SectionMark& mark : marks_) {
        if (mark.title.isEmpty())
            continue;
        const auto area = mark.area * scale;
        const float titleWidth = textWidth(mono(10.0f * scale), mark.title);
        g.setColour(kDim);
        g.drawText(mark.title, area.withWidth(titleWidth + 4.0f), juce::Justification::centredLeft);
        g.setColour(kBorder);
        g.drawHorizontalLine(juce::roundToInt(area.getCentreY()),
                             area.getX() + titleWidth + 12.0f * scale, area.getRight());
    }
}

void PluginEditor::timerCallback()
{
    pollParameters();
    pollMidi();
    keyboard_.followMidi();
}

void PluginEditor::describe(dsp::Param param)
{
    const dsp::ParamSpec& spec = dsp::paramSpec(param);
    const float plain = processor_.parameters().getRawParameterValue(spec.id)->load();

    juce::String note;
    const juce::String arrow = juce::String(juce::CharPointer_UTF8("\xe2\x86\x92 "));
    if (param == dsp::Param::EnvInt)
        note = arrow + choiceText(processor_, dsp::Param::EnvTarget);
    else if (param == dsp::Param::LfoInt)
        note = arrow + choiceText(processor_, dsp::Param::LfoTarget);

    const juce::String label = controlLabel(param);
    lcd_.showReadout(label.isEmpty() ? juce::String(spec.name).toUpperCase() : label,
                     dsp::formatValue(spec, plain), note);
}

// Anything that moves a control (mouse, automation, a controller) shows up on the display.
void PluginEditor::pollParameters()
{
    std::array<dsp::Param, dsp::kParamCount> moved{};
    std::size_t count = 0;
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        const auto param = static_cast<dsp::Param>(i);
        const float position = processor_.parameter(param)->getValue();
        if (std::abs(position - lastPositions_[i]) > kPositionEpsilon)
            moved[count++] = param;
        lastPositions_[i] = position;
    }

    if (!firstPoll_ && count > 0 && count <= kMaxChangesForReadout)
        describe(moved[count - 1]);
    firstPoll_ = false;
}

void PluginEditor::pollMidi()
{
    auto& mapper = processor_.midiMapper();

    const std::uint32_t activity = processor_.midiActivity();
    const std::uint32_t mapperActivity = mapper.activity();
    if (activity != lastMidiActivity_ || mapperActivity != lastMapperActivity_) {
        lastMidiActivity_ = activity;
        lastMapperActivity_ = mapperActivity;
        ledLevel_ = 1.0f;
        repaint(getWidth() - 100, 0, 100, 40);
    } else if (ledLevel_ > 0.0f) {
        ledLevel_ = std::max(0.0f, ledLevel_ - kLedDecayPerTick);
        repaint(getWidth() - 100, 0, 100, 40);
    }

    if (mapper.version() != lastMapVersion_) {
        lastMapVersion_ = mapper.version();
        for (auto& control : controls_)
            control->refreshMapping();
        // A completed learn ends map mode.
        if (mapMode_ && mapper.learningSlot() == host::MidiMapper::kUnbound) {
            setMapMode(false);
            lcd_.showMessage("MAPPED");
        }
    }
}

void PluginEditor::setMapMode(bool on)
{
    mapMode_ = on;
    mapButton_.setToggleState(on, juce::dontSendNotification);
    for (auto& control : controls_)
        control->setMapMode(on);
    if (!on)
        processor_.midiMapper().stopLearning();
    else
        lcd_.showMessage("CLICK A CONTROL, THEN MOVE A KNOB");
}

}  // namespace polylogue::ui

#include "host/HostTestSupport.h"
#include "presets/FactoryPresets.h"
#include "ui/Knob.h"
#include "ui/PanelLayout.h"
#include "ui/PluginEditor.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <memory>
#include <vector>

using namespace polylogue;
using namespace polylogue::test;

namespace {

struct EditorRig {
    TempDirectory storage;
    host::PolylogueProcessor processor;

    EditorRig() : processor(makeOptions())
    {
        processor.prepareToPlay(48000.0, 512);
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, 52, 0.8f), 0);
        for (int i = 0; i < 8; ++i) {
            buffer.clear();
            processor.processBlock(buffer, midi);
            midi.clear();
        }
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor()
    {
        return std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
    }

private:
    host::PolylogueProcessor::Options makeOptions()
    {
        auto options = optionsFor(storage.directory);
        options.editor = [](host::PolylogueProcessor& p) -> juce::AudioProcessorEditor* {
            return new ui::PluginEditor(p);
        };
        return options;
    }
};

// Fraction of pixels that are not the background colour.
double inkCoverage(const juce::Image& image)
{
    long lit = 0;
    for (int y = 0; y < image.getHeight(); ++y) {
        for (int x = 0; x < image.getWidth(); ++x) {
            if (image.getPixelAt(x, y).getBrightness() > 0.2f)
                ++lit;
        }
    }
    return static_cast<double>(lit) / (image.getWidth() * image.getHeight());
}

}  // namespace

TEST_CASE("the editor exists and reports its size")
{
    EditorRig rig;
    CHECK(rig.processor.hasEditor());
    auto editor = rig.editor();
    REQUIRE(editor != nullptr);
    CHECK(editor->getWidth() == 1200);
    CHECK(editor->getHeight() == 636);
}

TEST_CASE("the editor draws something at every size it allows")
{
    EditorRig rig;
    auto editor = rig.editor();
    for (auto [width, height] :
         {std::pair{1020, 541}, std::pair{1200, 636}, std::pair{2400, 1272}}) {
        editor->setSize(width, height);
        const juce::Image image =
            editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        REQUIRE(image.isValid());
        const double coverage = inkCoverage(image);
        INFO(width << "x" << height);
        // Thin lines cover proportionally less of a larger window.
        CHECK(coverage > 0.004);
        CHECK(coverage < 0.4);
    }
}

TEST_CASE("the editor keeps its aspect ratio")
{
    EditorRig rig;
    auto editor = rig.editor();
    REQUIRE(editor->getConstrainer() != nullptr);
    int width = 900;
    int height = 200;
    editor->getConstrainer()->checkComponentBounds(editor.get());
    juce::Rectangle<int> bounds(0, 0, width, height);
    editor->getConstrainer()->setBoundsForComponent(editor.get(), bounds, false, false, false,
                                                    false);
    CHECK(std::abs(static_cast<double>(editor->getWidth()) / editor->getHeight() - 1200.0 / 636.0) <
          0.05);
}

TEST_CASE("every preset can be shown")
{
    EditorRig rig;
    auto editor = rig.editor();
    for (const auto& entry : rig.processor.presets().entries()) {
        INFO(entry.name);
        rig.processor.presets().load(entry);
        const juce::Image image =
            editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f);
        CHECK(inkCoverage(image) > 0.01);
    }
}

TEST_CASE("the display, spectrum and map mode all paint")
{
    EditorRig rig;
    auto editor = rig.editor();
    auto& panel = static_cast<ui::PluginEditor&>(*editor);

    panel.display().visualizer().showSpectrum(true);
    CHECK(inkCoverage(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f)) >
          0.01);
    panel.display().visualizer().showSpectrum(false);

    panel.display().showReadout("CUTOFF", "1.20 kHz", "");
    panel.display().showMessage("SAVED");
    rig.processor.midiMapper().startLearning(2);
    CHECK(inkCoverage(editor->createComponentSnapshot(editor->getLocalBounds(), true, 1.0f)) >
          0.01);
}

TEST_CASE("an editor can be created and destroyed repeatedly")
{
    EditorRig rig;
    for (int i = 0; i < 5; ++i) {
        auto editor = rig.editor();
        rig.processor.presets().loadFactory(i);
    }
    SUCCEED();
}

TEST_CASE("typing a value into a knob sets its parameter")
{
    EditorRig rig;
    ui::Knob cutoff(rig.processor, dsp::Param::Cutoff, "CUTOFF");
    cutoff.setSize(72, 108);

    auto value = [&](dsp::Param p) {
        return rig.processor.parameters().getRawParameterValue(dsp::paramSpec(p).id)->load();
    };
    auto near = [](float a, float b) { return std::abs(a - b) <= 1e-3f * std::abs(b) + 1e-4f; };

    CHECK(cutoff.applyTypedValue("1.5k"));
    CHECK(near(value(dsp::Param::Cutoff), 1500.0f));
    CHECK(cutoff.applyTypedValue("  820 Hz "));
    CHECK(near(value(dsp::Param::Cutoff), 820.0f));
    CHECK(cutoff.applyTypedValue("2.5 kHz"));
    CHECK(near(value(dsp::Param::Cutoff), 2500.0f));
    CHECK(cutoff.applyTypedValue("999999"));  // clamped to the range
    CHECK(near(value(dsp::Param::Cutoff), 20000.0f));

    CHECK_FALSE(cutoff.applyTypedValue("loud"));
    CHECK_FALSE(cutoff.applyTypedValue(""));
    CHECK(near(value(dsp::Param::Cutoff), 20000.0f));  // unchanged by bad input

    ui::Knob decay(rig.processor, dsp::Param::AmpDecay, "DECAY");
    CHECK(decay.applyTypedValue("250 ms"));
    CHECK(near(value(dsp::Param::AmpDecay), 0.25f));
    CHECK(decay.applyTypedValue("1.5 s"));
    CHECK(near(value(dsp::Param::AmpDecay), 1.5f));

    ui::Knob detune(rig.processor, dsp::Param::Osc2Pitch, "PITCH");
    CHECK(detune.applyTypedValue("+25"));
    CHECK(near(value(dsp::Param::Osc2Pitch), 25.0f));
    CHECK(detune.applyTypedValue("-1200 ct"));
    CHECK(near(value(dsp::Param::Osc2Pitch), -1200.0f));

    ui::Knob level(rig.processor, dsp::Param::Level, "MASTER");
    CHECK(level.applyTypedValue("-6"));
    CHECK(near(value(dsp::Param::Level), -6.0f));

    ui::Knob voices(rig.processor, dsp::Param::Polyphony, "VOICES");
    CHECK(voices.applyTypedValue("12"));
    CHECK(value(dsp::Param::Polyphony) == 12.0f);
    CHECK(voices.applyTypedValue("99"));
    CHECK(value(dsp::Param::Polyphony) == 16.0f);
}

TEST_CASE("the text box opens over the knob showing the current value")
{
    EditorRig rig;
    ui::Knob knob(rig.processor, dsp::Param::Cutoff, "CUTOFF");
    knob.setSize(72, 108);

    CHECK_FALSE(knob.isEditingText());
    knob.beginTextEntry();
    CHECK(knob.isEditingText());

    // Painting with the box open must not disturb anything.
    const juce::Image image = knob.createComponentSnapshot(knob.getLocalBounds(), true, 2.0f);
    CHECK(image.isValid());
}

TEST_CASE("every parameter has exactly one control on the panel")
{
    std::vector<int> seen(dsp::kParamCount, 0);
    for (const ui::Section& section : ui::panelSections()) {
        for (const ui::Cell& cell : section.cells)
            ++seen[dsp::index(cell.param)];
    }
    for (std::size_t i = 0; i < seen.size(); ++i) {
        INFO(dsp::paramSpecs()[i].id);
        CHECK(seen[i] == 1);
    }
}

TEST_CASE("switches and knobs are used for the parameters they suit")
{
    for (const ui::Section& section : ui::panelSections()) {
        for (const ui::Cell& cell : section.cells) {
            const dsp::ParamSpec& spec = dsp::paramSpec(cell.param);
            INFO(spec.id);
            if (cell.kind == ui::CellKind::Knob)
                CHECK(spec.kind != dsp::ParamKind::Choice);
            else
                CHECK((spec.kind == dsp::ParamKind::Choice || spec.max - spec.min <= 4.0f));
        }
    }
}

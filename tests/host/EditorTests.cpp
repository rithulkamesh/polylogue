#include "host/HostTestSupport.h"
#include "presets/FactoryPresets.h"
#include "ui/PluginEditor.h"

#include <catch2/catch_test_macros.hpp>

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
    CHECK(editor->getHeight() == 620);
}

TEST_CASE("the editor draws something at every size it allows")
{
    EditorRig rig;
    auto editor = rig.editor();
    for (auto [width, height] :
         {std::pair{1020, 527}, std::pair{1200, 620}, std::pair{2400, 1240}}) {
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
    CHECK(std::abs(static_cast<double>(editor->getWidth()) / editor->getHeight() - 1200.0 / 620.0) <
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

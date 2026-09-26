// Renders the editor to a PNG without a plug-in host, for reviewing the interface and for docs.
#include "host/PluginProcessor.h"
#include "ui/PanelLayout.h"
#include "ui/PluginEditor.h"

#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

using namespace polylogue;

namespace {

struct Options {
    std::string preset = "Warm Pad";
    std::string out = "editor.png";
    std::string then;   // a second preset to switch to once the editor is showing
    std::string touch;  // a panel knob to nudge so its readout shows, e.g. "CUTOFF"
    float scale = 2.0f;
    int note = 48;
    bool spectrum = false;
    bool map = false;
};

bool parse(int argc, char** argv, Options& options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string_view flag = argv[i];
        if (flag == "--spectrum") {
            options.spectrum = true;
        } else if (flag == "--map") {
            options.map = true;
        } else if (i + 1 < argc) {
            const std::string value = argv[++i];
            if (flag == "--preset")
                options.preset = value;
            else if (flag == "--out")
                options.out = value;
            else if (flag == "--then")
                options.then = value;
            else if (flag == "--touch")
                options.touch = value;
            else if (flag == "--scale")
                options.scale = std::strtof(value.c_str(), nullptr);
            else if (flag == "--note")
                options.note = std::atoi(value.c_str());
            else
                return false;
        } else {
            return false;
        }
    }
    return true;
}

// Lets timers and asynchronous updates run for a moment.
void pump(int milliseconds)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds);
}

}  // namespace

int main(int argc, char** argv)
{
    Options options;
    if (!parse(argc, argv, options)) {
        std::cerr << "usage: polylogue-screenshot [--preset NAME] [--out FILE.png] [--scale N]\n"
                     "                           [--note N] [--touch KNOB] [--spectrum] [--map]\n";
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juce;

    const juce::File scratch = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getChildFile("polylogue-screenshot-" + juce::Uuid().toString());
    scratch.createDirectory();

    int status = 0;
    {
        host::PolylogueProcessor::Options processorOptions;
        processorOptions.dataDirectory = scratch;
        processorOptions.editor = [](host::PolylogueProcessor& p) -> juce::AudioProcessorEditor* {
            return new ui::PluginEditor(p);
        };
        host::PolylogueProcessor processor(std::move(processorOptions));
        processor.prepareToPlay(48000.0, 512);

        for (const auto& entry : processor.presets().entries()) {
            if (entry.name == juce::String(options.preset))
                processor.presets().load(entry);
        }

        // Play a note so the visualizer has something to show.
        juce::AudioBuffer<float> buffer(2, 512);
        juce::MidiBuffer midi;
        midi.addEvent(juce::MidiMessage::noteOn(1, options.note, 0.8f), 0);
        for (int block = 0; block < 12; ++block) {
            buffer.clear();
            processor.processBlock(buffer, midi);
            midi.clear();
        }

        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        pump(120);

        if (!options.then.empty()) {
            for (const auto& entry : processor.presets().entries()) {
                if (entry.name == juce::String(options.then))
                    processor.presets().load(entry);
            }
            pump(250);
        }
        if (!options.touch.empty()) {
            for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
                const auto param = static_cast<dsp::Param>(i);
                if (options.touch == ui::controlLabel(param)) {
                    auto* parameter = processor.parameter(param);
                    parameter->setValueNotifyingHost(parameter->getValue() * 0.9f + 0.05f);
                    break;
                }
            }
            pump(120);
        }
        if (options.map)
            processor.midiMapper().startLearning(host::MidiMapper::slotOf(dsp::Param::Cutoff));
        if (options.spectrum)
            static_cast<ui::PluginEditor&>(*editor).display().visualizer().showSpectrum(true);
        for (int block = 0; block < 4; ++block) {
            buffer.clear();
            processor.processBlock(buffer, midi);
        }
        pump(120);

        const auto image =
            editor->createComponentSnapshot(editor->getLocalBounds(), true, options.scale);
        juce::File target(options.out);
        target.deleteFile();
        juce::FileOutputStream stream(target);
        if (!stream.openedOk() || !juce::PNGImageFormat().writeImageToStream(image, stream)) {
            std::cerr << "cannot write " << options.out << "\n";
            status = 1;
        } else {
            std::cout << "wrote " << options.out << " (" << image.getWidth() << "x"
                      << image.getHeight() << ")\n";
        }
    }
    scratch.deleteRecursively();
    return status;
}

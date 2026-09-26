#include "host/PluginProcessor.h"
#include "ui/PluginEditor.h"

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    using namespace polylogue;
    host::PolylogueProcessor::Options options;
    options.editor = [](host::PolylogueProcessor& processor) -> juce::AudioProcessorEditor* {
        return new ui::PluginEditor(processor);
    };
    return new host::PolylogueProcessor(std::move(options));
}

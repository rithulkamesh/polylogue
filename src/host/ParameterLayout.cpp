#include "host/ParameterLayout.h"

#include "dsp/Parameters.h"

#include <memory>

namespace polylogue::host {
namespace {

using namespace polylogue::dsp;

std::unique_ptr<juce::RangedAudioParameter> makeFloat(const juce::ParameterID& id,
                                                      const ParamSpec& spec)
{
    const juce::NormalisableRange<float> range(
        spec.min, spec.max,
        [spec](float, float, float normalized) { return toPlain(spec, normalized); },
        [spec](float, float, float plain) { return toNormalized(spec, plain); },
        [spec](float, float, float plain) { return clampToLegal(spec, plain); });

    const auto attributes =
        juce::AudioParameterFloatAttributes()
            .withLabel(spec.unit)
            .withStringFromValueFunction(
                [spec](float value, int) { return juce::String(formatValue(spec, value)); })
            .withValueFromStringFunction([spec](const juce::String& text) {
                return parseValue(spec, text.toStdString()).value_or(spec.defaultValue);
            });

    return std::make_unique<juce::AudioParameterFloat>(id, spec.name, range, spec.defaultValue,
                                                       attributes);
}

std::unique_ptr<juce::RangedAudioParameter> makeInt(const juce::ParameterID& id,
                                                    const ParamSpec& spec)
{
    return std::make_unique<juce::AudioParameterInt>(
        id, spec.name, static_cast<int>(spec.min), static_cast<int>(spec.max),
        static_cast<int>(spec.defaultValue),
        juce::AudioParameterIntAttributes().withLabel(spec.unit));
}

std::unique_ptr<juce::RangedAudioParameter> makeChoice(const juce::ParameterID& id,
                                                       const ParamSpec& spec)
{
    juce::StringArray labels;
    for (const char* label : spec.choices)
        labels.add(label);
    return std::make_unique<juce::AudioParameterChoice>(id, spec.name, labels,
                                                        static_cast<int>(spec.defaultValue));
}

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (const ParamSpec& spec : paramSpecs()) {
        const juce::ParameterID id{spec.id, kParameterVersion};
        switch (spec.kind) {
        case ParamKind::Float:
            layout.add(makeFloat(id, spec));
            break;
        case ParamKind::Int:
            layout.add(makeInt(id, spec));
            break;
        case ParamKind::Choice:
            layout.add(makeChoice(id, spec));
            break;
        case ParamKind::Bool:
            layout.add(std::make_unique<juce::AudioParameterBool>(id, spec.name,
                                                                  spec.defaultValue >= 0.5f));
            break;
        }
    }
    return layout;
}

}  // namespace polylogue::host

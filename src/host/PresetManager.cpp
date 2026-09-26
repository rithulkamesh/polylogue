#include "host/PresetManager.h"

#include "presets/FactoryPresets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>

namespace polylogue::host {
namespace {

constexpr int kPresetFileVersion = 1;

juce::String valueText(float value)
{
    return juce::String::toDecimalStringWithSignificantFigures(static_cast<double>(value), 9);
}

// Hosts may hand a value back rounded a little differently, so "edited" means moved by more than
// a fraction of a percent of the control's travel.
constexpr float kEditedThreshold = 2e-3f;

bool sameSetting(const dsp::ParamSpec& spec, float a, float b)
{
    return std::abs(dsp::toNormalized(spec, a) - dsp::toNormalized(spec, b)) <= kEditedThreshold;
}

}  // namespace

PresetManager::PresetManager(juce::AudioProcessorValueTreeState& parameters,
                             const juce::File& dataDirectory)
    : parameters_(parameters), presetDirectory_(dataDirectory.getChildFile("Presets"))
{
    rescanUserPresets();
}

juce::File PresetManager::defaultDataDirectory()
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Polylogue");
}

void PresetManager::rescanUserPresets()
{
    juce::StringArray before;
    for (const Entry& entry : entries_)
        before.add(entry.name);
    entries_.clear();

    const auto factory = presets::factoryPresets();
    for (std::size_t i = 0; i < factory.size(); ++i)
        entries_.push_back({factory[i].name, factory[i].category, static_cast<int>(i), {}});

    juce::Array<juce::File> files =
        presetDirectory_.findChildFiles(juce::File::findFiles, false, "*.xml");
    files.sort();
    for (const juce::File& file : files) {
        const auto xml = juce::XmlDocument::parse(file);
        // JUCE's parser forgives truncated files, so also insist on some content.
        if (xml == nullptr || !xml->hasTagName("Preset") || xml->getChildByName("Param") == nullptr)
            continue;
        entries_.push_back({xml->getStringAttribute("name", file.getFileNameWithoutExtension()),
                            "User", -1, file});
    }

    juce::StringArray after;
    for (const Entry& entry : entries_)
        after.add(entry.name);
    if (onChanged_ && before != after)
        onChanged_();
}

void PresetManager::load(const Entry& entry)
{
    if (entry.isFactory()) {
        loadFactory(entry.factoryIndex);
        return;
    }

    const auto xml = juce::XmlDocument::parse(entry.file);
    if (xml == nullptr || !xml->hasTagName("Preset"))
        return;

    dsp::ParamValues values = dsp::ParamValues::defaults();
    for (const auto* child : xml->getChildWithTagNameIterator("Param")) {
        const auto param = dsp::paramFromId(child->getStringAttribute("id").toStdString());
        if (!param)
            continue;
        values[*param] = dsp::clampToLegal(dsp::paramSpec(*param),
                                           static_cast<float>(child->getDoubleAttribute("value")));
    }
    applyValues(values);
    markLoaded(entry.name, false);
}

void PresetManager::loadFactory(int index)
{
    const auto factory = presets::factoryPresets();
    if (index < 0 || static_cast<std::size_t>(index) >= factory.size())
        return;
    const auto& preset = factory[static_cast<std::size_t>(index)];
    applyValues(presets::resolve(preset));
    markLoaded(preset.name, false);
}

bool PresetManager::saveUser(const juce::String& name)
{
    const juce::String trimmed = name.trim();
    const juce::String fileName = juce::File::createLegalFileName(trimmed);
    if (trimmed.isEmpty() || fileName.isEmpty() || !presetDirectory_.createDirectory().wasOk())
        return false;

    juce::XmlElement root("Preset");
    root.setAttribute("name", trimmed);
    root.setAttribute("category", "User");
    root.setAttribute("version", kPresetFileVersion);

    const dsp::ParamValues values = currentValues();
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        auto* param = root.createNewChildElement("Param");
        param->setAttribute("id", dsp::paramSpecs()[i].id);
        param->setAttribute("value", valueText(values.values[i]));
    }

    if (!root.writeTo(presetDirectory_.getChildFile(fileName + ".xml")))
        return false;

    rescanUserPresets();
    markLoaded(trimmed, false);
    return true;
}

bool PresetManager::deleteUser(const Entry& entry)
{
    if (entry.isFactory() || !entry.file.existsAsFile() || !entry.file.deleteFile())
        return false;
    rescanUserPresets();
    return true;
}

dsp::ParamValues PresetManager::currentValues() const
{
    dsp::ParamValues values;
    for (std::size_t i = 0; i < dsp::kParamCount; ++i)
        values.values[i] = parameters_.getRawParameterValue(dsp::paramSpecs()[i].id)->load();
    return values;
}

void PresetManager::setValues(const dsp::ParamValues& values)
{
    applyValues(values);
    markLoaded(currentName_, false);
}

void PresetManager::restore(const juce::String& name, const dsp::ParamValues& values, bool modified)
{
    applyValues(values);
    markLoaded(name, modified);
}

bool PresetManager::isModified() const
{
    if (forceModified_)
        return true;
    const dsp::ParamValues now = currentValues();
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        if (!sameSetting(dsp::paramSpecs()[i], now.values[i], loaded_.values[i]))
            return true;
    }
    return false;
}

void PresetManager::applyValues(const dsp::ParamValues& values)
{
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        const dsp::ParamSpec& spec = dsp::paramSpecs()[i];
        auto* parameter = parameters_.getParameter(spec.id);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(dsp::clampToLegal(spec, values.values[i])));
        parameter->endChangeGesture();
    }
}

void PresetManager::markLoaded(const juce::String& name, bool modified)
{
    currentName_ = name;
    loaded_ = currentValues();
    forceModified_ = modified;
    if (onChanged_)
        onChanged_();
}

}  // namespace polylogue::host

#include "host/PluginProcessor.h"

#include "dsp/Axes.h"
#include "host/ParameterLayout.h"
#include "presets/FactoryPresets.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

namespace polylogue::host {
namespace {

constexpr int kStateVersion = 1;
constexpr int kMidiMapVersion = 2;

void writeBindings(const MidiMapper& mapper, juce::XmlElement& xml)
{
    xml.setAttribute("version", kMidiMapVersion);
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        auto* bind = xml.createNewChildElement("Bind");
        bind->setAttribute("id", dsp::paramSpecs()[i].id);
        bind->setAttribute("cc", mapper.controllerFor(static_cast<int>(i)));
    }
}

juce::String valueText(float value)
{
    return juce::String::toDecimalStringWithSignificantFigures(static_cast<double>(value), 9);
}

}  // namespace

PolylogueProcessor::PolylogueProcessor() : PolylogueProcessor(Options{}) {}

PolylogueProcessor::PolylogueProcessor(Options options)
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters_(*this, nullptr, "Polylogue", createParameterLayout()),
      editorFactory_(std::move(options.editor)),
      dataDirectory_(std::move(options.dataDirectory)),
      presets_(parameters_, dataDirectory_)
{
    for (std::size_t i = 0; i < dsp::kParamCount; ++i)
        rawValues_[i] = parameters_.getRawParameterValue(dsp::paramSpecs()[i].id);

    if (const auto xml = juce::XmlDocument::parse(midiMapFile()))
        readMidiMap(*xml);
    savedMapVersion_ = mapper_.version();

    engine_.setSettings(dsp::toSettings(dsp::effectiveValues(presets_.currentValues())));
    engine_.prepare(sampleRate_);

    presets_.setChangeCallback([this] {
        updateHostDisplay(
            ChangeDetails().withProgramChanged(true).withNonParameterStateChanged(true));
    });

    startTimerHz(30);
}

PolylogueProcessor::~PolylogueProcessor()
{
    stopTimer();
    presets_.setChangeCallback({});
    if (mapper_.version() != savedMapVersion_)
        writeMidiMap();
}

juce::RangedAudioParameter* PolylogueProcessor::parameter(dsp::Param param) const
{
    return parameters_.getParameter(dsp::paramSpec(param).id);
}

const juce::String PolylogueProcessor::getName() const
{
    return "Polylogue";
}

bool PolylogueProcessor::acceptsMidi() const
{
    return true;
}

bool PolylogueProcessor::producesMidi() const
{
    return false;
}

bool PolylogueProcessor::isMidiEffect() const
{
    return false;
}

double PolylogueProcessor::getTailLengthSeconds() const
{
    return 8.0;
}

bool PolylogueProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

void PolylogueProcessor::prepareToPlay(double sampleRate, int)
{
    sampleRate_ = sampleRate;
    sampleRateHz_.store(sampleRate, std::memory_order_relaxed);
    engine_.prepare(sampleRate);
}

void PolylogueProcessor::releaseResources() {}

void PolylogueProcessor::processBlock(juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int count = audio.getNumSamples();
    const int channels = audio.getNumChannels();
    if (channels == 0 || count == 0) {
        midi.clear();
        return;
    }

    const std::size_t keyboardCount = drainKeyboardQueue();
    const TranslatedMidi translated =
        translateMidi(midi, std::span(events_).subspan(keyboardCount), controls_);
    midi.clear();
    if (translated.sawAnyMessage)
        midiActivity_.fetch_add(1, std::memory_order_relaxed);

    const std::size_t eventCount = keyboardCount + translated.eventCount;
    monitorNotes({events_.data() + keyboardCount, translated.eventCount});

    // A late event still plays, just at the end of the block.
    for (std::size_t i = 0; i < eventCount; ++i)
        events_[i].offset = std::clamp(events_[i].offset, 0, count - 1);

    applyControlChanges({controls_.data(), translated.controlCount});
    engine_.setSettings(dsp::toSettings(dsp::effectiveValues(readParameters(count))));

    float* left = audio.getWritePointer(0);
    float* right = channels > 1 ? audio.getWritePointer(1) : left;
    engine_.process({events_.data(), eventCount}, left, right, count);
    for (int channel = 2; channel < channels; ++channel)
        audio.clear(channel, 0, count);

    scope_.write(left, count);
}

void PolylogueProcessor::queueKeyboardNote(int note, float velocity)
{
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    keyboardFifo_.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 > 0) {
        keyboardQueue_[static_cast<std::size_t>(start1)] = {note, velocity};
        keyboardFifo_.finishedWrite(1);
    }
}

bool PolylogueProcessor::popMonitoredNote(KeyEvent& event)
{
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    monitorFifo_.prepareToRead(1, start1, size1, start2, size2);
    if (size1 == 0)
        return false;
    event = monitorQueue_[static_cast<std::size_t>(start1)];
    monitorFifo_.finishedRead(1);
    return true;
}

// Fills the start of `events_` with the on-screen keyboard's notes, all at the block start.
std::size_t PolylogueProcessor::drainKeyboardQueue()
{
    std::size_t count = 0;
    while (count < kMaxKeyboardEventsPerBlock && keyboardFifo_.getNumReady() > 0) {
        int start1 = 0;
        int size1 = 0;
        int start2 = 0;
        int size2 = 0;
        keyboardFifo_.prepareToRead(1, start1, size1, start2, size2);
        const KeyEvent key = keyboardQueue_[static_cast<std::size_t>(start1)];
        keyboardFifo_.finishedRead(1);
        events_[count++] = key.velocity > 0.0f ? dsp::MidiEvent::noteOn(0, key.note, key.velocity)
                                               : dsp::MidiEvent::noteOff(0, key.note);
    }
    return count;
}

void PolylogueProcessor::monitorNotes(std::span<const dsp::MidiEvent> events)
{
    for (const dsp::MidiEvent& event : events) {
        const bool on = event.type == dsp::MidiEventType::NoteOn && event.value > 0.0f;
        const bool off = event.type == dsp::MidiEventType::NoteOff ||
                         (event.type == dsp::MidiEventType::NoteOn && event.value <= 0.0f);
        if (!on && !off)
            continue;

        int start1 = 0;
        int size1 = 0;
        int start2 = 0;
        int size2 = 0;
        monitorFifo_.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 > 0) {
            monitorQueue_[static_cast<std::size_t>(start1)] = {event.note, on ? event.value : 0.0f};
            monitorFifo_.finishedWrite(1);
        }
    }
}

void PolylogueProcessor::applyControlChanges(std::span<const ControlChange> controls)
{
    for (const ControlChange& change : controls) {
        const int slot = mapper_.slotFor(change.controller);
        if (slot == MidiMapper::kUnbound)
            continue;

        const auto index = static_cast<std::size_t>(slot);
        const dsp::ParamSpec& spec = dsp::paramSpecs()[index];
        const float plain = dsp::plainFromController(spec, change.value);
        const float normalized = dsp::toNormalized(spec, plain);
        overlays_[index] = {plain, normalized, static_cast<long>(kOverlaySeconds * sampleRate_)};

        int start1 = 0;
        int size1 = 0;
        int start2 = 0;
        int size2 = 0;
        controlFifo_.prepareToWrite(1, start1, size1, start2, size2);
        if (size1 > 0) {
            controlQueue_[static_cast<std::size_t>(start1)] = {slot, normalized};
            controlFifo_.finishedWrite(1);
        }
    }
}

dsp::ParamValues PolylogueProcessor::readParameters(int blockSamples)
{
    dsp::ParamValues values;
    for (std::size_t i = 0; i < dsp::kParamCount; ++i)
        values.values[i] = rawValues_[i]->load(std::memory_order_relaxed);

    for (std::size_t i = 0; i < overlays_.size(); ++i) {
        Overlay& overlay = overlays_[i];
        if (overlay.samplesLeft <= 0)
            continue;

        const float applied = dsp::toNormalized(dsp::paramSpecs()[i], values.values[i]);
        if (std::abs(applied - overlay.normalized) < 1e-3f) {
            overlay.samplesLeft = 0;  // the parameter has caught up
            continue;
        }
        values.values[i] = overlay.plain;
        overlay.samplesLeft -= blockSamples;
    }
    return values;
}

void PolylogueProcessor::bakeAxes()
{
    const dsp::ParamValues before = presets_.currentValues();
    const dsp::ParamValues after = dsp::bake(before);
    if (before.values != after.values)
        presets_.restore(presets_.currentName(), after, true);
}

void PolylogueProcessor::processPendingControlChanges()
{
    std::array<float, dsp::kParamCount> latest{};
    std::array<bool, dsp::kParamCount> moved{};

    const int ready = controlFifo_.getNumReady();
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    controlFifo_.prepareToRead(ready, start1, size1, start2, size2);
    for (int i = 0; i < size1 + size2; ++i) {
        const int slot = i < size1 ? start1 + i : start2 + (i - size1);
        const ControlMessage& message = controlQueue_[static_cast<std::size_t>(slot)];
        latest[static_cast<std::size_t>(message.param)] = message.normalized;
        moved[static_cast<std::size_t>(message.param)] = true;
    }
    controlFifo_.finishedRead(size1 + size2);

    for (std::size_t i = 0; i < moved.size(); ++i) {
        if (!moved[i])
            continue;
        auto* parameter = this->parameter(static_cast<dsp::Param>(i));
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost(latest[i]);
        parameter->endChangeGesture();
    }
}

void PolylogueProcessor::timerCallback()
{
    processPendingControlChanges();
    if (mapper_.version() != savedMapVersion_) {
        savedMapVersion_ = mapper_.version();
        writeMidiMap();
    }
}

juce::File PolylogueProcessor::midiMapFile() const
{
    return dataDirectory_.getChildFile("midi-map.xml");
}

void PolylogueProcessor::writeMidiMap() const
{
    juce::XmlElement xml("MidiMap");
    writeBindings(mapper_, xml);

    const juce::File file = midiMapFile();
    if (file.getParentDirectory().createDirectory().wasOk())
        xml.writeTo(file);
}

// The document lists every parameter, so it replaces the current map outright.
bool PolylogueProcessor::readMidiMap(const juce::XmlElement& xml)
{
    if (!xml.hasTagName("MidiMap") || xml.getIntAttribute("version") != kMidiMapVersion)
        return false;

    for (int slot = 0; slot < MidiMapper::kSlotCount; ++slot)
        mapper_.bind(slot, MidiMapper::kUnbound);
    std::array<bool, dsp::kParamCount> listed{};
    for (const auto* bind : xml.getChildWithTagNameIterator("Bind")) {
        if (const auto param = dsp::paramFromId(bind->getStringAttribute("id").toStdString())) {
            listed[dsp::index(*param)] = true;
            mapper_.bind(MidiMapper::slotOf(*param),
                         bind->getIntAttribute("cc", MidiMapper::kUnbound));
        }
    }
    // A file written before a parameter existed does not mention it: give it its default
    // controller, unless the file has already used that controller for something else.
    for (std::size_t i = 0; i < listed.size(); ++i) {
        const int controller = MidiMapper::defaultController(static_cast<dsp::Param>(i));
        if (!listed[i] && controller != MidiMapper::kUnbound &&
            mapper_.slotFor(controller) == MidiMapper::kUnbound)
            mapper_.bind(static_cast<int>(i), controller);
    }
    return true;
}

bool PolylogueProcessor::hasEditor() const
{
    return static_cast<bool>(editorFactory_);
}

juce::AudioProcessorEditor* PolylogueProcessor::createEditor()
{
    return editorFactory_ ? editorFactory_(*this) : nullptr;
}

// The host's program list is the browser's list: factory presets, then the user's own.
int PolylogueProcessor::getNumPrograms()
{
    return static_cast<int>(presets_.entries().size());
}

int PolylogueProcessor::getCurrentProgram()
{
    const auto& entries = presets_.entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].name == presets_.currentName())
            return static_cast<int>(i);
    }
    return 0;
}

void PolylogueProcessor::setCurrentProgram(int index)
{
    const auto& entries = presets_.entries();
    if (index >= 0 && static_cast<std::size_t>(index) < entries.size())
        presets_.load(entries[static_cast<std::size_t>(index)]);
}

const juce::String PolylogueProcessor::getProgramName(int index)
{
    const auto& entries = presets_.entries();
    if (index < 0 || static_cast<std::size_t>(index) >= entries.size())
        return {};
    return entries[static_cast<std::size_t>(index)].name;
}

void PolylogueProcessor::changeProgramName(int, const juce::String&) {}

void PolylogueProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    juce::XmlElement root("Polylogue");
    root.setAttribute("version", kStateVersion);
    root.setAttribute("preset", presets_.currentName());
    root.setAttribute("modified", presets_.isModified() ? 1 : 0);

    const dsp::ParamValues values = presets_.currentValues();
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        auto* param = root.createNewChildElement("Param");
        param->setAttribute("id", dsp::paramSpecs()[i].id);
        param->setAttribute("value", valueText(values.values[i]));
    }

    writeBindings(mapper_, *root.createNewChildElement("MidiMap"));

    copyXmlToBinary(root, destination);
}

void PolylogueProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr || !xml->hasTagName("Polylogue"))
        return;

    // Anything the saved state does not mention returns to its default, so loading is
    // deterministic however old the session is.
    dsp::ParamValues values = dsp::ParamValues::defaults();
    for (const auto* child : xml->getChildWithTagNameIterator("Param")) {
        const auto param = dsp::paramFromId(child->getStringAttribute("id").toStdString());
        if (!param)
            continue;
        values[*param] = dsp::clampToLegal(dsp::paramSpec(*param),
                                           static_cast<float>(child->getDoubleAttribute("value")));
    }
    presets_.restore(xml->getStringAttribute("preset", "Init"), values,
                     xml->getBoolAttribute("modified"));

    // This machine's own controller map wins; the session's copy is only a fallback.
    if (const auto* map = xml->getChildByName("MidiMap");
        map != nullptr && !midiMapFile().existsAsFile())
        readMidiMap(*map);
}

}  // namespace polylogue::host

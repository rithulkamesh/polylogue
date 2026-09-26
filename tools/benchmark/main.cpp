// Measures how much of a real-time budget the DSP uses. Run a Release build.
#include "dsp/Engine.h"
#include "dsp/Parameters.h"
#include "presets/FactoryPresets.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace polylogue;

namespace {

constexpr double kSampleRate = 48000.0;
constexpr int kBlock = 512;
constexpr double kSeconds = 10.0;

struct Scenario {
    const char* name;
    const char* preset;
    int voices;
    int polyphony;
};

dsp::SynthSettings settingsFor(const char* presetName, int polyphony)
{
    for (const auto& preset : presets::factoryPresets()) {
        if (std::string(presetName) == preset.name) {
            dsp::ParamValues values = presets::resolve(preset);
            values[dsp::Param::Polyphony] = static_cast<float>(polyphony);
            values[dsp::Param::KeyMode] = 0.0f;
            return dsp::toSettings(values);
        }
    }
    std::fprintf(stderr, "no preset %s\n", presetName);
    std::exit(1);
}

double run(const Scenario& scenario)
{
    dsp::Engine engine;
    engine.setSettings(settingsFor(scenario.preset, scenario.polyphony));
    engine.prepare(kSampleRate);

    std::vector<dsp::MidiEvent> notes;
    for (int i = 0; i < scenario.voices; ++i)
        notes.push_back(dsp::MidiEvent::noteOn(0, 36 + i * 3, 0.8f));

    std::vector<float> left(kBlock);
    std::vector<float> right(kBlock);
    const int blocks = static_cast<int>(kSeconds * kSampleRate / kBlock);

    engine.process(notes, left.data(), right.data(), kBlock);
    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < blocks; ++block)
        engine.process({}, left.data(), right.data(), kBlock);
    const std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;

    const double perBlock = elapsed.count() / blocks;
    const double budget = kBlock / kSampleRate;
    std::printf("%-34s %7.1f us/block %6.2f%% of one core   (%d active voices)\n", scenario.name,
                perBlock * 1e6, 100.0 * perBlock / budget, engine.voices().activeVoiceCount());
    return perBlock / budget;
}

}  // namespace

int main()
{
    std::printf("48 kHz, %d-sample blocks, %.0f s of audio per scenario\n\n", kBlock, kSeconds);
    run({"idle", "Glass Pad", 0, 8});
    run({"1 voice, pad", "Warm Pad", 1, 8});
    run({"8 voices, pad", "Warm Pad", 8, 8});
    run({"8 voices, bell (ring mod)", "Glass Bell", 8, 8});
    run({"8 voices, gong (noise + LFO)", "Tam Tam", 8, 8});
    run({"16 voices, pad", "Warm Pad", 16, 16});
    run({"16 voices, growl (drive, LFO)", "Growl", 16, 16});
    run({"16 voices, sync lead (drive)", "Crunch Lead", 16, 16});
    return 0;
}

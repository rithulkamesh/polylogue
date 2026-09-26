#include "dsp/Tuning.h"
#include "presets/FactoryPresets.h"
#include "support/Analysis.h"
#include "support/EngineRender.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <span>
#include <vector>

using namespace polylogue;
using namespace polylogue::test;

using dsp::SynthSettings;

namespace {

constexpr double kRates[] = {22050.0, 44100.0, 88200.0, 96000.0, 192000.0};

}  // namespace

TEST_CASE("every preset renders cleanly at every common sample rate")
{
    // Every preset at the two most common rates; a spread of them at the rest.
    for (double rate : kRates) {
        const bool common = rate == 44100.0 || rate == 96000.0;
        std::size_t index = 0;
        for (const auto& preset : presets::factoryPresets()) {
            if (!common && index++ % 5 != 0)
                continue;
            INFO(preset.name << " at " << rate);
            const auto out = renderEngine(dsp::toSettings(presets::resolve(preset)),
                                          {at(0.0, MidiEvent::noteOn(0, 48, 0.8f)),
                                           at(0.0, MidiEvent::noteOn(0, 60, 0.8f)),
                                           at(0.6, MidiEvent::noteOff(0, 48))},
                                          1.0, 512, rate);
            REQUIRE(allFinite(out));
            CHECK(peak(out) <= 1.0f);
            CHECK(peak(out) > 0.01f);
        }
    }
}

TEST_CASE("pitch is exact at every sample rate")
{
    for (double rate : kRates) {
        SynthSettings s = plainSaw();
        const auto out = renderEngine(s, {at(0.0, MidiEvent::noteOn(0, 57, 0.9f))}, 1.5, 512, rate);
        const auto begin = static_cast<std::size_t>(0.3 * rate);
        const auto end = static_cast<std::size_t>(1.4 * rate);
        const auto hz =
            zeroCrossingFrequency(std::span<const float>(out).subspan(begin, end - begin), rate);
        REQUIRE(hz.has_value());
        INFO("rate " << rate);
        CHECK(std::abs(centsBetween(*hz, dsp::midiToHz(57))) < 0.5);
    }
}

TEST_CASE("every MIDI note at every velocity stays finite and inside full scale")
{
    SynthSettings s = plainSaw();
    s.osc2Level = 1.0f;
    s.osc2Cents = 700.0f;
    s.syncRing = dsp::SyncRing::Sync;
    s.resonance = 0.8f;
    s.cutoffHz = 5000.0f;
    s.drive = 0.5f;

    for (double rate : {44100.0, 96000.0}) {
        for (int note = 0; note <= 127; ++note) {
            for (float velocity : {0.01f, 1.0f}) {
                const auto out = renderEngine(s, {at(0.0, MidiEvent::noteOn(0, note, velocity))},
                                              0.15, 512, rate);
                INFO("note " << note << " velocity " << velocity << " rate " << rate);
                REQUIRE(allFinite(out));
                CHECK(peak(out) <= 1.0f);
            }
        }
    }
}

TEST_CASE("a flood of notes, releases and pedal changes never breaks the engine")
{
    std::vector<TimedEvent> events;
    unsigned state = 12345;
    auto next = [&state] {
        state = state * 1664525u + 1013904223u;
        return state >> 8;
    };
    for (int i = 0; i < 4000; ++i) {
        const double time = static_cast<double>(next() % 3000) / 1000.0;
        const int note = static_cast<int>(next() % 128);
        switch (next() % 6) {
        case 0:
        case 1:
        case 2:
            events.push_back(
                at(time, MidiEvent::noteOn(0, note, static_cast<float>(next() % 128) / 127.0f)));
            break;
        case 3:
            events.push_back(at(time, MidiEvent::noteOff(0, note)));
            break;
        case 4:
            events.push_back(at(time, MidiEvent::sustain(0, next() % 2 == 0)));
            break;
        default:
            events.push_back(at(
                time, MidiEvent::pitchBend(0, static_cast<float>(next() % 2001) / 1000.0f - 1.0f)));
            break;
        }
    }

    std::size_t index = 0;
    for (const auto& preset : presets::factoryPresets()) {
        if (index++ % 3 != 0)
            continue;
        INFO(preset.name);
        const auto out = renderEngine(dsp::toSettings(presets::resolve(preset)), events, 3.5);
        REQUIRE(allFinite(out));
        CHECK(peak(out) <= 1.0f);
    }
}

TEST_CASE("stepping a smoothed parameter never clicks")
{
    constexpr int kTotal = 48000;
    constexpr int kChangeAt = 24000;

    auto render = [](auto&& change) {
        SynthSettings s = plainSaw();
        s.osc1Wave = dsp::Osc1Wave::Triangle;
        s.osc2Wave = dsp::Osc2Wave::Triangle;
        s.osc2Level = 0.5f;
        s.cutoffHz = 20000.0f;
        s.ampEnv = {dsp::EnvelopeType::AGD, 0.001f, 0.2f};

        dsp::Engine engine;
        engine.setSettings(s);
        engine.prepare(kSampleRate);

        std::vector<float> left(kTotal);
        std::vector<float> right(kTotal);
        const std::vector<MidiEvent> note = {MidiEvent::noteOn(0, 40, 0.9f)};
        engine.process(note, left.data(), right.data(), kChangeAt);
        change(s);
        engine.setSettings(s);
        engine.process({}, left.data() + kChangeAt, right.data() + kChangeAt, kTotal - kChangeAt);
        return left;
    };
    auto worstStep = [](const std::vector<float>& signal, int from, int to) {
        float worst = 0.0f;
        for (int i = from; i < to; ++i)
            worst = std::max(worst, std::abs(signal[static_cast<std::size_t>(i + 1)] -
                                             signal[static_cast<std::size_t>(i)]));
        return worst;
    };

    // The transition may be no steeper than the sound it settles into (a driven triangle really is
    // that sharp) or the sound it left, whichever is steeper. A jump would be far beyond both.
    auto check = [&](const char* name, auto&& change) {
        const auto signal = render(change);
        const float before = worstStep(signal, kChangeAt - 3000, kChangeAt - 1);
        const float after = worstStep(signal, kTotal - 4000, kTotal - 1);
        const float during = worstStep(signal, kChangeAt - 1, kChangeAt + 6000);
        INFO(name << ": before " << before << ", after " << after << ", during " << during);
        CHECK(during <= 1.5f * std::max(before, after));
    };

    check("level", [](SynthSettings& s) { s.outputGain = 0.05f; });
    check("oscillator 1 level", [](SynthSettings& s) { s.osc1Level = 0.0f; });
    check("oscillator 2 level", [](SynthSettings& s) { s.osc2Level = 1.0f; });
    check("drive", [](SynthSettings& s) { s.drive = 1.0f; });
    check("cutoff", [](SynthSettings& s) { s.cutoffHz = 200.0f; });
    check("resonance", [](SynthSettings& s) {
        s.resonance = 0.9f;
        s.cutoffHz = 900.0f;
    });
    check("shape", [](SynthSettings& s) { s.osc1Shape = 1.0f; });
    check("pitch bend", [](SynthSettings& s) { s.pitchBendSemitones = 5.0f; });
}

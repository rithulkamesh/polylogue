#include "presets/FactoryPresets.h"
#include "support/Analysis.h"
#include "support/EngineRender.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <set>
#include <string>
#include <vector>

using namespace polylogue;
using namespace polylogue::test;

namespace {

struct Rendered {
    std::vector<float> audio;
    double loudestWindowDb = -120.0;
};

Rendered play(const presets::FactoryPreset& preset, const std::vector<int>& notes, double hold,
              double seconds)
{
    std::vector<TimedEvent> events;
    for (int note : notes) {
        events.push_back(at(0.0, MidiEvent::noteOn(0, note, 0.8f)));
        events.push_back(at(hold, MidiEvent::noteOff(0, note)));
    }
    Rendered result;
    result.audio = renderEngine(dsp::toSettings(presets::resolve(preset)), events, seconds);

    constexpr std::size_t kWindow = 4800;
    for (std::size_t start = 0; start + kWindow <= result.audio.size(); start += kWindow) {
        const double level =
            decibels(rms(std::span<const float>(result.audio).subspan(start, kWindow)) + 1e-9);
        result.loudestWindowDb = std::max(result.loudestWindowDb, level);
    }
    return result;
}

}  // namespace

TEST_CASE("presets are named uniquely and belong to a known category")
{
    std::set<std::string> names;
    std::set<std::string> categories(presets::presetCategories().begin(),
                                     presets::presetCategories().end());
    for (const auto& preset : presets::factoryPresets()) {
        INFO(preset.name);
        CHECK(names.insert(preset.name).second);
        CHECK(categories.count(preset.category) == 1);
    }
}

TEST_CASE("every requested kind of sound has presets")
{
    std::set<std::string> present;
    for (const auto& preset : presets::factoryPresets())
        present.insert(preset.category);
    for (const char* wanted :
         {"Pad", "Bell", "Gong", "Bass", "Lead", "Pluck", "Ambient", "Distorted", "Keys"})
        CHECK(present.count(wanted) == 1);
    CHECK(presets::factoryPresets().size() >= 24);
}

TEST_CASE("preset values are legal and never repeat a parameter")
{
    for (const auto& preset : presets::factoryPresets()) {
        INFO(preset.name);
        std::set<dsp::Param> seen;
        for (const auto& entry : preset.values) {
            const dsp::ParamSpec& spec = dsp::paramSpec(entry.param);
            INFO(spec.id);
            CHECK(seen.insert(entry.param).second);
            CHECK(entry.value >= spec.min);
            CHECK(entry.value <= spec.max);
            CHECK(dsp::clampToLegal(spec, entry.value) == entry.value);
        }
    }
}

TEST_CASE("resolving a preset starts from the defaults")
{
    const auto defaults = dsp::ParamValues::defaults();
    const auto& preset = presets::factoryPresets().front();
    const auto values = presets::resolve(preset);

    std::set<dsp::Param> overridden;
    for (const auto& entry : preset.values)
        overridden.insert(entry.param);
    for (std::size_t i = 0; i < dsp::kParamCount; ++i) {
        const auto param = static_cast<dsp::Param>(i);
        if (overridden.count(param) == 0)
            CHECK(values[param] == defaults[param]);
    }
}

TEST_CASE("every preset plays a healthy, finite, unclipped sound and then ends")
{
    for (const auto& preset : presets::factoryPresets()) {
        INFO(preset.name);
        const Rendered r = play(preset, {48, 55, 60}, 2.0, 16.0);

        REQUIRE(allFinite(r.audio));
        CHECK(peak(r.audio) <= 1.0f);
        CHECK(peak(r.audio) > 0.05f);
        CHECK(r.loudestWindowDb > -26.0);
        CHECK(r.loudestWindowDb < -6.0);
        // Long releases are fine, but nothing may ring forever.
        const auto end = std::span<const float>(r.audio).subspan(r.audio.size() - 4800);
        CHECK(peak(end) < 1e-3f);
    }
}

TEST_CASE("presets are loudness-matched within a few decibels")
{
    std::vector<double> levels;
    for (const auto& preset : presets::factoryPresets())
        levels.push_back(play(preset, {48}, 2.0, 8.0).loudestWindowDb);

    const auto [lowest, highest] = std::minmax_element(levels.begin(), levels.end());
    CHECK(*highest - *lowest < 6.5);
}

TEST_CASE("categories behave like their names")
{
    auto duration = [](const char* name) {
        for (const auto& preset : presets::factoryPresets()) {
            if (std::string(name) == preset.name) {
                const auto audio = play(preset, {48}, 0.3, 14.0).audio;
                std::size_t last = 0;
                for (std::size_t i = 0; i < audio.size(); i += 480) {
                    const auto block = std::span<const float>(audio).subspan(
                        i, std::min<std::size_t>(480, audio.size() - i));
                    if (peak(block) > 0.003f)
                        last = i;
                }
                return static_cast<double>(last) / kSampleRate;
            }
        }
        return -1.0;
    };

    // Plucks die quickly even though the key is held; bells and gongs ring on.
    CHECK(duration("Pizzicato") < 2.5);
    CHECK(duration("Clav") < 1.5);
    CHECK(duration("Church Bell") > 4.0);
    CHECK(duration("Temple Gong") > 5.0);
    CHECK(duration("Ghost Gong") > 4.5);
}

TEST_CASE("pads swell in slowly")
{
    for (const auto& preset : presets::factoryPresets()) {
        if (std::string(preset.category) != "Pad")
            continue;
        INFO(preset.name);
        const auto audio = play(preset, {48, 55}, 6.0, 8.0).audio;
        const double early = rms(std::span<const float>(audio).subspan(0, 2400));
        const double later = rms(std::span<const float>(audio).subspan(96000, 4800));
        CHECK(later > 3.0 * early);
    }
}

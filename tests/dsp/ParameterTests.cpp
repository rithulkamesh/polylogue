#include "dsp/Parameters.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <set>
#include <string>
#include <vector>

using namespace polylogue::dsp;

TEST_CASE("the table has one spec per parameter, in enum order")
{
    REQUIRE(paramSpecs().size() == kParamCount);
    CHECK(std::string(paramSpec(Param::Level).id) == "level");
    CHECK(std::string(paramSpec(Param::Cutoff).id) == "cutoff");
    CHECK(std::string(paramSpec(Param::LfoTarget).id) == "lfo_target");
}

TEST_CASE("parameter ids are unique and non-empty")
{
    std::set<std::string> seen;
    for (const ParamSpec& spec : paramSpecs()) {
        CHECK(std::string(spec.id).size() > 0);
        CHECK(seen.insert(spec.id).second);
        CHECK(std::string(spec.name).size() > 0);
    }
}

// Saved sessions and presets refer to these strings. Renaming one breaks them.
TEST_CASE("parameter ids are stable")
{
    const std::vector<std::string> expected = {
        "level",       "polyphony",   "key_mode",       "octave",     "tune",       "bend_range",
        "glide_time",  "glide_mode",  "drive",          "osc1_wave",  "osc1_shape", "osc2_wave",
        "osc2_octave", "osc2_pitch",  "osc2_sync_ring", "osc2_shape", "osc1_level", "osc2_level",
        "cutoff",      "resonance",   "key_track",      "vel_cutoff", "amp_type",   "amp_attack",
        "amp_decay",   "vel_amp",     "env_type",       "env_attack", "env_decay",  "env_int",
        "env_target",  "lfo_wave",    "lfo_mode",       "lfo_rate",   "lfo_int",    "lfo_target",
        "chorus_mix",  "chorus_rate", "chorus_depth",   "macro1",     "macro2",     "macro3",
        "macro4",      "macro5",      "macro6",         "macro7",     "macro8"};
    REQUIRE(expected.size() == kParamCount);
    for (std::size_t i = 0; i < expected.size(); ++i)
        CHECK(expected[i] == paramSpecs()[i].id);
}

TEST_CASE("ids resolve back to their parameter")
{
    for (std::size_t i = 0; i < kParamCount; ++i)
        CHECK(paramFromId(paramSpecs()[i].id) == static_cast<Param>(i));
    CHECK_FALSE(paramFromId("no_such_parameter").has_value());
}

TEST_CASE("ranges are valid and defaults fall inside them")
{
    for (const ParamSpec& spec : paramSpecs()) {
        INFO(spec.id);
        CHECK(spec.min < spec.max);
        CHECK(spec.defaultValue >= spec.min);
        CHECK(spec.defaultValue <= spec.max);
        if (spec.curve == Curve::Exponential)
            CHECK(spec.min > 0.0f);
        if (spec.kind == ParamKind::Choice) {
            CHECK(static_cast<std::size_t>(spec.max) + 1 == spec.choices.size());
            CHECK(spec.min == 0.0f);
        }
    }
}

TEST_CASE("normalising and denormalising are inverses")
{
    for (const ParamSpec& spec : paramSpecs()) {
        INFO(spec.id);
        for (float n = 0.0f; n <= 1.0f; n += 0.05f) {
            const float plain = toPlain(spec, n);
            CHECK(plain >= spec.min);
            CHECK(plain <= spec.max);
            // Snapped parameters can only round-trip to a legal value.
            const float again = toPlain(spec, toNormalized(spec, plain));
            CHECK(std::abs(again - plain) <= 1e-3f * (spec.max - spec.min) + 1e-6f);
        }
        CHECK(toPlain(spec, 0.0f) == spec.min);
        CHECK(toPlain(spec, 1.0f) == spec.max);
    }
}

TEST_CASE("every curve is monotonic")
{
    for (const ParamSpec& spec : paramSpecs()) {
        INFO(spec.id);
        float previous = toPlain(spec, 0.0f);
        for (float n = 0.01f; n <= 1.0f; n += 0.01f) {
            const float plain = toPlain(spec, n);
            CHECK(plain >= previous);
            previous = plain;
        }
    }
}

TEST_CASE("detune has fine control at the centre and reaches full intervals")
{
    const ParamSpec& detune = paramSpec(Param::Osc2Pitch);
    CHECK(toPlain(detune, 0.5f) == 0.0f);
    CHECK(std::abs(toPlain(detune, 0.55f)) <= 10.0f);
    CHECK(toPlain(detune, 1.0f) == 1200.0f);
    CHECK(toPlain(detune, 0.0f) == -1200.0f);
    CHECK(toPlain(detune, 0.75f) == -toPlain(detune, 0.25f));
}

TEST_CASE("cutoff sweeps in equal ratios")
{
    const ParamSpec& cutoff = paramSpec(Param::Cutoff);
    const float a = toPlain(cutoff, 0.25f);
    const float b = toPlain(cutoff, 0.5f);
    const float c = toPlain(cutoff, 0.75f);
    CHECK(std::abs(b / a - c / b) < 0.01f);
}

TEST_CASE("choices, integers and steps snap to legal values")
{
    CHECK(clampToLegal(paramSpec(Param::KeyMode), 0.6f) == 1.0f);
    CHECK(clampToLegal(paramSpec(Param::Osc1Wave), 7.0f) == 2.0f);
    CHECK(clampToLegal(paramSpec(Param::Polyphony), 20.4f) == 16.0f);
    CHECK(clampToLegal(paramSpec(Param::Osc2Pitch), 12.4f) == 12.0f);
    CHECK(clampToLegal(paramSpec(Param::Cutoff), 5.0f) == 20.0f);
}

TEST_CASE("values format for the display")
{
    CHECK(formatValue(paramSpec(Param::Cutoff), 8000.0f) == "8.00 kHz");
    CHECK(formatValue(paramSpec(Param::Cutoff), 820.0f) == "820 Hz");
    CHECK(formatValue(paramSpec(Param::Cutoff), 45.0f) == "45.0 Hz");
    CHECK(formatValue(paramSpec(Param::ChorusRate), 0.6f) == "0.60 Hz");
    CHECK(formatValue(paramSpec(Param::AmpAttack), 0.002f) == "2.0 ms");
    CHECK(formatValue(paramSpec(Param::AmpDecay), 0.25f) == "250 ms");
    CHECK(formatValue(paramSpec(Param::AmpDecay), 2.5f) == "2.50 s");
    CHECK(formatValue(paramSpec(Param::Osc2Pitch), 7.0f) == "+7 ct");
    CHECK(formatValue(paramSpec(Param::Osc2Pitch), -12.0f) == "-12 ct");
    CHECK(formatValue(paramSpec(Param::Level), -6.0f) == "-6.0 dB");
    CHECK(formatValue(paramSpec(Param::Resonance), 0.4f) == "40%");
    CHECK(formatValue(paramSpec(Param::EnvInt), -0.25f) == "-25%");
    CHECK(formatValue(paramSpec(Param::Osc1Wave), 2.0f) == "Square");
    CHECK(formatValue(paramSpec(Param::Osc2Octave), 3.0f) == "2'");
    CHECK(formatValue(paramSpec(Param::Polyphony), 8.0f) == "8");
    CHECK(formatValue(paramSpec(Param::BendRange), 2.0f) == "2 st");
}

TEST_CASE("default parameters produce the default settings")
{
    CHECK(toSettings(ParamValues::defaults()) == SynthSettings{});
}

TEST_CASE("parameters map onto the settings they claim to")
{
    ParamValues v = ParamValues::defaults();
    v[Param::Level] = -6.0206f;
    v[Param::Polyphony] = 5.0f;
    v[Param::KeyMode] = 1.0f;
    v[Param::Osc1Wave] = 2.0f;
    v[Param::Osc2Wave] = 2.0f;
    v[Param::Osc2Octave] = 3.0f;
    v[Param::Osc2SyncRing] = 2.0f;
    v[Param::KeyTrack] = 1.0f;
    v[Param::AmpType] = 2.0f;
    v[Param::EnvTarget] = 2.0f;
    v[Param::LfoMode] = 2.0f;
    v[Param::LfoTarget] = 2.0f;
    v[Param::Cutoff] = 1234.0f;

    const SynthSettings s = toSettings(v);
    CHECK(std::abs(s.outputGain - 0.5f) < 1e-3f);
    CHECK(s.polyphony == 5);
    CHECK(s.keyMode == KeyMode::Mono);
    CHECK(s.osc1Wave == Osc1Wave::Square);
    CHECK(s.osc2Wave == Osc2Wave::Noise);
    CHECK(s.osc2Range == Osc2Range::Feet2);
    CHECK(s.syncRing == SyncRing::Ring);
    CHECK(s.keyTrack == 0.5f);
    CHECK(s.ampEnv.type == EnvelopeType::Gate);
    CHECK(s.modEnvTarget == EnvelopeTarget::Pitch2);
    CHECK(s.lfo.mode == LfoMode::OneShot);
    CHECK(s.lfo.target == LfoTarget::Cutoff);
    CHECK(s.cutoffHz == 1234.0f);
}

TEST_CASE("the level parameter's floor is silence")
{
    ParamValues v = ParamValues::defaults();
    v[Param::Level] = -60.0f;
    CHECK(toSettings(v).outputGain == 0.0f);
}

TEST_CASE("controller values select continuous parameters along their curve")
{
    const ParamSpec& cutoff = paramSpec(Param::Cutoff);
    CHECK(plainFromController(cutoff, 0) == cutoff.min);
    CHECK(plainFromController(cutoff, 127) == cutoff.max);
    CHECK(std::abs(plainFromController(cutoff, 64) - toPlain(cutoff, 64.0f / 127.0f)) < 1e-3f);
    CHECK(plainFromController(cutoff, -5) == cutoff.min);
    CHECK(plainFromController(cutoff, 500) == cutoff.max);
}

TEST_CASE("controller values split switches into equal bands")
{
    const ParamSpec& wave = paramSpec(Param::Osc1Wave);
    CHECK(plainFromController(wave, 0) == 0.0f);
    CHECK(plainFromController(wave, 42) == 0.0f);
    CHECK(plainFromController(wave, 43) == 1.0f);
    CHECK(plainFromController(wave, 85) == 1.0f);
    CHECK(plainFromController(wave, 86) == 2.0f);
    CHECK(plainFromController(wave, 127) == 2.0f);

    const ParamSpec& octave = paramSpec(Param::Octave);
    CHECK(plainFromController(octave, 0) == -2.0f);
    CHECK(plainFromController(octave, 127) == 2.0f);
    const ParamSpec& voices = paramSpec(Param::Polyphony);
    CHECK(plainFromController(voices, 0) == 1.0f);
    CHECK(plainFromController(voices, 127) == 16.0f);

    for (const ParamSpec& spec : paramSpecs()) {
        for (int value = 0; value <= 127; ++value) {
            const float plain = plainFromController(spec, value);
            INFO(spec.id << " " << value);
            CHECK(plain >= spec.min);
            CHECK(plain <= spec.max);
            CHECK(clampToLegal(spec, plain) == plain);
        }
    }
}

TEST_CASE("typed text is understood")
{
    CHECK(*parseValue(paramSpec(Param::Cutoff), "1.5k") == 1500.0f);
    CHECK(*parseValue(paramSpec(Param::Cutoff), "820 Hz") == 820.0f);
    CHECK(*parseValue(paramSpec(Param::Cutoff), "2.5 kHz") == 2500.0f);
    CHECK(std::abs(*parseValue(paramSpec(Param::AmpDecay), "250 ms") - 0.25f) < 1e-6f);
    CHECK(std::abs(*parseValue(paramSpec(Param::AmpDecay), "1.5 s") - 1.5f) < 1e-6f);
    CHECK(std::abs(*parseValue(paramSpec(Param::Resonance), "40%") - 0.4f) < 1e-6f);
    CHECK(*parseValue(paramSpec(Param::Osc2Pitch), "+7 ct") == 7.0f);
    CHECK(*parseValue(paramSpec(Param::Osc1Wave), "square") == 2.0f);
    CHECK(*parseValue(paramSpec(Param::Osc2Octave), "4'") == 2.0f);
    CHECK(*parseValue(paramSpec(Param::Cutoff), "999999") == 20000.0f);
    CHECK_FALSE(parseValue(paramSpec(Param::Cutoff), "loud").has_value());
    CHECK_FALSE(parseValue(paramSpec(Param::Osc1Wave), "noise").has_value());
}

TEST_CASE("formatted values parse back to themselves")
{
    for (const ParamSpec& spec : paramSpecs()) {
        INFO(spec.id);
        for (float n : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            const float plain = toPlain(spec, n);
            const auto parsed = parseValue(spec, formatValue(spec, plain));
            REQUIRE(parsed.has_value());
            // Display rounds, so allow for the precision shown.
            const float tolerance =
                spec.kind == ParamKind::Float ? 0.01f * (spec.max - spec.min) + 0.001f : 0.0f;
            CHECK(std::abs(*parsed - plain) <= tolerance);
        }
    }
}

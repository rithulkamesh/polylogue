#include "dsp/Chorus.h"
#include "dsp/ModMatrix.h"
#include "dsp/Oscillator.h"
#include "support/Analysis.h"
#include "support/EngineRender.h"
#include "support/VoiceRender.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

using namespace polylogue::dsp;
using namespace polylogue::test;

namespace {

constexpr int kFftSize = 8192;
constexpr double kFs = 48000.0;

std::vector<float> spectrum(std::span<const float> signal)
{
    Fft fft(kFftSize);
    std::vector<float> magnitudes(kFftSize / 2);
    fft.magnitudes(signal, magnitudes);
    return magnitudes;
}

double bin(int k)
{
    return static_cast<double>(k) * kFs / kFftSize;
}

}  // namespace

TEST_CASE("the sine oscillator is a pure tone at the requested pitch")
{
    Oscillator osc;
    osc.setWaveform(Waveform::Sine);
    osc.setFrequency(bin(350), kFs);
    osc.reset();
    std::vector<float> signal(kFftSize + 64);
    for (float& sample : signal)
        sample = osc.process();

    const auto magnitudes = spectrum(std::span<const float>(signal).subspan(64, kFftSize));
    CHECK(std::abs(magnitudes[350] - 1.0f) < 0.02f);
    for (std::size_t k = 1; k < magnitudes.size(); ++k) {
        if (k < 349 || k > 351)
            CHECK(magnitudes[k] < 1e-4f);
    }
}

TEST_CASE("phase modulation produces sidebands whose amplitudes follow the Bessel functions")
{
    // Carrier 57 bins, modulator 19 bins (ratio 3), index 1: sidebands at 57 +/- 19 n.
    constexpr double kIndex = 1.0;
    Oscillator carrier;
    Oscillator modulator;
    for (Oscillator* osc : {&carrier, &modulator})
        osc->setWaveform(Waveform::Sine);
    carrier.setFrequency(bin(57), kFs);
    modulator.setFrequency(bin(19), kFs);
    carrier.reset();
    modulator.reset();

    std::vector<float> signal(kFftSize + 100);
    for (float& sample : signal)
        sample = carrier.process(Oscillator::kNoReset,
                                 kIndex * static_cast<double>(modulator.process()));
    const auto magnitudes = spectrum(std::span<const float>(signal).subspan(100, kFftSize));

    // J0(1), J1(1), J2(1), J3(1)
    const double bessel[] = {0.76520, 0.44005, 0.11490, 0.01956};
    for (int order = 0; order <= 3; ++order) {
        for (int side : {-1, 1}) {
            const int k = 57 + side * 19 * order;
            if (k <= 0 || (order == 0 && side == -1))
                continue;
            INFO("order " << order << " bin " << k);
            CHECK(std::abs(static_cast<double>(magnitudes[static_cast<std::size_t>(k)]) -
                           bessel[order]) < 0.005 + 0.03 * bessel[order]);
        }
    }
}

TEST_CASE("zero modulation leaves the sine untouched")
{
    Oscillator plain;
    Oscillator modulated;
    for (Oscillator* osc : {&plain, &modulated}) {
        osc->setWaveform(Waveform::Sine);
        osc->setFrequency(440.0, kFs);
        osc->reset();
    }
    for (int i = 0; i < 1000; ++i)
        REQUIRE(plain.process() == modulated.process(Oscillator::kNoReset, 0.0));
}

namespace {

SynthSettings fmPatch(float index)
{
    SynthSettings s = plainSaw();
    s.syncRing = SyncRing::Fm;
    s.osc1Level = 1.0f;
    s.osc2Level = index;
    s.osc2Cents = 600.0f;  // modulator at the square root of two times the carrier
    return s;
}

double harmonicContent(const std::vector<float>& signal, double fundamentalHz)
{
    // Fraction of the energy that is not the fundamental.
    Fft fft(kFftSize);
    std::vector<float> magnitudes(kFftSize / 2);
    fft.magnitudes(std::span<const float>(signal).subspan(24000, kFftSize), magnitudes);
    const auto centre = static_cast<int>(std::lround(fundamentalHz * kFftSize / kFs));
    double total = 0.0;
    double fundamental = 0.0;
    for (int k = 1; k < kFftSize / 2; ++k) {
        const double power = static_cast<double>(magnitudes[static_cast<std::size_t>(k)]) *
                             static_cast<double>(magnitudes[static_cast<std::size_t>(k)]);
        total += power;
        if (std::abs(k - centre) <= 2)
            fundamental += power;
    }
    return 1.0 - fundamental / total;
}

}  // namespace

TEST_CASE("FM with no index is a pure sine at the played pitch")
{
    const auto out = renderNote(fmPatch(0.0f), {45, 1.0f, 1.5}, 1.5);
    const auto hz =
        zeroCrossingFrequency(std::span<const float>(out).subspan(24000, 24000), kSampleRate);
    REQUIRE(hz.has_value());
    CHECK(std::abs(centsBetween(*hz, 110.0)) < 1.0);
    CHECK(harmonicContent(out, 110.0) < 0.001);
}

TEST_CASE("raising the FM index fills the spectrum with inharmonic sidebands")
{
    const double low = harmonicContent(renderNote(fmPatch(0.03f), {45, 1.0f, 1.5}, 1.5), 110.0);
    const double high = harmonicContent(renderNote(fmPatch(0.8f), {45, 1.0f, 1.5}, 1.5), 110.0);
    CHECK(low < 0.2);
    CHECK(high > 0.6);
}

TEST_CASE("an envelope on the modulator level makes the strike bright and the tail pure")
{
    SynthSettings s = fmPatch(0.02f);
    s.ampEnv = {EnvelopeType::AD, 0.001f, 4.0f};
    s.modEnv = {EnvelopeType::AD, 0.001f, 0.6f};
    s.modEnvAmount = 0.9f;
    s.modEnvTarget = EnvelopeTarget::Level2;
    const auto out = renderNote(s, {45, 1.0f, 3.0}, 3.0);

    auto brightness = [&](double from, double to) {
        const auto part = std::span<const float>(out).subspan(
            static_cast<std::size_t>(from * kSampleRate),
            static_cast<std::size_t>((to - from) * kSampleRate));
        std::vector<float> diff(part.size() - 1);
        for (std::size_t i = 0; i + 1 < part.size(); ++i)
            diff[i] = part[i + 1] - part[i];
        return rms(diff) / rms(part);
    };
    CHECK(brightness(0.01, 0.15) > 2.5 * brightness(2.0, 2.8));
}

TEST_CASE("the mod envelope can move either oscillator level")
{
    SynthSettings s;
    s.modEnvAmount = 0.5f;

    ModMatrix matrix;
    matrix.prepare(kFs);

    s.modEnvTarget = EnvelopeTarget::Level1;
    matrix.configure(s);
    matrix.snap();
    ModOffsets offsets = matrix.evaluate({1.0f, 0.0f});
    CHECK(std::abs(offsets[index(ModDestination::Osc1Level)] - 0.5f) < 1e-5f);
    CHECK(offsets[index(ModDestination::Osc2Level)] == 0.0f);

    s.modEnvTarget = EnvelopeTarget::Level2;
    matrix.configure(s);
    for (int i = 0; i < 4800; ++i)
        offsets = matrix.evaluate({1.0f, 0.0f});
    CHECK(std::abs(offsets[index(ModDestination::Osc2Level)] - 0.5f) < 1e-3f);
    CHECK(std::abs(offsets[index(ModDestination::Osc1Level)]) < 1e-3f);
}

TEST_CASE("chorus at zero mix passes the input through untouched")
{
    Chorus chorus;
    chorus.prepare(kFs);
    chorus.setParameters(0.0f, 0.6f, 1.0f);
    std::mt19937 rng(3);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (int i = 0; i < 5000; ++i) {
        const float in = dist(rng);
        float left = 0.0f;
        float right = 0.0f;
        chorus.process(in, left, right);
        REQUIRE(left == in);
        REQUIRE(right == in);
    }
}

TEST_CASE("chorus widens a mono signal into stereo without changing its loudness much")
{
    Chorus chorus;
    chorus.prepare(kFs);
    chorus.setParameters(1.0f, 0.5f, 1.0f);
    std::mt19937 rng(8);
    std::uniform_real_distribution<float> dist(-0.5f, 0.5f);

    std::vector<float> in(48000);
    std::vector<float> left(in.size());
    std::vector<float> right(in.size());
    for (std::size_t i = 0; i < in.size(); ++i) {
        in[i] = dist(rng);
        chorus.process(in[i], left[i], right[i]);
    }

    REQUIRE(allFinite(left));
    REQUIRE(allFinite(right));
    CHECK(left != right);

    double dot = 0.0;
    for (std::size_t i = 4800; i < in.size(); ++i)
        dot += static_cast<double>(left[i]) * static_cast<double>(right[i]);
    const double correlation = dot / (rms(std::span<const float>(left).subspan(4800)) *
                                      rms(std::span<const float>(right).subspan(4800)) *
                                      static_cast<double>(in.size() - 4800));
    CHECK(correlation < 0.9);

    const double ratio = rms(std::span<const float>(left).subspan(4800)) / rms(in);
    CHECK(ratio > 0.5);
    CHECK(ratio < 1.6);
}

TEST_CASE("chorus depth changes the sound and every rate stays stable")
{
    for (double rate : {22050.0, 44100.0, 96000.0, 192000.0}) {
        Chorus deep;
        Chorus shallow;
        deep.prepare(rate);
        shallow.prepare(rate);
        deep.setParameters(1.0f, 2.0f, 1.0f);
        shallow.setParameters(1.0f, 2.0f, 0.0f);

        double difference = 0.0;
        const auto count = static_cast<std::size_t>(rate);
        for (std::size_t i = 0; i < count; ++i) {
            const float in = static_cast<float>(std::sin(0.05 * static_cast<double>(i)));
            float a[2];
            float b[2];
            deep.process(in, a[0], a[1]);
            shallow.process(in, b[0], b[1]);
            REQUIRE(std::isfinite(a[0]));
            REQUIRE(std::isfinite(a[1]));
            REQUIRE(std::abs(a[0]) < 3.0f);
            difference += std::abs(static_cast<double>(a[0] - b[0]));
        }
        CHECK(difference > 1.0);
    }
}

TEST_CASE("the engine's output is stereo only when the chorus is on")
{
    SynthSettings s = plainSaw();
    s.ampEnv = {EnvelopeType::AGD, 0.002f, 0.2f};

    auto channels = [&](float mix) {
        s.chorusMix = mix;
        Engine engine;
        engine.setSettings(s);
        engine.prepare(kSampleRate);
        std::vector<MidiEvent> notes = {MidiEvent::noteOn(0, 48, 0.8f)};
        std::vector<float> left(4800);
        std::vector<float> right(4800);
        engine.process(notes, left.data(), right.data(), 4800);
        return std::pair{left, right};
    };
    const auto [dryLeft, dryRight] = channels(0.0f);
    CHECK(dryLeft == dryRight);
    const auto [wetLeft, wetRight] = channels(0.8f);
    CHECK(wetLeft != wetRight);
    CHECK(peak(wetLeft) <= 1.0f);
}

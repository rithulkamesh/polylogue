#include "dsp/Oscillator.h"
#include "dsp/Tuning.h"
#include "support/Analysis.h"

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <vector>

using namespace polylogue::dsp;
using namespace polylogue::test;

namespace {

constexpr double kFs = 48000.0;

std::vector<float> render(Oscillator& osc, int count)
{
    std::vector<float> out(static_cast<std::size_t>(count));
    for (float& sample : out)
        sample = osc.process();
    return out;
}

Oscillator make(Waveform waveform, float shape, double hz)
{
    Oscillator osc;
    osc.setWaveform(waveform);
    osc.setShape(shape);
    osc.setFrequency(hz, kFs);
    return osc;
}

constexpr Waveform kWaveforms[] = {Waveform::Saw, Waveform::Triangle, Waveform::Square};

}  // namespace

TEST_CASE("oscillator pitch is exact across the keyboard")
{
    for (Waveform waveform : kWaveforms) {
        for (int note = 21; note <= 108; note += 12) {
            const double expected = midiToHz(note);
            Oscillator osc = make(waveform, 0.0f, expected);
            const auto signal = render(osc, static_cast<int>(kFs));

            const auto measured = zeroCrossingFrequency(signal, kFs);
            REQUIRE(measured.has_value());
            INFO("waveform " << static_cast<int>(waveform) << " note " << note);
            CHECK(std::abs(centsBetween(*measured, expected)) < 0.05);
        }
    }
}

TEST_CASE("oscillator output is bounded, finite and free of DC at every shape")
{
    for (Waveform waveform : kWaveforms) {
        for (float shape : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            for (double hz : {30.0, 440.0, 3520.0}) {
                Oscillator osc = make(waveform, shape, hz);
                const auto signal = render(osc, 48000);

                INFO("waveform " << static_cast<int>(waveform) << " shape " << shape << " hz "
                                 << hz);
                REQUIRE(allFinite(signal));
                CHECK(peak(signal) <= 1.15f);
                CHECK(std::abs(mean(signal)) < 0.02);
            }
        }
    }
}

TEST_CASE("oscillator repeats exactly when the period is a whole number of samples")
{
    // The first cycle is skipped: it lacks the residual of the wrap at t = 0.
    constexpr int kPeriod = 100;
    for (Waveform waveform : kWaveforms) {
        for (float shape : {0.0f, 0.4f, 1.0f}) {
            Oscillator osc = make(waveform, shape, kFs / kPeriod);
            const auto signal = render(osc, 40 * kPeriod);

            double worst = 0.0;
            for (std::size_t i = kPeriod; i + kPeriod < signal.size(); ++i)
                worst =
                    std::max(worst, static_cast<double>(std::abs(signal[i + kPeriod] - signal[i])));
            INFO("waveform " << static_cast<int>(waveform) << " shape " << shape);
            CHECK(worst < 1e-4);
        }
    }
}

TEST_CASE("band-limited waves alias far less than naive ones")
{
    constexpr int kFftSize = 8192;

    // Aliasing worsens with pitch; the limits follow the measured behaviour of the four-sample
    // kernel, about 25 dB better than a naive saw.
    struct Case {
        int fundamentalBin;
        double limitDb;
    };
    for (Case c : {Case{50, -42.0}, Case{200, -38.0}, Case{350, -34.0}}) {
        const double hz = c.fundamentalBin * kFs / kFftSize;

        Oscillator osc = make(Waveform::Saw, 0.0f, hz);
        const auto bandLimited = render(osc, kFftSize);

        std::vector<float> naive(kFftSize);
        double phase = 0.0;
        for (float& sample : naive) {
            sample = static_cast<float>(2.0 * phase - 1.0);
            phase += hz / kFs;
            phase -= std::floor(phase);
        }

        const double clean = nonHarmonicEnergyDb(bandLimited, kFftSize, c.fundamentalBin, 4);
        const double dirty = nonHarmonicEnergyDb(naive, kFftSize, c.fundamentalBin, 4);
        INFO("bin " << c.fundamentalBin << ": band-limited " << clean << " dB, naive " << dirty);
        CHECK(clean < c.limitDb);
        CHECK(dirty > clean + 15.0);
    }
}

TEST_CASE("square and triangle alias less than their naive forms")
{
    constexpr int kFftSize = 8192;
    constexpr int kBin = 200;
    const double hz = kBin * kFs / kFftSize;

    for (Waveform waveform : {Waveform::Square, Waveform::Triangle}) {
        Oscillator osc = make(waveform, 0.0f, hz);
        const auto signal = render(osc, kFftSize);

        std::vector<float> naive(kFftSize);
        double phase = 0.0;
        for (float& sample : naive) {
            sample = waveform == Waveform::Square
                         ? (phase < 0.5 ? 1.0f : -1.0f)
                         : static_cast<float>(phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
            phase += hz / kFs;
            phase -= std::floor(phase);
        }
        const double clean = nonHarmonicEnergyDb(signal, kFftSize, kBin, 4);
        const double dirty = nonHarmonicEnergyDb(naive, kFftSize, kBin, 4);
        INFO("waveform " << static_cast<int>(waveform) << ": " << clean << " vs naive " << dirty);
        CHECK(clean < dirty - 15.0);
    }
}

TEST_CASE("saw shape at maximum doubles the fundamental")
{
    constexpr int kFftSize = 8192;
    constexpr int kFundamentalBin = 350;
    Oscillator osc = make(Waveform::Saw, 1.0f, kFundamentalBin * kFs / kFftSize);
    const auto signal = render(osc, kFftSize);

    polylogue::dsp::Fft fft(kFftSize);
    std::vector<float> spectrum(kFftSize / 2);
    fft.magnitudes(signal, spectrum);
    CHECK(spectrum[kFundamentalBin] < 0.01f);
    CHECK(spectrum[2 * kFundamentalBin] > 0.3f);
}

TEST_CASE("square shape narrows the pulse until little energy remains")
{
    Oscillator wide = make(Waveform::Square, 0.0f, 220.0);
    Oscillator narrow = make(Waveform::Square, 1.0f, 220.0);
    CHECK(rms(render(narrow, 48000)) < 0.4 * rms(render(wide, 48000)));
}

TEST_CASE("hard sync locks the slave to the master period")
{
    constexpr int kMasterPeriod = 480;
    Oscillator master = make(Waveform::Saw, 0.0f, kFs / kMasterPeriod);
    Oscillator slave = make(Waveform::Saw, 0.0f, 3.37 * kFs / kMasterPeriod);

    std::vector<float> out(20 * kMasterPeriod);
    for (float& sample : out) {
        const double resetIn =
            master.wrapsNextSample() ? master.timeToWrap() : Oscillator::kNoReset;
        master.process();
        sample = slave.process(resetIn);
    }

    REQUIRE(allFinite(out));
    CHECK(peak(out) < 1.3f);
    double worst = 0.0;
    for (std::size_t i = kMasterPeriod; i + kMasterPeriod < out.size(); ++i)
        worst = std::max(worst, static_cast<double>(std::abs(out[i + kMasterPeriod] - out[i])));
    CHECK(worst < 1e-3);
}

TEST_CASE("reset makes output reproducible")
{
    Oscillator osc = make(Waveform::Triangle, 0.3f, 261.63);
    render(osc, 1000);
    osc.reset();
    const auto first = render(osc, 500);
    osc.reset();
    const auto second = render(osc, 500);
    CHECK(first == second);
}

TEST_CASE("frequencies beyond Nyquist stay finite")
{
    for (Waveform waveform : kWaveforms) {
        Oscillator osc = make(waveform, 0.5f, 60000.0);
        REQUIRE(allFinite(render(osc, 2048)));
    }
}

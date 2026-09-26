#include "dsp/Tuning.h"
#include "dsp/VoiceManager.h"
#include "support/Analysis.h"
#include "support/VoiceRender.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace polylogue::dsp;
using namespace polylogue::test;

namespace {

struct Rig {
    VoiceManager manager;
    SynthSettings settings = plainSaw();
    std::vector<float> scratch = std::vector<float>(kBlock, 0.0f);

    explicit Rig(int limit = 8)
    {
        settings.ampEnv = {EnvelopeType::AGD, 0.001f, 0.05f};
        manager.prepare(kSampleRate);
        manager.setVoiceLimit(limit);
    }

    // Advances the rig by roughly `seconds`, discarding the audio.
    void run(double seconds)
    {
        const int blocks = static_cast<int>(seconds * kSampleRate / kBlock) + 1;
        for (int i = 0; i < blocks; ++i) {
            std::fill(scratch.begin(), scratch.end(), 0.0f);
            manager.render(scratch.data(), kBlock, settings);
        }
    }

    bool isPlaying(int note) const
    {
        for (int i = 0; i < VoiceManager::kMaxVoices; ++i) {
            const auto info = manager.info(i);
            if (info.active && info.note == note)
                return true;
        }
        return false;
    }
};

}  // namespace

TEST_CASE("each note gets its own voice up to the limit")
{
    Rig rig(8);
    for (int i = 0; i < 8; ++i)
        rig.manager.noteOn(48 + i, 0.8f);
    rig.run(0.01);

    CHECK(rig.manager.activeVoiceCount() == 8);
    for (int i = 0; i < 8; ++i)
        CHECK(rig.isPlaying(48 + i));
}

TEST_CASE("note-off releases only its own voice, which then frees itself")
{
    Rig rig;
    rig.manager.noteOn(60, 0.8f);
    rig.manager.noteOn(64, 0.8f);
    rig.run(0.01);

    rig.manager.noteOff(60);
    rig.run(0.01);
    CHECK(rig.manager.activeVoiceCount() == 2);  // still in its release tail

    rig.run(0.3);
    CHECK(rig.manager.activeVoiceCount() == 1);
    CHECK(rig.isPlaying(64));
    CHECK_FALSE(rig.isPlaying(60));
}

TEST_CASE("repeating a held note retriggers its voice instead of adding one")
{
    Rig rig;
    rig.manager.noteOn(60, 0.8f);
    rig.run(0.01);
    rig.manager.noteOn(60, 0.8f);
    rig.run(0.01);
    CHECK(rig.manager.activeVoiceCount() == 1);
}

TEST_CASE("a released note that is played again reuses its own tail")
{
    Rig rig;
    rig.settings.ampEnv.decay = 0.5f;
    rig.manager.noteOn(60, 0.8f);
    rig.run(0.01);
    rig.manager.noteOff(60);
    rig.run(0.02);
    rig.manager.noteOn(60, 0.8f);
    rig.run(0.01);
    CHECK(rig.manager.activeVoiceCount() == 1);
}

TEST_CASE("stealing takes the oldest held voice when nothing is releasing")
{
    Rig rig(4);
    for (int note : {60, 62, 64, 65})
        rig.manager.noteOn(note, 0.8f);
    rig.run(0.02);

    rig.manager.noteOn(72, 0.8f);
    rig.run(0.02);

    CHECK(rig.manager.activeVoiceCount() == 4);
    CHECK(rig.isPlaying(72));
    CHECK_FALSE(rig.isPlaying(60));
    for (int note : {62, 64, 65})
        CHECK(rig.isPlaying(note));
}

TEST_CASE("stealing prefers a releasing voice over an older held one")
{
    Rig rig(4);
    rig.settings.ampEnv.decay = 1.0f;
    for (int note : {60, 62, 64, 65})
        rig.manager.noteOn(note, 0.8f);
    rig.run(0.02);
    rig.manager.noteOff(64);
    rig.run(0.02);

    rig.manager.noteOn(72, 0.8f);
    rig.run(0.02);

    CHECK(rig.isPlaying(60));
    CHECK(rig.isPlaying(62));
    CHECK(rig.isPlaying(65));
    CHECK(rig.isPlaying(72));
    CHECK_FALSE(rig.isPlaying(64));
}

TEST_CASE("stealing among releasing voices takes the quietest")
{
    Rig rig(2);
    rig.settings.ampEnv = {EnvelopeType::AGD, 0.001f, 1.0f};
    rig.manager.noteOn(60, 0.8f);
    rig.manager.noteOn(64, 0.8f);
    rig.run(0.02);
    rig.manager.noteOff(60);
    rig.run(0.3);
    rig.manager.noteOff(64);
    rig.run(0.02);

    // 60 has been fading far longer, so it is the quieter of the two.
    rig.manager.noteOn(72, 0.8f);
    rig.run(0.02);
    CHECK(rig.isPlaying(64));
    CHECK_FALSE(rig.isPlaying(60));
}

TEST_CASE("the sustain pedal holds released notes until it is lifted")
{
    Rig rig;
    rig.manager.setSustain(true);
    rig.manager.noteOn(60, 0.8f);
    rig.run(0.01);
    rig.manager.noteOff(60);
    rig.run(0.5);
    CHECK(rig.manager.activeVoiceCount() == 1);
    CHECK(rig.manager.info(0).held == true);

    rig.manager.setSustain(false);
    rig.run(0.4);
    CHECK(rig.manager.activeVoiceCount() == 0);
}

TEST_CASE("notes played and released with the pedal down retrigger rather than stack")
{
    Rig rig;
    rig.manager.setSustain(true);
    for (int i = 0; i < 3; ++i) {
        rig.manager.noteOn(60, 0.8f);
        rig.run(0.01);
        rig.manager.noteOff(60);
        rig.run(0.01);
    }
    CHECK(rig.manager.activeVoiceCount() == 1);
}

TEST_CASE("lowering the voice limit fades the excess voices out")
{
    Rig rig(8);
    for (int i = 0; i < 8; ++i)
        rig.manager.noteOn(48 + i, 0.8f);
    rig.run(0.02);
    REQUIRE(rig.manager.activeVoiceCount() == 8);

    rig.manager.setVoiceLimit(3);
    rig.run(0.02);
    CHECK(rig.manager.activeVoiceCount() == 3);

    for (int i = 0; i < 20; ++i)
        rig.manager.noteOn(70 + i, 0.8f);
    rig.run(0.02);
    CHECK(rig.manager.activeVoiceCount() == 3);
}

TEST_CASE("the voice limit is clamped to the pool")
{
    Rig rig;
    rig.manager.setVoiceLimit(1000);
    CHECK(rig.manager.voiceLimit() == VoiceManager::kMaxVoices);
    rig.manager.setVoiceLimit(-4);
    CHECK(rig.manager.voiceLimit() == 1);
}

TEST_CASE("all sound off silences every voice within a few milliseconds")
{
    Rig rig;
    for (int i = 0; i < 6; ++i)
        rig.manager.noteOn(50 + i, 0.8f);
    rig.run(0.02);
    rig.manager.allSoundOff();
    rig.run(0.005);
    CHECK(rig.manager.activeVoiceCount() == 0);
}

TEST_CASE("all notes off releases held and sustained notes")
{
    Rig rig;
    rig.manager.setSustain(true);
    rig.manager.noteOn(60, 0.8f);
    rig.manager.noteOn(64, 0.8f);
    rig.manager.noteOff(60);
    rig.run(0.01);
    rig.manager.allNotesOff();
    rig.run(0.4);
    CHECK(rig.manager.activeVoiceCount() == 0);
}

TEST_CASE("a stolen voice hands over without a click")
{
    SynthSettings s = plainSaw();
    s.osc1Wave = Osc1Wave::Triangle;
    s.cutoffHz = 400.0f;
    s.ampEnv = {EnvelopeType::AGD, 0.001f, 0.2f};

    VoiceManager manager;
    manager.prepare(kSampleRate);
    manager.setVoiceLimit(1);
    manager.noteOn(45, 0.8f);

    std::vector<float> out(static_cast<std::size_t>(kSampleRate * 0.4), 0.0f);
    const std::size_t stealAt = out.size() / 2;
    for (std::size_t pos = 0; pos < out.size(); pos += kBlock) {
        if (pos == stealAt - stealAt % kBlock)
            manager.noteOn(52, 0.8f);
        manager.render(out.data() + pos, kBlock, s);
    }

    float maxStep = 0.0f;
    for (std::size_t i = 1; i < out.size(); ++i)
        maxStep = std::max(maxStep, std::abs(out[i] - out[i - 1]));
    // A smooth low-passed triangle moves slowly; a click would jump by a large fraction of its
    // amplitude in one sample.
    CHECK(maxStep < 0.05f);
    CHECK(manager.info(0).note == 52);
}

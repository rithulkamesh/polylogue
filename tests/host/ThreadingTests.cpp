#include "host/HostTestSupport.h"
#include "support/Analysis.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <random>
#include <thread>

using namespace polylogue;
using namespace polylogue::test;

// Run under ThreadSanitizer (the `tsan` preset) this proves the audio thread and the message
// thread never race: the audio thread only reads atomics and writes to lock-free queues.
TEST_CASE("the audio thread and the message thread work at once without racing")
{
    Rig rig;
    std::atomic<bool> stop{false};
    std::atomic<long> blocks{0};
    std::atomic<bool> nonFinite{false};

    std::thread audio([&] {
        std::mt19937 rng(5);
        juce::AudioBuffer<float> buffer(2, 256);
        while (!stop.load()) {
            juce::MidiBuffer midi;
            const int note = 36 + static_cast<int>(rng() % 48);
            switch (rng() % 6) {
            case 0:
                midi.addEvent(juce::MidiMessage::noteOn(1, note, 0.8f), 10);
                break;
            case 1:
                midi.addEvent(juce::MidiMessage::noteOff(1, note), 20);
                break;
            case 2:
                midi.addEvent(juce::MidiMessage::controllerEvent(1,
                                                                 20 + static_cast<int>(rng() % 60),
                                                                 static_cast<int>(rng() % 128)),
                              5);
                break;
            case 3:
                midi.addEvent(juce::MidiMessage::pitchWheel(1, static_cast<int>(rng() % 16384)), 0);
                break;
            case 4:
                midi.addEvent(
                    juce::MidiMessage::controllerEvent(1, 64, static_cast<int>(rng() % 128)), 0);
                break;
            default:
                break;
            }
            buffer.clear();
            rig.processor.processBlock(buffer, midi);
            if (!allFinite({buffer.getReadPointer(0), 256}))
                nonFinite = true;
            ++blocks;
        }
    });

    std::mt19937 rng(9);
    std::vector<float> scopeCopy(512);
    juce::MemoryBlock state;
    const auto deadline = juce::Time::getMillisecondCounter() + 1500;
    int round = 0;
    while (juce::Time::getMillisecondCounter() < deadline) {
        for (const auto& spec : dsp::paramSpecs()) {
            if (rng() % 8 == 0)
                rig.processor.parameters().getParameter(spec.id)->setValueNotifyingHost(
                    static_cast<float>(rng() % 1001) / 1000.0f);
        }
        if (round % 7 == 0)
            rig.processor.presets().loadFactory(static_cast<int>(rng() % 30));
        if (round % 5 == 0) {
            rig.processor.getStateInformation(state);
            rig.processor.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
        }
        if (round % 3 == 0) {
            rig.processor.midiMapper().startLearning(
                static_cast<int>(rng() % host::MidiMapper::kSlotCount));
            rig.processor.midiMapper().bind(static_cast<int>(rng() % host::MidiMapper::kSlotCount),
                                            20 + static_cast<int>(rng() % 60));
        }
        rig.processor.queueKeyboardNote(40 + static_cast<int>(rng() % 40),
                                        rng() % 2 == 0 ? 0.7f : 0.0f);
        host::PolylogueProcessor::KeyEvent monitored;
        while (rig.processor.popMonitoredNote(monitored)) {
        }
        rig.processor.processPendingControlChanges();
        rig.processor.scope().readLatest(scopeCopy.data(), scopeCopy.size());
        (void)rig.processor.presets().isModified();
        (void)rig.processor.midiActivity();
        ++round;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    stop = true;
    audio.join();
    CHECK(blocks.load() > 50);
    CHECK_FALSE(nonFinite.load());
}

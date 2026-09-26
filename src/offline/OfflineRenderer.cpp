#include "offline/OfflineRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

namespace polylogue::offline {

StereoBuffer renderOffline(dsp::Engine& engine, std::span<const TimedEvent> events, double seconds,
                           double sampleRate, int blockSize)
{
    struct Scheduled {
        long sample;
        dsp::MidiEvent event;
    };
    std::vector<Scheduled> schedule;
    schedule.reserve(events.size());
    for (const TimedEvent& timed : events)
        schedule.push_back({std::lround(timed.seconds * sampleRate), timed.event});
    std::stable_sort(schedule.begin(), schedule.end(),
                     [](const Scheduled& a, const Scheduled& b) { return a.sample < b.sample; });

    const auto total = static_cast<long>(std::lround(seconds * sampleRate));
    StereoBuffer out{std::vector<float>(static_cast<std::size_t>(total)),
                     std::vector<float>(static_cast<std::size_t>(total))};

    std::vector<dsp::MidiEvent> block;
    std::size_t next = 0;
    for (long start = 0; start < total; start += blockSize) {
        const int count = static_cast<int>(std::min<long>(blockSize, total - start));

        block.clear();
        while (next < schedule.size() && schedule[next].sample < start + count) {
            dsp::MidiEvent event = schedule[next].event;
            event.offset = static_cast<int>(std::max(0L, schedule[next].sample - start));
            block.push_back(event);
            ++next;
        }
        engine.process(block, out.left.data() + start, out.right.data() + start, count);
    }
    return out;
}

}  // namespace polylogue::offline

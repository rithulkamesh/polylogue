#include "offline/WavWriter.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>

namespace polylogue::offline {
namespace {

void writeU16(std::ofstream& out, std::uint16_t value)
{
    const char bytes[2] = {static_cast<char>(value & 0xff), static_cast<char>(value >> 8)};
    out.write(bytes, 2);
}

void writeU32(std::ofstream& out, std::uint32_t value)
{
    writeU16(out, static_cast<std::uint16_t>(value & 0xffff));
    writeU16(out, static_cast<std::uint16_t>(value >> 16));
}

}  // namespace

bool writeWav(const std::filesystem::path& path, std::span<const float> interleaved, int channels,
              int sampleRate)
{
    std::ofstream out(path, std::ios::binary);
    if (!out)
        return false;

    const auto dataBytes = static_cast<std::uint32_t>(interleaved.size() * 2);
    const auto channelCount = static_cast<std::uint16_t>(channels);

    out.write("RIFF", 4);
    writeU32(out, 36 + dataBytes);
    out.write("WAVEfmt ", 8);
    writeU32(out, 16);
    writeU16(out, 1);
    writeU16(out, channelCount);
    writeU32(out, static_cast<std::uint32_t>(sampleRate));
    writeU32(out, static_cast<std::uint32_t>(sampleRate * channels * 2));
    writeU16(out, static_cast<std::uint16_t>(channels * 2));
    writeU16(out, 16);
    out.write("data", 4);
    writeU32(out, dataBytes);

    for (float sample : interleaved) {
        const float clamped = std::clamp(sample, -1.0f, 1.0f);
        writeU16(out, static_cast<std::uint16_t>(
                          static_cast<std::int16_t>(std::lround(clamped * 32767.0f))));
    }
    return static_cast<bool>(out);
}

}  // namespace polylogue::offline

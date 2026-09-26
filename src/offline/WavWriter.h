#pragma once

#include <filesystem>
#include <span>

namespace polylogue::offline {

// Writes interleaved float samples as 16-bit PCM. Returns false if the file cannot be written.
bool writeWav(const std::filesystem::path& path, std::span<const float> interleaved, int channels,
              int sampleRate);

}  // namespace polylogue::offline

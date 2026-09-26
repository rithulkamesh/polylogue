#pragma once

#include <cstdint>

namespace polylogue::dsp {

class Noise {
public:
    explicit Noise(std::uint32_t seed = 1) : state_(seed != 0 ? seed : 1) {}

    // White noise in [-1, 1).
    float next()
    {
        state_ ^= state_ << 13;
        state_ ^= state_ >> 17;
        state_ ^= state_ << 5;
        return static_cast<float>(static_cast<std::int32_t>(state_)) * (1.0f / 2147483648.0f);
    }

private:
    std::uint32_t state_;
};

}  // namespace polylogue::dsp

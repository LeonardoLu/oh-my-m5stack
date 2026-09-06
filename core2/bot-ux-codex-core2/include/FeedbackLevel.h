#pragma once

#include <stdint.h>

namespace corefeedback {

constexpr uint8_t kDefaultLevel = 3;
constexpr uint8_t kLevelCount = 6;

inline uint8_t normalizeLevel(uint8_t level)
{
    return level < kLevelCount ? level : kDefaultLevel;
}

// Level 3 preserves the shared synth's former default. The top level is safe
// because the synth retains its own 15,000 / 32,767 peak headroom.
inline uint8_t synthVolume(uint8_t level)
{
    static const uint8_t volumes[kLevelCount] = {0, 64, 120, 180, 220, 255};
    return volumes[normalizeLevel(level)];
}

} // namespace corefeedback

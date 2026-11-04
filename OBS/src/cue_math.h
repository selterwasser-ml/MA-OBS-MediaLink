#pragma once
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace maobs {
struct CueMath {
    // Cue mode values use 24-bit DMX, never 16-bit truncation.
    uint32_t start = 0;
    uint32_t target = 0;
    float progress = 1.0f;
    bool reached = true;
    bool small_range = false;

    void begin(uint32_t from, uint32_t to) {
        start = from; target = to;
        const int64_t delta = int64_t(start) - int64_t(target);
        small_range = std::llabs(delta) <= 16;
        reached = small_range;
        progress = small_range ? 1.0f : 0.0f;
    }
    float update(uint32_t raw) {
        if (reached) { progress = 1.0f; return progress; }
        const double delta = double(int64_t(target) - int64_t(start));
        if (std::abs(delta) <= 16.0) { reached = true; progress = 1.0f; return progress; }
        const double p = double(int64_t(raw) - int64_t(start)) / delta;
        progress = float(std::clamp(p, 0.0, 1.0));
        // One 24-bit fine-channel-sized tolerance (1024 ticks) for packet rounding.
        // Never retroactively un-complete when MA begins another fade before Lua update.
        if (p >= 1.0 || std::llabs(int64_t(raw) - int64_t(target)) <= 1024) {
            reached = true;
            progress = 1.0f;
        }
        return progress;
    }
};
}

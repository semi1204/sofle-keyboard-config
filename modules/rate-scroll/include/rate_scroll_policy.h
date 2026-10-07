/* SPDX-License-Identifier: MIT */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RATE_SCROLL_MAX_SCALE_PERCENT 125

static inline int rate_scroll_scale_percent(int64_t interval_ms, int previous_direction,
                                            int direction, bool has_previous,
                                            int previous_scale_percent) {
    if (!has_previous || previous_direction == 0 || direction == 0 ||
        previous_direction != direction) {
        return 100;
    }

    if (interval_ms <= 0) {
        if (previous_scale_percent < 100) {
            return 100;
        }
        return previous_scale_percent > RATE_SCROLL_MAX_SCALE_PERCENT
                   ? RATE_SCROLL_MAX_SCALE_PERCENT
                   : previous_scale_percent;
    }

    if (interval_ms >= 80) {
        return 100;
    }
    if (interval_ms <= 20) {
        return RATE_SCROLL_MAX_SCALE_PERCENT;
    }
    return 100 + (80 - interval_ms) * (RATE_SCROLL_MAX_SCALE_PERCENT - 100) / 60;
}

/* &msc adds every pressed binding to its current speed. Limit the combined
 * overlapping pulses to the requested target, rather than adding that target
 * again for every detent. */
static inline int32_t rate_scroll_clamp_increment(int32_t active_speed, int32_t requested) {
    if (requested > 0) {
        const int32_t available = requested - active_speed;
        return available > 0 ? available : 0;
    }

    if (requested < 0) {
        const int32_t available = requested - active_speed;
        return available < 0 ? available : 0;
    }

    return 0;
}

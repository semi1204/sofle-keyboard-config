/* SPDX-License-Identifier: MIT */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#define RATE_SCROLL_MAX_MULTIPLIER 3
#define RATE_SCROLL_MAX_ACTIVE_SPEED 16000

static inline int rate_scroll_multiplier(int64_t interval_ms, int previous_direction,
                                         int direction, bool has_previous,
                                         int previous_multiplier) {
    if (!has_previous || previous_direction == 0 || direction == 0 ||
        previous_direction != direction) {
        return 1;
    }

    if (interval_ms <= 0) {
        if (previous_multiplier < 1) {
            return 1;
        }
        return previous_multiplier > RATE_SCROLL_MAX_MULTIPLIER ? RATE_SCROLL_MAX_MULTIPLIER
                                                                 : previous_multiplier;
    }

    if (interval_ms >= 100) {
        return 1;
    }
    if (interval_ms >= 50) {
        return 2;
    }
    return RATE_SCROLL_MAX_MULTIPLIER;
}

static inline int32_t rate_scroll_clamp_increment(int32_t active_speed, int32_t requested) {
    if (requested > 0) {
        const int32_t available = RATE_SCROLL_MAX_ACTIVE_SPEED - active_speed;
        if (available <= 0) {
            return 0;
        }
        return requested < available ? requested : available;
    }

    if (requested < 0) {
        const int32_t available = -RATE_SCROLL_MAX_ACTIVE_SPEED - active_speed;
        if (available >= 0) {
            return 0;
        }
        return requested > available ? requested : available;
    }

    return 0;
}

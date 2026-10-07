#include <assert.h>

#include "rate_scroll_policy.h"

static void check_repeated_detents(int direction) {
    int32_t active_speed = 0;
    int previous_scale = 100;

    /* Reproduce the speed accumulation when many detents overlap. */
    for (int i = 0; i < 100; i++) {
        int scale = rate_scroll_scale_percent(i == 0 ? 0 : 10, direction, direction,
                                               i != 0, previous_scale);
        int32_t requested = direction * 20 * scale / 100;
        active_speed += rate_scroll_clamp_increment(active_speed, requested);
        assert(direction * active_speed <= 25);
        previous_scale = scale;
    }
    assert(active_speed == direction * 25);

    /* A slower detent cannot add more speed to a faster pulse still active. */
    assert(rate_scroll_clamp_increment(active_speed, direction * 20) == 0);
    active_speed -= direction * 20;
    active_speed += rate_scroll_clamp_increment(active_speed, direction * 20);
    assert(active_speed == direction * 20);
}

int main(void) {
    assert(rate_scroll_scale_percent(0, 0, 1, false, 100) == 100);
    assert(rate_scroll_scale_percent(100, 1, 1, true, 125) == 100);
    assert(rate_scroll_scale_percent(80, 1, 1, true, 125) == 100);
    assert(rate_scroll_scale_percent(79, 1, 1, true, 100) == 100);
    assert(rate_scroll_scale_percent(50, 1, 1, true, 100) == 112);
    assert(rate_scroll_scale_percent(20, 1, 1, true, 100) == 125);
    assert(rate_scroll_scale_percent(1, 1, 1, true, 100) == 125);
    assert(rate_scroll_scale_percent(0, 1, 1, true, 112) == 112);
    assert(rate_scroll_scale_percent(0, 1, 1, true, 200) == 125);
    assert(rate_scroll_scale_percent(0, 1, 1, true, 0) == 100);
    assert(rate_scroll_scale_percent(10, 1, -1, true, 125) == 100);

    int previous_scale = 100;
    for (int interval = 80; interval >= 1; interval--) {
        int scale = rate_scroll_scale_percent(interval, 1, 1, true, previous_scale);
        assert(scale >= previous_scale && scale <= 125);
        previous_scale = scale;
    }

    assert(rate_scroll_clamp_increment(0, 20) == 20);
    assert(rate_scroll_clamp_increment(20, 20) == 0);
    assert(rate_scroll_clamp_increment(20, 25) == 5);
    assert(rate_scroll_clamp_increment(25, 20) == 0);
    assert(rate_scroll_clamp_increment(0, -20) == -20);
    assert(rate_scroll_clamp_increment(-20, -25) == -5);
    assert(rate_scroll_clamp_increment(-25, -20) == 0);
    assert(rate_scroll_clamp_increment(0, 0) == 0);
    check_repeated_detents(1);
    check_repeated_detents(-1);
    return 0;
}

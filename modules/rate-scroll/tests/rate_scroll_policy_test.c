#include <assert.h>

#include "rate_scroll_policy.h"

int main(void) {
    assert(rate_scroll_multiplier(0, 0, 1, false, 1) == 1); /* first detent */
    assert(rate_scroll_multiplier(100, 1, 1, true, 4) == 1); /* idle reset */
    assert(rate_scroll_multiplier(75, 1, 1, true, 1) == 2);
    assert(rate_scroll_multiplier(49, 1, 1, true, 2) == 3);
    assert(rate_scroll_multiplier(24, 1, 1, true, 3) == 3);
    assert(rate_scroll_multiplier(0, 1, 1, true, 2) == 2); /* same-tick batch */
    assert(rate_scroll_multiplier(20, 1, -1, true, 4) == 1); /* direction reversal */

    assert(rate_scroll_clamp_increment(15900, 1000) == 100);
    assert(rate_scroll_clamp_increment(-15900, -1000) == -100);
    assert(rate_scroll_clamp_increment(16000, 1) == 0);
    assert(rate_scroll_clamp_increment(-16000, -1) == 0);
    assert(rate_scroll_clamp_increment(0, 120) == 120);

    return 0;
}

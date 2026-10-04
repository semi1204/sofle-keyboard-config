# Rate Scroll

`&rate_scroll` scales the vertical `&msc` speed from 1× to 4× using the time
between dispatched encoder detents. The runtime sensor behavior queue replaces
the original sensor timestamp, so this measures dispatch spacing rather than
the exact hardware detent time. `rsr_vol` uses `tap-ms = <0>` to avoid adding a
100 ms queue delay of its own.

Saved Studio/NVS encoder bindings can override keymap defaults. After flashing,
select the updated `rsr_vol` binding in Studio for each layer where accelerated
volume scrolling should apply.

# Rate Scroll

`&rate_scroll` scales the vertical `&msc` speed from 1× to 2× using the time
between dispatched encoder detents. The runtime sensor behavior queue replaces
the original sensor timestamp, so this measures dispatch spacing rather than
the exact hardware detent time. `rsr_vol` uses `tap-ms = <0>` to avoid adding a
100 ms queue delay of its own.

Saved Studio/NVS encoder settings can override keymap defaults. For each layer
where accelerated scrolling should apply, edit that encoder's runtime-rotate
binding in Studio: choose Rate Scroll with “Scroll Down 25” for clockwise and
“Scroll Up 25” for counter-clockwise, and set its tap delay to 0 ms.

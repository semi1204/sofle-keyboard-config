# Rate Scroll

`&rate_scroll` scales the vertical `&msc` speed smoothly from 1× to 1.25× using the time
between dispatched encoder detents. The runtime sensor behavior queue replaces
the original sensor timestamp, so this measures dispatch spacing rather than
the exact hardware detent time. `rsr_vol` uses `tap-ms = <0>` to avoid adding a
100 ms queue delay of its own. Overlapping 100 ms pulses share this speed
limit; repeated detents do not add their speeds together. Acceleration starts
below an 80 ms interval and reaches 1.25× at 20 ms or less. With the default
amount of 20, combined vertical speed is limited to 25.

Saved Studio/NVS encoder settings can override keymap defaults. For each layer
where accelerated scrolling should apply, edit that encoder's runtime-rotate
binding in Studio: choose Rate Scroll with “Scroll Down 20” for clockwise and
“Scroll Up 20” for counter-clockwise, and set its tap delay to 0 ms.

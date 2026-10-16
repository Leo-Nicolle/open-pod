---
aside: false
sidebar: false
pageClass: bench-live-page
---

# Scroll tuning

Tune fast scrolling by feel: how fast the wheel has to turn to shift up to jumps of 3, 2 or 1 letters instead of a row, and how long holding still shifts back down. The page talks to the Nucleo running the bench firmware (`pio run -e bench -t upload`) over Web Serial, like the [live bench](./bench-live), and sets the thresholds live with `#scrollcfg`. Without a board, <a href="?demo">try the demo</a>.

<ScrollConfig />

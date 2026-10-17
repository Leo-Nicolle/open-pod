# Tools

Small web tools made while building opentone. They all run in the browser, with nothing to install.

<div class="op-cards">
  <a class="op-card" href="./sync"><strong>opentone Sync</strong><span>Fill the SD card from your music folders, right in the browser: see what's new, convert what the player can't play, remove albums.</span></a>
  <a class="op-card" href="./clickwheel"><strong>Click wheel designer</strong><span>Generate a capacitive click wheel footprint (pads and edge cuts) as PNG for KiCad, and simulate how the finger position is interpolated.</span></a>
  <a class="op-card" href="./pinout"><strong>Pinout</strong><span>See which STM32F446 pin drives what on the final board (LQFP100) and on the Nucleo prototype (LQFP64).</span></a>
  <a class="op-card" href="./ui-builder"><strong>UI builder</strong><span>Preview every firmware screen, tweak the colour palette and export the sprites and theme headers.</span></a>
  <a class="op-card" href="./bench"><strong>Benchmarks</strong><span>Performance reports for each firmware version: scrolling, search, boot and audio playback measured on real hardware with a 20k-track library.</span></a>
  <a class="op-card" href="./bench-live"><strong>Live bench</strong><span>Connect to the bench board over USB: live buffer, CPU and latency charts, a remote control, and the bench scenarios run from the browser.</span></a>
  <a class="op-card" href="./memory-layout"><strong>Memory layout</strong><span>Every RAM and PSRAM allocation of the firmware, as an interactive map: sizes, owners, file and variable, drill down into the big buffers.</span></a>
  <a class="op-card" href="https://opentone.eu/userscripts/radiofrance-podcasts.user.js"><strong>Radio France podcast downloader</strong><span>A Tampermonkey userscript: on any Radio France podcast page, tick the episodes and download them into <code>~/Podcasts</code>, ready to sync.</span></a>
</div>

The Radio France downloader is a [userscript](https://opentone.eu/userscripts/radiofrance-podcasts.user.js):
install [Tampermonkey](https://www.tampermonkey.net/), open the link and confirm. It needs
Chrome or Edge (the File System Access API); Firefox/Safari fall back to a normal download.
Once the episodes are in <code>~/Podcasts</code>, add that folder as a library in
[opentone Sync](./sync) and it syncs to the card.

Full pin tables: [LQFP100 (final board)](./pinout-table-lqfp100) · [LQFP64 (Nucleo-F446RE)](./pinout-table).

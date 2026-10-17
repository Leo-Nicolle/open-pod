---
# https://vitepress.dev/reference/default-theme-home-page
layout: home

hero:
  name: "opentone"
  text: "An open-source mp3 player, built from scratch"
  tagline: "STM32 · FLAC · capacitive click wheel · Bluetooth. A music player with no smartphone, no cloud, no subscription. Work in progress, documented as it goes."
  image:
    src: /opentone_logo.svg
    alt: opentone logo
  actions:
    - theme: brand
      text: Read the blog
      link: /blog/
    - theme: alt
      text: Tools
      link: /tools/
    - theme: alt
      text: GitHub
      link: https://github.com/Leo-Nicolle/open-pod
---

<section class="op-section">

## In numbers

<p class="op-lead">Measured on the bench prototype (Nucleo-F446RE plus breakout boards). Each number comes from a blog post or a bench test.</p>

<div class="op-stats">
  <div class="op-stat">
    <div class="op-stat-value">61<span class="op-stat-unit">fps</span></div>
    <div class="op-stat-label">full-screen UI redraw on a 320×240 ILI9341, 16-bit parallel (183 fps for flat fills)</div>
  </div>
  <div class="op-stat">
    <div class="op-stat-value">48<span class="op-stat-unit">kHz / 24-bit</span></div>
    <div class="op-stat-label">FLAC playback, plus MP3, decoded by a VS1053b</div>
  </div>
  <div class="op-stat">
    <div class="op-stat-value">8<span class="op-stat-unit">MB</span></div>
    <div class="op-stat-label">PSRAM: ~38 s audio buffer, cover cache and the music index</div>
  </div>
  <div class="op-stat">
    <div class="op-stat-value">24<span class="op-stat-unit">clicks / turn</span></div>
    <div class="op-stat-label">capacitive click wheel (MPR121, 4 pads) with LRA haptic feedback</div>
  </div>
  <div class="op-stat">
    <div class="op-stat-value">180<span class="op-stat-unit">MHz</span></div>
    <div class="op-stat-label">STM32F446 Cortex-M4 running the UI, the SD card and the audio feed</div>
  </div>
  <div class="op-stat">
    <div class="op-stat-value">1.3<span class="op-stat-unit">MB/s</span></div>
    <div class="op-stat-label">SD card reads over SPI with 16 KB blocks</div>
  </div>
</div>

</section>

<section class="op-section">

## Specs

<p class="op-lead">What's inside today, and what the final iPod-sized board is planned to have.</p>

<table class="op-specs">
  <tbody>
    <tr>
      <th>MCU</th>
      <td>STM32F446 (Cortex-M4F, 180 MHz)<small>Prototype: Nucleo-F446RE (LQFP64). Final board: STM32F446VE (LQFP100).</small></td>
    </tr>
    <tr>
      <th>Display</th>
      <td>2.4" ILI9341 TFT, 320×240, RGB565<small>16-bit 8080 parallel bus driven straight from a GPIO port</small></td>
    </tr>
    <tr>
      <th>Audio</th>
      <td>VS1053b decoder: MP3 and FLAC up to 48 kHz / 24-bit, 3.5 mm jack<small>The PC sync tool converts hi-res files to 48 kHz / 16-bit</small></td>
    </tr>
    <tr>
      <th>Bluetooth</th>
      <td>Feasycom FSC-BT1035, A2DP source (SBC)<small>Audio goes from the decoder to the module over I2S; the MCU controls it with AT commands</small></td>
    </tr>
    <tr>
      <th>Storage</th>
      <td>microSD (SPI, 22.5 MHz) + 8 MB APS6404L PSRAM<small>The music index and search trie are built on a PC and loaded at boot</small></td>
    </tr>
    <tr>
      <th>Input</th>
      <td>Capacitive click wheel (MPR121) + 5 mechanical buttons<small>Design the wheel footprint with the <a href="./tools/clickwheel">click wheel designer</a></small></td>
    </tr>
    <tr>
      <th>Haptics</th>
      <td>DRV2605L driver + LRA motor</td>
    </tr>
    <tr>
      <th>Power</th>
      <td>Li-Po battery, USB-C charging, fuel gauge <span class="op-tag wip">planned</span><small>MCP73831 charger, MAX17048 gauge, power switches for SD / audio / Bluetooth</small></td>
    </tr>
  </tbody>
</table>

</section>

<section class="op-section">

## Status

<div class="op-status">
  <div>
    <h3>Works today <span class="op-tag ok">bench</span></h3>
    <ul>
      <li>Smooth MP3 and FLAC playback, buffered in PSRAM</li>
      <li>Fast UI: scrolling lists, fast scroll, transitions</li>
      <li>Album covers</li>
      <li>Library browsing (genres, artists, albums, tracks) and search</li>
      <li>Click wheel, buttons and haptics</li>
      <li>Bluetooth headphones: scan, pair, auto-reconnect</li>
      <li>Boot animation</li>
    </ul>
  </div>
  <div>
    <h3>Next <span class="op-tag wip">in progress</span></h3>
    <ul>
      <li>Battery, charging and power management</li>
      <li>USB file transfer</li>
      <li>Carrier PCB, then the iPod-sized board and enclosure</li>
      <li>Playlists, podcasts and audiobooks</li>
      <li>Accented characters in titles</li>
    </ul>
  </div>
</div>

</section>

<section class="op-section">

## Explore

<div class="op-cards">
  <a class="op-card" href="./manual/"><strong>User manual</strong><span>How to use an opentone. Coming once the hardware is final.</span></a>
  <a class="op-card" href="./tools/"><strong>Tools</strong><span>Click wheel designer, MCU pinout viewer and the UI builder.</span></a>
  <a class="op-card" href="./blog/"><strong>Blog</strong><span>Build log: screen, click wheel, FLAC, covers, Bluetooth…</span></a>
  <a class="op-card" href="./about"><strong>About</strong><span>Why build an iPod in 2026?</span></a>
</div>

</section>

<section class="op-section op-video">

## Current state

<Video src="/blog/10-10-state.mp4" aspectRatio="16/9"/>

</section>

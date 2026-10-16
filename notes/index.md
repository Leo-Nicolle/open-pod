# Notes summary — what each note file is for

The project keeps most of its written context as loose Markdown notes under
`docs/` (VitePress site, not published) and `.agents/`. This file is the index:
one short paragraph per note, plus pointers to the firmware sources under
`software/` that each note explains. Read the relevant note before touching the
code it references.

## Project overview & site

- `docs/about.md` — what OpenPod is and its part list (MCU, codec, display, SD,
  click wheel, battery). Partly dated: it still says ESP32 / VS1063 / ST7735,
  whereas the current hardware is STM32F446RE / VS1053b / ILI9341 (see the
  notes below). Read for intent, not current BOM.
- `docs/index.md` — VitePress homepage (hero + status video), not technical.
- `docs/roadmap.md` — short TODO: scrolling breadcrumb in the header, and
  non-ASCII/accented glyph support in the font pipeline (`scripts/font-converter.py`,
  `indexer/src/utils.ts::sanitizeName`, `software/src/ui/font_renderer.h`).
- `docs/notes.md` — loose link dump: Adafruit reference schematics, Kicad→3D
  tools, and LRA haptic motor/driver candidates (VG1030001XH). Haptics live in
  `software/src/` (DRV2605L driver).

## Hardware / pinout

- `docs/pinout-table.md` — **auto-generated** (do not hand-edit, source is
  `scripts/generate-pinout-md.mjs`): full STM32F446RE LQFP64 pin table with
  alternate functions and Nucleo-F446RE header mapping. The canonical pin
  reference when planning GPIO allocation.
- `docs/pinout.md` — thin VitePress page wrapping the interactive pinout viewer
  (`<PinoutStudio/>`); no content of its own.
- `docs/next-step-pinout.md` — plan for fitting the lock switch, power MOSFETs
  (backlight / SD+VS1053 / Bluetooth) and battery monitoring into the F446RE's
  pins. Verdict: fits with 0 spare. References `public/mapping.json`,
  `software/src/pinout.h`, `software/src/rendering/ILI9341_driver.h`. Read this
  before any pinout change.
- `docs/BT-pinout.md` — FSC-BT1035 module physical layout, quick wiring to the
  Nucleo, and full pin table. Cross-references `hardware/openpod/openpod.pretty/fsc-bt1035.kicad_mod`
  and the VS1053 I2S connection (GPIO4/6/7).
- `docs/new-nucleo-plan.md` — the strategic plan: Bluetooth is validated; buy a
  NUCLEO-F446ZE (LQFP144), design an "in-between" carrier PCB, then the
  iPod-sized final board. Contains the LQFP100 package recommendation and order
  of work. References `docs/recaps/bluetooth.md`, `docs/next-step-pinout.md`,
  `datasheets/lqfp100-pins.md`.

## Audio (VS1053b + FLAC)

- `docs/VS1053b_Audio_FIFO_Clocking_Buffer.md` — datasheet summary of the
  VS1053b FIFO (2048 B), DREQ, `SCI_CLOCKF` multipliers, SPI speed limits and
  streaming mode. Companion to the driver code.
- `docs/equilizer.md` — whether the VS1053b can have an EQ given the FLAC
  plugin: no room for a real multi-band EQ, use the ROM `SCI_BASS` bass/treble
  instead. Points at `software/src/sound/VS1053_driver.h:17` (`VS1053_REG_BASS`)
  and `VS1053_FLAC_plugin.h`. Also flags that `FLAC_README.md`'s "placeholder"
  claim is stale.
- `docs/boot-scratch-noise-problem.md` — diagnosis + fix for the boot "scratch"
  noise (VS1053 analog output collapsing on XRESET). Firmware fixes already
  applied in `software/src/sound/VS1053_driver.cpp` and `software/src/main.cpp`;
  remaining fixes are hardware (XRESET pull-up, mute/anti-pop stage), plus a
  list of schematic problems in `hardware/openpod/audio.kicad_sch`.
- `docs/recaps/vsli-chips.md` — VS1053b vs VS1063a vs VS1073a comparison:
  pin-compatible drop-ins, format support, power, RAM, and the CVDD 1.8V→1.25V
  caveat for VS1073a. The basis for any codec-chip swap decision.
- `docs/blog/09-10.md` — **the key audio architecture write-up**: smooth FLAC,
  bus speeds (SD/PSRAM/VS1053 measured), the feeder interrupt, the SPI1 bus
  lock, buffers/runway thresholds, and the breadboard crosstalk saga. Replaces
  older `memory-improvements.md`. This is the reference for `software/src/sound/`
  transfer speeds and buffer sizes.
- `software/src/sound/README.md` — overview of the enhanced VS1053b audio flow
  (clock config, 32-byte bursts, DREQ, PSRAM). Note: the "8KB FIFO" claim here
  conflicts with `docs/VS1053b_Audio_FIFO_Clocking_Buffer.md`'s 2048-byte FIFO.
- `software/src/sound/FLAC_README.md` — FLAC plugin framework; still labels the
  plugin data "placeholder" (out of date — the header holds the full 8208-word
  VLSI image, per `docs/equilizer.md`).

## Bluetooth

- `docs/recaps/bluetooth.md` — Feasycom module comparison for the A2DP-source
  role. Verdict: FSC-BT1035 (original) is EOL; use the footprint-compatible
  **FSC-BT1026x** family (pick C/D variants). Includes AT-command evidence that
  BT1026x supports A2DP source, and a pin-by-pin swap check. Firmware driver is
  `software/src/bluetooth/Bluetooth.hpp`, bring-up sketch `software/src/main_bt_test.cpp`.

- `.agents/bluetooth-design/design_handoff_openpod_bluetooth/README.md` — design
  handoff for the Bluetooth UI (open `Bluetooth UI.dc.html` next to it for the
  visual spec): header icon states (11×11 box at 270,10), signal bars, row kit,
  Settings › Bluetooth list, device screen, forget confirmation, rename, toasts,
  Now Playing codec tag, and the "what redraws" table. Implemented in the
  firmware:
  - `software/src/bluetooth/BtModel.{h,cpp}`: module state, AT line parsing,
    command queue, toasts, per-device alias/codec. Host-tested in
    `software/test/test_bluetooth.cpp`.
  - `software/src/bluetooth/Bluetooth.hpp`: UART driver (boot, I2S config, sends
    the model's commands).
  - `software/src/bluetooth/BtConfig.h`: aliases/codecs saved in
    `/openpod/bt.cfg`, written through MusicLookup's SdFat instance (not
    `FsFile::open(path)`, which uses whichever instance began last).
  - `software/src/state/bt_menu.{h,cpp}`: Settings/Bluetooth screen model (rows
    built on demand, selection kept in the route), and
    `software/src/state/text_entry.{h,cpp}`: letter-ribbon input shared with search.
  - `software/src/ui/menu_view.hpp` + `ui/bt_widgets.hpp`: drawing (chunk-hash
    redraws, icon box, toast). Wired in `ui/ui_engine.hpp`
    (`updateBluetooth()`) and `app/input.cpp` (`onMenuButtonClick()`).
  The same screens are previewed in the UI builder ("Bluetooth" section,
  `docs/.vitepress/theme/components/ui-builder/screens.ts`). RAM is tight: the
  first version took ~4 KB of static RAM, which left too little heap for the
  index loaders' 4 KB buffers (music index failed to load). Fixed by trimming the
  Bluetooth code to ~1.4 KB and making the loaders read 512-byte chunks into a
  stack buffer (`Music_lookup_load.cpp`, `Music_index.cpp`).

## MCU research

- `docs/recaps/NUCLEO-U5A5ZJ-Q.md` — reference notes for the STM32U5A5ZJT6Q
  (candidate replacement MCU): full 144-pin map, peripheral summary, low-power
  modes, and system-integration Q&A (does NOT replace the MCP73831 charger, the
  BT module, or the VS1053b DAC path).

## Display & UI

- `docs/clickwheel.md` — the click-wheel designer tool page and Kicad import
  instructions.
- `docs/ui-builder.md` — the UI builder tool page: export `sprites.h`,
  `theme.h`, `logo.h` (RGB565 arrays) into `software/src/ui/`.
- `.agents/screen-red-problem.md` — diagnosis record for the ILI9341 line
  artefacts + blue colour cast. Root causes found: `PC14`/`PC15` (D14/D15) dead
  as GPIO → `BROKEN_NUCLEO` build flag rewiring to `PA0`/`PA1`; then a double
  BGR/RGB swap in `theme.h` + `sprites.h`; then a macro-precedence bug in
  `theme.h`'s `RGB565()`. Rich trail of how the display bring-up was debugged.
- `.agents/LOGO_PIPELINE.md` — how `openpod_logo.h` is generated from
  `openpod_logo.py` (source of truth is the polygon list + palette, not the
  SVG), and how to theme the indexed sprites (swap the 16-entry palette, never
  the pixels). Firmware target: `software/src/ui/logo.h`.

## Plans & TODOs

- `.agents/TODO.md` — unprioritised idea backlog: UI/UX (settings, "goto",
  playlist system, shuffle), dev-logging macro, hardware power management,
  indexer as a Rust/Tauri app, podcasts/audiobooks (tree navigation, resume
  timestamps). A good first read when picking new work.
- `.agents/smooth-ui-and-playback-plan.md` — the phased audio-streaming plan
  (Phases 0–4, mostly done) with all measurements. Condensed companion to
  `docs/blog/09-10.md`; lists the exact files per phase
  (`AudioProfiler.h`, `AudioFifo.h`, `Spi1Bus.h`, `RefillPolicy.h`, `AudioYield.h`).
- `.agents/thumbnails-plan.md` — original plan for album-art thumbnail
  generation in the indexer (QOI vs raw565, `thumbs.bin` packing, sharp resize).
  Superseded in its details by `covers-plan.md`.
- `.agents/covers-plan.md` — album-cover thumbnails implementation plan +
  **status log**: PSRAM reallocation, the on-device benchmark (decision:
  **raw565**), indexer indexes (`track_to_album.bin`, `album_to_cover.bin`),
  firmware `software/src/storage/AlbumArt.h`, and UI wiring in
  `software/src/ui/nowplaying.hpp` + `ui_engine.hpp`. Phases 1–5 done.
- `notes/performance-and-memory.md` — (superseded by the paged `index.pack` +
  `PageStore`, see its status note) plan for a music index that scales to
  a full SD card. The whole index is copied into the 2 MB PSRAM music region at
  boot with **no bounds check** (overflow wraps onto the audio ring buffer), and
  at ~305–470 B/track it overflows at ~4.5–7k tracks. Steps: measure on a big
  dataset, bounds checks + indexer warning, read paths (`getTrackPath()`) and the
  search trie (`Music_index.cpp` PSRAM reads) from the SD card instead
  (→ ~57–75 B/track, ~28–37k tracks), then optional extra room (cover buffer,
  titles on SD, 16-bit ids). Firmware: `software/src/storage/Music_lookup*.cpp`,
  `Music_index.cpp`, `PSRAM_controller.hpp`; indexer: `indexer/src/`.
- `.agents/MP3 Player UI Design/design_handoff_openpod_title/README.md` — design
  handoff for the boot/title screen (logo + "openpod" wordmark, two intro
  animations, chunk wipe into the menu). Complements the boot-screen code in
  `software/src/ui/boot_screen.hpp`.

## Benchmarks

- `notes/benchmark-results.md` — analysis of the first full firmware bench run
  (6k-track card, all scenarios, background FLAC): 0 underruns, but a thin audio
  margin at a track start under load (24-bit FLAC holds ~1.5 s, per
  `software/src/sound/RefillPolicy.h`), audio work dominating the CPU, 850 ms
  screen transitions, the ~7,300-track PSRAM limit, and the bench's own
  measurement artifacts. Raw report and serial log in `benchmarks/`. The bench:
  `software/bench/`, `docs/tools/bench.md`.

## Firmware optimization

- `notes/stm32_production_optimization.md` — how to make dev-only hooks cost
  0 cycles in production builds. An empty function defined in another `.cpp`
  still costs ~4–8 Cortex-M cycles per call (`BL` + push + `BX LR`), because
  the compiler can't see across translation units. Three fixes: LTO (`-flto`
  in `software/platformio.ini`), an `inline` empty stub in the header behind
  `#ifdef`, or a no-op `((void)0)` macro. Relevant for bench/profiling hooks
  such as `software/src/sound/AudioProfiler.h` and `software/src/app/bench.cpp`.

## Software subsystems (READMEs)

- `software/src/storage/README.md` — the music index system: prefix trie in
  PSRAM, search API, and the (older) `Music_index` class. Note: this predates
  the current `Music_lookup`/`MusicLookup` API and `indexer/` output format
  (`music_index.bin`), which `docs/blog/01-07.md` describes.

## Blog (docs/blog/)

Chronological, mostly explanatory "how I built it" posts:
- `15-04.md` — designing the capacitive click wheel + the designer tool.
- `31-05.md` — wiring the ILI9341 (8080 16-bit parallel) to the Nucleo-F446RE.
- `01-06.md` — "vibecoding the UI", why Adafruit GFX was too slow, PSRAM
  questions.
- `03-06.md` — hardware scrolling (VSCRDEF/VSCRSADD), early double-buffering
  thoughts.
- `14-06.md` — fast list scrolling via 1/2/4-bit-per-pixel screen buffers.
- `01-07.md` — music indexing + search: relationship `.bin` files and the
  prefix trie (`music_index.bin`).
- `20-09.md` — album covers: Lanczos resize, QOI vs raw565 on-device benchmark
  (conclusion: raw565).
- `09-10.md` — smooth FLAC (see Audio section above; the most detailed).

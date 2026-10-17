---
aside: false
---

# Benchmarks

A hardware-in-the-loop test bench for the firmware. A Nucleo board with the real display, VS1053 and click wheel boots on a card holding a **large synthetic library** (20,000 tracks by default, with no audio files behind them) plus **one real sample per audio quality**. A script then drives it over serial, the way a user would: it scrolls, opens menus, searches, plays, seeks and skips tracks. It records how long each step took and what the audio buffer went through. Each firmware revision gets a report, and the reports are published below.

To watch a board live, drive it, or run the bench without the command line, see the [live bench](./bench-live). To compare any two reports, including ones not published yet, see [Compare reports](./bench-compare).

<BenchReport />

## What it measures

| Scenario | What it does | What it records |
|---|---|---|
| **boot** | Power-up to the first screen | Total boot time, library index load, search index load |
| **background** | Before anything else, starts the 16-bit/44.1 kHz FLAC sample on repeat (`#repeat 1`) | Its start time. It keeps playing through *scroll*, *navigation* and *search*, which are therefore measured with the audio feeder busy, and get restarted if it stopped |
| **scroll** | *Tracks*: 200 single-row ticks, then fast-scroll bursts of 24 rows, down and back | Time from wheel tick until the screen settles (p50/p95/max) |
| **navigation** | Artists → artist → album and back, Albums, Genres, at different depths | Time to open a list and to go back |
| **search** | Types eight queries, from `a` (thousands of hits) to `zz` (none), then scrolls the results | Time per keystroke (query plus redraw), hit counts |
| **startup** | Starts each sample, then hammers the UI for its first ~5 s: back through three screens, searches for `the` and `a`, opens *Tracks* and fast-scrolls | Underruns, lowest runway, longest main loop, and the runway second by second. The buffer is empty when a track starts: the riskiest moment for the audio. Compare with *playback*, which starts the same tracks idle |
| **playback** | Plays each sample for 15 s, seeks to 0:25, next, prev | Track start time, underruns, lowest buffer runway, feeder/SD CPU share, longest main loop |
| **stress** | Plays the 24-bit FLAC while scrolling the whole library hard for 25 s | Underruns and runway under UI load |

The inputs are injected into the click wheel's event queue, so they take exactly the path of a finger, haptics included. Latencies are measured on the device (from the moment the event is queued until no animation or transition is left to draw), so serial lag doesn't count.

The audio numbers come from the firmware's once-a-second monitor lines (`[prof …]` and `runway=… underrun=…`, see [Reading the diagnostics](/blog/09-10)).

### The samples

Generated with ffmpeg (chords over pink noise, so FLAC doesn't compress them away), then converted the way [opentone Sync](./sync) converts music for the card:

| Sample | On the card |
|---|---|
| mp3 64 kbit/s mono 22 kHz | as is |
| mp3 128 kbit/s, 320 kbit/s, V0 VBR | as is |
| FLAC 16-bit 44.1 kHz | as is |
| WAV 16-bit 44.1 kHz | FLAC |
| FLAC 24-bit 48 kHz | as is: the heaviest stream the player gets (~150 KB/s) |
| FLAC 24-bit 96 kHz | FLAC 16-bit 48 kHz |

### The library

`opentone bench-dataset` builds the index with the same code as a real sync. Its tracks are seeded fake names, so every run with the same seed gets the same library: about 13 tracks per album and 2 albums per artist, 40 genres, compilations, "feat." artists, "The …" names, accents, Cyrillic, Japanese and Greek, and titles too long for a row. The firmware only opens a track's file when it plays it, so browsing and search work without any audio. The samples are indexed as the album *opentone Bench Samples*.

## Running it

You need a spare microSD card (the bench replaces the card's index), a USB card reader, and the prototype on its Nucleo board.

```sh
cd software
npm install

# 1. With the card in the reader:
npm run bench -- prepare /media/$USER/SDCARD          # --tracks 50000 --seed 2 --covers
# 2. Unmount it and put it in the Nucleo, plugged in over USB. Then:
npm run bench -- run                                  # --port /dev/ttyACM0
```

`run` flashes the `bench` firmware (`pio run -e bench`), waits for it to boot, runs every scenario (about 6 minutes), and writes `bench/results/<revision>.json` together with the raw serial log. It also publishes the report to `docs/public/bench/`, which this page reads. Commit that folder to publish it.

| Option | |
|---|---|
| `--only scroll,search` | Run only some scenarios |
| `--no-flash` | Keep the firmware on the board (press reset when asked) |
| `--no-publish` | Write the report, but don't touch the docs |
| `--no-background` | Browse in silence, without the looping background sample |

Compare two revisions; it exits with an error on a regression above 10%, or on any new underrun:

```sh
npm run bench -- compare v0.4 v0.5-3-gabc123
```

Before burning a card, check that the dataset fits PSRAM and is searchable with the native tests: `OPENPOD_BENCH_CARD=/media/$USER/SDCARD npm test`.

::: warning Library size limit
The music index gets the last 2 MB of the 8 MB PSRAM (see `PSRAM_controller.hpp`), which holds about **7,000 tracks** with these names: at 6,000 the index ends at 7.99 MB, at 8,000 it already overflows. Nothing on the device checks for it yet (past the end, the PSRAM wraps around onto the audio buffer), so bench above that only once the index has more room.
:::

### The serial protocol

The `bench` build (`-DOPENPOD_BENCH=1`) adds these commands to the debug serial. Each answers one `#ok <cmd> …` or `#err <cmd> …` line; lines starting with `AT` still go to the Bluetooth module. Idle screen-off and the jump to Now Playing are disabled, so they don't interfere with a scenario.

| Command | Answer |
|---|---|
| `#click <select\|menu\|play\|next\|prev>`, `#long <button>` | Once the UI has settled: `us=` time it took |
| `#scroll <clicks>` | Same; negative clicks scroll down. One event of N clicks counts as N clicks at once for the fast-scroll speed, so a big one jumps by letters |
| `#spin <deg> <ms> [startDeg] [stillMs]` | Virtual finger: touches, turns `deg` (15° a click, positive = counter-clockwise = up) over `ms`, rests `stillMs` (100), lifts. `us=` from the end of the gesture to a settled UI, `events=` handled |
| `#hold <button[+button]> <ms>` | Virtual finger: holds the button(s). Same answer |
| `#gesture <ms>:<deg\|up>[+button] …` | Virtual finger: raw keyframes, e.g. `0:90 300:90 320:75 330:up`. Same answer |
| `#finger <deg\|up>`, `#btn <button> <1\|0>` | Live control (the live page's wheel): `#ok` at once |
| `#type <text>` | On the search screen: types it on the letter ribbon. `us=`, `max_us=` (slowest keystroke), `hits=` |
| `#clear` | Empties the search query |
| `#seek <seconds>` | Seeks the playing track, `us=` |
| `#repeat <0\|1>` | Loops the playing track at its end, instead of moving on |
| `#state` | Route, selection, highlighted row label, playing track, position, volume, search query and hits, and `fast=` (fast-scroll overlay), `letters=` (letters it shows), `key=` (the highlighted row's 3-letter fast-scroll key), `seek=` (seek mode), `screen=` (display on), `anim=` (a transition playing), `miss=` (index pages read from the card so far) |
| `#scrollcfg [s1=] [s2=] [s3=] [win=] [dwell=]` | Fast-scroll gears (`src/state/fast_scroll.h`): clicks per second to shift up to the 1st (3 letters), 2nd (2) and 3rd (first letters) gear, the speed window (clicks of travel each shift averages over), and how long holding still shifts a gear down (ms). Lifting the finger goes back to rows. Sets the given ones, answers all of them |
| `#cache [reset]` | Index page cache (`src/storage/PageStore.h`): `slots=`, `page=` bytes, `hits=`, `misses=`, `miss_us=` (time spent reading pages from the card) |
| `#dataset` | The card's `bench.json` and the counts the firmware loaded |
| `#stats`, `#version`, `#ping` | Uptime and free RAM, the git revision it was built from |

At the end of `setup()` it prints `#boot ms=… lookup_ms=… search_index_ms=… rev=…`.

Every wheel event the UI handles is echoed as `#ev <type> [b=<button>] [v=<clicks>] t=<board ms>`: `down`, `up`, `click`, `long`, `verylong`, `scroll`, `touch`, `untouch`. When list scrolling changes speed tier, `#ev fast l=<letters> v=<clicks/s>` (`l=0`: rows). That covers events from the real wheel, the virtual finger and `#click`/`#scroll`. While a finger is on the wheel, `#ev finger a=<deg>` follows it, at most 30 times a second (`a=-1` when lifted).

## Using the live bench

The [live bench](./bench-live) does the same from the browser, and shows the board as it runs.

1. Prepare a bench card (`npm run bench -- prepare <card>` in `software/`, see [Running it](#running-it)), put it in the Nucleo and flash the bench firmware: `pio run -e bench -t upload`.
2. Close anything else using the serial port (PlatformIO's monitor, `npm run bench -- run`): only one program can have it open. Then press **Connect** and pick the ST-Link.
3. The charts fill once a second from the firmware's audio monitor. Use the remote, the keyboard (arrows, Enter, Esc, Space), or the wheel itself; every input shows up under *Input to settled screen*.
4. **Run the bench** runs the scenarios here and compares the result with the latest published report. **Download report**, then `npm run bench -- import <file>` saves it to `bench/results/` and publishes it to the reports above.

On Linux, Web Serial needs the same permission as PlatformIO: your user in the `dialout` group (or a udev rule for the ST-Link). **Chromium installed as a snap** doesn't see serial ports at all until you allow it, then restart it:

```sh
sudo snap connect chromium:raw-usb
```

If the page connects but then reports the port as lost before receiving anything, the tty was left with `VMIN=0` by PlatformIO's monitor (or another pyserial tool), which Chrome before version 156 mistakes for a disconnect ([Chromium issue 559872992](https://github.com/Supporters-Of-Chromium-Based-Browsers/Bug-Bounty-Program/issues/144)). Reset it, then connect again:

```sh
stty -F /dev/ttyACM0 min 1 time 0
```

`npm run bench -- run` does this for you when it finishes.

| Chart | From | Look for |
|---|---|---|
| Buffer runway | `runway=` | How many seconds of audio are buffered ahead. Shaded seconds had underruns. |
| CPU time | `[prof …]` | Share of each second spent feeding the decoder (interrupt), reading the SD card, drawing the UI, and in the audio loop |
| Longest main loop | `maxLoop=` | The longest gap between two passes of the main loop: what makes the wheel feel late |
| Input to settled screen | `#ok … us=` | Time from an input until the screen stopped animating, per command |
| Stream rate | `rate=` | Bytes per second sent to the VS1053 |

## End-to-end tests

`#click` and `#scroll` hand the UI ready-made events. That's right for timing the UI, but it skips the wheel's own rules: 20 ms debounce, scroll clicks held 80 ms and dropped if the finger lifts meanwhile, long press at 800 ms, very long at 2000 ms, and the fast-scroll speed tiers.

The **virtual finger** (`#spin`, `#hold`, `#gesture`) stands in for the wheel's sensors instead: the angle the capacitive pads would read, and the button pins. Everything after that runs as with a real finger. The board plays the gesture itself, so USB timing doesn't blur a 300 ms flick. It hands the wheel back to the sensors 1.5 s after the finger lifts.

The tests live in `software/bench/e2e/*.spec.ts`. Each one drives the UI, then checks the state and the `#ev` events:

```sh
npm run bench -- e2e            # every test, on the board as it is
npm run bench -- e2e scroll     # tests whose name or group contains "scroll"
npm run bench -- e2e --flash    # flash the bench firmware first
```

The [live page](./bench-live) runs the same tests, shows every wheel event on a timeline, and its wheel works as a virtual finger. Drag around the ring to turn it, and hold a button as long as you like.

To add a test, add an entry to a spec file (or a new file listed in `e2e/index.ts`):

```ts
{
  group: "Scroll",
  name: "15° moves one row",
  async run(t) {
    await t.openRootItem("Tracks");
    const before = +(await t.state()).sel;
    await t.d.spin(-15, 150); // clockwise: down one row
    equal(+(await t.state()).sel, before + 1, "selected row");
  },
},
```

`t.waitFor(what, pred)` polls the state until something holds. `t.events()` / `t.eventsSince(t.mark())` return what the UI handled, with `sequence()` and `scrolled()` to compare them. `t.step(what)` labels a failure.

The bench scenarios keep `#click`/`#scroll`, so their numbers stay comparable across revisions. One scenario, **Fast scroll, real finger**, flicks the track list with `#spin`.

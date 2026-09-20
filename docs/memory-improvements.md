# Memory & SD-uptime improvements

Goal: minimize SD card up-time and make the best use of the 8 MB PSRAM chip,
iPod-style — preload aggressively in big bursts, then let the SD card go
idle for as long as possible, instead of the current continuous small-chunk
trickle. Background/analysis: see the audio-stream review in this
conversation; the findings below are ordered so each step is safe to ship on
its own.

## Status

§0-§4 are implemented in code. §5 (hardware SD power-switch) is intentionally
left as a future, separate hardware track. Files touched:

- `software/src/storage/PSRAM_controller.hpp` — fixed `PSRAM_SIZE`, added
  `NEXT_TRACK_BUFFER_BASE_ADDRESS`/`NEXT_TRACK_BUFFER_SIZE`, added
  static_asserts.
- `software/src/sound/Audio_buffer.h` / `.cpp` — hysteresis watermarks,
  chunk-capped `load()`, `getBufferedBytes()`, next-track prefetch API
  (`prepareNextTrack`/`prefetchNextTrack`/`consumeNextTrackIfMatches`/
  `invalidateNextTrack`), shared `_scratchBuffer`.
- `software/src/sound/AudioPlayer.h` / `.cpp` — `loop()` now gates refills on
  the low watermark and prefetches the next track otherwise;
  `startPlaying()` tries to adopt a matching prefetch before falling back to
  a fresh SD open; thin `prepareNextTrack`/`invalidateNextTrack` wrappers.
- `software/src/state/state.h` / `.cpp` — new `State::getNextTrackId()`,
  reusing the same `resolveTrackId` lookup `notifyTrackEnded` already used
  reactively, but callable proactively.
- `software/src/sound/player.cpp` — `handleTrackPlaybackStarted` now also
  resolves and prepares the next track's prefetch (or invalidates it at end
  of list) right after starting the current one.

Not done here (flagged, not blocking): the §0 audit of the *actual* PSRAM
footprint of a real, fully-populated `Music_index` wasn't performed — no
runtime bound currently exists on it (see note in §0 below), so this is
worth checking against a real library before shipping.

## 0. Fix the PSRAM_SIZE bug first (correctness, blocks everything else)

`software/src/storage/PSRAM_controller.hpp:6`:

```cpp
#define PSRAM_SIZE (64 * 1024 * 1024) // 64 Mb PSRAM chip size
```

The APS6404L is a 64-**Megabit** chip = **8 MB**, not 64 MB — `getCapacity()`
in the same file already returns the correct `8 * 1024 * 1024`. But
`MUSIC_INDEX_SIZE = PSRAM_SIZE - AUDIO_BUFFER_SIZE` uses the wrong constant,
so the music index thinks it owns ~58 MB of address space when only ~2 MB
physically exists past the 6 MB audio buffer. Any address past that real
boundary wraps around the physical 8 MB chip and silently corrupts the audio
ring buffer at address 0 — exactly the failure mode already called out in
the comment in `Audio_buffer.cpp` (`initializePSRAM()`) about the two
regions overlapping.

- [x] Change `PSRAM_SIZE` to `(8 * 1024 * 1024)`.
- [x] Add a compile-time assertion
      (`AUDIO_BUFFER_SIZE + NEXT_TRACK_BUFFER_SIZE < PSRAM_SIZE` in
      `PSRAM_controller.hpp`, plus `NEXT_TRACK_BUFFER_SIZE <= AUDIO_BUFFER_SIZE`
      in `Audio_buffer.h` guarding the no-wrap assumption in
      `consumeNextTrackIfMatches()`), so this class of bug fails to compile
      instead of silently corrupting audio.
- [ ] **Still open**: audit `Music_index`/`Music_lookup` for how large the
      on-PSRAM index actually gets with a full library. `MUSIC_INDEX_SIZE` is
      a documented budget, not an enforced bound — nothing in
      `Music_index.cpp` currently checks a write against it, so a large
      enough real library's index (which appears to be loaded fully into
      PSRAM at `MUSIC_INDEX_BASE_ADDRESS`, not streamed/paged from SD) could
      still silently overrun into the next-track prefetch/audio regions.
      Confirm it fits the real 2 MB remaining, or shrink `AUDIO_BUFFER_SIZE`/
      `NEXT_TRACK_BUFFER_SIZE` if it doesn't.

Do this before touching the buffering logic below: making the ring buffer
bigger/fuller (§2) makes this bug more likely to actually manifest.

## 1. Add hysteresis to the refill logic (the core behavior change)

Today, `Audio_buffer::load()` (`Audio_buffer.cpp:194`) refills the instant the
buffer drops below `AUDIO_TARGET_BUFFERED_BYTES` (256 KB), and
`AudioPlayer::loop()` calls `load()` on *every* main-loop tick. Net effect:
the SD card is touched in small 4 KB bursts (`AUDIO_PRELOAD_CHUNK_SIZE`)
continuously through playback — never idle for more than a fraction of a
second.

- [x] Added a second, lower watermark, `AUDIO_LOW_WATERMARK_BYTES`
      (`AUDIO_TARGET_BUFFERED_BYTES / 8`, ~12.5%) in `Audio_buffer.h`.
- [x] `AudioPlayer::loop()` now only calls `_buffer.load()` when
      `_buffer.getBufferedBytes() < AUDIO_LOW_WATERMARK_BYTES`; otherwise it
      spends that tick on `_buffer.prefetchNextTrack()` (§3) instead of
      touching the SD card at all.
- [x] Kept the existing seek-time bounded refill path
      (`AUDIO_SEEK_PRIME_BYTES`, `AudioPlayer::seekToSeconds`) untouched.
- [x] **Correction to this plan's original assumption**: simply raising the
      target did *not* automatically stay non-blocking - `load()` used to
      loop internally until the target was reached in one call, so a 4.5 MB
      target would have blocked the caller for up to ~1152 sequential 4 KB
      SD reads in a single call. Fixed by adding `AUDIO_MAX_CHUNKS_PER_LOAD_CALL`
      (4): `load()` now does at most 4 SD reads per call and relies on being
      re-invoked next tick to keep making progress, so a big refill still
      spreads across many ticks instead of stalling the UI once.

## 2. Raise the buffered target to actually use the reserved 6 MB

- [x] Raised `AUDIO_TARGET_BUFFERED_BYTES` to 4.5 MB, and resized
      `AUDIO_BUFFER_SIZE` itself from 6 MB to 5 MB in `PSRAM_controller.hpp`
      to make room for the new 1 MB `NEXT_TRACK_BUFFER_SIZE` region (§3)
      without growing past the physical 8 MB chip - `MUSIC_INDEX_BASE_ADDRESS`
      ends up at the same 6 MB offset as before, so the music index's actual
      location in PSRAM is unaffected.
- [ ] **Not verified on hardware** (no device available in this environment):
      confirm on real hardware that a 128 kbps MP3 actually coasts for
      several minutes between refills as expected, and that FLAC/high-bitrate
      files still behave reasonably (proportionally less idle time is
      expected, not a bug).
- [ ] **Not addressed**: `seekToSeconds`/`seekBySeconds` still call
      `resetRingBuffer()` unconditionally, discarding the whole (now much
      bigger, up to 4.5 MB) preloaded buffer on every seek. Still considered
      acceptable (seeks are user-driven and rare relative to playback time),
      but this got bigger with this change and doesn't yet have a comment
      calling out the trade-off at the call site - worth adding if seeking
      ever feels wasteful in practice.

## 3. Prefetch the next track ahead of time

`State::notifyTrackEnded` (`software/src/state/state.cpp:81`) only resolves
and starts the next track *after* the current one fully ends, even though
sequential next-track-id is knowable well before that point. Today every
track change costs a fresh burst of SD activity right at the transition.

- [x] `State::getNextTrackId()` (new, reuses the same `resolveTrackId`
      lookup `notifyTrackEnded` already used reactively) resolves the
      sequential next track proactively. `PodPlayer::handleTrackPlaybackStarted`
      calls it right after starting the current track and hands the result
      to `AudioPlayer::prepareNextTrack()`.
- [x] `AudioPlayer::loop()` calls `_buffer.prefetchNextTrack()` (one bounded
      `AUDIO_PRELOAD_CHUNK_SIZE` chunk per call) only on ticks where the
      current-track buffer is already healthy (above the low watermark),
      so it never competes with the current track's own refill for the SPI
      bus.
- [x] Sized the next-track preload at `NEXT_TRACK_BUFFER_SIZE` = 1 MB (linear
      region, not a ring - filled once from byte 0) - enough to smooth a
      transition, not a second full song buffer (8 MB budget, see §4).
- [x] Hand-off implemented as `Audio_buffer::consumeNextTrackIfMatches()`,
      called from `AudioPlayer::startPlaying()` before falling back to a
      fresh `setFileName()`: if the requested filename matches what was
      prefetched, it reopens that file at the resumed position (cheap - a
      directory lookup + seek, not a data re-read) and copies the
      already-prefetched bytes into the now-empty main ring buffer, so that
      portion needs no SD read at all.
- [x] Invalidation: `consumeNextTrackIfMatches()` always clears the prefetch
      slot (whether it matched or not), and `PodPlayer::handleTrackPlaybackStarted`
      calls `AudioPlayer::invalidateNextTrack()` when there's no next track
      (end of list) so a stale guess doesn't linger. Note: `AudioPlayer::forward()`/
      `rewind()` are still restart-the-current-file stubs (per their existing
      comments), not real track-skip - real track changes go through
      `State::startPlayback`/`EVENT_PLAYBACK_STARTED`, which is what's wired
      up here. If/when `forward()`/`rewind()` become real skip actions, wire
      `invalidateNextTrack()` there too if the new target doesn't match the
      current guess.

## 4. PSRAM memory budget — decide the split up front

Total PSRAM: 8 MB. After step 0's fix, budget explicitly instead of leaving
it implicit:

- [x] Split decided and encoded directly as the macros in
      `PSRAM_controller.hpp`: 5 MB current-track buffer (`AUDIO_BUFFER_SIZE`)
      + 1 MB next-track prefetch (`NEXT_TRACK_BUFFER_SIZE`) + 2 MB music
      index (`MUSIC_INDEX_SIZE`, now correctly computed - see §0's still-open
      audit item for whether a real library's index actually fits that).
- [x] Expectations documented (here and in the macro comments): this is
      enough for roughly one song fully resident plus a prefetched
      head-start on the next one — not "many songs" simultaneously resident
      like a RAM-heavy iPod. Getting closer to that would need a bigger
      PSRAM chip in a future hardware revision; not in scope here.

## 5. (Optional, hardware) True SD power-down

`pinout.h` only defines `CARDCS` (chip select) for the SD card — there's no
power-switch/load-switch GPIO on its supply rail. Software alone can only
stop clocking and deselect the card (SD spec's standby idle current — real
savings, but not zero).

- [ ] If chasing further power savings later: add a small P-MOSFET or load
      switch on the SD socket's 3.3V rail, gated by a spare GPIO, so
      firmware can fully cut SD power during the long idle windows §1-§3
      create, and re-enable + re-`begin()` the SD card right before the next
      scheduled burst refill.
- [ ] This is a hardware change (new component + PCB net), so track it
      separately from the firmware work above; not a blocker for §0-§4.

## Suggested order of work

1. §0 (bug fix) — safety, low effort, no behavior change to buffering itself. **Done.**
2. §1 + §2 together (hysteresis + bigger target) — the actual "idle for
   minutes instead of seconds" win, moderate effort, all in `Audio_buffer`. **Done.**
3. §3 (next-track prefetch) — bigger change, touches `AudioPlayer`/`state.cpp`
   interaction; do after §1/§2 are validated stable. **Done.**
4. §4 — just documentation/constants once §0-§3 land. **Done.**
5. §5 — separate hardware track, whenever a board revision is on the table. **Not started (hardware).**

## Remaining before calling this done

- **No hardware in this environment to test on.** All of the above compiles
  by inspection (matched against existing SdFat/PSRAM usage patterns
  elsewhere in the file) but hasn't been built or run on the actual STM32 +
  VS1053 + APS6404L hardware. Flash it and verify: normal playback, seeking,
  track-end transitions (both prefetched-and-matching and end-of-list), and
  that the SD card genuinely goes idle for extended stretches (e.g. probe
  `CARDCS`/SPI activity, or log timestamps around `SDtoPSRAM()` calls).
- The `Music_index` real-world size audit flagged in §0 - do this before
  trusting the 2 MB budget on a large library.
- `test/` only compiles `storage/*.cpp` for the native/doctest environment
  (`platformio.ini`'s `test_native` env) - none of this sound/ or state/ code
  has automated test coverage either before or after this change.

/*!
 * @file AudioPlayer.h
 * High-level audio player header with optimized FLAC support
 * OPTIMIZED FOR FLAC PLAYBACK - Uses efficient data feeding methods
 */

#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "Audio_buffer.h"
#include "AudioFifo.h"
#include "AudioYield.h"
#include "VS1053_driver.h"
#include <Arduino.h>
#include "pinout.h"

class AudioPlayer {
public:
  // Constructors
  AudioPlayer(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq, uint8_t sdCS);
  AudioPlayer(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t rst, uint8_t cs,
              uint8_t dcs, uint8_t dreq, uint8_t sdCS);
  ~AudioPlayer();

  // Core initialization and control
  // NOTE: spiFreq must NOT be 4000000 (Arduino_Core_STM32 3.x default): see
  // VS1053_driver::initSPI(). 1MHz is safe for VS1053 SCI (max ~ CLKI/7).
  bool begin(uint32_t spiFreq = 1000000UL, uint32_t sdFreq = 25000000UL);

  // PSRAM support
  bool enablePSRAM(bool enable = true);
  bool isPSRAMAvailable();

  // knownDurationSeconds, when > 0, comes from the indexer's precomputed
  // duration index and is forwarded to Audio_buffer::setFileName() to
  // short-circuit the on-device MP3-frame-scan duration guess.
  bool startPlaying(const char *filename, uint32_t knownDurationSeconds = 0);
  // Plays a short MP3 straight from memory (flash), no SD involved. It must
  // fit the fifo whole (AudioFifo::CAPACITY): the feeder interrupt then plays
  // it out without the main loop, which only has to call loop() to finish
  // the stream. stopPlaying() or startPlaying() cut it.
  bool playClip(const uint8_t *data, size_t len);
  // A clip still has bytes to send to the decoder.
  bool isClipPlaying() const { return _clipPlaying; }
  // endOfStream: the file was played to its end (the decoder plays out what
  // it holds) rather than stopped mid-track (it drops it right away).
  void stopPlaying(bool endOfStream = false);

  // Playback control
  void pausePlaying(bool pause = true);
  void resumePlaying();
  bool isPlaying();
  bool isPaused();
  void setLoop(bool loop);
  bool isLooping();

  // Seek support (approximate; see Audio_buffer::seekToSeconds)
  bool seekToSeconds(uint32_t targetSeconds);
  bool seekBySeconds(int deltaSeconds);

  // One-shot: true if the current track hit end-of-data since the last call.
  bool consumeTrackEnded();
  // True if the playing track's file could not be (re)opened: nothing more
  // will be read from it, the caller should stop playback.
  bool hasFailed() { return _playing && _buffer.openFailed(); }

  VS1053_driver &getDriver() { return _driver; }
  // Audio settings
  void setVolume(uint8_t left, uint8_t right);
  // Elapsed playback time in seconds (VS1053 hardware counter, reset on
  // every startPlaying()).
  uint16_t getDecodeTime();
  // Estimated total track length in seconds (0 if not yet known).
  uint32_t getDurationSeconds() { return _buffer.getDurationSeconds(); }
  void setPlaySpeed(uint16_t speed);
  uint16_t getPlaySpeed();

  // Advanced configuration
  bool configureAudioQuality(uint32_t targetSampleRate = 48000);

  // Next-track prefetch (see docs/blog/09-10.md, "Buffers"): call once the
  // current track starts, with whatever comes next (if known), so its
  // opening bytes get pulled into PSRAM ahead of time and the eventual
  // track change usually needs zero fresh SD activity.
  void prepareNextTrack(const char *filename, uint32_t knownDurationSeconds = 0);
  // Discards an in-progress next-track prefetch, e.g. on a user skip that
  // invalidates the sequential-next guess before it's ever consumed.
  void invalidateNextTrack();

  bool isFLACFile(const char *filename); // NEW: FLAC detection

  // Testing and diagnostics
  void sineTest(uint8_t freq, uint16_t duration);
  void dumpRegisters();
  bool testBasicPlayback();

  void loop();

  // Prints the [audio] buffer monitor line once a second.
  void setDebugMonitorEnabled(bool enabled) { _monitorEnabled = enabled; }

  // Call on user input / while the UI animates: background SD work (refill,
  // next-track prefetch) then waits USER_ACTIVITY_HOLDOFF_MS, unless the
  // buffer runs low (see loop()).
  void noteUserActivity() { _lastUserActivityMs = millis(); }



  // The L1 fifo's storage, as scratch for the link bench. Only valid while
  // nothing plays (the feeder reads it during playback).
  uint8_t *benchScratch() { return _fifo.storage(); }
  static const size_t BENCH_SCRATCH_LEN = AudioFifo::CAPACITY;

private:
  // Core objects
  VS1053_driver _driver;
  Audio_buffer _buffer;

  // State variables
  bool _playing;
  bool _paused;
  bool _looping;
  bool _trackEnded = false;
  bool _clipPlaying = false; // see playClip()

  // --- L1 fifo + feeder interrupt -------------------------------------------
  // The VS1053 is fed from a timer interrupt, out of the L1 fifo in MCU RAM,
  // so playback no longer depends on how often the main loop comes around
  // (the chip's own buffer is only ~16 ms of FLAC, 12 KiB with the FLAC
  // plugin). The main loop keeps the fifo topped up from PSRAM (or straight
  // from SD while PSRAM is empty, see Audio_buffer::setFrontFifo()).
  AudioFifo _fifo;
  HardwareTimer *_feederTimer = nullptr;
  // On while a stop sends its end fill bytes, or playing and not paused.
  volatile bool _feederEnabled = false;
  void updateFeeder() {
    _feederEnabled = stopping() || _clipPlaying || (_playing && !_paused);
  }
  static void feederIsr();
  static void yieldHook(); // see AudioYield.h
  // Diagnostic: -DOPENPOD_FEED_MAIN_LOOP feeds from loop() instead of the
  // timer interrupt (see feedFromMainLoop()).
#ifdef OPENPOD_FEED_MAIN_LOOP
  static const bool FEED_FROM_MAIN_LOOP = true;
#else
  static const bool FEED_FROM_MAIN_LOOP = false;
#endif
  void feedFromMainLoop();
  static const uint32_t FEEDER_HZ = 1000;
  // The most one tick may send, ~0.75 ms of SPI so a tick stays under the
  // 1 ms period: 1 KB at 11.25 MHz, 512 B with the 5.6 MHz breadboard cap
  // (OPENPOD_FEEDER_SDI_MAX_HZ in platformio.ini). Either way 500+ KB/s of
  // feeding capacity, against ~90-200 KB/s for FLAC.
#if defined(OPENPOD_FEEDER_SDI_MAX_HZ) && OPENPOD_FEEDER_SDI_MAX_HZ > 0 &&    \
    OPENPOD_FEEDER_SDI_MAX_HZ < 11250000
  static const size_t FEEDER_MAX_BYTES_PER_TICK = 512;
#else
  static const size_t FEEDER_MAX_BYTES_PER_TICK = 1024;
#endif
  // Per main-loop pass: PSRAM -> fifo moves at most this much (~12 ms).
  static const size_t FIFO_TOPUP_MAX_BYTES = 16 * 1024;
  // Feeder counters (interrupt writes, monitor reads and resets).
  volatile uint32_t _isrUnderruns = 0; // chip wanted data, fifo empty
  volatile uint32_t _isrBusySkips = 0; // tick skipped: SPI1 lock held
  // Longest run of consecutive ticks where DREQ was high and nothing was
  // sent (the chip's own buffer draining); see feederIsr().
  volatile uint32_t _isrStallTicks = 0;
  volatile uint32_t _isrMaxStall = 0;
  // Longest time between two feeder ticks (should be ~1 ms).
  volatile uint32_t _isrLastTickCycles = 0;
  volatile uint32_t _isrMaxTickGapCycles = 0;
  // The decoder was fed a stream that hasn't been stopped yet (see
  // beginStop()); stopping when it's clean skips the stop sequence, e.g.
  // stopping twice in a row on a track change.
  bool _decoderDirty = false;

  // --- Asynchronous stop (datasheet 10.5.1 / 10.5.2) -------------------------
  // The end fill bytes go through the fifo and the feeder; loop() advances
  // the sequence (serviceStop()) instead of blocking ~100 ms on every track
  // change. Meanwhile the next track primes into PSRAM, and only reaches
  // the fifo once the decoder is clear (onDecoderReady()).
  //  - end of stream: DrainThenCancel (fill bytes play out what the decoder
  //    holds) -> Cancelling -> done
  //  - mid-track: Cancelling (drop it right away) -> Flushing -> done
  enum class StopPhase : uint8_t { None, DrainThenCancel, Cancelling, Flushing };
  volatile StopPhase _stopPhase = StopPhase::None;
  bool _stopEndOfStream = false;
  uint8_t _stopFillByte = 0;
  uint32_t _stopFillBytes = 0;  // 2052, or 12288 for the FLAC plugin
  uint32_t _stopQueued = 0;     // fill bytes queued while cancelling
  unsigned long _stopStartMs = 0;
  unsigned long _stopLastPollMs = 0;
  bool stopping() const { return _stopPhase != StopPhase::None; }
  void beginStop(bool endOfStream);
  void serviceStop();
  void abortStop();
  void stopDone();
  void onDecoderReady();
  // Blocks until a stop in progress completes (bounded by its deadline).
  void finishStop();
  static const unsigned long END_FILL_TIMEOUT_MS = 1000;
  // The current stream goes through the FLAC plugin (see beginStop()).
  bool _streamIsFlac = false;
  static const int FLAC_END_FILL_BYTES = 12288;
  uint32_t _seekOffsetSeconds = 0;

  // Volume ducking around a seek (datasheet 10.5.4 recommends lowering
  // volume during fast forward/rewind to mask the decoder resync).
  bool _duckingForSeek = false;
  uint16_t _savedVolumeRaw = 0;
  unsigned long _duckRestoreAt = 0;
  static const uint8_t SEEK_DUCK_ATTENUATION = 20; // 0.5dB steps => ~10dB
  static const unsigned long SEEK_DUCK_RESTORE_DELAY = 150; // ms
  void restoreVolumeFromDuck();

  // --- Audio buffer starvation monitoring -----------------------------------
  // Counters accumulate every loop tick, but a compact summary is only
  // printed every AUDIO_MONITOR_INTERVAL_MS - no per-tick serial spam.
  static const unsigned long AUDIO_MONITOR_INTERVAL_MS = 1000;
  bool _monitorEnabled = false; // see setDebugMonitorEnabled()
  uint32_t _monitorMinRunwayMs = UINT32_MAX;
  size_t _monitorMinBuffered = (size_t)-1;
  size_t _monitorMaxBuffered = 0;
  unsigned long _monitorLastReport = 0;
  // Timing (per window): time in load()/prefetchNextTrack(), time moving
  // PSRAM -> fifo, and the largest gap between loop() calls.
  uint32_t _monitorLoadUs = 0;
  uint32_t _monitorFillUs = 0;
  uint32_t _monitorMinFifo = AudioFifo::CAPACITY;
  unsigned long _monitorLoopMaxMs = 0;
  unsigned long _monitorPrevLoopMs = 0;

  // Refill latch and pacing: see RefillPolicy.h.
  bool _refilling = false;
  unsigned long _lastUserActivityMs = 0;
  unsigned long _lastBusyChunkMs = 0;
  static const unsigned long USER_ACTIVITY_HOLDOFF_MS = 600;

  // Stream rate, for the runway (see streamBytesPerSecond()).
  volatile uint32_t _isrBytesSent = 0; // all bytes sent to the VS1053
  uint32_t _measuredBytesPerSecond = 0;
  unsigned long _rateWindowStartMs = 0;
  uint32_t _rateWindowStartBytes = 0;
  bool _rateWindowValid = false;
  uint32_t streamBytesPerSecond();
  void updateRateEstimate(unsigned long nowMs);
  void resetRateEstimate();
  void resetMonitorWindow(unsigned long nowMs);

  // Read synchronously before a track starts (straight into the fifo, PSRAM
  // being empty): ~270 ms of CD-quality FLAC, ~25 ms of SD time. loop()'s
  // refills take over from there.
  static const size_t AUDIO_PRIME_BYTES = 24 * 1024;
  static const unsigned long AUDIO_PRIME_TIMEOUT_MS = 5000;

  static AudioPlayer *_instance;
};

#endif // AUDIOPLAYER_H
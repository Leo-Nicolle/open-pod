/*!
 * @file AudioPlayer.h
 * High-level audio player header with optimized FLAC support
 * OPTIMIZED FOR FLAC PLAYBACK - Uses efficient data feeding methods
 */

#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include "Audio_buffer.h"
#include "VS1053_driver.h"
#include <Arduino.h>
#include "pinout.h"
// Interrupt types
#define AUDIOPLAYER_PIN_INT 1
#define AUDIOPLAYER_TIMER_INT 2

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
  bool
  startPlayingSimple(const char *filename); 
  bool startPlayingEnhanced(
      const char *filename); 
  bool playFile(const char *filename);
  void stopPlaying();

  // Playback control
  void pausePlaying(bool pause = true);
  void resumePlaying();
  bool isPlaying();
  bool isPaused();
  void setLoop(bool loop);
  bool isLooping();
  void forward();
  void rewind();

  // Seek support (approximate; see Audio_buffer::seekToSeconds)
  bool seekToSeconds(uint32_t targetSeconds);
  bool seekBySeconds(int deltaSeconds);

  // One-shot: true if the current track hit end-of-data since the last call.
  bool consumeTrackEnded();

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

  // Next-track prefetch (see memory-improvements.md §3): call once the
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

  // Interrupt and processing control
  bool useInterrupt(uint8_t type);
  void loop();

  // Off by default: the periodic starvation-monitor Serial.printf() (see
  // AUDIO_MONITOR_INTERVAL_MS below) is bigger than the STM32 core's 64-byte
  // TX ring buffer (Serial.h SERIAL_TX_BUFFER_SIZE), so printing it every
  // second costs a real, blocking write() stall once a second - on every
  // codec, not just FLAC. Enable only while actively debugging the feed path
  // (see flac-feed-problem.md).
  void setDebugMonitorEnabled(bool enabled) { _monitorEnabled = enabled; }

  // OPTIMIZED: Buffer management methods
  bool primeBuffer();
  bool primeBufferEnhanced(bool isHighQuality);
  bool feedBuffer();
  // Feeds the VS1053 from the DREQ interrupt: reads one SDI burst (32 bytes)
  // out of the PSRAM ring buffer and sends it. Kept minimal - no locks, no
  // EOF handling - the main loop owns refill + end-of-track detection.
  void feedFromISR();
  void sendEndFillSequence();

  // ISR methods
  static void timerISR();
  static void pinISR();

private:
  // Core objects
  VS1053_driver _driver;
  Audio_buffer _buffer;

  // State variables
  bool _playing;
  bool _paused;
  bool _looping;
  bool _usingInterrupts;
  bool _needsFeeding;
  uint8_t _interruptType;
  bool _trackEnded = false;
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
  uint32_t _monitorUnderflows = 0; // feedBuffer() hit an empty ring buffer
  uint32_t _monitorLowCount = 0;   // ticks spent below the low watermark
  size_t _monitorMinBuffered = (size_t)-1;
  size_t _monitorMaxBuffered = 0;
  unsigned long _monitorLastReport = 0;
  // Timing (per window): time in load()/prefetchNextTrack(), time in the
  // feed while-loop, and the largest gap between loop() calls (reveals how
  // fast the whole main loop is actually running).
  uint32_t _monitorLoadUs = 0;
  uint32_t _monitorFeedUs = 0;
  unsigned long _monitorLoopMaxMs = 0;
  unsigned long _monitorPrevLoopMs = 0;

  // Bound the feed loop so the main loop stays responsive. The VS1053's DREQ
  // stays high for a long time at the 4.5x FLAC clock, so an unbounded feed
  // loop blocks the UI for seconds. Feeding ~100ms per tick keeps the UI
  // responsive while the (healthy) ring buffer absorbs any transient shortfall.
  static const unsigned long AUDIO_MAX_FEED_MS = 100;

  // Bounded prime: buffer this many bytes before flipping _playing, so the
  // VS1053 has slack from sample one instead of stuttering while the
  // per-tick refill catches up (see memory-improvements.md).
  static const size_t AUDIO_PRIME_BYTES = 512 * 1024; // ~4s of 44.1k/16 FLAC
  static const unsigned long AUDIO_PRIME_TIMEOUT_MS = 5000;

  // Feed the VS1053 from a larger RAM buffer rather than 32 bytes at a time.
  // sendData() still bursts to the chip in 32-byte SDI chunks (datasheet
  // limit), but readData() then pulls this many bytes out of the PSRAM ring
  // buffer in one transaction instead of dozens - the 8 MHz SPI PSRAM has
  // heavy per-transaction overhead and 32-byte reads were leaving it
  // saturated (see memory-improvements.md §monitoring).
  static const size_t AUDIO_FEED_BUFFER_LEN = 512;

  // Buffer for audio data
  uint8_t _feedBuffer[AUDIO_FEED_BUFFER_LEN];
  // Small dedicated buffer used by the DREQ ISR (one 32-byte SDI burst).
  uint8_t _isrFeedBuffer[32];

  // Static instance for ISR access
  static AudioPlayer *_instance;

  // Internal methods
  void handleInterrupt();
};

#endif // AUDIOPLAYER_H
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

  bool startPlaying(const char *filename);
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

  bool isFLACFile(const char *filename); // NEW: FLAC detection

  // Testing and diagnostics
  void sineTest(uint8_t freq, uint16_t duration);
  void dumpRegisters();
  bool testBasicPlayback();

  // Interrupt and processing control
  bool useInterrupt(uint8_t type);
  void loop();

  // OPTIMIZED: Buffer management methods
  bool primeBuffer();
  bool primeBufferEnhanced(bool isHighQuality);
  bool feedBuffer();
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

  // Buffer for audio data
  uint8_t _feedBuffer[AUDIO_DATABUFFERLEN];

  // Static instance for ISR access
  static AudioPlayer *_instance;

  // Internal methods
  void handleInterrupt();
};

#endif // AUDIOPLAYER_H
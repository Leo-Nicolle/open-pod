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
  bool begin(uint32_t spiFreq = 4000000UL, uint32_t sdFreq = 25000000UL);

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

  VS1053_driver &getDriver() { return _driver; }
  // Audio settings
  void setVolume(uint8_t left, uint8_t right);
  uint16_t getDecodeTime();
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
  void feedBuffer();
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

  // Buffer for audio data
  uint8_t _feedBuffer[AUDIO_DATABUFFERLEN];

  // Static instance for ISR access
  static AudioPlayer *_instance;

  // Internal methods
  void handleInterrupt();
};

#endif // AUDIOPLAYER_H
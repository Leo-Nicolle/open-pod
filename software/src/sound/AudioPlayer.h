/*!
 * @file AudioPlayer.h
 * High-level audio player with playback control methods
 */

#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <Arduino.h>
#include "VS1053_driver.h"
#include "Audio_buffer.h"

#define FAST_ISR

// Interrupt types
#define AUDIOPLAYER_TIMER_INT 255
#define AUDIOPLAYER_PIN_INT 5

/*!
 * @brief High-level audio player class with playback control
 */
class AudioPlayer {
public:
  AudioPlayer(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq, uint8_t sdCS);
  AudioPlayer(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t rst, 
              uint8_t cs, uint8_t dcs, uint8_t dreq, uint8_t sdCS);
  ~AudioPlayer();

  // Initialization
  bool begin(uint32_t spiFreq = 1000000, uint32_t sdFreq = 25000000);
  bool enablePSRAM(bool enable = true);
  bool isPSRAMAvailable();

  // Playback control
  bool startPlaying(const char* filename, bool preload = false);
  bool playFile(const char* filename, bool preload = false);
  void stopPlaying();
  void pausePlaying(bool pause = true);
  void resumePlaying();
  bool isPlaying();
  bool isPaused();
  
  // Playback options
  void setLoop(bool loop);
  bool isLooping();
  void forward();
  void rewind();
  
  // Volume and settings
  void setVolume(uint8_t left, uint8_t right);
  void setVolume(uint8_t volume) { setVolume(volume, volume); }
  uint16_t getDecodeTime();
  void setPlaySpeed(uint16_t speed);
  uint16_t getPlaySpeed();
  
  // Interrupt handling
  bool useInterrupt(uint8_t type);
  void processDeferred();
  
  // Audio quality configuration
  bool configureAudioQuality(uint32_t targetSampleRate = 48000);
  uint32_t getMaxSampleRate() { return _driver.getMaxSampleRate(); }
  
  // Utility methods
  void sineTest(uint8_t freq, uint16_t duration);
  void dumpRegisters();
  bool testBasicPlayback();
  bool startPlayingSimple(const char* filename);  // Minimal configuration test
  bool startPlayingEnhanced(const char* filename); // Enhanced for FLAC/high-quality
  static bool isMP3File(const char* filename);
  static bool isFLACFile(const char* filename);
  
  // Access to underlying components
  VS1053_driver& getDriver() { return _driver; }
  Audio_buffer& getBuffer() { return _buffer; }

  // ISR methods
  static void timerISR();
  static void pinISR();

  // Friend access for ISR
  friend class VS1053_driver;
  volatile bool _playing;

protected:
  VS1053_driver _driver;
  Audio_buffer _buffer;
  
  volatile bool _needsFeeding;
  volatile bool _paused;
  bool _looping;
  bool _usingInterrupts;
  uint8_t _interruptType;
  
  // Data buffer for feeding VS1053
  uint8_t _feedBuffer[AUDIO_DATABUFFERLEN];
  
  static AudioPlayer* _instance;
  
  void feedBuffer();
  void handleInterrupt();
  void refill();
  bool primeBuffer();
  bool primeBufferEnhanced(bool isHighQuality = false);
};

#endif // AUDIOPLAYER_H
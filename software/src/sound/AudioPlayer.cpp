/*!
 * @file AudioPlayer.cpp
 * High-level audio player implementation with playback control methods
 */

#include "AudioPlayer.h"

AudioPlayer* AudioPlayer::_instance = nullptr;

AudioPlayer::AudioPlayer(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq, uint8_t sdCS)
    : _driver(rst, cs, dcs, dreq), _buffer(sdCS), _playing(false), _paused(false),
      _looping(false), _usingInterrupts(false), _needsFeeding(false) {
  _instance = this;
}

AudioPlayer::AudioPlayer(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t rst,
                         uint8_t cs, uint8_t dcs, uint8_t dreq, uint8_t sdCS)
    : _driver(mosi, miso, sclk, rst, cs, dcs, dreq), _buffer(sdCS), _playing(false), 
      _paused(false), _looping(false), _usingInterrupts(false), _needsFeeding(false) {
  _instance = this;
}

AudioPlayer::~AudioPlayer() {
  stopPlaying();
  _instance = nullptr;
}

bool AudioPlayer::begin(uint32_t spiFreq, uint32_t sdFreq) {
  Serial.println("Initializing AudioPlayer with enhanced clock configuration...");
  
  if (!_driver.begin(spiFreq)) {
    return false;
  }
  
  if (!_buffer.begin(sdFreq)) {
    return false;
  }
  
  // Display audio capabilities after optimal clock configuration
  Serial.printf("Audio system ready - Max sample rate: %lu Hz\n", _driver.getMaxSampleRate());
  
  return true;
}

bool AudioPlayer::enablePSRAM(bool enable) {
  return _buffer.enablePSRAM(enable);
}

bool AudioPlayer::isPSRAMAvailable() {
  return _buffer.isPSRAMAvailable();
}

bool AudioPlayer::startPlaying(const char *filename, bool preload) {
  stopPlaying();

  // Try PSRAM preload first if requested and available
  if (preload && _buffer.isPSRAMAvailable()) {
    if (_buffer.preloadFile(filename)) {
      _playing = true;
      _paused = false;
      
      // Prime the VS1053 buffer
      if (primeBuffer()) {
        // Enable interrupts after initial feeding
        useInterrupt(AUDIOPLAYER_PIN_INT);
        return true;
      }
    }
  }

  // Fallback to direct file reading
  if (_buffer.openFile(filename)) {
    _playing = true;
    _paused = false;
    
    // Prime the buffer
    if (primeBuffer()) {
      // Enable interrupts after priming
      useInterrupt(AUDIOPLAYER_PIN_INT);
      return true;
    }
  }

  return false;
}

bool AudioPlayer::playFile(const char *filename, bool preload) {
  if (!startPlaying(filename, preload)) return false;

  while (_playing) {
    if (!_usingInterrupts && _driver.readyForData()) {
      feedBuffer();
    }
    delay(1);
  }
  return true;
}

void AudioPlayer::stopPlaying() {
  _playing = false;
  _paused = false;
  _needsFeeding = false;
  
  // Disable interrupts first
  if (_usingInterrupts) {
    detachInterrupt(digitalPinToInterrupt(_driver.getDREQPin()));
    _usingInterrupts = false;
  }
  
  _buffer.closeFile();
  _buffer.clearPreloadBuffer();

  // Send end-of-file sequence
  for (int i = 0; i < 64; i++) {
    if (_driver.readyForData()) {
      uint8_t endData = 0;
      _driver.sendData(&endData, 1);
    }
  }
}

void AudioPlayer::pausePlaying(bool pause) { 
  _paused = pause; 
}

void AudioPlayer::resumePlaying() { 
  _paused = false; 
}

bool AudioPlayer::isPlaying() { 
  return _playing && !_paused; 
}

bool AudioPlayer::isPaused() { 
  return _paused; 
}

void AudioPlayer::setLoop(bool loop) { 
  _looping = loop; 
}

bool AudioPlayer::isLooping() { 
  return _looping; 
}

void AudioPlayer::forward() {
  // For now, just restart the file
  // Could be enhanced to skip to next track or fast forward
  if (_playing) {
    _buffer.seekToStart();
  }
}

void AudioPlayer::rewind() {
  if (_playing) {
    _buffer.seekToStart();
  }
}

void AudioPlayer::setVolume(uint8_t left, uint8_t right) {
  _driver.setVolume(left, right);
}

uint16_t AudioPlayer::getDecodeTime() {
  return _driver.getDecodeTime();
}

void AudioPlayer::setPlaySpeed(uint16_t speed) {
  _driver.setPlaySpeed(speed);
}

uint16_t AudioPlayer::getPlaySpeed() {
  return _driver.getPlaySpeed();
}

void AudioPlayer::sineTest(uint8_t freq, uint16_t duration) {
  _driver.sineTest(freq, duration);
}

void AudioPlayer::dumpRegisters() {
  _driver.dumpRegisters();
}

bool AudioPlayer::isMP3File(const char *filename) {
  return Audio_buffer::isMP3File(filename);
}

void AudioPlayer::processDeferred() {
  if (_needsFeeding) {
    _needsFeeding = false;
    if (_playing && !_paused && _driver.readyForData()) {
      feedBuffer();
    }
  }
}

bool AudioPlayer::primeBuffer() {
  // Prime the VS1053 buffer with initial data
  // With enhanced clock configuration, we can feed more aggressively
  int feedCount = 0;
  const int maxFeeds = 15; // Increased from 10 due to higher clock speeds
  
  for (int i = 0; i < maxFeeds; i++) {
    if (_driver.readyForData()) {
      feedBuffer();
      feedCount++;
      // Small delay to allow VS1053 to process data
      delayMicroseconds(100);
    } else {
      break;
    }
  }
  
  Serial.printf("Primed buffer with %d feeds\n", feedCount);
  return feedCount > 0;
}

void AudioPlayer::feedBuffer() {
  if (!_playing || _paused || !_driver.readyForData()) return;

  size_t bytesRead = _buffer.readData(_feedBuffer, AUDIO_DATABUFFERLEN);
  if (bytesRead > 0) {
    // Use optimized burst sending for better performance
    _driver.sendDataBurst(_feedBuffer, bytesRead);
    
    // With enhanced clock configuration and 2048-byte FIFO, we can potentially
    // feed more data if the VS1053 is ready for it
    if (_driver.readyForData() && bytesRead == AUDIO_DATABUFFERLEN) {
      // Try to send one more chunk if available and VS1053 is still ready
      size_t extraBytes = _buffer.readData(_feedBuffer, AUDIO_DATABUFFERLEN);
      if (extraBytes > 0) {
        _driver.sendDataBurst(_feedBuffer, extraBytes);
      }
    }
  } else {
    // End of data
    if (_looping) {
      _buffer.seekToStart();
    } else {
      stopPlaying();
    }
  }
}

bool AudioPlayer::useInterrupt(uint8_t type) {
  if (type == AUDIOPLAYER_PIN_INT) {
    attachInterrupt(digitalPinToInterrupt(_driver.getDREQPin()), &AudioPlayer::pinISR, RISING);
    _usingInterrupts = true;
    _interruptType = type;
    return true;
  } else if (type == 0) {
    detachInterrupt(digitalPinToInterrupt(_driver.getDREQPin()));
    _usingInterrupts = false;
    return true;
  }
  return false;
}

// ISR methods
void AudioPlayer::timerISR() {
  if (_instance) {
    _instance->handleInterrupt();
  }
}

void AudioPlayer::pinISR() {
  // Minimal ISR - just call refill directly
  if (_instance && _instance->_playing && !_instance->_paused) {
    _instance->refill();
  }
}

void AudioPlayer::handleInterrupt() {
  if (_playing && !_paused) {
    refill();
  }
}

void AudioPlayer::refill() {
  // Only feed if DREQ is still high and we're playing
  if (_driver.readyForData() && _playing && !_paused) {
    feedBuffer();
  }
}

bool AudioPlayer::configureAudioQuality(uint32_t targetSampleRate) {
  Serial.printf("Configuring audio quality for %lu Hz target sample rate\n", targetSampleRate);
  
  // Stop any current playback
  bool wasPlaying = _playing;
  if (wasPlaying) {
    stopPlaying();
  }
  
  // Configure optimal clock based on target sample rate
  uint32_t xtalFreq = VS1053_XTALI_12_288MHZ;
  
  // For very high sample rates, we might need a higher crystal frequency
  if (targetSampleRate > 96000) {
    Serial.println("Warning: Target sample rate > 96kHz may require 24.576MHz crystal");
    xtalFreq = VS1053_XTALI_24_576MHZ;
  }
  
  // Reconfigure the VS1053 with optimal settings
  bool success = _driver.configureOptimalClock(xtalFreq);
  
  if (success) {
    uint32_t maxSampleRate = _driver.getMaxSampleRate();
    Serial.printf("Audio quality configured - Max sample rate: %lu Hz\n", maxSampleRate);
    
    if (maxSampleRate >= targetSampleRate) {
      Serial.printf("Target sample rate %lu Hz is supported\n", targetSampleRate);
    } else {
      Serial.printf("Warning: Target sample rate %lu Hz exceeds maximum %lu Hz\n",
                   targetSampleRate, maxSampleRate);
    }
  } else {
    Serial.println("Failed to configure audio quality");
  }
  
  return success;
}
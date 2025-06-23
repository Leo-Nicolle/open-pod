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
  Serial.printf("Starting playback: %s (preload: %s)\n", filename, preload ? "yes" : "no");
  stopPlaying();

  // Try PSRAM preload first if requested and available
  if (preload && _buffer.isPSRAMAvailable()) {
    Serial.println("Attempting PSRAM preload...");
    if (_buffer.preloadFile(filename)) {
      Serial.println("PSRAM preload successful");
      _playing = true;
      _paused = false;
      
      // Prime the VS1053 buffer
      if (primeBuffer()) {
        // Use polling mode instead of interrupts for more reliable operation
        // useInterrupt(AUDIOPLAYER_PIN_INT);
        Serial.println("Buffer primed successfully (PSRAM mode)");
        return true;
      } else {
        Serial.println("Buffer priming failed (PSRAM mode)");
      }
    } else {
      Serial.println("PSRAM preload failed, falling back to direct file reading");
    }
  }

  // Fallback to direct file reading
  Serial.println("Opening file for direct reading...");
  if (_buffer.openFile(filename)) {
    Serial.printf("File opened successfully: %s\n", filename);
    _playing = true;
    _paused = false;
    
    // Prime the buffer
    if (primeBuffer()) {
      // Use polling mode instead of interrupts for more reliable operation
      // useInterrupt(AUDIOPLAYER_PIN_INT);
      Serial.println("Buffer primed successfully (direct file mode)");
      return true;
    } else {
      Serial.println("Buffer priming failed (direct file mode)");
    }
  } else {
    Serial.printf("Failed to open file: %s\n", filename);
  }

  Serial.println("startPlaying failed");
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
  // In polling mode, actively check and feed the buffer
  if (_playing && !_paused && _driver.readyForData()) {
    feedBuffer();
  }
  
  // Legacy interrupt-based feeding
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
    // Use simple, reliable data sending (revert from burst mode)
    _driver.sendData(_feedBuffer, bytesRead);
    
    // Debug: Print first few bytes to verify data integrity
    static int debugCount = 0;
    if (debugCount < 3) {
      Serial.printf("Feed %d: %d bytes [%02X %02X %02X %02X...]\n",
                   debugCount, bytesRead,
                   _feedBuffer[0], _feedBuffer[1], _feedBuffer[2], _feedBuffer[3]);
      debugCount++;
    }
  } else {
    // End of data
    Serial.println("End of audio data reached");
    if (_looping) {
      Serial.println("Looping back to start");
      _buffer.seekToStart();
    } else {
      Serial.println("Stopping playback");
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

bool AudioPlayer::testBasicPlayback() {
  Serial.println("=== VS1053 Basic Playback Test ===");
  
  // Test 1: Check if VS1053 is responsive
  uint16_t status = _driver.readRegister(VS1053_REG_STATUS);
  Serial.printf("VS1053 Status: 0x%04X\n", status);
  
  if (!(status & 0x0040)) {
    Serial.println("ERROR: VS1053 not responding!");
    return false;
  }
  
  // Test 2: Check clock configuration
  uint16_t clockf = _driver.readRegister(VS1053_REG_CLOCKF);
  Serial.printf("Clock Config: 0x%04X\n", clockf);
  
  // Test 3: Check DREQ pin
  bool dreqStatus = _driver.readyForData();
  Serial.printf("DREQ Status: %s\n", dreqStatus ? "HIGH (Ready)" : "LOW (Busy)");
  
  // Test 4: Check volume setting
  uint16_t volume = _driver.readRegister(VS1053_REG_VOLUME);
  Serial.printf("Volume: 0x%04X\n", volume);
  
  // Test 5: Simple sine wave test
  Serial.println("Testing sine wave generation...");
  _driver.sineTest(0x44, 500); // 500ms test tone
  
  Serial.println("Basic test completed");
  return true;
}

bool AudioPlayer::startPlayingSimple(const char *filename) {
  Serial.printf("Starting SIMPLE playback: %s\n", filename);
  stopPlaying();

  // Use direct file reading only (no PSRAM, no complex configuration)
  Serial.println("Opening file for simple direct reading...");
  if (_buffer.openFile(filename)) {
    Serial.printf("File opened successfully: %s\n", filename);
    _playing = true;
    _paused = false;
    
    // Simple buffer priming - just a few feeds
    for (int i = 0; i < 5; i++) {
      if (_driver.readyForData()) {
        size_t bytesRead = _buffer.readData(_feedBuffer, AUDIO_DATABUFFERLEN);
        if (bytesRead > 0) {
          _driver.sendData(_feedBuffer, bytesRead);
        }
      }
    }
    
    Serial.println("Simple playback started - no interrupts, polling only");
    return true;
  } else {
    Serial.printf("Failed to open file: %s\n", filename);
    return false;
  }
}

bool AudioPlayer::startPlayingEnhanced(const char *filename) {
  Serial.printf("Starting ENHANCED playback: %s\n", filename);
  stopPlaying();

  // Check file type and configure accordingly
  bool isFlac = isFLACFile(filename);
  if (isFlac) {
    Serial.println("FLAC file detected - loading FLAC plugin and configuring for high-quality playback");
    
    // Load FLAC plugin first
    if (!_driver.loadFLACPlugin()) {
      Serial.println("Failed to load FLAC plugin");
      return false;
    }
    
    // Verify plugin is loaded
    if (!_driver.isFLACPluginLoaded()) {
      Serial.println("FLAC plugin not properly loaded");
      return false;
    }
    
    Serial.println("FLAC plugin loaded successfully");
    
    // Optimize SPI speed for higher data rates
    _driver.optimizeSPISpeed();
  } else {
    Serial.println("Non-FLAC file - using standard configuration");
  }

  // Try PSRAM preload for high-quality files
  if (_buffer.isPSRAMAvailable()) {
    Serial.println("Attempting PSRAM preload for enhanced quality...");
    if (_buffer.preloadFile(filename)) {
      Serial.println("PSRAM preload successful");
      _playing = true;
      _paused = false;
      
      // Enhanced buffer priming for high-quality audio
      if (primeBufferEnhanced(isFlac)) {
        Serial.println("Enhanced buffer primed successfully (PSRAM mode)");
        return true;
      } else {
        Serial.println("Enhanced buffer priming failed (PSRAM mode)");
      }
    } else {
      Serial.println("PSRAM preload failed, falling back to direct file reading");
    }
  }

  // Fallback to direct file reading with enhanced feeding
  Serial.println("Opening file for enhanced direct reading...");
  if (_buffer.openFile(filename)) {
    Serial.printf("File opened successfully: %s\n", filename);
    _playing = true;
    _paused = false;
    
    // Enhanced buffer priming
    if (primeBufferEnhanced(isFlac)) {
      Serial.println("Enhanced buffer primed successfully (direct file mode)");
      return true;
    } else {
      Serial.println("Enhanced buffer priming failed (direct file mode)");
    }
  } else {
    Serial.printf("Failed to open file: %s\n", filename);
  }

  Serial.println("Enhanced playback failed");
  return false;
}

bool AudioPlayer::isFLACFile(const char *filename) {
  const char *ext = strrchr(filename, '.');
  return ext && (strcasecmp(ext, ".flac") == 0 || strcasecmp(ext, ".fla") == 0);
}

bool AudioPlayer::primeBufferEnhanced(bool isHighQuality) {
  // Enhanced buffer priming for high-quality audio (FLAC, high-bitrate MP3)
  int feedCount = 0;
  const int maxFeeds = isHighQuality ? 20 : 15; // More feeds for FLAC
  
  Serial.printf("Enhanced priming for %s quality audio...\n", isHighQuality ? "high" : "standard");
  
  for (int i = 0; i < maxFeeds; i++) {
    if (_driver.readyForData()) {
      size_t bytesRead = _buffer.readData(_feedBuffer, AUDIO_DATABUFFERLEN);
      if (bytesRead > 0) {
        _driver.sendData(_feedBuffer, bytesRead);
        feedCount++;
        
        // For high-quality audio, feed more aggressively
        if (isHighQuality && _driver.readyForData() && i < maxFeeds - 1) {
          size_t extraBytes = _buffer.readData(_feedBuffer, AUDIO_DATABUFFERLEN);
          if (extraBytes > 0) {
            _driver.sendData(_feedBuffer, extraBytes);
            feedCount++;
            i++; // Account for the extra feed
          }
        }
        
        // Shorter delay for high-quality to maintain data flow
        delayMicroseconds(isHighQuality ? 50 : 100);
      }
    } else {
      break;
    }
  }
  
  Serial.printf("Enhanced priming completed with %d feeds\n", feedCount);
  return feedCount > 0;
}
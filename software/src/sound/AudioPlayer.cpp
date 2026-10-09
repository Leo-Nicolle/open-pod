/*!
 * @file AudioPlayer.cpp
 */

#include "AudioPlayer.h"
#include "AudioProfiler.h"
#include "Spi1Bus.h"

AudioPlayer *AudioPlayer::_instance = nullptr;

AudioPlayer::AudioPlayer(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq,
                         uint8_t sdCS)
    : _driver(rst, cs, dcs, dreq), _buffer(sdCS), _playing(false),
      _paused(false), _looping(false) {
  _instance = this;
}

AudioPlayer::AudioPlayer(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t rst,
                         uint8_t cs, uint8_t dcs, uint8_t dreq, uint8_t sdCS)
    : _driver(mosi, miso, sclk, rst, cs, dcs, dreq), _buffer(sdCS),
      _playing(false), _paused(false), _looping(false) {
  _instance = this;
}

AudioPlayer::~AudioPlayer() {
  stopPlaying();
  _instance = nullptr;
}

bool AudioPlayer::begin(uint32_t spiFreq, uint32_t sdFreq) {
  Serial.println(
      "Initializing AudioPlayer with enhanced clock configuration...");

  if (!_driver.begin(spiFreq)) {
    return false;
  }

  if (!_buffer.begin(sdFreq)) {
    return false;
  }
  _buffer.setFrontFifo(&_fifo);

  // Lowest priority: UART, I2C and SysTick interrupts preempt it.
  _feederTimer = new HardwareTimer(TIM7);
  _feederTimer->setOverflow(FEEDER_HZ, HERTZ_FORMAT);
  _feederTimer->setInterruptPriority(15, 0);
  _feederTimer->attachInterrupt(&AudioPlayer::feederIsr);
  _feederTimer->resume();
  // A tick that found SPI1 busy runs again as soon as the bus is released:
  // a software update event fires the timer interrupt right away.
  spi1bus::onFreed = []() { TIM7->EGR = TIM_EGR_UG; };

  // Display audio capabilities after optimal clock configuration
  Serial.printf("Audio system ready - Max sample rate: %lu Hz\n",
                _driver.getMaxSampleRate());

  return true;
}

bool AudioPlayer::startPlaying(const char *filename,
                               uint32_t knownDurationSeconds) {
  Serial.printf("Starting playback: %s\n", filename);
  unsigned long startMs = millis();
  stopPlaying();

  bool isFlac = isFLACFile(filename);
  _streamIsFlac = isFlac;
  if (isFlac) {
    if (!_driver.prepareFLACPlayback()) {
      return false;
    }
  }
  // If this exact file was already prefetched while the previous track was
  // playing, adopt it (reopen at the resumed position + copy the prefetched
  // bytes into the ring buffer) instead of reading its opening bytes from
  // SD again from scratch.
  if (!_buffer.consumeNextTrackIfMatches(filename, knownDurationSeconds)) {
    _buffer.setFileName(filename, knownDurationSeconds);
  }

  // Bounded prime (straight into the fifo, PSRAM being empty) before the
  // feeder starts. Bounded by a timeout AND a no-progress guard so a
  // slow/empty SD can't hang the track change.
  unsigned long primeStart = millis();
  size_t primed = _buffer.getBufferedBytes();
  while (primed < AUDIO_PRIME_BYTES &&
         millis() - primeStart < AUDIO_PRIME_TIMEOUT_MS) {
    _buffer.load(AUDIO_PRIME_BYTES);
    size_t now = _buffer.getBufferedBytes();
    if (now == primed) {
      break; // no forward progress: file exhausted or SD stalled
    }
    primed = now;
  }
  if (_buffer.openFailed()) {
    stopPlaying();
    return false;
  }
  // An adopted next-track prefetch lands in PSRAM, not the fifo.
  _buffer.fillFrontFromPSRAM(AudioFifo::CAPACITY);

  _driver.resetDecodeTime();
  _seekOffsetSeconds = 0;
  _playing = true;
  _paused = false;
  _decoderDirty = true;
  updateFeeder();
  Serial.printf("Track started in %lu ms (%u bytes primed)\n",
                millis() - startMs, (unsigned)primed);
  return true;
}

void AudioPlayer::stopPlaying(bool endOfStream) {
  _playing = false;
  _paused = false;
  updateFeeder(); // stops the consumer before the fifo is reset

  _fifo.reset();
  _buffer.resetRingBuffer();
  _refilling = false;
  if (_decoderDirty) {
    sendEndFillSequence(endOfStream);
  }
}

void AudioPlayer::pausePlaying(bool pause) {
  _paused = pause;
  updateFeeder();
}

void AudioPlayer::resumePlaying() { pausePlaying(false); }

// Diagnostic mode (OPENPOD_FEED_MAIN_LOOP): same fifo and same send code
// as the feeder interrupt, but run from the main loop like Phase 1 did.
void AudioPlayer::feedFromMainLoop() {
  if (!FEED_FROM_MAIN_LOOP || !_feederEnabled) {
    return;
  }
  uint32_t t0 = audioprof::now();
  size_t sent = 0;
  while (_driver.dreqHigh() && _fifo.size() > 0) {
    sent += _driver.isrFeed(_fifo, 4096);
  }
  if (sent) {
    audioprof::add(audioprof::FEED_ISR, t0);
    audioprof::addBytes(audioprof::FEED_ISR, sent);
  }
}

void AudioPlayer::feederIsr() {
  AudioPlayer *p = _instance;
  if (!p) {
    return;
  }
  // Tick regularity: a gap well over 1 ms means the interrupt itself was
  // held off (higher-priority interrupts, or interrupts disabled).
  uint32_t nowCycles = audioprof::now();
  uint32_t gap = nowCycles - p->_isrLastTickCycles;
  p->_isrLastTickCycles = nowCycles;
  if (gap > p->_isrMaxTickGapCycles) {
    p->_isrMaxTickGapCycles = gap;
  }
  if (!p->_feederEnabled || FEED_FROM_MAIN_LOOP) {
    return;
  }
  // A stall is a run of ticks where the chip asked for data (DREQ high)
  // and got none, whatever the reason (bus busy, fifo empty).
  bool wanted = p->_driver.dreqHigh();
  size_t sent = 0;
  if (spi1bus::busy()) {
    p->_isrBusySkips = p->_isrBusySkips + 1;
    if (wanted) {
      spi1bus::feedPending = true;
    }
  } else {
    uint32_t t0 = audioprof::now();
    sent = p->_driver.isrFeed(p->_fifo, FEEDER_MAX_BYTES_PER_TICK);
    if (sent) {
      audioprof::add(audioprof::FEED_ISR, t0);
      audioprof::addBytes(audioprof::FEED_ISR, sent);
    }
  }
  if (wanted && sent == 0) {
    p->_isrStallTicks = p->_isrStallTicks + 1;
    if (p->_isrStallTicks > p->_isrMaxStall) {
      p->_isrMaxStall = p->_isrStallTicks;
    }
  } else {
    p->_isrStallTicks = 0;
  }
  if (p->_fifo.size() == 0 && p->_driver.dreqHigh()) {
    p->_isrUnderruns = p->_isrUnderruns + 1;
  }
}

bool AudioPlayer::isPlaying() { return _playing && !_paused; }

bool AudioPlayer::isPaused() { return _paused; }

void AudioPlayer::setLoop(bool loop) { _looping = loop; }

bool AudioPlayer::isLooping() { return _looping; }

void AudioPlayer::setVolume(uint8_t left, uint8_t right) {
  _driver.setVolume(left, right);
}

uint16_t AudioPlayer::getDecodeTime() {
  // The chip keeps the last track's decode time after a stop; nothing is
  // playing (e.g. a track start is pending), so report the start instead.
  if (!_playing) return 0;
  return (uint16_t)(_driver.getDecodeTime() + _seekOffsetSeconds);
}

void AudioPlayer::setPlaySpeed(uint16_t speed) { _driver.setPlaySpeed(speed); }

uint16_t AudioPlayer::getPlaySpeed() { return _driver.getPlaySpeed(); }

void AudioPlayer::sineTest(uint8_t freq, uint16_t duration) {
  _driver.sineTest(freq, duration);
}

void AudioPlayer::dumpRegisters() { _driver.dumpRegisters(); }
void AudioPlayer::loop() {
  if (_duckingForSeek && millis() >= _duckRestoreAt) {
    restoreVolumeFromDuck();
  }
  if (!_playing) return;

  // Everything has been handed to the feeder and it sent the last byte:
  // the end fill sequence (in stopPlaying) plays out what the chip holds.
  if (_buffer.isFileExhausted()) {
    Serial.println("End of audio data reached");
    _trackEnded = true;
    stopPlaying(true);
    return;
  }
  if (_paused) return;

  // --- monitor (rate-limited) ---
  unsigned long loopEnterMs = millis();
  if (_monitorPrevLoopMs != 0) {
    unsigned long period = loopEnterMs - _monitorPrevLoopMs;
    if (period > _monitorLoopMaxMs) _monitorLoopMaxMs = period;
  }
  _monitorPrevLoopMs = loopEnterMs;
  if (_fifo.size() < _monitorMinFifo) _monitorMinFifo = _fifo.size();

  // 1. Keep the feeder's fifo topped up from PSRAM.
  unsigned long fillUsStart = micros();
  feedFromMainLoop();
  _buffer.fillFrontFromPSRAM(FIFO_TOPUP_MAX_BYTES);
  feedFromMainLoop();
  _monitorFillUs += (uint32_t)(micros() - fillUsStart);

  size_t buffered = _buffer.getBufferedBytes();
  if (buffered < _monitorMinBuffered) _monitorMinBuffered = buffered;
  if (buffered > _monitorMaxBuffered) _monitorMaxBuffered = buffered;
  if (buffered < AUDIO_LOW_WATERMARK_BYTES) _monitorLowCount++;

  // 2. Hysteresis: a refill starts once the buffer drains below the low
  // watermark and keeps going, a few chunks per tick, until the buffer
  // reaches the target (or the file is fully read). Then the SD card stays
  // idle until the next crossing.
  //
  // Per-tick budget: short while there's plenty buffered, so the UI keeps
  // running between chunks; long when the buffer is actually low. The
  // feeder interrupt keeps playing from the fifo meanwhile.
  unsigned long loadUsStart = micros();
  if (buffered < AUDIO_LOW_WATERMARK_BYTES) {
    _refilling = true;
  }
  // While the user is interacting (wheel input, screen transition), a
  // refill step (SD read + PSRAM write, ~15 ms) would eat the UI's frame
  // time, so SD work backs off:
  //  - plenty buffered: the refill waits until the user stops;
  //  - otherwise: a trickle of one chunk per USER_BUSY_CHUNK_INTERVAL_MS
  //    (~200 KB/s, above any FLAC rate), so the buffer still grows;
  //  - nearly empty (AUDIO_CRITICAL_BYTES): full budget, audio first.
  unsigned long nowMs = millis();
  bool userBusy = nowMs - _lastUserActivityMs < USER_ACTIVITY_HOLDOFF_MS;
  bool critical = buffered < AUDIO_CRITICAL_BYTES;
  bool runRefill = _refilling;
  unsigned long budgetMs = buffered < AUDIO_URGENT_BYTES
                               ? AUDIO_MAX_LOAD_MS
                               : AUDIO_RELAXED_LOAD_MS;
  if (_refilling && userBusy && !critical) {
    if (buffered >= AUDIO_URGENT_BYTES ||
        nowMs - _lastBusyChunkMs < USER_BUSY_CHUNK_INTERVAL_MS) {
      runRefill = false; // resumes once the user stops / at the next slot
    } else {
      budgetMs = 1; // checked before each chunk: exactly one chunk
      _lastBusyChunkMs = nowMs;
    }
  }
  if (runRefill) {
    _buffer.load(AUDIO_TARGET_BUFFERED_BYTES,
                 FEED_FROM_MAIN_LOOP ? std::function<void()>([this]() {
                   feedFromMainLoop();
                 })
                                     : std::function<void()>(),
                 budgetMs);
    if (_buffer.getBufferedBytes() >= AUDIO_TARGET_BUFFERED_BYTES ||
        _buffer.isFileFullyRead() || _buffer.openFailed()) {
      _refilling = false;
    }
  } else if (!_refilling && !userBusy) {
    // 3. Buffer is healthy: use the otherwise-idle SD bus to make background
    // progress on the next track's prefetch instead, one chunk per tick.
    _buffer.prefetchNextTrack();
    feedFromMainLoop();
  }
  _monitorLoadUs += (uint32_t)(micros() - loadUsStart);

  if (loopEnterMs - _monitorLastReport >= AUDIO_MONITOR_INTERVAL_MS) {
    if (_monitorEnabled) {
      Serial.printf(
          "[audio] buf=%u/%u min=%u max=%u low=%u l1min=%u underrun=%lu "
          "busySkip=%lu maxStall=%lums tickGap=%luus loop=%lums load=%lums "
          "fill=%lums\n",
          (unsigned)buffered, (unsigned)AUDIO_TARGET_BUFFERED_BYTES,
          (unsigned)_monitorMinBuffered, (unsigned)_monitorMaxBuffered,
          (unsigned)_monitorLowCount, (unsigned)_monitorMinFifo,
          (unsigned long)_isrUnderruns, (unsigned long)_isrBusySkips,
          (unsigned long)(_isrMaxStall * 1000 / FEEDER_HZ),
          (unsigned long)(_isrMaxTickGapCycles / (SystemCoreClock / 1000000UL)),
          (unsigned long)_monitorLoopMaxMs,
          (unsigned long)(_monitorLoadUs / 1000),
          (unsigned long)(_monitorFillUs / 1000));
    }
    _monitorLastReport = loopEnterMs;
    _monitorMinBuffered = (size_t)-1;
    _monitorMaxBuffered = 0;
    _monitorLowCount = 0;
    _monitorMinFifo = AudioFifo::CAPACITY;
    _isrUnderruns = 0;
    _isrBusySkips = 0;
    _isrMaxStall = 0;
    _isrMaxTickGapCycles = 0;
    _monitorLoadUs = 0;
    _monitorFillUs = 0;
    _monitorLoopMaxMs = 0;
  }
}

void AudioPlayer::prepareNextTrack(const char *filename,
                                   uint32_t knownDurationSeconds) {
  _buffer.prepareNextTrack(filename, knownDurationSeconds);
}

void AudioPlayer::invalidateNextTrack() { _buffer.invalidateNextTrack(); }

bool AudioPlayer::consumeTrackEnded() {
  bool ended = _trackEnded;
  _trackEnded = false;
  return ended;
}

bool AudioPlayer::seekToSeconds(uint32_t targetSeconds) {
  if (!_playing) {
    return false;
  }
  // Datasheet 10.5.4: no jump while a header is mid-decode.
  if (!_driver.canJump()) {
    return false;
  }

  // Datasheet 10.5.4 recommends lowering volume during a seek to mask the
  // decoder resync glitch. Only duck once per seek "session" - if a new
  // seek commits before loop() has restored the previous one, don't
  // re-duck an already-ducked volume (see restoreVolumeFromDuck()).
  if (!_duckingForSeek) {
    _savedVolumeRaw = _driver.getVolumeRaw();
    uint8_t left = (uint8_t)min(255, (_savedVolumeRaw >> 8) + SEEK_DUCK_ATTENUATION);
    uint8_t right = (uint8_t)min(255, (_savedVolumeRaw & 0xFF) + SEEK_DUCK_ATTENUATION);
    _driver.setVolume(left, right);
    _duckingForSeek = true;
  }
  uint8_t endFillByte = _driver.getEndFillByte();

  _feederEnabled = false; // stop the consumer while the fifo is rewritten
  if (!_buffer.seekToSeconds(targetSeconds)) {
    updateFeeder();
    restoreVolumeFromDuck();
    return false;
  }
  // The jump must be preceded by end fill bytes so the decoder flushes what
  // it has queued (12288 for the FLAC plugin). They're queued in the fifo
  // ahead of the new position's data instead of being sent here, so the
  // seek doesn't block while the chip drains its buffer.
  _fifo.reset();
  _fifo.fill(endFillByte, _streamIsFlac ? FLAC_END_FILL_BYTES : 2048);
  // Bounded prime (straight into the fifo behind the fill bytes); loop()'s
  // refill takes it from there.
  _buffer.load(AUDIO_SEEK_PRIME_BYTES);
  updateFeeder();

  _driver.resetDecodeTime(); // Hardware counter can't jump; restart at 0.
  _seekOffsetSeconds = targetSeconds;
  // Keep the duck through the resync window.
  _duckRestoreAt = millis() + SEEK_DUCK_RESTORE_DELAY;
  return true;
}

void AudioPlayer::restoreVolumeFromDuck() {
  if (_duckingForSeek) {
    _driver.setVolume((uint8_t)(_savedVolumeRaw >> 8), (uint8_t)(_savedVolumeRaw & 0xFF));
    _duckingForSeek = false;
  }
}

bool AudioPlayer::seekBySeconds(int deltaSeconds) {
  int target = constrain((int)getDecodeTime() + deltaSeconds, 0,
                         (int)getDurationSeconds());
  return seekToSeconds((uint32_t)target);
}

// Ends the stream the decoder is working on (datasheet 10.5.1 / 10.5.2).
// The FLAC plugin has a 12 KiB stream buffer and needs 12288 end fill bytes
// where the built-in decoders need 2052 (vs1053b-patches.pdf, "FLAC
// Decoder"). The whole sequence is bounded by one second (the datasheet's
// own limit before falling back to a software reset): DREQ can legitimately
// stay low for a whole FLAC frame (~100 ms) while the decoder catches up.
void AudioPlayer::sendEndFillSequence(bool endOfStream) {
  unsigned long startMs = millis();
  _decoderDirty = false;

  uint8_t endFillBuffer[32];
  memset(endFillBuffer, _driver.getEndFillByte(), sizeof(endFillBuffer));
  const int fillBytes = _streamIsFlac ? FLAC_END_FILL_BYTES : 2052;

  // Sends `bytes` end fill bytes; false once the deadline passes.
  auto sendFill = [&](int bytes) {
    while (bytes > 0) {
      if (millis() - startMs > END_FILL_TIMEOUT_MS) {
        return false;
      }
      if (_driver.readyForData()) {
        _driver.sendData(endFillBuffer, sizeof(endFillBuffer));
        bytes -= sizeof(endFillBuffer);
      }
    }
    return true;
  };
  // Sets SM_CANCEL, then sends end fill bytes 32 at a time until the
  // decoder clears it (at most fillBytes).
  auto cancel = [&]() {
    _driver.writeRegister(VS1053_REG_MODE,
                          VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_CANCEL);
    for (int sent = 0; sent < fillBytes; sent += 32) {
      if (!(_driver.readRegister(VS1053_REG_MODE) & VS1053_MODE_SM_CANCEL)) {
        return true;
      }
      if (!sendFill(32)) {
        return false;
      }
    }
    return !(_driver.readRegister(VS1053_REG_MODE) & VS1053_MODE_SM_CANCEL);
  };

  bool ok;
  if (endOfStream) {
    // 10.5.1, the file was played to its end: end fill bytes first so the
    // decoder plays out what it holds, then cancel.
    ok = sendFill(fillBytes) && cancel();
  } else {
    // 10.5.2, stopping mid-stream: cancel first (the decoder drops what it
    // holds right away), then end fill bytes to flush it.
    ok = cancel() && sendFill(fillBytes);
  }
  if (!ok) {
    Serial.println("Cancel didn't complete, soft reset");
    _driver.softReset();
  }
  Serial.printf("End fill sequence (%s) completed in %lu ms\n",
                endOfStream ? "end of stream" : "cancel", millis() - startMs);
}

bool AudioPlayer::configureAudioQuality(uint32_t targetSampleRate) {
  Serial.printf("Configuring audio quality for %lu Hz target sample rate\n",
                targetSampleRate);

  // Stop any current playback
  bool wasPlaying = _playing;
  if (wasPlaying) {
    stopPlaying();
  }

  // Configure optimal clock based on target sample rate
  uint32_t xtalFreq = VS1053_XTALI_12_288MHZ;

  // For very high sample rates, we might need a higher crystal frequency
  if (targetSampleRate > 96000) {
    Serial.println(
        "Warning: Target sample rate > 96kHz may require 24.576MHz crystal");
    xtalFreq = VS1053_XTALI_24_576MHZ;
  }

  // Reconfigure the VS1053 with optimal settings
  bool success = _driver.configureOptimalClock(xtalFreq);

  if (success) {
    uint32_t maxSampleRate = _driver.getMaxSampleRate();
    Serial.printf("Audio quality configured - Max sample rate: %lu Hz\n",
                  maxSampleRate);

    if (maxSampleRate >= targetSampleRate) {
      Serial.printf("Target sample rate %lu Hz is supported\n",
                    targetSampleRate);
    } else {
      Serial.printf(
          "Warning: Target sample rate %lu Hz exceeds maximum %lu Hz\n",
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
  Serial.printf("DREQ Status: %s\n",
                dreqStatus ? "HIGH (Ready)" : "LOW (Busy)");

  // Test 4: Check volume setting
  uint16_t volume = _driver.readRegister(VS1053_REG_VOLUME);
  Serial.printf("Volume: 0x%04X\n", volume);

  // Test 5: Simple sine wave test
  Serial.println("Testing sine wave generation...");
  _driver.sineTest(0x44, 500); // 500ms test tone

  Serial.println("Basic test completed");
  return true;
}

bool AudioPlayer::isFLACFile(const char *filename) {
  const char *ext = strrchr(filename, '.');
  return ext && (strcasecmp(ext, ".flac") == 0 || strcasecmp(ext, ".fla") == 0);
}


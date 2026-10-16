/*!
 * @file AudioPlayer.cpp
 */

#include "AudioPlayer.h"
#include "../logging.h"
#include "AudioProfiler.h"
#include "Spi1Bus.h"
#include "RefillPolicy.h"

AudioPlayer *AudioPlayer::_instance = nullptr;

AudioPlayer::AudioPlayer(uint8_t rst, uint8_t cs, uint8_t dcs, uint8_t dreq,
                         uint8_t sdCS, SdFat &sd)
    : _driver(rst, cs, dcs, dreq), _buffer(sdCS, sd), _playing(false),
      _paused(false), _looping(false) {
  _instance = this;
}

AudioPlayer::AudioPlayer(uint8_t mosi, uint8_t miso, uint8_t sclk, uint8_t rst,
                         uint8_t cs, uint8_t dcs, uint8_t dreq, uint8_t sdCS,
                         SdFat &sd)
    : _driver(mosi, miso, sclk, rst, cs, dcs, dreq), _buffer(sdCS, sd),
      _playing(false), _paused(false), _looping(false) {
  _instance = this;
}

AudioPlayer::~AudioPlayer() {
  stopPlaying();
  finishStop();
  _instance = nullptr;
}

bool AudioPlayer::begin(uint32_t spiFreq, uint32_t sdFreq) {
  LOG_CAT(VS1053, "Initializing AudioPlayer with enhanced clock configuration...");

  if (!_driver.begin(spiFreq)) {
    return false;
  }

  // No SD card is not fatal: the feeder still comes up, so clips from flash
  // (the startup sound) play; file playback then fails to open its files.
  if (!_buffer.begin(sdFreq)) {
    LOG_WARN("AudioPlayer: no SD card, only clips from memory will play");
  }
  _buffer.setFrontFifo(&_fifo);
  audioYieldHook() = &AudioPlayer::yieldHook;

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
  LOG_INFO("Audio system ready - Max sample rate: %lu Hz",
           _driver.getMaxSampleRate());

  return true;
}

bool AudioPlayer::startPlaying(const char *filename,
                               uint32_t knownDurationSeconds) {
  LOG_INFO("Starting playback: %s", filename);
  unsigned long startMs = millis();
  stopPlaying(); // asynchronous: usually still running below

  bool isFlac = isFLACFile(filename);
  if (isFlac && !_driver.flacPluginLoaded()) {
    finishStop(); // loading the plugin needs a stopped decoder
  }
  _streamIsFlac = isFlac;
  if (isFlac) {
    if (!_driver.prepareFLACPlayback()) {
      return false;
    }
  }
  // If this exact file was already prefetched while the previous track was
  // playing, adopt the start of it instead of reading it from SD again.
  if (!_buffer.consumeNextTrackIfMatches(filename, knownDurationSeconds)) {
    _buffer.setFileName(filename, knownDurationSeconds);
  }

  // Bounded prime: straight into the fifo when the decoder is ready, into
  // PSRAM while a stop still sends its end fill bytes (those bytes don't
  // count). Bounded by a timeout AND a no-progress guard so a slow/empty SD
  // can't hang the track change.
  auto primedBytes = [this]() {
    return stopping() ? _buffer.getPSRAMBufferedBytes()
                      : _buffer.getBufferedBytes();
  };
  unsigned long primeStart = millis();
  size_t primed = primedBytes();
  while (primed < AUDIO_PRIME_BYTES &&
         millis() - primeStart < AUDIO_PRIME_TIMEOUT_MS) {
    _buffer.load(AUDIO_PRIME_BYTES + (_buffer.getBufferedBytes() - primed));
    size_t now = primedBytes();
    if (now == primed) {
      break; // no forward progress: file exhausted or SD stalled
    }
    primed = now;
  }
  if (_buffer.openFailed()) {
    stopPlaying();
    return false;
  }

  _seekOffsetSeconds = 0;
  _playing = true;
  _paused = false;
  if (!stopping()) {
    onDecoderReady(); // otherwise when the stop completes (serviceStop())
  }
  updateFeeder();
  LOG_CAT(AUDIO_BUFFER, "Track started in %lu ms (%u bytes primed%s)",
          millis() - startMs, (unsigned)primed,
          stopping() ? ", previous stream still stopping" : "");
  return true;
}

// The decoder is clear: the new stream's data may reach the fifo.
void AudioPlayer::onDecoderReady() {
  _buffer.setFrontBypass(true);
  // An adopted prefetch, or a prime made while stopping, sits in PSRAM.
  _buffer.fillFrontFromPSRAM(AudioFifo::CAPACITY);
  _driver.resetDecodeTime();
  _decoderDirty = true;
  resetRateEstimate();
  // Fresh monitor window: the previous one may span idle time (or boot).
  resetMonitorWindow(millis());
}

// endOfStream: the file was played to its end (the decoder plays out what
// it holds) rather than stopped mid-track (it drops it right away).
bool AudioPlayer::playClip(const uint8_t *data, size_t len) {
  if (len > AudioFifo::CAPACITY || _playing || _clipPlaying || stopping() ||
      !_feederTimer) {
    return false;
  }
  _streamIsFlac = false; // 2052 end fill bytes when it ends
  _fifo.reset();
  _fifo.write(data, len);
  _driver.resetDecodeTime();
  _decoderDirty = true;
  _clipPlaying = true;
  updateFeeder();
  return true;
}

void AudioPlayer::stopPlaying(bool endOfStream) {
  _clipPlaying = false;
  _playing = false;
  _paused = false;
  _buffer.resetRingBuffer();
  _buffer.setFrontBypass(false);
  _refilling = false;
  if (stopping()) {
    updateFeeder();
    return; // the stop in progress also covers this one
  }
  if (_decoderDirty) {
    _decoderDirty = false;
    beginStop(endOfStream);
  } else {
    _feederEnabled = false; // nothing was sent: just drop what's queued
    _fifo.reset();
    _buffer.setFrontBypass(true);
  }
  updateFeeder();
}

// Datasheet 10.5.1 / 10.5.2. The FLAC plugin has a 12 KiB stream buffer and
// needs 12288 end fill bytes where the built-in decoders need 2052
// (vs1053b-patches.pdf, "FLAC Decoder").
void AudioPlayer::beginStop(bool endOfStream) {
  _stopEndOfStream = endOfStream;
  _stopFillBytes = _streamIsFlac ? FLAC_END_FILL_BYTES : 2052;
  _stopFillByte = _driver.getEndFillByte();
  _stopStartMs = millis();
  _stopLastPollMs = 0;
  _stopQueued = 0;
  if (endOfStream) {
    // The fifo is empty at the end of a stream (that's how the end is
    // detected): queue the fill bytes, cancel once they're out.
    _fifo.fill(_stopFillByte, _stopFillBytes);
    _stopPhase = StopPhase::DrainThenCancel;
  } else {
    _feederEnabled = false; // drop the unsent rest of the old stream
    _fifo.reset();
    _driver.writeRegister(VS1053_REG_MODE,
                          VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_CANCEL);
    _stopQueued = _fifo.fill(_stopFillByte, min(2048UL, (unsigned long)_stopFillBytes));
    _stopPhase = StopPhase::Cancelling;
  }
  updateFeeder();
}

void AudioPlayer::serviceStop() {
  if (!stopping()) {
    return;
  }
  unsigned long now = millis();
  // The datasheet's own limit before falling back to a software reset:
  // DREQ can legitimately stay low for a whole FLAC frame (~100 ms).
  if (now - _stopStartMs > END_FILL_TIMEOUT_MS) {
    abortStop();
    return;
  }
  switch (_stopPhase) {
  case StopPhase::DrainThenCancel:
    if (_fifo.size() == 0) {
      _driver.writeRegister(VS1053_REG_MODE,
                            VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_CANCEL);
      _stopQueued = _fifo.fill(_stopFillByte, min(2048UL, (unsigned long)_stopFillBytes));
      _stopPhase = StopPhase::Cancelling;
    }
    break;
  case StopPhase::Cancelling:
    // Poll SM_CANCEL at most once a millisecond, and only while DREQ is
    // high: the register read otherwise waits (up to 10 ms) for it.
    if (now != _stopLastPollMs && _driver.dreqHigh()) {
      _stopLastPollMs = now;
      if (!(_driver.readRegister(VS1053_REG_MODE) & VS1053_MODE_SM_CANCEL)) {
        if (_stopEndOfStream) {
          stopDone();
        } else {
          // 10.5.2 step 5: end fill bytes after the cancel, to flush.
          _feederEnabled = false;
          _fifo.reset();
          _fifo.fill(_stopFillByte, _stopFillBytes);
          _stopPhase = StopPhase::Flushing;
          updateFeeder();
        }
        return;
      }
    }
    // Keep end fill bytes flowing until the decoder clears SM_CANCEL, up
    // to one buffer's worth (then the datasheet says: software reset).
    if (_fifo.size() < 1024) {
      if (_stopQueued < _stopFillBytes) {
        _stopQueued += _fifo.fill(
            _stopFillByte,
            min(2048UL, (unsigned long)(_stopFillBytes - _stopQueued)));
      } else if (_fifo.size() == 0) {
        abortStop();
      }
    }
    break;
  case StopPhase::Flushing:
    if (_fifo.size() == 0) {
      stopDone();
    }
    break;
  case StopPhase::None:
    break;
  }
}

void AudioPlayer::abortStop() {
  LOG_WARN("Cancel didn't complete, soft reset");
  _feederEnabled = false;
  _fifo.reset();
  _driver.softReset(); // also wipes the FLAC plugin
  stopDone();
  if (_playing && _streamIsFlac) {
    _driver.prepareFLACPlayback(); // the waiting track needs it back
  }
}

void AudioPlayer::stopDone() {
  LOG_CAT(AUDIO_BUFFER, "End fill sequence (%s) completed in %lu ms",
          _stopEndOfStream ? "end of stream" : "cancel",
          millis() - _stopStartMs);
  _stopPhase = StopPhase::None;
  updateFeeder();
  if (_playing) {
    onDecoderReady();
  } else {
    _buffer.setFrontBypass(true);
  }
}

void AudioPlayer::finishStop() {
  while (stopping()) {
    serviceStop(); // bounded by END_FILL_TIMEOUT_MS
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
  _isrBytesSent = _isrBytesSent + sent;
  if (sent) {
    audioprof::add(audioprof::FEED_ISR, t0);
    audioprof::addBytes(audioprof::FEED_ISR, sent);
  }
}

// audioYield() from long main-loop work: top the fifo up from PSRAM if
// there's room. Not while a stop is in progress (the fifo then carries end
// fill bytes) and not reentrantly.
void AudioPlayer::yieldHook() {
  static bool inside = false;
  AudioPlayer *p = _instance;
  if (!p || inside || !p->_playing || p->stopping() ||
      p->_fifo.freeSpace() < 4096) {
    return;
  }
  inside = true;
  p->_buffer.fillFrontFromPSRAM(FIFO_TOPUP_MAX_BYTES);
  inside = false;
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
    p->_isrBytesSent = p->_isrBytesSent + sent;
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
  if (p->_fifo.size() == 0 && p->_driver.dreqHigh() &&
      p->_stopPhase == StopPhase::None) {
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
  if (!_playing || stopping()) return 0;
  return (uint16_t)(_driver.getDecodeTime() + _seekOffsetSeconds);
}

void AudioPlayer::setPlaySpeed(uint16_t speed) { _driver.setPlaySpeed(speed); }

uint16_t AudioPlayer::getPlaySpeed() { return _driver.getPlaySpeed(); }

void AudioPlayer::sineTest(uint8_t freq, uint16_t duration) {
  _driver.sineTest(freq, duration);
}

void AudioPlayer::dumpRegisters() { _driver.dumpRegisters(); }
void AudioPlayer::loop() {
  serviceStop();
  // A clip is over once the feeder has sent its last byte: finish it with
  // the end fill sequence, as for the end of a file.
  if (_clipPlaying && !stopping() && _fifo.size() == 0) {
    _clipPlaying = false;
    _decoderDirty = false;
    beginStop(true);
  }
  if (_duckingForSeek && millis() >= _duckRestoreAt) {
    restoreVolumeFromDuck();
  }
  if (!_playing) return;

  // Everything has been handed to the feeder and it sent the last byte:
  // the end fill sequence (in stopPlaying) plays out what the chip holds.
  if (!stopping() && _buffer.isFileExhausted()) {
    LOG_CAT(AUDIO_BUFFER, "End of audio data reached");
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
  if (!stopping()) {
    _buffer.fillFrontFromPSRAM(FIFO_TOPUP_MAX_BYTES);
  }
  feedFromMainLoop();
  _monitorFillUs += (uint32_t)(micros() - fillUsStart);

  size_t buffered = _buffer.getBufferedBytes();
  uint32_t runway = refill::runwayMs(buffered, streamBytesPerSecond());
  if (buffered < _monitorMinBuffered) _monitorMinBuffered = buffered;
  if (buffered > _monitorMaxBuffered) _monitorMaxBuffered = buffered;
  if (runway < _monitorMinRunwayMs) _monitorMinRunwayMs = runway;

  // 2./3. SD refill or next-track prefetch, per RefillPolicy.h: refill in
  // bursts from LOW_RUNWAY_MS up to the PSRAM target (SD idle in between),
  // backing off while the user interacts unless the runway gets critical.
  // The feeder interrupt keeps playing from the fifo meanwhile.
  unsigned long loadUsStart = micros();
  unsigned long nowMs = millis();
  refill::Inputs in;
  in.runwayMs = runway;
  in.refilling = _refilling;
  in.targetReached = buffered >= AUDIO_TARGET_BUFFERED_BYTES ||
                     _buffer.isFileFullyRead() || _buffer.openFailed();
  in.userBusy = nowMs - _lastUserActivityMs < USER_ACTIVITY_HOLDOFF_MS;
  in.msSinceBusyChunk = nowMs - _lastBusyChunkMs;
  refill::Decision d = refill::decide(in);
  _refilling = d.refilling;
  if (d.load) {
    if (d.busyChunk) {
      _lastBusyChunkMs = nowMs;
    }
    _buffer.load(AUDIO_TARGET_BUFFERED_BYTES,
                 FEED_FROM_MAIN_LOOP ? std::function<void()>([this]() {
                   feedFromMainLoop();
                 })
                                     : std::function<void()>(),
                 d.budgetUs);
  } else if (d.prefetch) {
    _buffer.prefetchNextTrack();
    feedFromMainLoop();
  }
  _monitorLoadUs += (uint32_t)(micros() - loadUsStart);

  updateRateEstimate(loopEnterMs);

  if (loopEnterMs - _monitorLastReport >= AUDIO_MONITOR_INTERVAL_MS) {
    if (_monitorEnabled) {
      uint32_t logStart = audioprof::now();
      // One line a second; see docs/blog/09-10.md, "Reading the
      // diagnostics".
      LOG_CAT(AUDIO_BUFFER,
              "runway=%lu.%lus min=%lu.%lus rate=%luKB/s buf=%uKB "
              "fifoMin=%uKB underrun=%lu stall=%lums tick=%luus load=%lums "
              "chunk=%luus%s",
              (unsigned long)(runway / 1000), (unsigned long)(runway % 1000 / 100),
              (unsigned long)(_monitorMinRunwayMs / 1000),
              (unsigned long)(_monitorMinRunwayMs % 1000 / 100),
              (unsigned long)(streamBytesPerSecond() / 1000),
              (unsigned)(buffered / 1024), (unsigned)(_monitorMinFifo / 1024),
              (unsigned long)_isrUnderruns,
              (unsigned long)(_isrMaxStall * 1000 / FEEDER_HZ),
              (unsigned long)(_isrMaxTickGapCycles / (SystemCoreClock / 1000000UL)),
              (unsigned long)(_monitorLoadUs / 1000),
              (unsigned long)_buffer.getChunkCostUs(),
              _refilling ? " refilling" : "");
      audioprof::moveTo(audioprof::BENCH_IO, logStart);
    }
    resetMonitorWindow(loopEnterMs);
  }
}

void AudioPlayer::resetMonitorWindow(unsigned long nowMs) {
  _monitorLastReport = nowMs;
  _monitorMinBuffered = (size_t)-1;
  _monitorMaxBuffered = 0;
  _monitorMinRunwayMs = UINT32_MAX;
  _monitorMinFifo = AudioFifo::CAPACITY;
  _isrUnderruns = 0;
  _isrBusySkips = 0;
  _isrMaxStall = 0;
  _isrMaxTickGapCycles = 0;
  _monitorLoadUs = 0;
  _monitorFillUs = 0;
  _monitorLoopMaxMs = 0;
}

// Higher of the file's average rate (size / indexed duration) and the rate
// the feeder actually sends at: the higher one underestimates the runway,
// so the refill starts early rather than late.
uint32_t AudioPlayer::streamBytesPerSecond() {
  uint32_t fileRate = _buffer.estimatedBytesPerSecond();
  return fileRate > _measuredBytesPerSecond ? fileRate
                                            : _measuredBytesPerSecond;
}

// Once a second: bytes the feeder sent over the window, smoothed. In steady
// state the feeder sends exactly what the decoder consumes (it only sends
// while DREQ is high).
void AudioPlayer::updateRateEstimate(unsigned long nowMs) {
  unsigned long windowMs = nowMs - _rateWindowStartMs;
  if (windowMs < 1000) {
    return;
  }
  uint32_t sent = _isrBytesSent;
  uint32_t rate = (uint32_t)((uint64_t)(sent - _rateWindowStartBytes) * 1000 /
                             windowMs);
  _rateWindowStartMs = nowMs;
  _rateWindowStartBytes = sent;
  if (!_rateWindowValid) {
    _rateWindowValid = true; // first window after a start/seek: skip it
    return;
  }
  _measuredBytesPerSecond = _measuredBytesPerSecond
                                ? _measuredBytesPerSecond -
                                      _measuredBytesPerSecond / 4 + rate / 4
                                : rate;
}

void AudioPlayer::resetRateEstimate() {
  _measuredBytesPerSecond = 0;
  _rateWindowValid = false;
  _rateWindowStartMs = millis();
  _rateWindowStartBytes = _isrBytesSent;
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
  if (!_playing || stopping()) {
    return false; // no jumping while the previous stream is being stopped
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
  _rateWindowValid = false; // the window includes the end fill bytes
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

bool AudioPlayer::configureAudioQuality(uint32_t targetSampleRate) {
  LOG_INFO("Configuring audio quality for %lu Hz target sample rate",
           targetSampleRate);

  // Stop any current playback
  bool wasPlaying = _playing;
  if (wasPlaying) {
    stopPlaying();
  }
  finishStop(); // the clock change needs a stopped decoder

  // Configure optimal clock based on target sample rate
  uint32_t xtalFreq = VS1053_XTALI_12_288MHZ;

  // For very high sample rates, we might need a higher crystal frequency
  if (targetSampleRate > 96000) {
    LOG_WARN("Target sample rate > 96kHz may require 24.576MHz crystal");
    xtalFreq = VS1053_XTALI_24_576MHZ;
  }

  // Reconfigure the VS1053 with optimal settings
  bool success = _driver.configureOptimalClock(xtalFreq);

  if (success) {
    uint32_t maxSampleRate = _driver.getMaxSampleRate();
    LOG_INFO("Audio quality configured - Max sample rate: %lu Hz",
             maxSampleRate);

    if (maxSampleRate >= targetSampleRate) {
      LOG_INFO("Target sample rate %lu Hz is supported",
               targetSampleRate);
    } else {
      LOG_WARN("Target sample rate %lu Hz exceeds maximum %lu Hz",
          targetSampleRate, maxSampleRate);
    }
  } else {
    LOG_ERROR("Failed to configure audio quality");
  }

  return success;
}

bool AudioPlayer::testBasicPlayback() {
  LOG_INFO("=== VS1053 Basic Playback Test ===");

  // Test 1: Check if VS1053 is responsive
  uint16_t status = _driver.readRegister(VS1053_REG_STATUS);
  LOG_INFO("VS1053 Status: 0x%04X", status);

  if (!(status & 0x0040)) {
    LOG_ERROR("VS1053 not responding!");
    return false;
  }

  // Test 2: Check clock configuration
  uint16_t clockf = _driver.readRegister(VS1053_REG_CLOCKF);
  LOG_INFO("Clock Config: 0x%04X", clockf);

  // Test 3: Check DREQ pin
  bool dreqStatus = _driver.readyForData();
  LOG_INFO("DREQ Status: %s",
           dreqStatus ? "HIGH (Ready)" : "LOW (Busy)");

  // Test 4: Check volume setting
  uint16_t volume = _driver.readRegister(VS1053_REG_VOLUME);
  LOG_INFO("Volume: 0x%04X", volume);

  // Test 5: Simple sine wave test
  LOG_INFO("Testing sine wave generation...");
  _driver.sineTest(0x44, 500); // 500ms test tone

  LOG_INFO("Basic test completed");
  return true;
}

bool AudioPlayer::isFLACFile(const char *filename) {
  const char *ext = strrchr(filename, '.');
  return ext && (strcasecmp(ext, ".flac") == 0 || strcasecmp(ext, ".fla") == 0);
}


#include "Audio_buffer.h"

Audio_buffer::Audio_buffer(uint8_t sdCS) : _sdCS(sdCS) {
  // psram = nullptr;
  _psramBaseAddress = 0;
  resetRingBuffer();
}

Audio_buffer::~Audio_buffer() {
  // if (psram) {
    // delete psram;
    // psram = nullptr;
  // }
}

bool Audio_buffer::begin(uint32_t sdFreq) {
  if (!_sd.begin(SdSpiConfig(_sdCS, SHARED_SPI, SD_SCK_MHZ(25)))) {
    Serial.println("SD card initialization failed");
    return false;
  }
  initializePSRAM();
  return true;
}

bool Audio_buffer::initializePSRAM() {

  if (!psram.init()) {
    Serial.println("PSRAM init or test failed");
    return false;
  }
  // NOTE: must stay AUDIO_BUFFER_BASE_ADDRESS, not some other offset - the
  // ring buffer spans [base, base + _psramBufferSize), and MusicLookup's
  // index is placed right after it at MUSIC_INDEX_BASE_ADDRESS. A stray
  // offset here (there used to be a leftover 0x100000 from an older memory
  // layout) makes the two regions overlap, so streaming past a few MB of
  // audio silently corrupts the music index the player is reading tracks
  // from.
  _psramBaseAddress = AUDIO_BUFFER_BASE_ADDRESS;

  Serial.printf("PSRAM OK: %d bytes\n", _psramBufferSize);
  resetRingBuffer();
  return true;
}

void Audio_buffer::resetRingBuffer() {
  _psramHead = 0;
  _psramTail = 0;
  _bufferFull = false;
  _psramDataSize = 0;
  _psramPosition = 0;
}
uint32_t Audio_buffer::skipID3Header(FsFile &file) {
  uint8_t id3header[10];
  file.seekSet(0);
  if (file.read(id3header, 10) != 10)
    return 0;

  if (id3header[0] == 'I' && id3header[1] == 'D' && id3header[2] == '3') {
    return ((uint32_t)id3header[6] << 21) | ((uint32_t)id3header[7] << 14) |
           ((uint32_t)id3header[8] << 7) | (uint32_t)id3header[9] + 10;
  }

  return 0;
}

void Audio_buffer::setFileName(const char *filename,
                               uint32_t knownDurationSeconds) {
  if (file.isOpen()) {
    file.close();
  }
  strncpy(_currentFileName, filename, sizeof(_currentFileName) - 1);
  _currentFileName[sizeof(_currentFileName) - 1] =
      '\0';    // Ensure null-termination
  filePos = 0; // Reset file position
  _durationSeconds = knownDurationSeconds;
}
bool Audio_buffer::openFile(bool nextFile) {
  File &f = nextFile ? _nextFile : file;
  const char *fname = nextFile ? _nextFileName : _currentFileName;
  size_t &fsize = nextFile ? _nextFileSize : fileSize;

  if (!f.open(fname, O_RDONLY)) {
    Serial.printf("Can't open: %s\n", fname);
    return false;
  }
  fsize = f.size();
  return true;
}

bool Audio_buffer::closeFile() {
  if (file.isOpen()) {
    file.close();
    return true;
  }
  return false;
}

bool Audio_buffer::SDtoPSRAM(bool nextFile) {
  // Prefetching with nothing queued: refuse before even trying to open an
  // empty filename (openFile() would just fail anyway, but noisily).
  if (nextFile && _nextFileName[0] == '\0') {
    return false;
  }

  File &f = nextFile ? _nextFile : file;
  size_t &fsize = nextFile ? _nextFileSize : fileSize;
  int &fpos = nextFile ? _nextFilePos : filePos;
  uint32_t &dataStart = nextFile ? _nextDataStart : _dataStart;

  if (!f.isOpen()) {
    if (!openFile(nextFile)) {
      if (nextFile) {
        _nextFileName[0] = '\0'; // give up on this guess
      }
      return false;
    }
    // fpos is 0 here for a fresh file (both setFileName() and
    // prepareNextTrack() reset it), so this always skips the ID3 header on
    // first read; a resumed fpos (e.g. after consumeNextTrackIfMatches())
    // re-seeks to where streaming left off instead.
    dataStart = fpos ? (uint32_t)fpos : skipID3Header(f);
    fpos = (int)dataStart;
    f.seekSet(dataStart);
    if (!nextFile) {
      Serial.printf("Loading file: %s, size: %d bytes, ~%lu s\n",
                    _currentFileName, f.size(), _durationSeconds);
    }
  }

  size_t remainingFile = fsize - fpos;
  size_t freeSpace = nextFile
                         ? (NEXT_TRACK_BUFFER_SIZE - _nextPrefetchedBytes)
                         : getPSRAMFreeSpace();
  if (freeSpace == 0) {
    return false; // Destination is full, can't write more
  }

  size_t toRead =
      min(remainingFile, min((size_t)AUDIO_PRELOAD_CHUNK_SIZE, freeSpace));
  if (toRead == 0) {
    return false;
  }

  size_t read = f.read(_scratchBuffer, toRead);
  if (read == 0) {
    return false; // End of file
  }

  noInterrupts();
  if (nextFile) {
    psram.writeData(NEXT_TRACK_BUFFER_BASE_ADDRESS + _nextPrefetchedBytes,
                    _scratchBuffer, read);
  } else {
    bool wraps = (_psramHead + read) > _psramBufferSize;
    size_t writeSize = wraps ? (_psramBufferSize - _psramHead) : read;
    size_t wrapWriteSize = wraps ? (read - writeSize) : 0;
    psram.writeData(_psramBaseAddress + _psramHead, _scratchBuffer, writeSize);
    if (wrapWriteSize > 0) {
      psram.writeData(_psramBaseAddress, _scratchBuffer + writeSize,
                      wrapWriteSize);
    }
    _psramHead = (_psramHead + read) % _psramBufferSize;
    // Mark the buffer full when the head catches the tail, but keep the
    // file open while streaming (closing/reopening per refill starved the
    // decoder).
    if (_psramHead == _psramTail) {
      _bufferFull = true;
    }
    _psramDataSize += read;
    _psramPosition = 0;
  }
  interrupts();

  if (nextFile) {
    _nextPrefetchedBytes += read;
  }
  fpos += read;

  return true;
}
bool Audio_buffer::load(size_t targetBufferedBytes) {
  // Keep topping up the ring buffer (not just one 4KB chunk) so a slow
  // main-loop tick (display/wheel work) can't starve playback before the
  // next refill. Bounded to AUDIO_MAX_CHUNKS_PER_LOAD_CALL reads per call so
  // a big refill (from AUDIO_LOW_WATERMARK_BYTES up to a multi-MB target)
  // spreads across many calls/ticks instead of blocking the caller in one
  // long synchronous burst - AudioPlayer::loop() calls this every tick, so
  // it naturally keeps making progress across the following ticks.
  uint8_t chunksThisCall = 0;
  while (!_bufferFull && getPSRAMDataSize() < targetBufferedBytes &&
         chunksThisCall < AUDIO_MAX_CHUNKS_PER_LOAD_CALL) {
    if (!SDtoPSRAM()) {
      break; // buffer full or end of file
    }
    chunksThisCall++;
  }
  return true;
}

void Audio_buffer::prepareNextTrack(const char *filename,
                                    uint32_t knownDurationSeconds) {
  // Drop any previous (now-irrelevant) guess and its partial progress.
  _nextFile.close();
  strncpy(_nextFileName, filename, sizeof(_nextFileName) - 1);
  _nextFileName[sizeof(_nextFileName) - 1] = '\0';
  _nextFilePos = 0;
  _nextFileSize = 0;
  _nextDataStart = 0;
  _nextDurationSeconds = knownDurationSeconds;
  _nextPrefetchedBytes = 0;
}

bool Audio_buffer::prefetchNextTrack() {
  // Same open/skip-ID3/read-a-chunk operation SDtoPSRAM() does for the
  // current track, just aimed at the next-track prefetch region instead of
  // the main ring buffer - see SDtoPSRAM()'s nextFile branches.
  return SDtoPSRAM(true);
}

bool Audio_buffer::consumeNextTrackIfMatches(const char *filename,
                                             uint32_t knownDurationSeconds) {
  bool matches =
      _nextFileName[0] != '\0' && strcmp(_nextFileName, filename) == 0;
  bool hasPrefetchedData = matches && _nextPrefetchedBytes > 0;
  bool adopted = false;

  if (hasPrefetchedData) {
    uint32_t adoptFilePos = (uint32_t)_nextFilePos;
    uint32_t adoptDataStart = _nextDataStart;
    uint32_t adoptDuration =
        knownDurationSeconds ? knownDurationSeconds : _nextDurationSeconds;
    uint32_t adoptBytes = _nextPrefetchedBytes;

    _nextFile.close();
    closeFile();
    strncpy(_currentFileName, filename, sizeof(_currentFileName) - 1);
    _currentFileName[sizeof(_currentFileName) - 1] = '\0';
    // Don't rely on File copy-assignment to "move" the open next-track
    // handle over - SdFat handle semantics vary by version/backend. Closing
    // and reopening at the resumed position is just a directory lookup +
    // seek (not a data re-read), and is safe regardless. openFile(false)
    // reopens _currentFileName (set above) and refreshes fileSize from it -
    // the same helper SDtoPSRAM() uses for a fresh current-track open.
    if (openFile(false) && file.seekSet(adoptFilePos)) {
      filePos = (int)adoptFilePos;
      _dataStart = adoptDataStart;
      _durationSeconds = adoptDuration;

      // Copy the already-prefetched bytes into the main ring buffer, which
      // stopPlaying()'s resetRingBuffer() already emptied before
      // startPlaying() called us - a plain append from an empty ring, and
      // (per the static_assert in Audio_buffer.h) never wraps.
      uint32_t remaining = adoptBytes;
      uint32_t srcOffset = 0;
      while (remaining > 0) {
        size_t chunk =
            (size_t)min((uint32_t)AUDIO_PRELOAD_CHUNK_SIZE, remaining);
        psram.readData(NEXT_TRACK_BUFFER_BASE_ADDRESS + srcOffset,
                       _scratchBuffer, chunk);
        psram.writeData(_psramBaseAddress + _psramHead, _scratchBuffer, chunk);
        _psramHead += chunk;
        srcOffset += chunk;
        remaining -= chunk;
      }
      _psramDataSize += adoptBytes;
      Serial.printf("Adopted %lu prefetched bytes for %s\n", adoptBytes,
                    filename);
      adopted = true;
    } else {
      Serial.printf("Adopt: failed to reopen prefetched track: %s\n",
                    filename);
    }
  }

  // Whether we matched, adopted, or not: this slot's guess is now spent for
  // this transition. Free it so the caller can prepareNextTrack() again for
  // whatever follows the track that's about to start.
  _nextFile.close();
  _nextFileName[0] = '\0';
  _nextPrefetchedBytes = 0;
  _nextFilePos = 0;
  _nextFileSize = 0;
  _nextDataStart = 0;
  _nextDurationSeconds = 0;

  return adopted;
}

void Audio_buffer::invalidateNextTrack() {
  _nextFile.close();
  _nextFileName[0] = '\0';
  _nextFilePos = 0;
  _nextFileSize = 0;
  _nextDataStart = 0;
  _nextDurationSeconds = 0;
  _nextPrefetchedBytes = 0;
}

size_t Audio_buffer::readData(uint8_t *buffer, size_t maxLen) {
  if ((_psramTail == _psramHead) && !_bufferFull)
    return 0;

  size_t available = getPSRAMDataSize();
  size_t toRead = (maxLen < available) ? maxLen : available;
  
  if (toRead == 0)
    return 0;

  // Handle ring buffer wrapping
  size_t firstChunk = (toRead < (_psramBufferSize - _psramTail)) ? toRead : (_psramBufferSize - _psramTail);
  size_t secondChunk = toRead - firstChunk;

  noInterrupts();
  psram.readData(_psramBaseAddress + _psramTail, buffer, firstChunk);
  if (secondChunk > 0)
    psram.readData(_psramBaseAddress, buffer + firstChunk, secondChunk);
  interrupts();

  _psramTail = (_psramTail + toRead) % _psramBufferSize;
  _bufferFull = false;

  return toRead;
}

bool Audio_buffer::seekToSeconds(uint32_t targetSeconds) {
  if (!file.isOpen() || _durationSeconds == 0) {
    return false;
  }
  targetSeconds = min(targetSeconds, _durationSeconds);
  uint32_t audioBytes = (fileSize > _dataStart) ? (fileSize - _dataStart) : 0;
  if (audioBytes == 0) {
    return false;
  }
  uint32_t bytesPerSecond = audioBytes / max(_durationSeconds, (uint32_t)1);
  uint32_t targetPos = _dataStart + targetSeconds * bytesPerSecond;
  if (targetPos >= fileSize) {
    targetPos = fileSize > 0 ? fileSize - 1 : 0;
  }
  if (!file.seekSet(targetPos)) {
    return false;
  }
  filePos = (int)targetPos;
  // Discard stale pre-seek buffered audio.
  resetRingBuffer();
  return true;
}

size_t Audio_buffer::getPSRAMDataSize() {
  if (_bufferFull)
    return _psramBufferSize;
  return (_psramHead >= _psramTail)
             ? (_psramHead - _psramTail)
             : (_psramBufferSize - (_psramTail - _psramHead));
}
size_t Audio_buffer::getPSRAMFreeSpace() {
  return _psramBufferSize - getPSRAMDataSize();
}

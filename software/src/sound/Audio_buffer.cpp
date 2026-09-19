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

// Scans forward from dataStart for the first valid MPEG audio frame sync,
// reads its bitrate, and estimates duration as (audio bytes) / (bitrate).
// This is exact for CBR files (the common case for ripped libraries) and
// approximate for VBR files, since only the first frame's bitrate is used.
uint32_t Audio_buffer::estimateDurationSeconds(uint32_t dataStart) {
  static const uint16_t kBitrateKbpsV1L3[16] = {0,   32,  40,  48,  56,
                                                64,  80,  96,  112, 128,
                                                160, 192, 224, 256, 320, 0};
  static const uint16_t kBitrateKbpsV2L3[16] = {0,  8,  16, 24, 32,  40,
                                                48, 56, 64, 80, 96,  112,
                                                128, 144, 160, 0};
  const uint32_t maxScan = 4096;
  if (dataStart >= fileSize)
    return 0;

  file.seekSet(dataStart);
  int prevByte = -1;
  for (uint32_t scanned = 0; scanned < maxScan; scanned++) {
    int b = file.read();
    if (b < 0)
      break;
    if (prevByte == 0xFF && (b & 0xE0) == 0xE0) {
      uint8_t rest[2];
      if (file.read(rest, 2) != 2)
        break;
      uint8_t versionBits = (b >> 3) & 0x3;  // 00=v2.5, 10=v2, 11=v1
      uint8_t layerBits = (b >> 1) & 0x3;    // 01=Layer III
      uint8_t bitrateIndex = (rest[0] >> 4) & 0xF;
      if (versionBits != 0x1 && layerBits == 0x1 && bitrateIndex > 0 &&
          bitrateIndex < 15) {
        uint16_t kbps = (versionBits == 0x3) ? kBitrateKbpsV1L3[bitrateIndex]
                                             : kBitrateKbpsV2L3[bitrateIndex];
        if (kbps > 0) {
          uint32_t audioBytes = fileSize - dataStart;
          return (uint32_t)(((uint64_t)audioBytes * 8) / ((uint64_t)kbps * 1000));
        }
      }
      break; // malformed header at the first sync found; give up
    }
    prevByte = b;
  }
  return 0;
}

void Audio_buffer::setFileName(const char *filename) {
  if (file.isOpen()) {
    file.close();
  }
  strncpy(_currentFileName, filename, sizeof(_currentFileName) - 1);
  _currentFileName[sizeof(_currentFileName) - 1] =
      '\0';    // Ensure null-termination
  filePos = 0; // Reset file position
  _durationSeconds = 0;
}
bool Audio_buffer::openFile() {
  if (!file.open(_currentFileName, O_RDONLY)) {
    Serial.printf("Can't open: %s\n", _currentFileName);
    return false;
  }
  fileSize = file.size();
  return true;
}

bool Audio_buffer::closeFile() {
  if (file.isOpen()) {
    file.close();
    return true;
  }
  return false;
}

bool Audio_buffer::SDtoPSRAM() {
  if (!file.isOpen()) {
    if (!openFile()) {
      return false;
    }
    _dataStart = filePos ? (uint32_t)filePos : skipID3Header(file);
    if (!filePos) {
      _durationSeconds = estimateDurationSeconds(_dataStart);
    }
    file.seekSet(_dataStart);
    Serial.printf("Loading file: %s, size: %d bytes, ~%lu s\n",
                  _currentFileName, file.size(), _durationSeconds);
  }
  
  size_t remainingFile = fileSize - filePos;
  size_t freeSpace = getPSRAMFreeSpace();
  
  if (freeSpace == 0) {
    return false; // Buffer is full, can't write more
  }
  
  size_t toRead =
      min(remainingFile, min((size_t)AUDIO_PRELOAD_CHUNK_SIZE, freeSpace));
  static uint8_t temp[AUDIO_PRELOAD_CHUNK_SIZE];
  
  size_t read = file.read(temp, toRead);
  if (read == 0) {
    return false; // End of file
  }

  bool wraps = (_psramHead + read) > _psramBufferSize;
  size_t writeSize = wraps ? (_psramBufferSize - _psramHead) : read;
  size_t wrapWriteSize = wraps ? (read - writeSize) : 0;
  noInterrupts();
  psram.writeData(_psramBaseAddress + _psramHead, temp, writeSize);
  if (wrapWriteSize > 0) {
    psram.writeData(_psramBaseAddress, temp + writeSize, wrapWriteSize);
  }
  _psramHead = (_psramHead + read) % _psramBufferSize;
  interrupts();

  // Mark the buffer full when the head catches the tail, but keep the file
  // open while streaming (closing/reopening per refill starved the decoder).
  if (_psramHead == _psramTail) {
    _bufferFull = true;
  }

  filePos += read;
  _psramDataSize += read;
  _psramPosition = 0;
  
  return true;
}
bool Audio_buffer::load(size_t targetBufferedBytes) {
  // Keep topping up the ring buffer (not just one 4KB chunk) so a slow
  // main-loop tick (display/wheel work) can't starve playback before the
  // next refill.
  while (!_bufferFull && getPSRAMDataSize() < targetBufferedBytes) {
    if (!SDtoPSRAM()) {
      break; // buffer full or end of file
    }
  }
  return true;
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

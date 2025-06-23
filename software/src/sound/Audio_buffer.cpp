#include "Audio_buffer.h"

Audio_buffer::Audio_buffer(uint8_t sdCS) : _sdCS(sdCS) {
  _psram = nullptr;
  _psramBaseAddress = 0;
  resetRingBuffer();
}

Audio_buffer::~Audio_buffer() {
  if (_psram) {
    delete _psram;
    _psram = nullptr;
  }
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
  if (_psram)
    return true;

  _psram = new SPI_PSRAM();
  if (!_psram->init()) {
    Serial.println("PSRAM init or test failed");
    delete _psram;
    _psram = nullptr;
    return false;
  }

  _psramBaseAddress = 0x100000;

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
void Audio_buffer::setFileName(const char *filename) {
  strncpy(_currentFileName, filename, sizeof(_currentFileName) - 1);
  _currentFileName[sizeof(_currentFileName) - 1] =
      '\0';    // Ensure null-termination
  filePos = 0; // Reset file position
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
    file.seekSet(filePos ? filePos : skipID3Header(file));
    Serial.printf("Loading file: %s, size: %d bytes\n", _currentFileName,
                  file.size());
  }
  size_t remainingFile = fileSize - filePos;
  // Calculate free space in ring buffer
  size_t freeSpace = getPSRAMFreeSpace();
  if (freeSpace == 0) {
    file.close();
    return false; // Buffer is full, can't write more
  }
  size_t toRead =
      min(remainingFile, min((size_t)AUDIO_PRELOAD_CHUNK_SIZE, freeSpace));
  size_t read = file.read(temp, toRead);

  bool wraps = (_psramHead + read) > _psramBufferSize;
  size_t writeSize = wraps ? (_psramBufferSize - _psramHead) : read;
  size_t wrapWriteSize = wraps ? (read - writeSize) : 0;
  noInterrupts();
  _psram->writeData(_psramBaseAddress + _psramHead, temp, writeSize);
  if (wrapWriteSize > 0) {
    _psram->writeData(_psramBaseAddress, temp + writeSize, wrapWriteSize);
  }
  _psramHead = (_psramHead + read) % _psramBufferSize;
  interrupts();
  // Set buffer full flag if head catches up to tail
  if (_psramHead == _psramTail) {
    closeFile();
    _bufferFull = true;
  }

  filePos += read;

  _psramDataSize += read;
  _psramPosition = 0;
  return true;
}
bool Audio_buffer::load() {
  if (!_psram)
    return false;

  if (_bufferFull && getPSRAMDataSize() > (_psramBufferSize / 10))
    return true;
  SDtoPSRAM();
  return true;
}

size_t Audio_buffer::readData(uint8_t *buffer, size_t maxLen) {
  if (!_psram)
    return 0;

  if ((_psramTail == _psramHead) && !_bufferFull)
    return 0;

  size_t available = getPSRAMDataSize();

  size_t toRead = min(maxLen, available);
  if (toRead == 0)
    return 0;

  size_t firstChunk = min(toRead, (size_t)(_psramBufferSize - _psramTail));
  size_t secondChunk = toRead - firstChunk;

  noInterrupts();
  _psram->readData(_psramBaseAddress + _psramTail, buffer, firstChunk);
  if (secondChunk)
    _psram->readData(_psramBaseAddress, buffer + firstChunk, secondChunk);
  interrupts();

  _psramTail = (_psramTail + toRead) % _psramBufferSize;
  _bufferFull = false;

  return toRead;
}

size_t Audio_buffer::getPSRAMDataSize() {
  return _bufferFull ? 0
         : (_psramHead >= _psramTail)
             ? (_psramHead - _psramTail)
             : (_psramBufferSize - (_psramTail - _psramHead));
}
size_t Audio_buffer::getPSRAMFreeSpace() {
  return _psramBufferSize - getPSRAMDataSize();
}

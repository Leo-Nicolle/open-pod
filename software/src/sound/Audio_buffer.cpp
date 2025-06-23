/*!
 * @file Audio_buffer.cpp
 * Audio buffer management implementation for PSRAM and SD card file loading
 */

#include "Audio_buffer.h"

Audio_buffer::Audio_buffer(uint8_t sdCS) : _sdCS(sdCS) {
#ifdef HAS_PSRAM_SUPPORT
  _psramEnabled = false;
  _filePreloaded = false;
  _psramBufferSize = 0;
  _psramDataSize = 0;
  _psramPosition = 0;
  _psramController = nullptr;
  _psramBaseAddress = 0;
#endif
}

Audio_buffer::~Audio_buffer() {
  closeFile();
#ifdef HAS_PSRAM_SUPPORT
  freePSRAM();
#endif
}

bool Audio_buffer::begin(uint32_t sdFreq) {
  // Initialize SD card
  if (!_sd.begin(SdSpiConfig(_sdCS, SHARED_SPI, SD_SCK_MHZ(25)))) {
    Serial.println("SD card initialization failed");
    return false;
  }
  
#ifdef HAS_PSRAM_SUPPORT
  initializePSRAM();
#endif
  return true;
}

bool Audio_buffer::isPSRAMAvailable() {
#ifdef HAS_PSRAM_SUPPORT
  return (_psramController != nullptr && _psramEnabled);
#else
  return false;
#endif
}

bool Audio_buffer::enablePSRAM(bool enable) {
#ifdef HAS_PSRAM_SUPPORT
  if (enable && !_psramEnabled) {
    return initializePSRAM();
  } else if (!enable && _psramEnabled) {
    freePSRAM();
    _psramEnabled = false;
  }
  return _psramEnabled;
#else
  return false;
#endif
}

#ifdef HAS_PSRAM_SUPPORT
bool Audio_buffer::initializePSRAM() {
  if (_psramController) return true;

  _psramController = new SPI_PSRAM();
  if (!_psramController->init()) {
    Serial.println("PSRAM init failed");
    delete _psramController;
    _psramController = nullptr;
    return false;
  }

  if (!_psramController->testExtended()) {
    Serial.println("PSRAM test failed");
    delete _psramController;
    _psramController = nullptr;
    return false;
  }

  _psramBaseAddress = 0x100000;
  _psramBufferSize = AUDIO_PSRAM_BUFFER_SIZE;
  _psramEnabled = true;

  Serial.printf("PSRAM OK: %d bytes\n", _psramBufferSize);
  return true;
}

void Audio_buffer::freePSRAM() {
  if (_psramController) {
    delete _psramController;
    _psramController = nullptr;
    _psramBufferSize = 0;
    _psramDataSize = 0;
    _psramPosition = 0;
    _filePreloaded = false;
    _psramBaseAddress = 0;
  }
}

bool Audio_buffer::preloadFile(const char *filename) {
  if (!_psramEnabled || !_psramController) return false;

  FsFile file;
  if (!file.open(filename, O_RDONLY)) {
    Serial.printf("Can't open: %s\n", filename);
    return false;
  }

  uint32_t dataStart = skipID3Header(file);
  file.seekSet(dataStart);

  size_t fileSize = file.size() - dataStart;
  size_t bytesToRead = min(fileSize, (size_t)_psramBufferSize);

  Serial.printf("Preloading %d bytes...\n", bytesToRead);

  size_t totalRead = 0;
  uint8_t tempBuffer[AUDIO_PRELOAD_CHUNK_SIZE];

  while (totalRead < bytesToRead) {
    size_t chunkSize = min((size_t)AUDIO_PRELOAD_CHUNK_SIZE, bytesToRead - totalRead);
    size_t bytesRead = file.read(tempBuffer, chunkSize);
    if (bytesRead == 0) break;

    // Atomic PSRAM write
    noInterrupts();
    _psramController->writeData(_psramBaseAddress + totalRead, tempBuffer, bytesRead);
    interrupts();

    totalRead += bytesRead;
    
    if (totalRead % (AUDIO_PRELOAD_CHUNK_SIZE * 4) == 0) {
      yield();
    }
  }

  file.close();
  _psramDataSize = totalRead;
  _psramPosition = 0;
  _filePreloaded = true;

  Serial.printf("Preloaded: %d bytes\n", _psramDataSize);
  return true;
}
#endif

bool Audio_buffer::openFile(const char *filename) {
  closeFile();

  if (!_currentFile.open(filename, O_RDONLY)) {
    Serial.printf("Failed to open: %s\n", filename);
    return false;
  }

  uint32_t dataStart = skipID3Header(_currentFile);
  _currentFile.seekSet(dataStart);
  return true;
}

void Audio_buffer::closeFile() {
  if (_currentFile.isOpen()) {
    _currentFile.close();
  }
}

bool Audio_buffer::isFileOpen() {
  return _currentFile.isOpen();
}

size_t Audio_buffer::getFileSize() {
  if (_currentFile.isOpen()) {
    return _currentFile.size();
  }
  return 0;
}

void Audio_buffer::clearPreloadBuffer() {
#ifdef HAS_PSRAM_SUPPORT
  _filePreloaded = false;
  _psramPosition = 0;
  _psramDataSize = 0;
#endif
}

bool Audio_buffer::isFilePreloaded() {
#ifdef HAS_PSRAM_SUPPORT
  return _filePreloaded;
#else
  return false;
#endif
}

size_t Audio_buffer::getPreloadedSize() {
#ifdef HAS_PSRAM_SUPPORT
  return _psramDataSize;
#else
  return 0;
#endif
}

size_t Audio_buffer::getPreloadedRemaining() {
#ifdef HAS_PSRAM_SUPPORT
  if (_filePreloaded && _psramDataSize > _psramPosition) {
    return _psramDataSize - _psramPosition;
  }
#endif
  return 0;
}

size_t Audio_buffer::readData(uint8_t* buffer, size_t maxLen) {
#ifdef HAS_PSRAM_SUPPORT
  if (_filePreloaded) {
    return readFromPSRAM(buffer, maxLen);
  } else {
#endif
    return readFromFile(buffer, maxLen);
#ifdef HAS_PSRAM_SUPPORT
  }
#endif
}

void Audio_buffer::seekToStart() {
#ifdef HAS_PSRAM_SUPPORT
  if (_filePreloaded) {
    _psramPosition = 0;
  } else {
#endif
    if (_currentFile.isOpen()) {
      uint32_t dataStart = skipID3Header(_currentFile);
      _currentFile.seekSet(dataStart);
    }
#ifdef HAS_PSRAM_SUPPORT
  }
#endif
}

bool Audio_buffer::isEndOfData() {
#ifdef HAS_PSRAM_SUPPORT
  if (_filePreloaded) {
    return _psramPosition >= _psramDataSize;
  } else {
#endif
    return !_currentFile.isOpen() || _currentFile.available() == 0;
#ifdef HAS_PSRAM_SUPPORT
  }
#endif
}

#ifdef HAS_PSRAM_SUPPORT
size_t Audio_buffer::readFromPSRAM(uint8_t* buffer, size_t maxLen) {
  if (_psramPosition >= _psramDataSize) {
    return 0; // End of data
  }

  size_t bytesToRead = min((uint32_t)maxLen, _psramDataSize - _psramPosition);
  
  // Atomic PSRAM read
  noInterrupts();
  _psramController->readData(_psramBaseAddress + _psramPosition, buffer, bytesToRead);
  interrupts();
  
  _psramPosition += bytesToRead;
  return bytesToRead;
}
#endif

size_t Audio_buffer::readFromFile(uint8_t* buffer, size_t maxLen) {
  if (!_currentFile.isOpen()) {
    return 0;
  }

  return _currentFile.read(buffer, maxLen);
}

uint32_t Audio_buffer::skipID3Header(FsFile &file) {
  uint8_t id3header[10];
  uint32_t position = 0;

  file.seekSet(0);
  if (file.read(id3header, 10) == 10) {
    if (id3header[0] == 'I' && id3header[1] == 'D' && id3header[2] == '3') {
      uint32_t tagSize = ((uint32_t)id3header[6] << 21) |
                         ((uint32_t)id3header[7] << 14) |
                         ((uint32_t)id3header[8] << 7) | 
                         (uint32_t)id3header[9];
      position = tagSize + 10;
      Serial.printf("Skipping ID3: %d bytes\n", position);
    }
  }

  file.seekSet(position);
  return position;
}

bool Audio_buffer::isMP3File(const char *filename) {
  const char *ext = strrchr(filename, '.');
  return ext && (strcasecmp(ext, ".mp3") == 0);
}
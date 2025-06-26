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
  size_t freeSpace = getPSRAMFreeSpace();
  
  if (freeSpace == 0) {
    file.close();
    return false; // Buffer is full, can't write more
  }
  
  // Use large DMA buffer for efficient transfers
  size_t toRead = min(remainingFile, min((size_t)AUDIO_PRELOAD_CHUNK_SIZE, freeSpace));
  
  // Get direct access to DMA write buffer (skip 4 bytes for PSRAM command header)
  uint8_t* dmaBuffer = psram.getDMAWriteBuffer() + 4;
  
  unsigned long t0 = micros();
  size_t read = file.read(dmaBuffer, toRead);
  unsigned long t1 = micros();
  
  if (read == 0) {
    file.close();
    return false;
  }
  
  // Handle ring buffer wrapping efficiently
  bool wraps = (_psramHead + read) > _psramBufferSize;
  noInterrupts();
  if (wraps) {
    // Split write across buffer boundary
    size_t writeSize = _psramBufferSize - _psramHead;
    size_t wrapWriteSize = read - writeSize;
    
    // First chunk to end of buffer
    psram.writeData(_psramBaseAddress + _psramHead, dmaBuffer, writeSize, false);
    // Second chunk to beginning of buffer
    if (wrapWriteSize > 0) {
      psram.writeData(_psramBaseAddress, dmaBuffer + writeSize, wrapWriteSize,false);
    }
  } else {
    // Single contiguous write
    psram.writeData(_psramBaseAddress + _psramHead, dmaBuffer, read, false);
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
  if (!psram.isInitialized())
    return false;

  if (_bufferFull && getPSRAMDataSize() > (_psramBufferSize / 10))
    return true;
  SDtoPSRAM();
  return true;
}

size_t Audio_buffer::readData(uint8_t *buffer, size_t maxLen) {
  if (!psram.isInitialized()) 
    return 0;

  if ((_psramTail == _psramHead) && !_bufferFull)
    return 0;

  size_t available = getPSRAMDataSize();
  size_t toRead = min(maxLen, available);
  
  if (toRead == 0)
    return 0;

  // For small reads (typical audio chunks), use direct transfer
  if (toRead <= AUDIO_DATABUFFERLEN) {
    size_t firstChunk = min(toRead, (size_t)(_psramBufferSize - _psramTail));
    size_t secondChunk = toRead - firstChunk;

    noInterrupts();
    psram.readData(_psramBaseAddress + _psramTail, buffer, firstChunk);
    if (secondChunk)
      psram.readData(_psramBaseAddress, buffer + firstChunk, secondChunk);
    interrupts();
  }
  // For larger reads, use DMA buffer for efficiency
  else {
    uint8_t* dmaReadBuf = psram.getDMAReadBuffer();
    size_t totalRead = 0;
    
    while (totalRead < toRead) {
      size_t chunkSize = min(toRead - totalRead, (size_t)AUDIO_LARGE_READ_SIZE);
      size_t currentTail = (_psramTail + totalRead) % _psramBufferSize;
      
      size_t firstChunk = min(chunkSize, (size_t)(_psramBufferSize - currentTail));
      size_t secondChunk = chunkSize - firstChunk;
      
      noInterrupts();
      // Read into DMA buffer first
      psram.readData(_psramBaseAddress + currentTail, dmaReadBuf, firstChunk);
      if (secondChunk)
        psram.readData(_psramBaseAddress, dmaReadBuf + firstChunk, secondChunk);
      interrupts();
      
      // Copy from DMA buffer to user buffer
      memcpy(buffer + totalRead, dmaReadBuf, chunkSize);
      totalRead += chunkSize;
    }
  }

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

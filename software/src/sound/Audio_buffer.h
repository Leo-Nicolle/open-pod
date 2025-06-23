/*!
 * @file Audio_buffer.h
 * @brief Audio buffer manager using PSRAM and SD card files (optimized for VS1053b)
 */

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <Arduino.h>
#include <SdFat.h>

#include "../storage/PSRAM_controller.hpp"

// Buffer configuration optimized for VS1053b
#define AUDIO_DATABUFFERLEN 32                // Matches VS1053_BURST_SIZE for feeding
#define AUDIO_PRELOAD_CHUNK_SIZE 32         // Chunk size for preloading (VS1053b FIFO)

/*!
 * @class Audio_buffer
 * @brief Manages audio streaming from SD card using a circular buffer in PSRAM
 */
class Audio_buffer {
public:
  explicit Audio_buffer(uint8_t sdCS);
  ~Audio_buffer();

  // Initialization
  bool begin(uint32_t sdFreq = 25000000);

  void setFileName(const char *filename);
  bool load();
  size_t readData(uint8_t *buffer, size_t maxLen);
  SdFat &getSD() { return _sd; }
  void resetRingBuffer();

protected:
  // SD card
  uint8_t _sdCS;
  SdFat _sd;
  File file;
  size_t fileSize;
  char _currentFileName[64]; 
  uint8_t _dataBuffer[AUDIO_DATABUFFERLEN];
  // temp to read from SD card to PSRAM
  uint8_t temp[AUDIO_PRELOAD_CHUNK_SIZE];
  int filePos = 0; // Current position in the file
  SPI_PSRAM* _psram = nullptr;
  uint32_t _psramBaseAddress = 0;
  static const uint32_t _psramBufferSize = AUDIO_BUFFER_SIZE;

  // Ring buffer positions
  size_t _psramHead = 0;   // Next write position
  size_t _psramTail = 0;   // Next read position
  bool _bufferFull = false;
  // Preload tracking
  uint32_t _psramDataSize = 0; // Total preloaded data size
  uint32_t _psramPosition = 0; // Current position in preloaded data

  // Internals
  bool initializePSRAM();
  bool openFile();
  bool closeFile();
  bool SDtoPSRAM();
  uint32_t skipID3Header(FsFile &file);
  size_t getPSRAMDataSize(); // Returns the size of data in PSRAM buffer 
  size_t getPSRAMFreeSpace(); // Returns the free space in PSRAM buffer
};

#endif // AUDIO_BUFFER_H

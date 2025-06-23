/*!
 * @file Audio_buffer.h
 * Audio buffer management for PSRAM and SD card file loading
 */

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <Arduino.h>
#include <SdFat.h>

#define HAS_PSRAM_SUPPORT
#include "../storage/PSRAM_controller.hpp"

// Buffer configuration optimized for VS1053b
#define AUDIO_DATABUFFERLEN 32        // Matches VS1053_BURST_SIZE for optimal feeding
#define AUDIO_PSRAM_BUFFER_SIZE (1024 * 1024)  // 1MB for PSRAM
#define AUDIO_PRELOAD_CHUNK_SIZE 2048          // Matches VS1053b FIFO size
#define AUDIO_VS1053_FIFO_SIZE 2048            // VS1053b internal FIFO size

/*!
 * @brief Audio buffer management class for PSRAM and SD card operations
 */
class Audio_buffer {
public:
  Audio_buffer(uint8_t sdCS);
  ~Audio_buffer();

  bool begin(uint32_t sdFreq = 25000000);
  bool enablePSRAM(bool enable = true);
  bool isPSRAMAvailable();
  
  // File operations
  bool openFile(const char* filename);
  void closeFile();
  bool isFileOpen();
  size_t getFileSize();
  
  // PSRAM operations
  bool preloadFile(const char* filename);
  void clearPreloadBuffer();
  bool isFilePreloaded();
  size_t getPreloadedSize();
  size_t getPreloadedRemaining();
  
  // Data reading
  size_t readData(uint8_t* buffer, size_t maxLen);
  void seekToStart();
  bool isEndOfData();
  
  // Utility functions
  static bool isMP3File(const char* filename);
  SdFat& getSD() { return _sd; }

protected:
  uint8_t _sdCS;
  SdFat _sd;
  FsFile _currentFile;
  
  // Data buffer for reading
  uint8_t _dataBuffer[AUDIO_DATABUFFERLEN];
  
#ifdef HAS_PSRAM_SUPPORT
  bool _psramEnabled;
  bool _filePreloaded;
  uint32_t _psramBufferSize;
  uint32_t _psramDataSize;
  uint32_t _psramPosition;
  SPI_PSRAM* _psramController;
  uint32_t _psramBaseAddress;
#endif

  bool initializePSRAM();
  void freePSRAM();
  uint32_t skipID3Header(FsFile& file);
  size_t readFromPSRAM(uint8_t* buffer, size_t maxLen);
  size_t readFromFile(uint8_t* buffer, size_t maxLen);
};

#endif // AUDIO_BUFFER_H
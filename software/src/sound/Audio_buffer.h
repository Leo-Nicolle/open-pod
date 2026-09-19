/*!
 * @file Audio_buffer.h
 * @brief Audio buffer manager using PSRAM and SD card files (optimized for VS1053b)
 */

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <Arduino.h>
#include <SdFat.h>
#include "../pinout.h"

#include "../storage/PSRAM_controller.hpp"

// Buffer configuration optimized for VS1053b
#define AUDIO_DATABUFFERLEN 32                // Matches VS1053_BURST_SIZE for feeding
#define AUDIO_PRELOAD_CHUNK_SIZE 4096         // 4KB chunks for efficient transfers
#define AUDIO_LARGE_READ_SIZE 4096            // For PSRAM reads
// load() tops the ring buffer up to this many buffered bytes every time it's
// called. A single 4KB chunk per main-loop tick can't outrun the drain once
// display/wheel work makes a tick slow, so keep several seconds of headroom.
#define AUDIO_TARGET_BUFFERED_BYTES (256 * 1024)
// Bounded refill used right after a seek: just enough to bridge until the
// next couple of loop() ticks top the buffer back up to the full target
// above. Filling all the way to AUDIO_TARGET_BUFFERED_BYTES synchronously
// mid-playback blocks the main loop for many SD reads, during which nothing
// feeds the VS1053 - its small internal FIFO drains dry and produces an
// audible gap.
#define AUDIO_SEEK_PRIME_BYTES (32 * 1024)

/*!
 * @class Audio_buffer
 * @brief Manages audio streaming from SD card using a circular buffer in PSRAM
 */
class SDToPSRAMTest; // Forward declaration

class Audio_buffer {
public:
  explicit Audio_buffer(uint8_t sdCS);
  ~Audio_buffer();

  // Initialization
  bool begin(uint32_t sdFreq = 25000000);

  void setFileName(const char *filename);
  // Tops the ring buffer up to targetBufferedBytes (default: the full
  // streaming target). Pass a smaller value for a bounded refill, e.g.
  // right after a seek (see AUDIO_SEEK_PRIME_BYTES).
  bool load(size_t targetBufferedBytes = AUDIO_TARGET_BUFFERED_BYTES);
  size_t readData(uint8_t *buffer, size_t maxLen);
  SdFat &getSD() { return _sd; }
  void resetRingBuffer();
  // Estimated total track length in seconds (0 if not yet known/parseable).
  // Assumes CBR; VBR files will be approximate.
  uint32_t getDurationSeconds() const { return _durationSeconds; }
  // Jumps to an approximate byte offset for targetSeconds (same CBR-linear
  // assumption as getDurationSeconds; may land mid MPEG-frame, causing a
  // brief decode glitch until the VS1053 resyncs). Discards buffered audio.
  bool seekToSeconds(uint32_t targetSeconds);

  friend class SDToPSRAMTest; // Allow SDToPSRAMTest to access private/protected members

protected:
  // SD card
  uint8_t _sdCS;
  SdFat _sd;
  File file;
  size_t fileSize;
  char _currentFileName[64];
  uint8_t _dataBuffer[AUDIO_DATABUFFERLEN];
  // No more temp buffer - we'll use DMA buffers directly
  int filePos = 0; // Current position in the file
  // SPI_PSRAM _psram = nullptr;
  uint32_t _psramBaseAddress = 0;
  static const uint32_t _psramBufferSize = AUDIO_BUFFER_SIZE;

  // Ring buffer positions
  size_t _psramHead = 0;   // Next write position
  size_t _psramTail = 0;   // Next read position
  bool _bufferFull = false;
  // Preload tracking
  uint32_t _psramDataSize = 0; // Total preloaded data size
  uint32_t _psramPosition = 0; // Current position in preloaded data
  uint32_t _durationSeconds = 0; // Estimated track length, set on file open
  uint32_t _dataStart = 0;     // First audio byte offset (past ID3 header)

  // Internals
  bool initializePSRAM();
  bool openFile();
  bool closeFile();
  bool SDtoPSRAM();
  uint32_t skipID3Header(FsFile &file);
  uint32_t estimateDurationSeconds(uint32_t dataStart);
  size_t getPSRAMDataSize(); // Returns the size of data in PSRAM buffer
  size_t getPSRAMFreeSpace(); // Returns the free space in PSRAM buffer
};

#endif // AUDIO_BUFFER_H

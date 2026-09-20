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
// load() tops the ring buffer up to this many buffered bytes once triggered.
// Sized to use most of the 5 MB AUDIO_BUFFER_SIZE reservation (leaving some
// headroom rather than the exact byte capacity). See memory-improvements.md.
#define AUDIO_TARGET_BUFFERED_BYTES (4608UL * 1024UL) // 4.5 MB
// AudioPlayer::loop() only triggers a refill once the buffer has drained
// below this low watermark, instead of topping up on every tick. This is
// what actually creates a long SD-idle valley between big refill bursts,
// instead of the buffer hovering right at the target via continuous tiny
// top-ups (which is what a single watermark would do, since draining and
// refilling both happen every tick).
#define AUDIO_LOW_WATERMARK_BYTES (AUDIO_TARGET_BUFFERED_BYTES / 8) // ~12.5%
// Each load() call performs at most this many SD reads before returning, so
// a large refill (from the low watermark up to the target) spreads across
// many main-loop ticks instead of blocking it in one big synchronous burst.
#define AUDIO_MAX_CHUNKS_PER_LOAD_CALL 4
// Bounded refill used right after a seek: just enough to bridge until the
// next couple of loop() ticks top the buffer back up to the full target
// above. Filling all the way to AUDIO_TARGET_BUFFERED_BYTES synchronously
// mid-playback blocks the main loop for many SD reads, during which nothing
// feeds the VS1053 - its small internal FIFO drains dry and produces an
// audible gap.
#define AUDIO_SEEK_PRIME_BYTES (32 * 1024)

// consumeNextTrackIfMatches() copies an already-prefetched next track into
// the (empty) main ring buffer assuming it can never wrap - guard that
// invariant here rather than only in a comment.
static_assert(NEXT_TRACK_BUFFER_SIZE <= AUDIO_BUFFER_SIZE,
             "a full next-track prefetch must always fit in the empty ring "
             "buffer without wrapping");

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

  // knownDurationSeconds, when > 0, comes from the indexer's precomputed
  // duration index; duration is otherwise left unknown (0) - there is no
  // on-device fallback estimate.
  void setFileName(const char *filename, uint32_t knownDurationSeconds = 0);
  // Tops the ring buffer up to targetBufferedBytes (default: the full
  // streaming target). Pass a smaller value for a bounded refill, e.g.
  // right after a seek (see AUDIO_SEEK_PRIME_BYTES).
  bool load(size_t targetBufferedBytes = AUDIO_TARGET_BUFFERED_BYTES);
  size_t readData(uint8_t *buffer, size_t maxLen);
  SdFat &getSD() { return _sd; }
  void resetRingBuffer();
  // Track length in seconds as supplied via setFileName()/prepareNextTrack()
  // (from the indexer's precomputed duration index); 0 if not known.
  uint32_t getDurationSeconds() const { return _durationSeconds; }
  // Jumps to an approximate byte offset for targetSeconds (same CBR-linear
  // assumption as getDurationSeconds; may land mid MPEG-frame, causing a
  // brief decode glitch until the VS1053 resyncs). Discards buffered audio.
  bool seekToSeconds(uint32_t targetSeconds);

  // Public wrapper so AudioPlayer can implement refill hysteresis (only
  // calling load() below AUDIO_LOW_WATERMARK_BYTES) without reaching into
  // protected ring-buffer internals.
  size_t getBufferedBytes() { return getPSRAMDataSize(); }

  // --- Next-track prefetch (see memory-improvements.md §3) ---
  // Call once the current track starts playing, with the filename/duration
  // of whatever comes next (if known). Begins background prefetching of its
  // opening bytes into the dedicated NEXT_TRACK_BUFFER_SIZE region via
  // prefetchNextTrack(), so a later track change usually needs zero fresh
  // SD activity. Replaces any previous (now-stale) prefetch guess.
  void prepareNextTrack(const char *filename, uint32_t knownDurationSeconds = 0);
  // Pulls up to one AUDIO_PRELOAD_CHUNK_SIZE chunk of the prepared next
  // track into its prefetch region. Call opportunistically (e.g. from
  // AudioPlayer::loop() only while the main buffer is healthy) so this
  // never competes with the current track's own refill for the SPI bus.
  // Returns true if there's more to prefetch, false once done/nothing queued.
  bool prefetchNextTrack();
  // If filename matches what prepareNextTrack() was given AND at least some
  // of it has been prefetched, adopts it as the new current file (reopening
  // it at the resumed position) and copies the prefetched bytes into the
  // now-empty main ring buffer - skipping a fresh SD read for that portion.
  // Always clears the prefetch slot (right guess or wrong) so the caller can
  // prepareNextTrack() again for whatever follows. Returns whether it adopted.
  bool consumeNextTrackIfMatches(const char *filename, uint32_t knownDurationSeconds);
  // Discards any in-progress/prepared next-track prefetch (e.g. a user skip
  // that invalidates the sequential-next guess before it's ever consumed).
  void invalidateNextTrack();

  friend class SDToPSRAMTest; // Allow SDToPSRAMTest to access private/protected members

protected:
  // SD card
  uint8_t _sdCS;
  SdFat _sd;
  File file;
  size_t fileSize;
  char _currentFileName[64];
  uint8_t _dataBuffer[AUDIO_DATABUFFERLEN];
  // Shared scratch buffer for SD<->PSRAM chunks (SDtoPSRAM) and PSRAM<->PSRAM
  // chunked copies (consumeNextTrackIfMatches). Never used concurrently -
  // everything here runs from the single-threaded main loop.
  uint8_t _scratchBuffer[AUDIO_PRELOAD_CHUNK_SIZE];
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

  // --- Next-track prefetch state (linear, not a ring - filled once from
  // byte 0, consumed once at adoption time) ---
  File _nextFile;
  char _nextFileName[64] = {0};
  size_t _nextFileSize = 0;
  int _nextFilePos = 0;
  uint32_t _nextDataStart = 0;
  uint32_t _nextDurationSeconds = 0;
  uint32_t _nextPrefetchedBytes = 0; // Bytes already sitting in the prefetch region

  // Internals
  bool initializePSRAM();
  // Opens the current (nextFile=false) or prefetch (nextFile=true) file
  // handle and records its size into the matching fileSize/_nextFileSize.
  bool openFile(bool nextFile = false);
  bool closeFile();
  // Reads one chunk from SD into the current track's ring buffer
  // (nextFile=false) or the next-track prefetch region (nextFile=true).
  // Both are otherwise the same operation - open-if-needed, skip the ID3
  // header once, read a chunk, advance the matching position - just against
  // a different file handle and a different PSRAM destination/capacity.
  bool SDtoPSRAM(bool nextFile = false);
  uint32_t skipID3Header(FsFile &file);
  size_t getPSRAMDataSize(); // Returns the size of data in PSRAM buffer
  size_t getPSRAMFreeSpace(); // Returns the free space in PSRAM buffer
};

#endif // AUDIO_BUFFER_H

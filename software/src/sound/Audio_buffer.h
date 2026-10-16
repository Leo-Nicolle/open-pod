/*!
 * @file Audio_buffer.h
 * @brief Audio buffer manager using PSRAM and SD card files (optimized for VS1053b)
 */

#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <Arduino.h>
#include <SdFat.h>
#include <functional>
#include "../pinout.h"
#include "AudioFifo.h"

#include "../storage/PSRAM_controller.hpp"

// Buffer configuration optimized for VS1053b
#define AUDIO_DATABUFFERLEN 32                // Matches VS1053_BURST_SIZE for feeding
#define AUDIO_PRELOAD_CHUNK_SIZE 4096         // 4KB chunks for efficient transfers
#define AUDIO_LARGE_READ_SIZE 4096            // For PSRAM reads
// load() tops the ring buffer up to this many buffered bytes once triggered.
// Sized to use most of the 4 MB AUDIO_BUFFER_SIZE reservation (leaving some
// headroom rather than the exact byte capacity, same ~0.5 MB margin as
// before 1 MB was reallocated to COVER_BUFFER_SIZE - see
// PSRAM_controller.hpp and docs/blog/09-10.md).
#define AUDIO_TARGET_BUFFERED_BYTES (3584UL * 1024UL) // 3.5 MB
// Safety cap on the chunks one load() call may read, whatever its budget.
#define AUDIO_MAX_CHUNKS_PER_LOAD_CALL 16
// Default load() budget: the most one call may spend (prime, seek, a low
// buffer). The refill policy (RefillPolicy.h) passes smaller budgets.
#define AUDIO_FULL_LOAD_BUDGET_US 40000UL
// Bounded refill used right after a seek: just enough to bridge until the
// next couple of loop() ticks top the buffer back up to the full target
// above. Filling all the way to AUDIO_TARGET_BUFFERED_BYTES synchronously
// mid-playback blocks the main loop for many SD reads, during which nothing
// feeds the VS1053 - its small internal FIFO drains dry and produces an
// audible gap.
#define AUDIO_SEEK_PRIME_BYTES (32 * 1024)
// consumeNextTrackIfMatches() adopts at most this much of a prefetch (one
// L1 fifo's worth, ~25 ms of PSRAM reads).
#define NEXT_TRACK_ADOPT_MAX_BYTES (32 * 1024)

// consumeNextTrackIfMatches() copies an already-prefetched next track into
// the (empty) main ring buffer assuming it can never wrap - guard that
// invariant here rather than only in a comment.
static_assert(NEXT_TRACK_BUFFER_SIZE <= AUDIO_BUFFER_SIZE,
             "a full next-track prefetch must always fit in the empty ring "
             "buffer without wrapping");

// AUDIO_TARGET_BUFFERED_BYTES is a hand-picked literal, not derived from
// AUDIO_BUFFER_SIZE - catch it silently exceeding the buffer's actual
// physical capacity if that reservation ever shrinks again.
static_assert(AUDIO_TARGET_BUFFERED_BYTES < AUDIO_BUFFER_SIZE,
             "the ring buffer must never be asked to fill past its own "
             "physical PSRAM reservation");

/*!
 * @class Audio_buffer
 * @brief Manages audio streaming from SD card using a circular buffer in PSRAM
 */
class SDToPSRAMTest; // Forward declaration

class Audio_buffer {
public:
  // The SD -> PSRAM chunk buffer, lent to the index page cache
  // (PageStore::setScratch): both only use it within one call of their own,
  // and audioYield() (the only thing a page load calls out to) doesn't.
  uint8_t *scratch() { return _scratchBuffer; }
  static const uint32_t SCRATCH_SIZE = AUDIO_PRELOAD_CHUNK_SIZE;
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
  //
  // onChunkLoaded, if given, runs after every chunk (the main-loop feeding
  // diagnostic uses it; the feeder interrupt doesn't need it).
  //
  // budgetUs: the call always reads one chunk, then more only while
  // elapsed + getChunkCostUs() fits in the budget (and at most
  // AUDIO_MAX_CHUNKS_PER_LOAD_CALL). A budget of 1 means exactly one chunk.
  bool load(size_t targetBufferedBytes = AUDIO_TARGET_BUFFERED_BYTES,
           const std::function<void()> &onChunkLoaded = nullptr,
           uint32_t budgetUs = AUDIO_FULL_LOAD_BUDGET_US);
  // Running estimate (EMA) of what one load() chunk costs: SD read + PSRAM
  // or fifo write.
  uint32_t getChunkCostUs() const { return _chunkCostUs; }
  // Average stream rate of the current file from its size and indexed
  // duration; 0 when the duration isn't known.
  uint32_t estimatedBytesPerSecond() const {
    if (_durationSeconds == 0 || fileSize <= _dataStart) return 0;
    return (uint32_t)((fileSize - _dataStart) / _durationSeconds);
  }
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

  // Audio waiting to be played: PSRAM ring + front fifo (when set).
  size_t getBufferedBytes() {
    return getPSRAMDataSize() + (_front ? _front->size() : 0);
  }
  size_t getPSRAMBufferedBytes() { return getPSRAMDataSize(); }

  // --- L1 front buffer (see AudioFifo.h) ---
  // The stream is played from `front` first, then the PSRAM ring. While
  // the ring is empty (track start, after a seek, or a refill fell
  // behind), SD chunks go straight into the front fifo when they fit,
  // skipping the PSRAM write + read; otherwise they go to the ring and
  // fillFrontFromPSRAM() moves them over. Order is preserved because the
  // fifo is only appended to directly while the ring holds nothing.
  // The caller owns the fifo and resets it (the consumer must be stopped).
  void setFrontFifo(AudioFifo *front) { _front = front; }
  // Off while the decoder is being stopped (the fifo then carries end fill
  // bytes for the previous stream): new data must wait in PSRAM.
  void setFrontBypass(bool enabled) { _frontBypass = enabled; }
  // Moves ring data into the front fifo, in PSRAM reads of at most
  // `chunk` bytes, until the fifo has less than `chunk` free, the ring is
  // empty, or `maxBytes` moved. Returns the bytes moved.
  size_t fillFrontFromPSRAM(size_t maxBytes, size_t chunk = 4096);

  // True when the current track is genuinely over: the source file has been
  // fully read AND the ring buffer is empty. False when the buffer is merely
  // empty because SDtoPSRAM() hasn't topped it back up yet - the caller must
  // keep streaming and refill rather than treat that gap as end-of-track.
  bool isFileExhausted(); // also requires the front fifo to be empty
  // True once every byte of the current file has been read from SD (the
  // ring buffer may still hold some of it).
  bool isFileFullyRead() { return file.isOpen() && (size_t)filePos >= fileSize; }

  // --- Next-track prefetch (see docs/blog/09-10.md, "Buffers") ---
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

  // True once the current track's file failed to open. Sticky until the
  // next file is set, so a missing file is reported once instead of being
  // retried on every load() tick.
  bool openFailed() const { return _openFailed; }

  friend class SDToPSRAMTest; // Allow SDToPSRAMTest to access private/protected members

protected:
  AudioFifo *_front = nullptr;
  bool _frontBypass = true;
  // See getChunkCostUs(). Starts at a typical measured value (~5 ms SD
  // read + ~4 ms PSRAM write for 4 KB on the breadboard).
  uint32_t _chunkCostUs = 9000;
  // SD card
  uint8_t _sdCS;
  SdFat _sd;
  File file;
  size_t fileSize;
  char _currentFileName[256]; // same size as PodPlayer::currentPath
  uint8_t _dataBuffer[AUDIO_DATABUFFERLEN];
  // Shared scratch buffer for SD<->PSRAM chunks (SDtoPSRAM) and PSRAM<->PSRAM
  // chunked copies (consumeNextTrackIfMatches). Never used concurrently -
  // everything here runs from the single-threaded main loop.
  uint8_t _scratchBuffer[AUDIO_PRELOAD_CHUNK_SIZE];
  int filePos = 0; // Current position in the file
  bool _openFailed = false; // see openFailed()
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
  char _nextFileName[256] = {0};
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
  // For a FLAC stream at `start`: writes a minimal "fLaC" + STREAMINFO
  // header to the destination and returns the offset of the first audio
  // frame (skipping cover art, tags, seek table, padding). Returns `start`
  // unchanged for anything else.
  uint32_t skipFLACMetadata(FsFile &file, uint32_t start, bool nextFile);
  // Appends bytes to the current track's ring buffer (nextFile=false) or
  // the next-track prefetch region; the caller checked there's room.
  void appendToDestination(bool nextFile, const uint8_t *data, size_t len);
  size_t getPSRAMDataSize(); // Returns the size of data in PSRAM buffer
  size_t getPSRAMFreeSpace(); // Returns the free space in PSRAM buffer
};

#endif // AUDIO_BUFFER_H

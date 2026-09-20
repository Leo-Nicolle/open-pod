#include <doctest.h>

#include <vector>

#include "../src/sound/Audio_buffer.h"
#include "audio_buffer_test_helpers.h"
#include "mocks/SdFat.h"
#include "mocks/fake_psram.h"

using openpod_test::buildFakeAudioFile;
using openpod_test::checkSequentialRun;
using openpod_test::FakeFs;
using openpod_test::fakePsram;

namespace {

void resetAll() {
  fakePsram().reset();
  FakeFs::get().reset();
}

// Repeatedly calls load() until the ring buffer holds `targetBytes` or the
// source file is exhausted. A single load() call only pulls up to
// AUDIO_MAX_CHUNKS_PER_LOAD_CALL chunks (see Audio_buffer.h) so a big refill
// spreads across ticks instead of blocking one call - tests that want a full
// file buffered have to drive that loop themselves, same as
// AudioPlayer::loop() does in production.
void loadUntil(Audio_buffer &buf, size_t targetBytes, int maxCalls = 64) {
  for (int i = 0; i < maxCalls && buf.getBufferedBytes() < targetBytes; i++) {
    buf.load(targetBytes);
  }
}

} // namespace

// Gets friend access via Audio_buffer's `friend class SDToPSRAMTest;` grant
// (src/sound/Audio_buffer.h) to poke the ring buffer's head/tail directly.
// This lets the wraparound test force the buffer into a position right at
// its physical end using a tiny fake file, instead of having to actually
// stream multiple megabytes through it to reach AUDIO_BUFFER_SIZE.
class SDToPSRAMTest {
public:
  static void forceRingPosition(Audio_buffer &buf, size_t head, size_t tail,
                                bool full) {
    buf._psramHead = head;
    buf._psramTail = tail;
    buf._bufferFull = full;
  }
};

TEST_CASE("Audio_buffer - streams a single file's exact bytes in order") {
  resetAll();
  const uint32_t size = 10000; // spans more than one AUDIO_PRELOAD_CHUNK_SIZE
  FakeFs::get().addFile("/music/a.raw", buildFakeAudioFile(1, size));

  Audio_buffer buf(4);
  REQUIRE(buf.begin());

  buf.setFileName("/music/a.raw");
  loadUntil(buf, size);
  CHECK(buf.getBufferedBytes() == size);

  std::vector<uint8_t> out(size);
  CHECK(buf.readData(out.data(), out.size()) == size);
  checkSequentialRun(out.data(), out.size(), 1, 0);

  // Fully drained: nothing left to read.
  CHECK(buf.getBufferedBytes() == 0);
  uint8_t extra[4];
  CHECK(buf.readData(extra, sizeof(extra)) == 0);
}

TEST_CASE("Audio_buffer - a single load() call is bounded, refilling spreads "
         "across several calls") {
  resetAll();
  const uint32_t size = 200000; // far more than one load() call will pull
  FakeFs::get().addFile("/music/a.raw", buildFakeAudioFile(1, size));

  Audio_buffer buf(4);
  REQUIRE(buf.begin());
  buf.setFileName("/music/a.raw");

  const size_t maxPerCall =
      (size_t)AUDIO_MAX_CHUNKS_PER_LOAD_CALL * AUDIO_PRELOAD_CHUNK_SIZE;

  buf.load(size);
  size_t afterFirstCall = buf.getBufferedBytes();
  CHECK(afterFirstCall > 0);
  CHECK(afterFirstCall <= maxPerCall);

  buf.load(size);
  size_t afterSecondCall = buf.getBufferedBytes();
  CHECK(afterSecondCall > afterFirstCall);
  CHECK(afterSecondCall <= 2 * maxPerCall);

  // A redundant load() once already at the requested target reads nothing
  // more (it's an on/off gate here - AudioPlayer::loop() is what adds the
  // low-watermark hysteresis on top of this).
  loadUntil(buf, afterSecondCall);
  buf.load(afterSecondCall);
  CHECK(buf.getBufferedBytes() == afterSecondCall);
}

TEST_CASE("Audio_buffer - ring buffer wraparound preserves byte order across "
         "the physical boundary") {
  resetAll();
  const uint32_t size = 4000;
  FakeFs::get().addFile("/music/wrap.raw", buildFakeAudioFile(3, size));

  Audio_buffer buf(4);
  REQUIRE(buf.begin());

  // Position the (empty) ring buffer 100 bytes from its physical end, so
  // writing this file's 4000 bytes must wrap: 100 bytes land at the end,
  // the remaining 3900 wrap around to the start.
  const size_t nearEnd = (size_t)AUDIO_BUFFER_SIZE - 100;
  SDToPSRAMTest::forceRingPosition(buf, nearEnd, nearEnd, false);

  buf.setFileName("/music/wrap.raw");
  loadUntil(buf, size);
  CHECK(buf.getBufferedBytes() == size);

  std::vector<uint8_t> out(size);
  CHECK(buf.readData(out.data(), out.size()) == size);
  checkSequentialRun(out.data(), out.size(), 3, 0);
}

TEST_CASE("Audio_buffer - prefetching the next track never touches the "
         "current track's ring buffer") {
  resetAll();
  const uint32_t curSize = 20000;
  const uint32_t nextChunks = 3;
  FakeFs::get().addFile("/music/a.raw", buildFakeAudioFile(1, curSize));
  FakeFs::get().addFile("/music/b.raw", buildFakeAudioFile(2, 50000));

  Audio_buffer buf(4);
  REQUIRE(buf.begin());

  buf.setFileName("/music/a.raw");
  loadUntil(buf, curSize);
  CHECK(buf.getBufferedBytes() == curSize);

  buf.prepareNextTrack("/music/b.raw");
  for (uint32_t i = 0; i < nextChunks; i++) {
    CHECK(buf.prefetchNextTrack() == true);
  }

  // The current track's ring buffer is exactly as it was before prefetching.
  CHECK(buf.getBufferedBytes() == curSize);
  std::vector<uint8_t> out(curSize);
  CHECK(buf.readData(out.data(), out.size()) == curSize);
  checkSequentialRun(out.data(), out.size(), 1, 0);

  // And the prefetch region independently holds file b's bytes, untouched
  // by anything that happened to the ring buffer.
  const size_t prefetched = (size_t)nextChunks * AUDIO_PRELOAD_CHUNK_SIZE;
  std::vector<uint8_t> peeked(prefetched);
  fakePsram().peek(NEXT_TRACK_BUFFER_BASE_ADDRESS, peeked.data(),
                   (uint32_t)peeked.size());
  checkSequentialRun(peeked.data(), peeked.size(), 2, 0);
}

TEST_CASE("Audio_buffer - consumeNextTrackIfMatches adopts prefetched bytes "
         "and streaming continues seamlessly") {
  resetAll();
  const uint32_t curSize = 8000;
  const uint32_t nextSize = 50000;
  const uint32_t prefetchChunks = 3;
  FakeFs::get().addFile("/music/a.raw", buildFakeAudioFile(1, curSize));
  FakeFs::get().addFile("/music/b.raw", buildFakeAudioFile(2, nextSize));

  Audio_buffer buf(4);
  REQUIRE(buf.begin());

  // File a is "currently playing" and fully buffered.
  buf.setFileName("/music/a.raw");
  loadUntil(buf, curSize);
  CHECK(buf.getBufferedBytes() == curSize);

  // File b gets background-prefetched while a is still streaming.
  buf.prepareNextTrack("/music/b.raw");
  for (uint32_t i = 0; i < prefetchChunks; i++) {
    buf.prefetchNextTrack();
  }

  // File a finishes and the ring buffer empties out, same as
  // AudioPlayer::stopPlaying()'s resetRingBuffer() before the next
  // startPlaying() call adopts whatever was prefetched.
  std::vector<uint8_t> drained(curSize);
  CHECK(buf.readData(drained.data(), drained.size()) == curSize);
  buf.resetRingBuffer();

  // Track change: adopt the prefetched bytes instead of re-reading them
  // from SD.
  CHECK(buf.consumeNextTrackIfMatches("/music/b.raw", 0) == true);
  const size_t adopted = (size_t)prefetchChunks * AUDIO_PRELOAD_CHUNK_SIZE;
  CHECK(buf.getBufferedBytes() == adopted);

  // Streaming continues from exactly where the prefetch left off - no gap,
  // no repeated bytes.
  loadUntil(buf, nextSize);
  CHECK(buf.getBufferedBytes() == nextSize);

  std::vector<uint8_t> out(nextSize);
  CHECK(buf.readData(out.data(), out.size()) == nextSize);
  checkSequentialRun(out.data(), out.size(), 2, 0);
}

TEST_CASE("Audio_buffer - consumeNextTrackIfMatches declines a mismatched or "
         "not-yet-prefetched guess") {
  resetAll();
  const uint32_t size = 4000;
  FakeFs::get().addFile("/music/a.raw", buildFakeAudioFile(1, size));
  FakeFs::get().addFile("/music/b.raw", buildFakeAudioFile(2, size));

  Audio_buffer buf(4);
  REQUIRE(buf.begin());

  // Nothing was ever prepared.
  CHECK(buf.consumeNextTrackIfMatches("/music/a.raw", 0) == false);

  // Prepared but not yet actually prefetched (0 bytes pulled).
  buf.prepareNextTrack("/music/a.raw");
  CHECK(buf.consumeNextTrackIfMatches("/music/a.raw", 0) == false);

  // Prepared and prefetched, but the track that's actually starting is a
  // different file (e.g. the user skipped instead of the sequential-next
  // guess playing out) - must not adopt someone else's bytes.
  buf.prepareNextTrack("/music/a.raw");
  buf.prefetchNextTrack();
  CHECK(buf.consumeNextTrackIfMatches("/music/b.raw", 0) == false);

  // Falls back to a normal fresh open + read, and gets the right file.
  buf.setFileName("/music/b.raw");
  loadUntil(buf, size);
  std::vector<uint8_t> out(size);
  CHECK(buf.readData(out.data(), out.size()) == size);
  checkSequentialRun(out.data(), out.size(), 2, 0);
}

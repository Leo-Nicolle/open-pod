#pragma once
#include <stdint.h>

// Fast scrolling on long lists (see docs/manual, "Scrolling"): the faster the
// wheel turns, the bigger each click's jump. Slow: one row per click. Then
// each click jumps to the next 3-letter prefix (ABA -> ABB), then the next
// 2-letter one, then the next first letter, and the overlay shows that many
// letters of the highlighted title.
//
// Music lists are in alphabetical order (the syncer gives ids in ICU
// primary order), so a prefix is one contiguous block, a "bucket". The
// syncer writes where each bucket of the full lists starts (letter tables in
// index.pack); scoped lists (an artist's albums...) are searched here.

// Finest prefix a list of `total` items jumps by: 1 letter up to 150 items,
// 2 up to 1500, 3 beyond (finer buckets would be a row or two each).
int fastScrollLetterCount(int total);

// The bucket of `title` at `letters` (1-3) letters, into `out` (at least 4
// bytes): a title that doesn't start with a letter is "#" (they all sort
// first); then letters uppercased and anything else a space. Same rules as
// scroll_key in syncer/crates/core/src/index/binary.rs.
void fastScrollKey(const char *title, int letters, char *out);

// Writes the key of list item `index` into `out`; false if it can't be read.
typedef bool (*FastScrollKeyAt)(void *ctx, int index, int letters, char *out);

// Where one click lands from `from` in a list of `total` items: dir > 0,
// the start of the next bucket; dir < 0, the start of this bucket, or of the
// previous one when already there. Gallops then bisects, so it reads
// O(log bucket size) keys. Stays at `from` at the ends of the list.
int fastScrollJump(int from, int dir, int letters, int total,
                   FastScrollKeyAt keyAt, void *ctx);

// Speed thresholds, in wheel clicks per second (24 clicks per turn).
// Tunable live over the bench console (#scrollcfg).
struct ScrollTuning {
  uint16_t letters3; // from here, 3-letter jumps
  uint16_t letters2; // 2-letter jumps
  uint16_t letters1; // first-letter jumps
  uint16_t windowMs; // speed = clicks in the last windowMs
  uint16_t gapMs;    // a pause this long starts over at row speed
  uint8_t dropPct;   // drop a tier only below this % of its threshold
};
extern ScrollTuning scrollTuning;

// The wheel's speed and the jump tier it picks. Pure: the caller passes
// the time, so it runs in native tests.
class ScrollSpeed {
public:
  ScrollSpeed() { reset(); }
  void reset();

  // A scroll event of `clicks` (signed) at `nowMs`. Returns the letters to
  // jump by for it (0: rows), at most `maxLetters` (0: rows only).
  int update(int clicks, uint32_t nowMs, int maxLetters);

  // Clicks per second at the last update.
  uint16_t clicksPerSecond() const { return speed; }

private:
  static const int HISTORY = 16;
  uint32_t times[HISTORY];
  int16_t counts[HISTORY];
  uint8_t head; // next slot
  uint8_t used;
  int8_t dir;
  uint8_t tier; // 0 rows, 1: 3 letters, 2: 2 letters, 3: 1 letter
  uint16_t speed;
  uint32_t last;
};

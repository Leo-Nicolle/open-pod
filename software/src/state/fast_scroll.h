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

// Shift thresholds, in wheel clicks per second (24 clicks per turn).
// Tunable live over the bench console (#scrollcfg).
struct ScrollTuning {
  uint16_t shift1;       // rows -> the finest letter jumps (3 on long lists)
  uint16_t shift2;       // -> one letter coarser (2)
  uint16_t shift3;       // -> one more (1, first letters)
  uint16_t windowClicks; // speed = average over the last this many clicks
  uint16_t dwellMs;      // finger still this long: one gear down
};
extern ScrollTuning scrollTuning;

// The jump size, as gears picked from the wheel's speed. Pure: the caller
// passes the time, so it runs in native tests.
//
// Speed shifts up, one gear at a time: the average over the last
// windowClicks clicks (12: half a turn) of travel has to reach the next
// gear's threshold, and each shift starts that window over, so every gear
// takes its own half turn and a flick can't skip ahead. Once in a gear,
// every click is one jump whatever the speed, so slowing down steps through
// the letters at the finger's pace, both ways (turning back keeps the gear).
// Holding still for dwellMs shifts one gear down (zooming back in), and
// lifting the finger goes back to rows.
//
// Gears are counted from the finest letter jumps the list allows: on a list
// that allows only first letters, the first shift is already 1 letter.
class ScrollSpeed {
public:
  ScrollSpeed() { reset(); }
  void reset();

  // A scroll event of `clicks` (signed), read at `atMs` (the wheel sample's
  // time, see ClickWheel::Event::at). Returns the letters to jump by for it
  // (0: rows), at most `maxLetters` (0: rows only).
  int update(int clicks, uint32_t atMs, int maxLetters);

  // Call often: shifts a gear down once the wheel has been still for
  // dwellMs since the last click or shift. True when it did.
  bool tick(uint32_t nowMs);

  // The finger left the wheel: back to rows.
  void touchEnd() { reset(); }

  // Letters each click jumps by now (0: rows).
  int letters() const { return gear ? maxLetters - gear + 1 : 0; }

  // Clicks per second at the last update.
  uint16_t clicksPerSecond() const { return speed; }

private:
  void restartWindow();

  // Enough events for the longest window (a click each, plus the one it
  // starts from): windowClicks is capped at HISTORY - 1, one turn.
  static const int HISTORY = 25;
  uint32_t times[HISTORY];
  uint8_t counts[HISTORY];
  uint8_t head; // next slot
  uint8_t used;
  int8_t dir;
  uint8_t gear; // 0 rows, else shifts so far (at most maxLetters)
  uint8_t maxLetters;
  uint16_t speed;
  uint32_t last; // last click or shift
};

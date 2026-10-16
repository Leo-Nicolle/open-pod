#include "fast_scroll.h"
#include <string.h>

// Starting values, tuned on the bench with the virtual finger (a slow
// browse is ~8 clicks/s, a 400 ms flick of a full turn ~60).
ScrollTuning scrollTuning = {16, 28, 40, 12, 520};

int fastScrollLetterCount(int total) {
  if (total > 1500)
    return 3;
  if (total > 150)
    return 2;
  return 1;
}

static bool isLetter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

void fastScrollKey(const char *title, int letters, char *out) {
  if (!title || !isLetter(title[0])) {
    out[0] = '#';
    out[1] = '\0';
    return;
  }
  if (letters > 3)
    letters = 3;
  int n = 0;
  for (; n < letters && title[n]; n++) {
    char c = title[n];
    out[n] = isLetter(c) ? (char)(c >= 'a' ? c - 'a' + 'A' : c) : ' ';
  }
  out[n] = '\0';
}

namespace {
struct Probe {
  FastScrollKeyAt keyAt;
  void *ctx;
  int letters;
  // Whether item i is in the bucket `key` (unreadable items count as out).
  bool in(int i, const char *key) const {
    char k[4];
    return keyAt(ctx, i, letters, k) && strcmp(k, key) == 0;
  }
};

// First index in [lo, hi] that isn't in `key`, given lo - 1 is in it and
// hi isn't (hi may be one past the list).
int bisectOut(const Probe &p, const char *key, int lo, int hi) {
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (p.in(mid, key))
      lo = mid + 1;
    else
      hi = mid;
  }
  return lo;
}

// First index in [lo, hi] that is in `key`, given hi is in it and lo - 1
// isn't (lo may be 0).
int bisectStart(const Probe &p, const char *key, int lo, int hi) {
  while (lo < hi) {
    int mid = lo + (hi - lo) / 2;
    if (p.in(mid, key))
      hi = mid;
    else
      lo = mid + 1;
  }
  return lo;
}

// Start of the bucket holding `at`.
int bucketStart(const Probe &p, int at) {
  char key[4];
  if (!p.keyAt(p.ctx, at, p.letters, key))
    return at;
  int step = 1, inside = at;
  for (;;) {
    int probe = inside - step;
    if (probe < 0) {
      return bisectStart(p, key, 0, inside);
    }
    if (!p.in(probe, key)) {
      return bisectStart(p, key, probe + 1, inside);
    }
    inside = probe;
    step *= 2;
  }
}
} // namespace

int fastScrollJump(int from, int dir, int letters, int total,
                   FastScrollKeyAt keyAt, void *ctx) {
  if (total <= 0 || from < 0 || from >= total)
    return from;
  Probe p = {keyAt, ctx, letters};
  if (dir < 0) {
    if (from == 0)
      return 0;
    int start = bucketStart(p, from);
    return start < from ? start : bucketStart(p, from - 1);
  }
  char key[4];
  if (!keyAt(ctx, from, letters, key))
    return from;
  // Gallop forward from `from` until out of the bucket, then bisect.
  int step = 1, inside = from;
  for (;;) {
    int probe = inside + step;
    if (probe >= total) {
      int next = bisectOut(p, key, inside + 1, total);
      return next < total ? next : from; // the last bucket: stay
    }
    if (!p.in(probe, key)) {
      return bisectOut(p, key, inside + 1, probe);
    }
    inside = probe;
    step *= 2;
  }
}

void ScrollSpeed::reset() {
  head = used = 0;
  dir = 0;
  gear = 0;
  maxLetters = 0;
  speed = 0;
  last = 0;
}

void ScrollSpeed::restartWindow() { head = used = 0; }

int ScrollSpeed::update(int clicks, uint32_t atMs, int maxL) {
  const ScrollTuning &t = scrollTuning;
  maxLetters = (uint8_t)(maxL > 0 ? maxL : 0);
  if (gear > maxLetters)
    gear = maxLetters;
  int d = clicks < 0 ? -1 : 1;
  // A new turn, the other way or after a pause: the speed starts over (the
  // gear doesn't, see tick()). A click read before the last shift (held
  // back by the wheel's lift-off delay) doesn't move `last` back.
  int32_t since = (int32_t)(atMs - last);
  if (used == 0 || d != dir || since >= (int32_t)t.dwellMs)
    restartWindow();
  dir = (int8_t)d;
  if (since > 0)
    last = atMs;
  int n = clicks < 0 ? -clicks : clicks;
  times[head] = atMs;
  counts[head] = (uint8_t)(n > 255 ? 255 : n);
  head = (uint8_t)((head + 1) % HISTORY);
  if (used < HISTORY)
    used++;

  // The time the last `win` clicks took: an event's clicks were turned
  // between the sample before it and its own, so they span from the event
  // before them to this one.
  uint16_t win = t.windowClicks;
  if (win < 1)
    win = 1;
  if (win > HISTORY - 1)
    win = HISTORY - 1;
  uint32_t sum = 0, v = 0;
  bool full = false;
  for (int i = 0; i < used; i++) {
    int slot = (head - 1 - i + HISTORY) % HISTORY;
    if (i > 0) {
      uint32_t span = atMs - times[slot];
      v = sum * 1000 / (span > 0 ? span : 1);
      if (sum >= win) {
        full = true;
        break;
      }
    }
    sum += counts[slot];
  }
  speed = (uint16_t)(v > 0xFFFF ? 0xFFFF : v);

  // One gear up, and the next one needs a window of its own, from here.
  const uint16_t th[3] = {t.shift1, t.shift2, t.shift3};
  if (full && gear < maxLetters && speed >= th[gear]) {
    gear++;
    restartWindow();
    times[head] = atMs;
    counts[head] = 0;
    head = 1;
    used = 1;
  }
  return letters();
}

bool ScrollSpeed::tick(uint32_t nowMs) {
  if (gear == 0 || nowMs - last < scrollTuning.dwellMs)
    return false;
  gear--;
  last = nowMs;
  restartWindow();
  return true;
}

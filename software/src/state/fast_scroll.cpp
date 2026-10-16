#include "fast_scroll.h"
#include <string.h>

// Starting values, tuned on the bench with the virtual finger (a slow
// browse is ~8 clicks/s, a 400 ms flick of a full turn ~60).
ScrollTuning scrollTuning = {12, 25, 40, 250, 300, 70};

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
  tier = 0;
  speed = 0;
  last = 0;
}

int ScrollSpeed::update(int clicks, uint32_t nowMs, int maxLetters) {
  const ScrollTuning &t = scrollTuning;
  int d = clicks < 0 ? -1 : 1;
  if (used == 0 || d != dir || nowMs - last >= t.gapMs) {
    // A new turn (or the other way): start over, precise.
    head = used = 0;
    tier = 0;
  }
  dir = (int8_t)d;
  last = nowMs;
  times[head] = nowMs;
  counts[head] = (int16_t)(clicks < 0 ? -clicks : clicks);
  head = (uint8_t)((head + 1) % HISTORY);
  if (used < HISTORY)
    used++;

  // Clicks over the time they took: from the oldest event in the window to
  // this one (its own clicks came before it, so they don't count). Counting
  // clicks per window instead jitters at its edge: 8 clicks/s reads 8 or 12.
  // Alone in the window, an event's clicks count over the whole window.
  uint32_t sum = 0, span = 0, oldest = 0;
  for (int i = 0; i < used; i++) {
    int slot = (head - 1 - i + HISTORY) % HISTORY;
    if (nowMs - times[slot] >= t.windowMs)
      break;
    sum += (uint32_t)counts[slot];
    span = nowMs - times[slot];
    oldest = (uint32_t)counts[slot];
  }
  uint32_t v;
  if (span > 0)
    v = (sum - oldest) * 1000 / span;
  else
    v = t.windowMs ? sum * 1000 / t.windowMs : 0;
  speed = (uint16_t)(v > 0xFFFF ? 0xFFFF : v);

  // Up at once, down only clearly below the current tier's threshold.
  const uint16_t th[4] = {0, t.letters3, t.letters2, t.letters1};
  uint8_t want = 0;
  for (uint8_t k = 3; k >= 1; k--) {
    if (speed >= th[k]) {
      want = k;
      break;
    }
  }
  if (want > tier) {
    tier = want;
  } else {
    while (tier > want && (uint32_t)speed * 100 < (uint32_t)th[tier] * t.dropPct)
      tier--;
  }
  if (tier == 0 || maxLetters <= 0)
    return 0;
  int letters = 4 - tier;
  return letters < maxLetters ? letters : maxLetters;
}

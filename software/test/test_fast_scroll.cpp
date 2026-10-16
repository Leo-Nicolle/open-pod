#include <doctest.h>
#include "../src/state/fast_scroll.h"
#include <cstring>
#include <string>
#include <vector>

namespace {
// Array-backed list for fastScrollJump; counts key reads.
struct List {
  std::vector<std::string> names;
  int reads = 0;
  static bool keyAt(void *ctx, int i, int letters, char *out) {
    List *l = (List *)ctx;
    l->reads++;
    if (i < 0 || i >= (int)l->names.size())
      return false;
    fastScrollKey(l->names[i].c_str(), letters, out);
    return true;
  }
  int jump(int from, int dir, int letters) {
    return fastScrollJump(from, dir, letters, (int)names.size(), keyAt, this);
  }
};
} // namespace

TEST_SUITE("Fast scroll") {
  TEST_CASE("Letter count follows the list length") {
    CHECK(fastScrollLetterCount(12) == 1);
    CHECK(fastScrollLetterCount(150) == 1);
    CHECK(fastScrollLetterCount(151) == 2);
    CHECK(fastScrollLetterCount(1500) == 2);
    CHECK(fastScrollLetterCount(1501) == 3);
  }

  TEST_CASE("Keys match the syncer's scroll_key") {
    // Same cases as syncer/crates/core/src/index/binary.rs scroll_keys.
    struct {
      const char *name;
      int letters;
      const char *key;
    } cases[] = {
        {"abba", 1, "A"},    {"abba", 2, "AB"},   {"abba", 3, "ABB"},
        {"A Day", 3, "A D"}, {"A-ha", 2, "A "},   {"AC/DC", 3, "AC "},
        {"2Pac", 1, "#"},    {"(What's", 3, "#"}, {"", 2, "#"},
        {"Z", 3, "Z"},
    };
    char out[4];
    for (auto &c : cases) {
      fastScrollKey(c.name, c.letters, out);
      CHECK_MESSAGE(std::string(out) == c.key, c.name, " at ", c.letters);
    }
    fastScrollKey(nullptr, 1, out);
    CHECK(std::string(out) == "#");
  }

  TEST_CASE("Jumps land on bucket starts, both ways") {
    List l;
    l.names = {"10cc", "(x)", "A Day", "Aardvark", "Abba", "abc",
               "Beck", "Bjork", "Blur", "Cake"};
    // 1 letter: # [0,2) A [2,6) B [6,9) C [9]
    CHECK(l.jump(0, 1, 1) == 2);
    CHECK(l.jump(3, 1, 1) == 6);
    CHECK(l.jump(6, 1, 1) == 9);
    CHECK(l.jump(9, 1, 1) == 9); // last bucket: stays
    CHECK(l.jump(8, -1, 1) == 6); // inside B: to its start
    CHECK(l.jump(6, -1, 1) == 2); // at B's start: to A's
    CHECK(l.jump(2, -1, 1) == 0);
    CHECK(l.jump(0, -1, 1) == 0);
    // 2 letters: # A_ AA AB(4,5) BE BJ BL CA
    CHECK(l.jump(2, 1, 2) == 3);
    CHECK(l.jump(4, 1, 2) == 6);
    CHECK(l.jump(5, -1, 2) == 4);
    // 3 letters: ABB and ABC are their own buckets
    CHECK(l.jump(4, 1, 3) == 5);
  }

  TEST_CASE("A big bucket costs a few reads") {
    List l;
    for (int i = 0; i < 5000; i++)
      l.names.push_back("Aaa");
    for (int i = 0; i < 3; i++)
      l.names.push_back("Bbb");
    l.reads = 0;
    CHECK(l.jump(10, 1, 1) == 5000);
    CHECK(l.reads < 40);
    l.reads = 0;
    CHECK(l.jump(4990, -1, 1) == 0);
    CHECK(l.reads < 40);
  }

  // Spins `n` single clicks `ms` apart in direction `dir`; the last letters.
  static int spin(ScrollSpeed &s, uint32_t &t, int n, uint32_t ms, int dir = -1,
                  int maxLetters = 3) {
    int letters = 0;
    for (int i = 0; i < n; i++)
      letters = s.update(dir, t += ms, maxLetters);
    return letters;
  }

  TEST_CASE("Speed shifts up one gear at a time") {
    ScrollTuning saved = scrollTuning;
    scrollTuning = {12, 25, 40, 12, 350};
    ScrollSpeed s;
    uint32_t t = 1000;
    // 8 clicks/s: rows.
    CHECK(spin(s, t, 15, 125) == 0);
    // 20 clicks/s: rows until the half turn window is mostly fast...
    CHECK(spin(s, t, 5, 50) == 0);
    // ...then 3 letters, and it stays there at that speed.
    CHECK(spin(s, t, 10, 50) == 3);
    CHECK(spin(s, t, 15, 50) == 3);
    // 33 clicks/s: 2 letters; 66: 1 letter.
    CHECK(spin(s, t, 15, 30) == 2);
    CHECK(spin(s, t, 15, 15) == 1);
    // A list that allows 2 letters at most: its last gear is 1 letter.
    CHECK(s.update(-1, t += 15, 2) == 1);
    scrollTuning = saved;
  }

  TEST_CASE("Each gear takes its own half turn") {
    ScrollTuning saved = scrollTuning;
    scrollTuning = {12, 25, 40, 12, 350};
    ScrollSpeed s;
    uint32_t t = 1000;
    // A quick flick shorter than the window: rows, however fast.
    CHECK(spin(s, t, 12, 10) == 0);
    // The 12th click after the first one: one gear, not straight to 1 letter.
    CHECK(spin(s, t, 1, 10) == 3);
    CHECK(spin(s, t, 11, 10) == 3);
    CHECK(spin(s, t, 1, 10) == 2);
    CHECK(spin(s, t, 12, 10) == 1);
    // One event of many clicks has nothing to be timed against.
    s.reset();
    CHECK(s.update(-24, t += 1000, 3) == 0);
    scrollTuning = saved;
  }

  TEST_CASE("A gear holds while slowing down and turning back") {
    ScrollTuning saved = scrollTuning;
    scrollTuning = {12, 25, 40, 12, 350};
    ScrollSpeed s;
    uint32_t t = 1000;
    CHECK(spin(s, t, 13, 10) == 3);
    CHECK(spin(s, t, 13, 10) == 2);
    CHECK(spin(s, t, 13, 10) == 1);
    // Slow, one letter per click at the finger's pace.
    CHECK(spin(s, t, 10, 300) == 1);
    // Back the other way, still letters.
    CHECK(spin(s, t, 3, 300, 1) == 1);
    CHECK(!s.tick(t + 300));
    scrollTuning = saved;
  }

  TEST_CASE("Holding still shifts down, lifting goes back to rows") {
    ScrollTuning saved = scrollTuning;
    scrollTuning = {12, 25, 40, 12, 350};
    ScrollSpeed s;
    uint32_t t = 1000;
    spin(s, t, 13, 10);
    spin(s, t, 13, 10);
    REQUIRE(spin(s, t, 13, 10) == 1);
    CHECK(!s.tick(t + 349));
    CHECK(s.tick(t += 350));
    CHECK(s.letters() == 2);
    // Each shift down waits its own dwell.
    CHECK(!s.tick(t + 200));
    // Clicks in the lower gear jump by it, and push the next shift back.
    CHECK(spin(s, t, 2, 200) == 2);
    CHECK(s.tick(t += 350));
    CHECK(s.letters() == 3);
    CHECK(s.tick(t += 350));
    CHECK(s.letters() == 0);
    CHECK(!s.tick(t += 1000));
    // Up again from rows, then lift off.
    REQUIRE(spin(s, t, 13, 10) == 3);
    s.touchEnd();
    CHECK(s.letters() == 0);
    CHECK(s.update(-1, t += 10, 3) == 0);
    scrollTuning = saved;
  }

  TEST_CASE("Short lists have fewer gears") {
    ScrollTuning saved = scrollTuning;
    scrollTuning = {12, 25, 40, 12, 350};
    ScrollSpeed s;
    uint32_t t = 1000;
    // First letters only: the first shift, at the first threshold.
    CHECK(spin(s, t, 13, 50, -1, 1) == 1);
    CHECK(spin(s, t, 30, 10, -1, 1) == 1);
    // No letters on a list that doesn't allow them.
    s.reset();
    CHECK(spin(s, t, 30, 10, -1, 0) == 0);
    scrollTuning = saved;
  }

  TEST_CASE("The speed doesn't depend on how often the wheel is read") {
    ScrollTuning saved = scrollTuning;
    scrollTuning = {12, 25, 40, 12, 350};
    // 30 clicks/s, read every click (a fast loop) or 3 at a time every
    // 100 ms (a loop slowed down by the UI): same speed, same gear.
    ScrollSpeed fast, slow;
    int lf = 0, ls = 0;
    uint32_t t = 1000;
    fast.update(-1, t, 3);
    slow.update(-1, t, 3);
    for (int i = 1; i <= 12; i++) {
      lf = fast.update(-1, t + i * 100 / 3, 3);
      if (i % 3 == 0)
        ls = slow.update(-3, t + i * 100 / 3, 3);
    }
    CHECK(lf == 3);
    CHECK(ls == 3);
    CHECK(fast.clicksPerSecond() >= 29);
    CHECK(fast.clicksPerSecond() <= 31);
    CHECK(slow.clicksPerSecond() == fast.clicksPerSecond());
    scrollTuning = saved;
  }
}

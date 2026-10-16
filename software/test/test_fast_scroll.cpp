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

  TEST_CASE("Speed tiers rise with the wheel's speed") {
    ScrollTuning saved = scrollTuning;
    scrollTuning = {12, 25, 40, 250, 300, 70};
    ScrollSpeed s;
    uint32_t t = 1000;
    // 8 clicks/s: rows.
    for (int i = 0; i < 10; i++)
      CHECK(s.update(-1, t += 125, 3) == 0);
    // 20 clicks/s: 3 letters (once the window holds enough clicks).
    int letters = 0;
    for (int i = 0; i < 10; i++)
      letters = s.update(-1, t += 50, 3);
    CHECK(letters == 3);
    // 33 clicks/s: 2 letters; 66: 1 letter.
    for (int i = 0; i < 10; i++)
      letters = s.update(-1, t += 30, 3);
    CHECK(letters == 2);
    for (int i = 0; i < 10; i++)
      letters = s.update(-1, t += 15, 3);
    CHECK(letters == 1);
    // A short list allows 1 letter at most... and a long one never goes
    // finer than it allows.
    CHECK(s.update(-1, t += 15, 2) == 1);
    scrollTuning = saved;
  }

  TEST_CASE("Hysteresis, reversal and pauses") {
    ScrollTuning saved = scrollTuning;
    scrollTuning = {12, 25, 40, 250, 300, 70};
    ScrollSpeed s;
    uint32_t t = 1000;
    int letters = 0;
    for (int i = 0; i < 10; i++)
      letters = s.update(-1, t += 50, 3); // 20/s
    REQUIRE(letters == 3);
    // 10/s is above 70% of 12: still 3 letters.
    for (int i = 0; i < 6; i++)
      letters = s.update(-1, t += 100, 3);
    CHECK(letters == 3);
    // Turning back starts over at rows.
    CHECK(s.update(1, t += 50, 3) == 0);
    for (int i = 0; i < 10; i++)
      letters = s.update(1, t += 50, 3);
    CHECK(letters == 3);
    // So does a pause.
    CHECK(s.update(1, t += 400, 3) == 0);
    // One event of many clicks is fast by itself.
    s.reset();
    CHECK(s.update(-24, t += 1000, 3) == 1);
    // No letters on a list that doesn't allow them.
    CHECK(s.update(-24, t += 10, 0) == 0);
    scrollTuning = saved;
  }
}

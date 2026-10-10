#include <doctest.h>
#include "../src/state/fast_scroll.h"
#include <cstring>

TEST_SUITE("Fast scroll label") {
  TEST_CASE("Letter count follows the list length") {
    CHECK(fastScrollLetterCount(12) == 1);
    CHECK(fastScrollLetterCount(150) == 1);
    CHECK(fastScrollLetterCount(151) == 2);
    CHECK(fastScrollLetterCount(1500) == 2);
    CHECK(fastScrollLetterCount(1501) == 3);
  }

  TEST_CASE("Letters only, uppercased") {
    char out[4];
    fastScrollLabel("The National Anthem", 12, out);
    CHECK(strcmp(out, "T") == 0);
    fastScrollLabel("The National Anthem", 800, out);
    CHECK(strcmp(out, "TH") == 0);
    fastScrollLabel("The National Anthem", 5000, out);
    CHECK(strcmp(out, "THE") == 0);
    fastScrollLabel("99 Luftballons", 800, out);
    CHECK(strcmp(out, "LU") == 0);
    fastScrollLabel("a-ha", 5000, out);
    CHECK(strcmp(out, "AHA") == 0);
  }

  TEST_CASE("Short titles keep what they have") {
    char out[4];
    fastScrollLabel("U2", 5000, out);
    CHECK(strcmp(out, "U") == 0);
  }

  TEST_CASE("No letters gives #") {
    char out[4];
    fastScrollLabel("!!!", 12, out);
    CHECK(strcmp(out, "#") == 0);
    fastScrollLabel("", 12, out);
    CHECK(strcmp(out, "#") == 0);
    fastScrollLabel(nullptr, 12, out);
    CHECK(strcmp(out, "#") == 0);
  }
}

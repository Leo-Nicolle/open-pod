#include <doctest.h>

#include "../src/sound/RefillPolicy.h"

using namespace refill;

namespace {

Inputs idle(uint32_t runway, bool refilling) {
  Inputs in;
  in.runwayMs = runway;
  in.refilling = refilling;
  in.targetReached = false;
  in.userBusy = false;
  in.msSinceBusyChunk = 1000;
  return in;
}

} // namespace

TEST_SUITE("RefillPolicy") {
  TEST_CASE("runway is buffered bytes over the stream rate") {
    CHECK(runwayMs(90000, 90000) == 1000);
    CHECK(runwayMs(16000 * 5, 16000) == 5000); // 5 s of 128 kbps MP3
    // Unknown rate: CD PCM, the pessimistic choice.
    CHECK(runwayMs(FALLBACK_BYTES_PER_SECOND, 0) == 1000);
    // No overflow on a full 3.5 MB buffer.
    CHECK(runwayMs(3584UL * 1024UL, 16000) == 229376);
  }

  TEST_CASE("above the low runway nothing loads, the prefetch runs") {
    Decision d = decide(idle(LOW_RUNWAY_MS + 1, false));
    CHECK_FALSE(d.refilling);
    CHECK_FALSE(d.load);
    CHECK(d.prefetch);
  }

  TEST_CASE("a refill starts below the low runway and latches") {
    Decision d = decide(idle(LOW_RUNWAY_MS - 1, false));
    CHECK(d.refilling);
    CHECK(d.load);
    CHECK(d.budgetUs == RELAXED_BUDGET_US);
    CHECK_FALSE(d.prefetch);
    // Still refilling well above the low runway, until the target.
    d = decide(idle(60000, true));
    CHECK(d.refilling);
    CHECK(d.load);
  }

  TEST_CASE("the refill stops once the target is reached") {
    Inputs in = idle(60000, true);
    in.targetReached = true;
    Decision d = decide(in);
    CHECK_FALSE(d.refilling);
    CHECK_FALSE(d.load);
    CHECK(d.prefetch);
  }

  TEST_CASE("a fully read file never latches a refill") {
    Inputs in = idle(100, false); // end of the track: little runway left
    in.targetReached = true;      // ... but nothing more to read
    Decision d = decide(in);
    CHECK_FALSE(d.refilling);
    CHECK_FALSE(d.load);
  }

  TEST_CASE("below the urgent runway the budget is full") {
    Decision d = decide(idle(URGENT_RUNWAY_MS - 1, true));
    CHECK(d.load);
    CHECK(d.budgetUs == FULL_BUDGET_US);
  }

  TEST_CASE("user busy, plenty of runway: the refill waits") {
    Inputs in = idle(URGENT_RUNWAY_MS + 1, true);
    in.userBusy = true;
    Decision d = decide(in);
    CHECK(d.refilling);
    CHECK_FALSE(d.load);
    CHECK_FALSE(d.prefetch);
  }

  TEST_CASE("user busy, short runway: one chunk per interval") {
    Inputs in = idle(URGENT_RUNWAY_MS - 1, true);
    in.userBusy = true;
    in.msSinceBusyChunk = BUSY_CHUNK_INTERVAL_MS;
    Decision d = decide(in);
    CHECK(d.load);
    CHECK(d.busyChunk);
    CHECK(d.budgetUs == ONE_CHUNK_BUDGET_US);

    in.msSinceBusyChunk = BUSY_CHUNK_INTERVAL_MS - 1;
    d = decide(in);
    CHECK_FALSE(d.load);
  }

  TEST_CASE("user busy, low runway: the trickle reads two chunks") {
    Inputs in = idle(BUSY_BOOST_RUNWAY_MS - 1, true);
    in.userBusy = true;
    in.msSinceBusyChunk = BUSY_CHUNK_INTERVAL_MS;
    Decision d = decide(in);
    CHECK(d.load);
    CHECK(d.busyChunk);
    CHECK(d.budgetUs == TWO_CHUNK_BUDGET_US);
  }

  TEST_CASE("user busy, critical runway: full budget anyway") {
    Inputs in = idle(CRITICAL_RUNWAY_MS - 1, true);
    in.userBusy = true;
    in.msSinceBusyChunk = 0;
    Decision d = decide(in);
    CHECK(d.load);
    CHECK_FALSE(d.busyChunk);
    CHECK(d.budgetUs == FULL_BUDGET_US);
  }

  TEST_CASE("no prefetch while the user is busy") {
    Inputs in = idle(LOW_RUNWAY_MS + 1, false);
    in.userBusy = true;
    Decision d = decide(in);
    CHECK_FALSE(d.load);
    CHECK_FALSE(d.prefetch);
  }
}

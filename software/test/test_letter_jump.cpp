#include <doctest.h>
#include <cstring>
#include <string>
#include <vector>

#include "../src/state/fast_scroll.h"
#include "../src/state/state.h"
#include "test_helpers.h"

namespace {

// 200 tracks in alphabetical order: "Aaa 0".."Eda 4" (5 first letters x 4
// second x 2 third x 5), one album, one artist, one genre.
std::vector<std::string> trackNames() {
  std::vector<std::string> names;
  for (char a = 'A'; a <= 'E'; a++)
    for (char b = 'a'; b <= 'd'; b++)
      for (char c = 'a'; c <= 'b'; c++)
        for (int k = 0; k < 5; k++)
          names.push_back(std::string() + a + b + c + " " + char('0' + k));
  return names;
}

void registerAlphabetCatalog(bool letterTables) {
  using namespace openpod_test;
  resetAll();
  auto &fs = FakeFs::get();
  std::vector<std::string> names = trackNames();
  std::vector<std::pair<uint32_t, std::string>> tracks, paths;
  std::vector<std::pair<uint32_t, uint16_t>> durations;
  std::vector<std::pair<uint32_t, uint32_t>> track_to_album;
  std::vector<uint32_t> all;
  for (uint32_t i = 0; i < names.size(); i++) {
    tracks.push_back(std::make_pair(i, names[i]));
    paths.push_back(std::make_pair(i, "/m/" + names[i] + ".mp3"));
    durations.push_back(std::make_pair(i, (uint16_t)60));
    track_to_album.push_back(std::make_pair(i, 0u));
    all.push_back(i);
  }
  std::vector<std::pair<uint32_t, std::string>> one;
  one.push_back(std::make_pair(0u, std::string("One")));
  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> toAll, toAlbum;
  toAll.push_back(std::make_pair(0u, all));
  toAlbum.push_back(std::make_pair(0u, std::vector<uint32_t>{0}));
  fs.addFile("/openpod/artist_index.bin", buildStringIndex(one));
  fs.addFile("/openpod/album_index.bin", buildStringIndex(one));
  fs.addFile("/openpod/genre_index.bin", buildStringIndex(one));
  fs.addFile("/openpod/track_index.bin", buildStringIndex(tracks));
  fs.addFile("/openpod/path_index.bin", buildStringIndex(paths));
  fs.addFile("/openpod/duration_index.bin", buildDurationIndex(durations));
  fs.addFile("/openpod/track_to_album.bin", buildTrackToAlbumIndex(track_to_album));
  fs.addFile("/openpod/artist_to_albums.bin", buildRelation(toAlbum));
  fs.addFile("/openpod/artist_to_tracks.bin", buildRelation(toAll));
  fs.addFile("/openpod/album_to_tracks.bin", buildRelation(toAll));
  fs.addFile("/openpod/genre_to_albums.bin", buildRelation(toAlbum));
  fs.addFile("/openpod/genre_to_tracks.bin", buildRelation(toAll));
  std::vector<CoverIndexEntry> covers;
  covers.push_back({0u, 0u, 0u, 1});
  fs.addFile("/openpod/album_to_cover.bin", buildAlbumCoverIndex(covers));
  if (letterTables) {
    fs.addFile("/openpod/track_letters_1.bin", buildLetterTable(names, 1, fastScrollKey));
    fs.addFile("/openpod/track_letters_2.bin", buildLetterTable(names, 2, fastScrollKey));
    fs.addFile("/openpod/track_letters_3.bin", buildLetterTable(names, 3, fastScrollKey));
  }
}

std::string selected(State &state) { return state.getCurrentTrackName(); }

// The same jumps, whichever way the buckets are found.
void checkJumps(State &state) {
  // 1 letter: A [0,40) B [40,80) ...
  state.letterJump(-1, 1);
  CHECK(state.getSelectedIndex() == 40);
  CHECK(selected(state) == "Baa 0");
  state.letterJump(-2, 1);
  CHECK(state.getSelectedIndex() == 120);
  // 2 letters: Da [120,130) Db [130,140)
  state.letterJump(-1, 2);
  CHECK(state.getSelectedIndex() == 130);
  // 3 letters: Dba [130,135) Dbb [135,140)
  state.letterJump(-1, 3);
  CHECK(state.getSelectedIndex() == 135);
  // Back: from inside a bucket to its start, then the one before.
  state.scrollDown();
  state.scrollDown();
  state.letterJump(1, 1);
  CHECK(state.getSelectedIndex() == 120);
  state.letterJump(1, 1);
  CHECK(state.getSelectedIndex() == 80);
  // The last bucket stays put going down; the first going up.
  state.letterJump(-20, 1);
  CHECK(state.getSelectedIndex() == 160);
  state.letterJump(20, 1);
  CHECK(state.getSelectedIndex() == 0);
  // The highlighted row stays on screen with the right name.
  state.letterJump(-3, 1);
  int row = state.getRelativeSelectedIndex();
  REQUIRE(row >= 0);
  REQUIRE(row < ELEMENTS_PER_SCREEN);
  CHECK(std::string(state.getVisibleElements()[row]) == "Daa 0");
}

} // namespace

TEST_SUITE("Letter jumps") {
  TEST_CASE("Full list: found with the syncer's letter tables") {
    registerAlphabetCatalog(true);
    MusicLookup lookup;
    REQUIRE(lookup.init());
    CHECK(lookup.letterBucket(3, 1, 10, 1) == 40);
    CHECK(lookup.letterBucket(3, 1, 40, -1) == 0);
    CHECK(lookup.letterBucket(3, 1, 45, -1) == 40);
    CHECK(lookup.letterBucket(3, 3, 199, 1) == 199);
    CHECK(lookup.letterBucket(0, 1, 0, 1) == -1); // no artist table

    State state;
    state.loadCurrentRouteData(lookup);
    state.goToSelected(lookup); // ROOT -> "Tracks"
    REQUIRE(state.getTotalElements() == 200);
    checkJumps(state);
  }

  TEST_CASE("Without tables: found by reading names") {
    registerAlphabetCatalog(false);
    MusicLookup lookup;
    REQUIRE(lookup.init());
    State state;
    state.loadCurrentRouteData(lookup);
    state.goToSelected(lookup); // ROOT -> "Tracks"
    REQUIRE(state.getTotalElements() == 200);
    checkJumps(state);
  }

  TEST_CASE("Scoped list: an album's tracks") {
    registerAlphabetCatalog(true);
    MusicLookup lookup;
    REQUIRE(lookup.init());
    State state;
    state.loadCurrentRouteData(lookup);
    state.scrollDown();
    state.scrollDown();
    state.goToSelected(lookup); // ROOT -> "Albums"
    state.goToSelected(lookup); // -> "One" -> its tracks
    REQUIRE(state.getCurrentRoute().type == Route_t::TRACKS);
    REQUIRE(state.getTotalElements() == 200);
    checkJumps(state);
  }

  TEST_CASE("The overlay carries its letter count") {
    registerAlphabetCatalog(false);
    MusicLookup lookup;
    REQUIRE(lookup.init());
    State state;
    state.setFastScroll(3);
    CHECK(state.getFastScrollLetters() == 3);
    CHECK(state.getFastScrollActive());
    state.setFastScroll(0);
    CHECK_FALSE(state.getFastScrollActive());
  }
}

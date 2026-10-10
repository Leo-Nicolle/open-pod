#include <doctest.h>
#include <cstring>
#include <string>
#include <vector>

#include "../src/state/search.h"
#include "../src/state/state.h"
#include "../src/storage/Music_index.h"
#include "../src/storage/Music_lookup.h"
#include "bin_helpers.h"
#include "test_helpers.h"

namespace {

struct TrieResult {
  uint32_t type;
  uint32_t id;
  std::string name;
  uint32_t relevance;
};

struct TrieLeaf {
  std::string key;
  std::vector<TrieResult> results;
};

// A music_index.bin (indexer exportToBinary layout) whose root has one leaf
// child per entry. Not a canonical radix trie (siblings may share a first
// letter), but MusicIndex::findNode only needs the keys to match.
std::vector<uint8_t> buildFlatMusicIndex(const std::vector<TrieLeaf> &leaves) {
  std::string pool;
  std::vector<uint32_t> offsets;
  auto intern = [&](const std::string &s) {
    uint32_t off = (uint32_t)pool.size();
    pool += s;
    offsets.push_back(off);
    return off;
  };

  intern(""); // root key
  std::vector<uint32_t> keyOffsets;
  for (const auto &leaf : leaves)
    keyOffsets.push_back(intern(leaf.key));
  std::vector<std::vector<uint32_t>> nameOffsets(leaves.size());
  uint32_t resultCount = 0;
  for (size_t i = 0; i < leaves.size(); i++) {
    for (const auto &r : leaves[i].results) {
      nameOffsets[i].push_back(intern(r.name));
      resultCount++;
    }
  }

  std::vector<uint8_t> out;
  openpod_test::pushU32(out, (uint32_t)pool.size());
  openpod_test::pushU32(out, (uint32_t)leaves.size() + 1);
  openpod_test::pushU32(out, resultCount);
  openpod_test::pushU32(out, (uint32_t)offsets.size());
  for (char c : pool)
    out.push_back((uint8_t)c);
  for (uint32_t off : offsets)
    openpod_test::pushU32(out, off);

  // root
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, (uint32_t)leaves.size());
  openpod_test::pushU32(out, 1);
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, 0);

  uint32_t firstResult = 0;
  for (size_t i = 0; i < leaves.size(); i++) {
    openpod_test::pushU32(out, keyOffsets[i]);
    openpod_test::pushU32(out, (uint32_t)leaves[i].key.size());
    openpod_test::pushU32(out, 0);
    openpod_test::pushU32(out, 0);
    openpod_test::pushU32(out, (uint32_t)leaves[i].results.size());
    openpod_test::pushU32(out, firstResult);
    firstResult += (uint32_t)leaves[i].results.size();
  }

  for (size_t i = 0; i < leaves.size(); i++) {
    for (size_t j = 0; j < leaves[i].results.size(); j++) {
      const auto &r = leaves[i].results[j];
      openpod_test::pushU32(out, r.type);
      openpod_test::pushU32(out, r.id);
      openpod_test::pushU32(out, nameOffsets[i][j]);
      openpod_test::pushU32(out, (uint32_t)r.name.size());
      openpod_test::pushU32(out, r.relevance);
    }
  }
  return out;
}

// registerStateTestCatalog() ("Solo Artist" / "Double Album" with "First
// Song" + "Second Song" / genre "Indie") plus a trie over it. No
// album_to_artist.bin, so album rows exercise the first-track fallback.
void setupSearchCatalog() {
  openpod_test::resetAll();
  openpod_test::registerStateTestCatalog();
  auto &fs = openpod_test::FakeFs::get();

  std::vector<std::pair<uint32_t, uint32_t>> track_to_artist;
  track_to_artist.push_back(std::make_pair(1u, 1u));
  track_to_artist.push_back(std::make_pair(2u, 1u));
  fs.addFile("/openpod/track_to_artist.bin",
             openpod_test::buildTrackToAlbumIndex(track_to_artist));

  std::vector<TrieLeaf> leaves = {
      {"first song", {{SEARCH_TRACK, 1, "first song", 100}}},
      {"second song", {{SEARCH_TRACK, 2, "second song", 100}}},
      // word key: both tracks, plus a duplicate of track 1
      {"song",
       {{SEARCH_TRACK, 2, "second song", 40},
        {SEARCH_TRACK, 1, "first song", 40},
        {SEARCH_TRACK, 1, "first song", 30}}},
      {"double album", {{SEARCH_ALBUM, 1, "double album", 100}}},
      {"solo artist", {{SEARCH_ARTIST, 1, "solo artist", 100}}},
      {"indie", {{SEARCH_GENRE, 1, "indie", 100}}},
      // ten unnamed tracks, to scroll through
      {"many",
       {{SEARCH_TRACK, 10, "m0", 100}, {SEARCH_TRACK, 11, "m1", 100},
        {SEARCH_TRACK, 12, "m2", 100}, {SEARCH_TRACK, 13, "m3", 100},
        {SEARCH_TRACK, 14, "m4", 100}, {SEARCH_TRACK, 15, "m5", 100},
        {SEARCH_TRACK, 16, "m6", 100}, {SEARCH_TRACK, 17, "m7", 100},
        {SEARCH_TRACK, 18, "m8", 100}, {SEARCH_TRACK, 19, "m9", 100}}},
  };
  fs.addFile("/openpod/music_index.bin", buildFlatMusicIndex(leaves));
}

// Types `text` (lowercase letters, digits and spaces) through the ribbon.
void type(SearchModel &search, MusicLookup &lookup, const char *text) {
  for (const char *p = text; *p; p++) {
    if (*p == ' ') {
      search.space(lookup);
      continue;
    }
    char c = (*p >= 'a' && *p <= 'z') ? *p - 'a' + 'A' : *p;
    int target = (int)(strchr(SearchModel::RIBBON, c) - SearchModel::RIBBON);
    search.moveCursor(target - search.getCursor());
    search.append(lookup);
  }
}

} // namespace

TEST_SUITE("Search model") {
  TEST_CASE("Search - normalizeQuery lowercases and drops a leading article") {
    char out[33];
    SearchModel::normalizeQuery("KID A", out, sizeof(out));
    CHECK(std::string(out) == "kid a");
    SearchModel::normalizeQuery("the nat", out, sizeof(out));
    CHECK(std::string(out) == "nat");
    SearchModel::normalizeQuery("les x", out, sizeof(out));
    CHECK(std::string(out) == "x");
    // an article alone is searched as the word itself
    SearchModel::normalizeQuery("the ", out, sizeof(out));
    CHECK(std::string(out) == "the");
    // only whole leading words count
    SearchModel::normalizeQuery("theory", out, sizeof(out));
    CHECK(std::string(out) == "theory");
    SearchModel::normalizeQuery("a", out, sizeof(out));
    CHECK(std::string(out) == "a");
  }

  TEST_CASE("Search - rankHits dedups by (type, id) and sorts") {
    search_result_t raw[5] = {
        {SEARCH_TRACK, 1, 0, 9, 50},
        {SEARCH_TRACK, 1, 0, 9, 100}, // same track, better relevance
        {SEARCH_ALBUM, 1, 0, 5, 100}, // same id, other type: kept
        {SEARCH_ARTIST, 3, 0, 3, 100},
        {SEARCH_TRACK, 4, 0, 4, 60},
    };
    SearchModel::Hit hits[5];
    int n = SearchModel::rankHits(raw, 5, hits, 5);

    REQUIRE(n == 4);
    // relevance 100, shorter names first
    CHECK(hits[0].type == SEARCH_ARTIST);
    CHECK(hits[1].type == SEARCH_ALBUM);
    CHECK(hits[2].type == SEARCH_TRACK);
    CHECK(hits[2].id == 1);
    CHECK(hits[2].relevance == 100);
    CHECK(hits[3].id == 4);
  }

  TEST_CASE("Search - ribbon cursor clamps at both ends") {
    SearchModel search;
    CHECK(search.getCursorChar() == 'A');
    CHECK(search.moveCursor(-3) == false);
    CHECK(search.moveCursor(100) == true);
    CHECK(search.getCursorChar() == '9');
    CHECK(search.moveCursor(1) == false);
  }

  TEST_CASE("Search - typing finds tracks, deduped, with artist subtitles") {
    setupSearchCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());
    MusicIndex index;
    REQUIRE(index.init(lookup.getLastPSRAMAddress()));
    SearchModel search;
    search.setIndex(&index);

    type(search, lookup, "son");

    CHECK(std::string(search.getQuery()) == "son");
    REQUIRE(search.getHitCount() == 2);
    // equal relevance: the shorter name first
    CHECK(search.getHit(0).id == 1);
    CHECK(std::string(search.getRowTitle(0)) == "First Song");
    CHECK(std::string(search.getRowSub(0)) == "Solo Artist");
    CHECK(std::string(search.getRowTitle(1)) == "Second Song");
    CHECK(search.getRowTitle(2)[0] == '\0');
  }

  TEST_CASE("Search - album, artist and genre rows") {
    setupSearchCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());
    MusicIndex index;
    REQUIRE(index.init(lookup.getLastPSRAMAddress()));
    SearchModel search;
    search.setIndex(&index);

    type(search, lookup, "dou");
    REQUIRE(search.getHitCount() == 1);
    CHECK(search.getHit(0).type == SEARCH_ALBUM);
    CHECK(std::string(search.getRowTitle(0)) == "Double Album");
    // no album_to_artist.bin: the first track's artist
    CHECK(std::string(search.getRowSub(0)) == "Solo Artist");

    search.reset();
    type(search, lookup, "sol");
    REQUIRE(search.getHitCount() == 1);
    CHECK(std::string(search.getRowTitle(0)) == "Solo Artist");
    CHECK(std::string(search.getRowSub(0)) == "1 album");

    search.reset();
    type(search, lookup, "ind");
    REQUIRE(search.getHitCount() == 1);
    CHECK(std::string(search.getRowTitle(0)) == "Indie");
    CHECK(std::string(search.getRowSub(0)) == "1 album");
  }

  TEST_CASE("Search - space and backspace at the edges") {
    setupSearchCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());
    MusicIndex index;
    REQUIRE(index.init(lookup.getLastPSRAMAddress()));
    SearchModel search;
    search.setIndex(&index);

    CHECK(search.space(lookup) == false);     // no leading space
    CHECK(search.backspace(lookup) == false); // nothing to delete

    type(search, lookup, "first ");
    CHECK(search.space(lookup) == false); // no double space
    CHECK(std::string(search.getQuery()) == "first ");
    CHECK(search.getHitCount() == 1);

    type(search, lookup, "x");
    CHECK(search.getHitCount() == 0);
    CHECK(search.backspace(lookup) == true);
    CHECK(std::string(search.getQuery()) == "first ");
    CHECK(search.getHitCount() == 1);
  }

  TEST_CASE("Search - results mode needs hits; scrolling keeps the row visible") {
    setupSearchCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());
    MusicIndex index;
    REQUIRE(index.init(lookup.getLastPSRAMAddress()));
    SearchModel search;
    search.setIndex(&index);

    CHECK(search.enterResults() == false); // no hits yet
    CHECK(search.getSelectedHit() == nullptr);

    type(search, lookup, "many");
    REQUIRE(search.getHitCount() == 10);
    CHECK(search.getCapacity() == 6);
    CHECK(search.scroll(1, lookup) == false); // typing: no highlight

    REQUIRE(search.enterResults());
    CHECK(search.getMode() == SearchModel::BROWSING);
    CHECK(search.getCapacity() == 7);
    CHECK(search.getSelected() == 0);

    search.scroll(6, lookup);
    CHECK(search.getSelected() == 6);
    CHECK(search.getFirst() == 0);
    search.scroll(1, lookup);
    CHECK(search.getSelected() == 7);
    CHECK(search.getFirst() == 1);
    search.scroll(100, lookup);
    CHECK(search.getSelected() == 9);
    CHECK(search.getFirst() == 3);
    search.scroll(-9, lookup);
    CHECK(search.getSelected() == 0);
    CHECK(search.getFirst() == 0);

    CHECK(search.exitResults() == true);
    CHECK(search.getMode() == SearchModel::TYPING);
  }
}

TEST_SUITE("State search navigation") {
  // ROOT -> Search, with `text` typed and the results entered.
  void openSearch(State &state, MusicLookup &lookup, MusicIndex &index,
                  const char *text) {
    state.getSearch().setIndex(&index);
    state.loadCurrentRouteData(lookup);
    for (int i = 0; i < 4; i++)
      state.scrollDown(); // "Search" root menu item
    state.goToSelected(lookup);
    REQUIRE(state.getCurrentRoute().type == Route_t::SEARCH_RESULTS);
    type(state.getSearch(), lookup, text);
    state.searchEnterResults();
  }

  TEST_CASE("State - opening an artist from search scopes its albums") {
    setupSearchCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());
    MusicIndex index;
    REQUIRE(index.init(lookup.getLastPSRAMAddress()));
    State state;
    openSearch(state, lookup, index, "sol");

    state.goToSelected(lookup);

    CHECK(state.getCurrentRoute().type == Route_t::ALBUMS);
    CHECK(state.getCurrentRoute().scope == Route_t::ARTISTS);
    CHECK(std::string(state.getHeaderTitle()) == "Solo Artist");
    REQUIRE(state.getTotalElements() == 1);
    CHECK(std::strcmp(state.getVisibleElements()[0], "Double Album") == 0);

    // Album -> its tracks, still resolved through the artist scope
    state.goToSelected(lookup);
    CHECK(state.getCurrentRoute().type == Route_t::TRACKS);
    CHECK(state.getTotalElements() == 2);
  }

  TEST_CASE("State - back from a search result returns to the search") {
    setupSearchCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());
    MusicIndex index;
    REQUIRE(index.init(lookup.getLastPSRAMAddress()));
    State state;
    openSearch(state, lookup, index, "dou");

    state.goToSelected(lookup);
    CHECK(state.getCurrentRoute().type == Route_t::TRACKS);
    CHECK(state.getCurrentRoute().scope == Route_t::ALBUMS);
    CHECK(state.getTotalElements() == 2);

    state.back(lookup);
    CHECK(state.getCurrentRoute().type == Route_t::SEARCH_RESULTS);
    CHECK(std::string(state.getSearch().getQuery()) == "dou");
    CHECK(state.getSearch().getMode() == SearchModel::BROWSING);
    CHECK(state.getSearch().getHitCount() == 1);
  }

  TEST_CASE("State - playing a track from search queues its album") {
    setupSearchCatalog();
    MusicLookup lookup;
    REQUIRE(lookup.init());
    MusicIndex index;
    REQUIRE(index.init(lookup.getLastPSRAMAddress()));
    State state;
    openSearch(state, lookup, index, "second");

    state.goToSelected(lookup);

    CHECK(state.getCurrentRoute().type == Route_t::NOW_PLAYING);
    CHECK(state.getPlayingTrackId() == 2);
    CHECK(state.getPlayingTrackIndex() == 1); // second track of the album
    CHECK(std::string(state.getPlayingTrackName()) == "Second Song");

    // previous walks back through the album
    CHECK(state.previousTrack(lookup) == true);
    CHECK(state.getPlayingTrackId() == 1);
  }
}

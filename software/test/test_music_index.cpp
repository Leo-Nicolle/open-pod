#include <doctest.h>
#include <cstring>
#include <vector>

#include "../src/storage/Music_index.h"
#include "bin_helpers.h"
#include "test_helpers.h"

// Build a small but valid music_index.bin (matching the indexer's
// exportToBinary layout):
//   header: string_pool_size, node_count, result_count, string_offset_count
//   string pool, string offsets, nodes, results
//
// Catalog encoded in the trie:
//   root
//    +-- "ab"  -> result 0 (track  id 1, name "alpha")
//    +-- "ac"  -> result 1 (artist id 2, name "acid")
static std::vector<uint8_t> buildMinimalMusicIndex() {
  std::vector<uint8_t> out;

  const char *pool = "abacalphaacid"; // "" + "ab" + "ac" + "alpha" + "acid"
  const uint32_t poolSize = 13;
  const uint32_t nodeCount = 3;
  const uint32_t resultCount = 2;
  const uint32_t stringOffsetCount = 5;

  openpod_test::pushU32(out, poolSize);
  openpod_test::pushU32(out, nodeCount);
  openpod_test::pushU32(out, resultCount);
  openpod_test::pushU32(out, stringOffsetCount);

  for (uint32_t i = 0; i < poolSize; i++)
    out.push_back((uint8_t)pool[i]);

  const uint32_t offsets[5] = {0, 0, 2, 4, 9}; // "", "ab", "ac", "alpha", "acid"
  for (uint32_t i = 0; i < 5; i++)
    openpod_test::pushU32(out, offsets[i]);

  // node 0: root, key "", 2 children starting at 1, no results
  openpod_test::pushU32(out, 0);  // key_offset
  openpod_test::pushU32(out, 0);  // key_length
  openpod_test::pushU32(out, 2);  // child_count
  openpod_test::pushU32(out, 1);  // first_child_offset
  openpod_test::pushU32(out, 0);  // result_count
  openpod_test::pushU32(out, 0);  // first_result_offset

  // node 1: key "ab", no children, 1 result at 0
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, 2);
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, 1);
  openpod_test::pushU32(out, 0);

  // node 2: key "ac", no children, 1 result at 1
  openpod_test::pushU32(out, 2);
  openpod_test::pushU32(out, 2);
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, 1);
  openpod_test::pushU32(out, 1);

  // result 0: type=track(0), id=1, name "alpha" (offset 4, len 5)
  openpod_test::pushU32(out, 0);
  openpod_test::pushU32(out, 1);
  openpod_test::pushU32(out, 4);
  openpod_test::pushU32(out, 5);
  openpod_test::pushU32(out, 100);

  // result 1: type=artist(1), id=2, name "acid" (offset 9, len 4)
  openpod_test::pushU32(out, 1);
  openpod_test::pushU32(out, 2);
  openpod_test::pushU32(out, 9);
  openpod_test::pushU32(out, 4);
  openpod_test::pushU32(out, 90);

  return out;
}

TEST_CASE("MusicIndex - init loads the binary trie") {
  openpod_test::resetAll();
  openpod_test::FakeFs::get().addFile("/openpod/music_index.bin",
                                      buildMinimalMusicIndex());

  MusicIndex idx;
  CHECK(idx.init(0x200000) == true);
  CHECK(idx.isInitialized() == true);
}

TEST_CASE("MusicIndex - init fails when file is missing") {
  openpod_test::resetAll();

  MusicIndex idx;
  CHECK(idx.init(0x200000) == false);
  CHECK(idx.isInitialized() == false);
}

TEST_CASE("MusicIndex - prefix search returns the matching subtree") {
  openpod_test::resetAll();
  openpod_test::FakeFs::get().addFile("/openpod/music_index.bin",
                                      buildMinimalMusicIndex());

  MusicIndex idx;
  REQUIRE(idx.init(0x200000));

  search_result_t results[10];

  // "ab" -> the "ab" node -> track "alpha"
  uint32_t n = idx.searchByPrefix("ab", results, 10);
  CHECK(n == 1);
  CHECK(results[0].type == SEARCH_TRACK);
  CHECK(results[0].id == 1);

  // "ac" -> the "ac" node -> artist "acid"
  n = idx.searchByPrefix("ac", results, 10);
  CHECK(n == 1);
  CHECK(results[0].type == SEARCH_ARTIST);
  CHECK(results[0].id == 2);

  // unknown prefix -> no results
  CHECK(idx.searchByPrefix("zz", results, 10) == 0);

  // empty prefix -> no results
  CHECK(idx.searchByPrefix("", results, 10) == 0);
}

TEST_CASE("MusicIndex - type-filtered searches") {
  openpod_test::resetAll();
  openpod_test::FakeFs::get().addFile("/openpod/music_index.bin",
                                      buildMinimalMusicIndex());

  MusicIndex idx;
  REQUIRE(idx.init(0x200000));

  search_result_t results[10];

  uint32_t n = idx.searchTracks("ab", results, 10);
  CHECK(n == 1);
  CHECK(results[0].type == SEARCH_TRACK);

  n = idx.searchArtists("ac", results, 10);
  CHECK(n == 1);
  CHECK(results[0].type == SEARCH_ARTIST);

  // no albums or genres in this trie
  CHECK(idx.searchAlbums("ab", results, 10) == 0);
  CHECK(idx.searchGenres("ab", results, 10) == 0);
}

TEST_CASE("MusicIndex - getResultName and search type name") {
  openpod_test::resetAll();
  openpod_test::FakeFs::get().addFile("/openpod/music_index.bin",
                                      buildMinimalMusicIndex());

  MusicIndex idx;
  REQUIRE(idx.init(0x200000));

  search_result_t results[10];
  uint32_t n = idx.searchByPrefix("ab", results, 10);
  REQUIRE(n == 1);

  char name[64];
  idx.getResultName(results[0], name, sizeof(name));
  CHECK(std::strcmp(name, "alpha") == 0);

  CHECK(std::strcmp(idx.getSearchTypeName(SEARCH_TRACK), "Track") == 0);
  CHECK(std::strcmp(idx.getSearchTypeName(SEARCH_ARTIST), "Artist") == 0);
  CHECK(std::strcmp(idx.getSearchTypeName(SEARCH_ALBUM), "Album") == 0);
  CHECK(std::strcmp(idx.getSearchTypeName(SEARCH_GENRE), "Genre") == 0);
}

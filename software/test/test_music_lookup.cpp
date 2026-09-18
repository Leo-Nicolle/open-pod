#include <doctest.h>
#include <cstring>

#include "../src/storage/Music_lookup.h"
#include "test_helpers.h"

TEST_CASE("MusicLookup - init with complete catalog") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();

  MusicLookup lookup;
  CHECK(lookup.init() == true);
  CHECK(lookup.isInitialized() == true);
}

TEST_CASE("MusicLookup - init fails when a file is missing") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();
  openpod_test::FakeFs::get().files.erase("/openpod/track_index.bin");

  MusicLookup lookup;
  CHECK(lookup.init() == false);
  CHECK(lookup.isInitialized() == false);
}

TEST_CASE("MusicLookup - name lookups") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  char buf[256];
  CHECK(lookup.getArtistName(1, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Artist One") == 0);
  CHECK(lookup.getArtistName(2, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Artist Two") == 0);

  CHECK(lookup.getAlbumName(1, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Album A") == 0);
  CHECK(lookup.getGenreName(1, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Rock") == 0);
  CHECK(lookup.getTrackName(2, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Track Two") == 0);

  CHECK(lookup.getArtistName(999, buf, sizeof(buf)) == false);
  CHECK(lookup.getTrackName(999, buf, sizeof(buf)) == false);
}

TEST_CASE("MusicLookup - track path lookup") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  char buf[256];
  CHECK(lookup.getTrackPath(1, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "/music/artist_one/track_one.flac") == 0);
  CHECK(lookup.getTrackPath(2, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "/music/artist_two/track_two.flac") == 0);
  CHECK(lookup.getTrackPath(999, buf, sizeof(buf)) == false);
}

TEST_CASE("MusicLookup - browse all artists, albums and genres") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  char buffer[4096];
  const char *ptrs[16];

  uint32_t n = lookup.getAllArtists(buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 2);
  CHECK(std::strcmp(ptrs[0], "Artist One") == 0);
  CHECK(std::strcmp(ptrs[1], "Artist Two") == 0);

  n = lookup.getAllAlbums(buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 2);
  CHECK(std::strcmp(ptrs[0], "Album A") == 0);
  CHECK(std::strcmp(ptrs[1], "Album B") == 0);

  n = lookup.getAllGenres(buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 1);
  CHECK(std::strcmp(ptrs[0], "Rock") == 0);
}

TEST_CASE("MusicLookup - relationship string lookups") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  char buffer[4096];
  const char *ptrs[16];

  uint32_t n = lookup.getTracksByArtist(1, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 1);
  CHECK(std::strcmp(ptrs[0], "Track One") == 0);

  n = lookup.getAlbumsByArtist(1, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 1);
  CHECK(std::strcmp(ptrs[0], "Album A") == 0);

  n = lookup.getTracksByAlbum(2, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 1);
  CHECK(std::strcmp(ptrs[0], "Track Two") == 0);

  n = lookup.getTracksByGenre(1, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 2);

  n = lookup.getAlbumsByGenre(1, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 2);
}

TEST_CASE("MusicLookup - id accessors") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  CHECK(lookup.getArtistIdAtIndex(0) == 1);
  CHECK(lookup.getArtistIdAtIndex(1) == 2);
  CHECK(lookup.getAlbumIdAtIndex(0) == 1);
  CHECK(lookup.getAlbumIdAtIndex(1) == 2);
  CHECK(lookup.getGenreIdAtIndex(0) == 1);

  CHECK(lookup.getTrackIdByArtistAtIndex(1, 0) == 1);
  CHECK(lookup.getAlbumIdByArtistAtIndex(1, 0) == 1);
  CHECK(lookup.getTrackIdByAlbumAtIndex(2, 0) == 2);
  CHECK(lookup.getTrackIdByGenreAtIndex(1, 1) == 2);
  CHECK(lookup.getAlbumIdByGenreAtIndex(1, 1) == 2);
}

TEST_CASE("MusicLookup - printStats does not crash") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  lookup.printStats();
  CHECK(true);
}

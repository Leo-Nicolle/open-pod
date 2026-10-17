#include <doctest.h>
#include <cstring>

#include "../src/storage/Music_lookup.h"
#include "test_helpers.h"

TEST_CASE("MusicLookup - init with complete catalog") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  CHECK(lookup.init() == true);
  CHECK(lookup.isInitialized() == true);
}

TEST_CASE("MusicLookup - init fails when a file is missing") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();
  opentone_test::FakeFs::get().files.erase("/opentone/track_index.bin");

  MusicLookup lookup;
  CHECK(lookup.init() == false);
  CHECK(lookup.isInitialized() == false);
}

TEST_CASE("MusicLookup - name lookups") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  char buf[256];
  CHECK(lookup.getArtistName(0, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Artist One") == 0);
  CHECK(lookup.getArtistName(1, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Artist Two") == 0);

  CHECK(lookup.getAlbumName(0, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Album A") == 0);
  CHECK(lookup.getGenreName(0, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Rock") == 0);
  CHECK(lookup.getTrackName(1, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "Track Two") == 0);

  CHECK(lookup.getArtistName(999, buf, sizeof(buf)) == false);
  CHECK(lookup.getTrackName(999, buf, sizeof(buf)) == false);
}

TEST_CASE("MusicLookup - track path lookup") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  char buf[256];
  CHECK(lookup.getTrackPath(0, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "/music/artist_one/track_one.flac") == 0);
  CHECK(lookup.getTrackPath(1, buf, sizeof(buf)) == true);
  CHECK(std::strcmp(buf, "/music/artist_two/track_two.flac") == 0);
  CHECK(lookup.getTrackPath(999, buf, sizeof(buf)) == false);
}

TEST_CASE("MusicLookup - browse all artists, albums and genres") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

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

  n = lookup.getAllTracks(buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 2);
  CHECK(std::strcmp(ptrs[0], "Track One") == 0);
  CHECK(std::strcmp(ptrs[1], "Track Two") == 0);
}

TEST_CASE("MusicLookup - relationship string lookups") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  char buffer[4096];
  const char *ptrs[16];

  uint32_t n = lookup.getTracksByArtist(0, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 1);
  CHECK(std::strcmp(ptrs[0], "Track One") == 0);

  n = lookup.getAlbumsByArtist(0, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 1);
  CHECK(std::strcmp(ptrs[0], "Album A") == 0);

  n = lookup.getTracksByAlbum(1, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 1);
  CHECK(std::strcmp(ptrs[0], "Track Two") == 0);

  n = lookup.getTracksByGenre(0, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 2);

  n = lookup.getAlbumsByGenre(0, buffer, sizeof(buffer), ptrs, 16);
  CHECK(n == 2);
}

TEST_CASE("MusicLookup - id accessors") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  // Ids are dense and in list order.
  CHECK(lookup.getArtistIdAtIndex(0) == 0);
  CHECK(lookup.getArtistIdAtIndex(1) == 1);
  CHECK(lookup.getAlbumIdAtIndex(0) == 0);
  CHECK(lookup.getAlbumIdAtIndex(1) == 1);
  CHECK(lookup.getGenreIdAtIndex(0) == 0);
  CHECK(lookup.getTrackIdAtIndex(0) == 0);
  CHECK(lookup.getTrackIdAtIndex(1) == 1);
  CHECK(lookup.getTrackIdAtIndex(2) == 0xFFFFFFFFu);

  CHECK(lookup.getTrackIdByArtistAtIndex(0, 0) == 0);
  CHECK(lookup.getAlbumIdByArtistAtIndex(0, 0) == 0);
  CHECK(lookup.getTrackIdByAlbumAtIndex(1, 0) == 1);
  CHECK(lookup.getTrackIdByGenreAtIndex(0, 1) == 1);
  CHECK(lookup.getAlbumIdByGenreAtIndex(0, 1) == 1);
  CHECK(lookup.getAlbumIdByGenreAtIndex(0, 2) == 0xFFFFFFFFu);
}

TEST_CASE("MusicLookup - track to album lookup") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  CHECK(lookup.getAlbumIdForTrack(0) == 0);
  CHECK(lookup.getAlbumIdForTrack(1) == 1);
  CHECK(lookup.getAlbumIdForTrack(999) == 0xFFFFFFFFu);
}

TEST_CASE("MusicLookup - track to artist lookup") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  CHECK(lookup.getArtistIdForTrack(0) == 0);
  CHECK(lookup.getArtistIdForTrack(1) == 1);
  CHECK(lookup.getArtistIdForTrack(999) == 0xFFFFFFFFu);

  char name[64];
  REQUIRE(lookup.getArtistName(lookup.getArtistIdForTrack(1), name,
                               sizeof(name)));
  CHECK(std::strcmp(name, "Artist Two") == 0);
}

TEST_CASE("MusicLookup - the track->artist section is optional") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();
  opentone_test::FakeFs::get().files.erase("/opentone/track_to_artist.bin");

  MusicLookup lookup;
  REQUIRE(lookup.init());

  CHECK(lookup.getArtistIdForTrack(0) == 0xFFFFFFFFu);
  CHECK(lookup.getAlbumIdForTrack(1) == 1);
}

TEST_CASE("MusicLookup - album cover lookup") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  album_cover_entry_t cover;
  CHECK(lookup.getAlbumCoverEntry(0, cover) == true);
  CHECK(cover.offset == 0);
  CHECK(cover.length == 100);
  CHECK(cover.format == 1);

  CHECK(lookup.getAlbumCoverEntry(1, cover) == true);
  CHECK(cover.offset == 100);
  CHECK(cover.length == 200);
  CHECK(cover.format == 0);

  CHECK(lookup.getAlbumCoverEntry(999, cover) == false);
}

TEST_CASE("MusicLookup - printStats does not crash") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  lookup.printStats();
  CHECK(true);
}

TEST_CASE("MusicLookup - relation counts") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();

  MusicLookup lookup;
  REQUIRE(lookup.init());

  CHECK(lookup.getAlbumCountForArtist(0) == 1);
  CHECK(lookup.getAlbumCountForGenre(0) == 2);
  CHECK(lookup.getTrackCountForAlbum(1) == 1);
  CHECK(lookup.getAlbumCountForArtist(999) == 0);

  CHECK(lookup.getTrackIndexInAlbum(1, 1) == 0);
  CHECK(lookup.getTrackIndexInAlbum(1, 0) == -1);
}

TEST_CASE("MusicLookup - album artist from the album->artist section") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog();
  // Deliberately differs from the tracks' artists, to tell it from the
  // fallback. Dense by album id: album 0 -> artist 1.
  std::vector<std::pair<uint32_t, uint32_t>> album_to_artist;
  album_to_artist.push_back(std::make_pair(0u, 1u));
  opentone_test::FakeFs::get().addFile(
      "/opentone/album_to_artist.bin",
      opentone_test::buildTrackToAlbumIndex(album_to_artist));

  MusicLookup lookup;
  REQUIRE(lookup.init());

  CHECK(lookup.getArtistIdForAlbum(0) == 1);
}

TEST_CASE("MusicLookup - album artist falls back to the first track's") {
  opentone_test::resetAll();
  opentone_test::registerSampleCatalog(); // no album->artist section

  MusicLookup lookup;
  REQUIRE(lookup.init());

  CHECK(lookup.getArtistIdForAlbum(1) == 1); // album 1 -> track 1 -> artist 1
  CHECK(lookup.getArtistIdForAlbum(999) == 0xFFFFFFFFu);
}

TEST_CASE("MusicLookup - long lists load in windows") {
  opentone_test::resetAll();
  opentone_test::registerLargeCatalog();
  MusicLookup lookup;
  REQUIRE(lookup.init());

  CHECK(lookup.getArtistCount() == 300);
  char buffer[4096];
  const char *names[LOOKUP_WINDOW];
  REQUIRE(lookup.getAllArtists(buffer, sizeof(buffer), names, LOOKUP_WINDOW, 250) == 50);
  CHECK(strcmp(names[0], "Artist 251") == 0);
  CHECK(strcmp(names[49], "Artist 300") == 0);

  // An album's tracks past the old 50-id cap, after another album's.
  CHECK(lookup.getTrackCountForAlbum(1) == 120);
  REQUIRE(lookup.getTracksByAlbum(1, buffer, sizeof(buffer), names, LOOKUP_WINDOW, 100) == 20);
  CHECK(strcmp(names[0], "Track 101") == 0);
  CHECK(strcmp(names[19], "Track 120") == 0);
  CHECK(lookup.getTrackIndexInAlbum(1, 100) == 100);
  CHECK(lookup.getTrackIndexInAlbum(0, 121) == 1);
}

TEST_CASE("MusicLookup - a full buffer stops loading instead of overflowing") {
  opentone_test::resetAll();
  opentone_test::registerLargeCatalog();
  MusicLookup lookup;
  REQUIRE(lookup.init());

  // Room for 3 names of 11 bytes ("Artist 001" + NUL), then a canary.
  char buffer[40];
  memset(buffer, 'X', sizeof(buffer));
  const char *names[LOOKUP_WINDOW];
  CHECK(lookup.getAllArtists(buffer, 33, names, LOOKUP_WINDOW, 0) == 3);
  CHECK(strcmp(names[2], "Artist 003") == 0);
  for (int i = 33; i < 40; i++) {
    CHECK(buffer[i] == 'X');
  }
}

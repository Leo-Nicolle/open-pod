// Shared helpers for storage tests: reset the fakes and register a small
// sample music catalog in the in-memory file system.
#pragma once
#include <cstdio>

#include "bin_helpers.h"
#include "mocks/SdFat.h"
#include "mocks/fake_psram.h"

namespace openpod_test {

inline void resetAll() {
  fakePsram().reset();
  FakeFs::get().reset();
}

inline void registerSampleCatalog() {
  auto &fs = FakeFs::get();

  std::vector<std::pair<uint32_t, std::string>> artists;
  artists.push_back(std::make_pair(1u, std::string("Artist One")));
  artists.push_back(std::make_pair(2u, std::string("Artist Two")));
  fs.addFile("/openpod/artist_index.bin", buildStringIndex(artists));

  std::vector<std::pair<uint32_t, std::string>> albums;
  albums.push_back(std::make_pair(1u, std::string("Album A")));
  albums.push_back(std::make_pair(2u, std::string("Album B")));
  fs.addFile("/openpod/album_index.bin", buildStringIndex(albums));

  std::vector<std::pair<uint32_t, std::string>> genres;
  genres.push_back(std::make_pair(1u, std::string("Rock")));
  fs.addFile("/openpod/genre_index.bin", buildStringIndex(genres));

  std::vector<std::pair<uint32_t, std::string>> tracks;
  tracks.push_back(std::make_pair(1u, std::string("Track One")));
  tracks.push_back(std::make_pair(2u, std::string("Track Two")));
  fs.addFile("/openpod/track_index.bin", buildStringIndex(tracks));

  std::vector<std::pair<uint32_t, std::string>> paths;
  paths.push_back(
      std::make_pair(1u, std::string("/music/artist_one/track_one.flac")));
  paths.push_back(
      std::make_pair(2u, std::string("/music/artist_two/track_two.flac")));
  fs.addFile("/openpod/path_index.bin", buildStringIndex(paths));

  std::vector<std::pair<uint32_t, uint16_t>> durations;
  durations.push_back(std::make_pair(1u, (uint16_t)200));
  durations.push_back(std::make_pair(2u, (uint16_t)180));
  fs.addFile("/openpod/duration_index.bin", buildDurationIndex(durations));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> artist_to_albums;
  artist_to_albums.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  artist_to_albums.push_back(std::make_pair(2u, std::vector<uint32_t>{2}));
  fs.addFile("/openpod/artist_to_albums.bin", buildRelation(artist_to_albums));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> artist_to_tracks;
  artist_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  artist_to_tracks.push_back(std::make_pair(2u, std::vector<uint32_t>{2}));
  fs.addFile("/openpod/artist_to_tracks.bin", buildRelation(artist_to_tracks));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> album_to_tracks;
  album_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  album_to_tracks.push_back(std::make_pair(2u, std::vector<uint32_t>{2}));
  fs.addFile("/openpod/album_to_tracks.bin", buildRelation(album_to_tracks));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> genre_to_albums;
  genre_to_albums.push_back(
      std::make_pair(1u, std::vector<uint32_t>{1, 2}));
  fs.addFile("/openpod/genre_to_albums.bin", buildRelation(genre_to_albums));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> genre_to_tracks;
  genre_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1, 2}));
  fs.addFile("/openpod/genre_to_tracks.bin", buildRelation(genre_to_tracks));

  std::vector<std::pair<uint32_t, uint32_t>> track_to_album;
  track_to_album.push_back(std::make_pair(1u, 1u));
  track_to_album.push_back(std::make_pair(2u, 2u));
  fs.addFile("/openpod/track_to_album.bin", buildTrackToAlbumIndex(track_to_album));

  // Same dense uint32-per-track layout as track_to_album.
  std::vector<std::pair<uint32_t, uint32_t>> track_to_artist;
  track_to_artist.push_back(std::make_pair(1u, 1u));
  track_to_artist.push_back(std::make_pair(2u, 2u));
  fs.addFile("/openpod/track_to_artist.bin", buildTrackToAlbumIndex(track_to_artist));

  std::vector<CoverIndexEntry> album_to_cover;
  album_to_cover.push_back({1u, 0u, 100u, 1});
  album_to_cover.push_back({2u, 100u, 200u, 0});
  fs.addFile("/openpod/album_to_cover.bin", buildAlbumCoverIndex(album_to_cover));
}

// Additive, self-contained catalog used only by the State tests (never by
// test_music_lookup.cpp, so its exact-count assertions on registerSampleCatalog()
// stay untouched). registerSampleCatalog()'s catalog gives every album exactly
// one track, so it can't exercise multi-track playback (auto-advance to the
// next track). This one puts two tracks under a single album so State tests
// can navigate ROOT -> Albums -> "Double Album" -> Tracks and land on a
// two-item TRACKS list.
inline void registerStateTestCatalog() {
  auto &fs = FakeFs::get();

  std::vector<std::pair<uint32_t, std::string>> artists;
  artists.push_back(std::make_pair(1u, std::string("Solo Artist")));
  fs.addFile("/openpod/artist_index.bin", buildStringIndex(artists));

  std::vector<std::pair<uint32_t, std::string>> albums;
  albums.push_back(std::make_pair(1u, std::string("Double Album")));
  fs.addFile("/openpod/album_index.bin", buildStringIndex(albums));

  std::vector<std::pair<uint32_t, std::string>> genres;
  genres.push_back(std::make_pair(1u, std::string("Indie")));
  fs.addFile("/openpod/genre_index.bin", buildStringIndex(genres));

  std::vector<std::pair<uint32_t, std::string>> tracks;
  tracks.push_back(std::make_pair(1u, std::string("First Song")));
  tracks.push_back(std::make_pair(2u, std::string("Second Song")));
  fs.addFile("/openpod/track_index.bin", buildStringIndex(tracks));

  std::vector<std::pair<uint32_t, std::string>> paths;
  paths.push_back(
      std::make_pair(1u, std::string("/music/solo/first.flac")));
  paths.push_back(
      std::make_pair(2u, std::string("/music/solo/second.flac")));
  fs.addFile("/openpod/path_index.bin", buildStringIndex(paths));

  std::vector<std::pair<uint32_t, uint16_t>> durations;
  durations.push_back(std::make_pair(1u, (uint16_t)150));
  durations.push_back(std::make_pair(2u, (uint16_t)170));
  fs.addFile("/openpod/duration_index.bin", buildDurationIndex(durations));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> artist_to_albums;
  artist_to_albums.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  fs.addFile("/openpod/artist_to_albums.bin", buildRelation(artist_to_albums));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> artist_to_tracks;
  artist_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1, 2}));
  fs.addFile("/openpod/artist_to_tracks.bin", buildRelation(artist_to_tracks));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> album_to_tracks;
  album_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1, 2}));
  fs.addFile("/openpod/album_to_tracks.bin", buildRelation(album_to_tracks));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> genre_to_albums;
  genre_to_albums.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  fs.addFile("/openpod/genre_to_albums.bin", buildRelation(genre_to_albums));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> genre_to_tracks;
  genre_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1, 2}));
  fs.addFile("/openpod/genre_to_tracks.bin", buildRelation(genre_to_tracks));

  std::vector<std::pair<uint32_t, uint32_t>> track_to_album;
  track_to_album.push_back(std::make_pair(1u, 1u));
  track_to_album.push_back(std::make_pair(2u, 1u));
  fs.addFile("/openpod/track_to_album.bin", buildTrackToAlbumIndex(track_to_album));

  std::vector<CoverIndexEntry> album_to_cover;
  album_to_cover.push_back({1u, 0u, 50u, 1});
  fs.addFile("/openpod/album_to_cover.bin", buildAlbumCoverIndex(album_to_cover));
}

// Lists longer than the State's window (LOOKUP_WINDOW) and than the old
// caps (200 entries, 50 related ids): 300 artists "Artist 001".."Artist 300";
// album 1 "Short Album" (tracks 201-203) and album 2 "Long Album" (tracks
// 1-120 "Track 001".."Track 120"), so album 2's tracks sit after album 1's
// in album_to_tracks.
inline void registerLargeCatalog() {
  auto &fs = FakeFs::get();
  char name[32];

  std::vector<std::pair<uint32_t, std::string>> artists;
  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> artist_to_albums;
  for (uint32_t id = 1; id <= 300; id++) {
    snprintf(name, sizeof(name), "Artist %03u", (unsigned)id);
    artists.push_back(std::make_pair(id, std::string(name)));
    artist_to_albums.push_back(std::make_pair(id, std::vector<uint32_t>{id == 1 ? 2u : 1u}));
  }
  fs.addFile("/openpod/artist_index.bin", buildStringIndex(artists));
  fs.addFile("/openpod/artist_to_albums.bin", buildRelation(artist_to_albums));

  std::vector<std::pair<uint32_t, std::string>> albums;
  albums.push_back(std::make_pair(1u, std::string("Short Album")));
  albums.push_back(std::make_pair(2u, std::string("Long Album")));
  fs.addFile("/openpod/album_index.bin", buildStringIndex(albums));

  std::vector<std::pair<uint32_t, std::string>> genres;
  genres.push_back(std::make_pair(1u, std::string("Jazz")));
  fs.addFile("/openpod/genre_index.bin", buildStringIndex(genres));

  std::vector<std::pair<uint32_t, std::string>> tracks;
  std::vector<std::pair<uint32_t, std::string>> paths;
  std::vector<std::pair<uint32_t, uint16_t>> durations;
  std::vector<std::pair<uint32_t, uint32_t>> track_to_album;
  std::vector<uint32_t> long_album, short_album;
  for (uint32_t id = 1; id <= 120; id++) long_album.push_back(id);
  for (uint32_t id = 201; id <= 203; id++) short_album.push_back(id);
  for (uint32_t id : long_album) {
    snprintf(name, sizeof(name), "Track %03u", (unsigned)id);
    tracks.push_back(std::make_pair(id, std::string(name)));
    track_to_album.push_back(std::make_pair(id, 2u));
  }
  for (uint32_t id : short_album) {
    snprintf(name, sizeof(name), "Short %u", (unsigned)id);
    tracks.push_back(std::make_pair(id, std::string(name)));
    track_to_album.push_back(std::make_pair(id, 1u));
  }
  for (auto &t : tracks) {
    paths.push_back(std::make_pair(t.first, "/music/" + t.second + ".flac"));
    durations.push_back(std::make_pair(t.first, (uint16_t)180));
  }
  fs.addFile("/openpod/track_index.bin", buildStringIndex(tracks));
  fs.addFile("/openpod/path_index.bin", buildStringIndex(paths));
  fs.addFile("/openpod/duration_index.bin", buildDurationIndex(durations));
  fs.addFile("/openpod/track_to_album.bin", buildTrackToAlbumIndex(track_to_album));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> album_to_tracks;
  album_to_tracks.push_back(std::make_pair(1u, short_album));
  album_to_tracks.push_back(std::make_pair(2u, long_album));
  fs.addFile("/openpod/album_to_tracks.bin", buildRelation(album_to_tracks));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> artist_to_tracks;
  artist_to_tracks.push_back(std::make_pair(1u, long_album));
  fs.addFile("/openpod/artist_to_tracks.bin", buildRelation(artist_to_tracks));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> genre_to_albums;
  genre_to_albums.push_back(std::make_pair(1u, std::vector<uint32_t>{1, 2}));
  fs.addFile("/openpod/genre_to_albums.bin", buildRelation(genre_to_albums));
  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> genre_to_tracks;
  genre_to_tracks.push_back(std::make_pair(1u, long_album));
  fs.addFile("/openpod/genre_to_tracks.bin", buildRelation(genre_to_tracks));

  std::vector<CoverIndexEntry> album_to_cover;
  album_to_cover.push_back({1u, 0u, 50u, 1});
  album_to_cover.push_back({2u, 50u, 50u, 1});
  fs.addFile("/openpod/album_to_cover.bin", buildAlbumCoverIndex(album_to_cover));
}

// Two genres, each scoped to its own distinct artist/album/track, so a test
// can tell "Genres -> just this genre's albums" apart from "Genres -> every
// album regardless of genre" - registerSampleCatalog()'s single genre covers
// every album, so it can't distinguish the two.
inline void registerMultiGenreTestCatalog() {
  auto &fs = FakeFs::get();

  std::vector<std::pair<uint32_t, std::string>> artists;
  artists.push_back(std::make_pair(1u, std::string("Django Reinhardt")));
  artists.push_back(std::make_pair(2u, std::string("MC Solaar")));
  fs.addFile("/openpod/artist_index.bin", buildStringIndex(artists));

  std::vector<std::pair<uint32_t, std::string>> albums;
  albums.push_back(std::make_pair(1u, std::string("Djangology")));
  albums.push_back(std::make_pair(2u, std::string("Qui Seme Le Vent")));
  fs.addFile("/openpod/album_index.bin", buildStringIndex(albums));

  std::vector<std::pair<uint32_t, std::string>> genres;
  genres.push_back(std::make_pair(1u, std::string("Gipsy Jazz")));
  genres.push_back(std::make_pair(2u, std::string("Hip Hop")));
  fs.addFile("/openpod/genre_index.bin", buildStringIndex(genres));

  std::vector<std::pair<uint32_t, std::string>> tracks;
  tracks.push_back(std::make_pair(1u, std::string("Minor Swing")));
  tracks.push_back(std::make_pair(2u, std::string("Caroline")));
  fs.addFile("/openpod/track_index.bin", buildStringIndex(tracks));

  std::vector<std::pair<uint32_t, std::string>> paths;
  paths.push_back(std::make_pair(1u, std::string("/music/django/minor_swing.flac")));
  paths.push_back(std::make_pair(2u, std::string("/music/mcsolaar/caroline.flac")));
  fs.addFile("/openpod/path_index.bin", buildStringIndex(paths));

  std::vector<std::pair<uint32_t, uint16_t>> durations;
  durations.push_back(std::make_pair(1u, (uint16_t)180));
  durations.push_back(std::make_pair(2u, (uint16_t)220));
  fs.addFile("/openpod/duration_index.bin", buildDurationIndex(durations));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> artist_to_albums;
  artist_to_albums.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  artist_to_albums.push_back(std::make_pair(2u, std::vector<uint32_t>{2}));
  fs.addFile("/openpod/artist_to_albums.bin", buildRelation(artist_to_albums));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> artist_to_tracks;
  artist_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  artist_to_tracks.push_back(std::make_pair(2u, std::vector<uint32_t>{2}));
  fs.addFile("/openpod/artist_to_tracks.bin", buildRelation(artist_to_tracks));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> album_to_tracks;
  album_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  album_to_tracks.push_back(std::make_pair(2u, std::vector<uint32_t>{2}));
  fs.addFile("/openpod/album_to_tracks.bin", buildRelation(album_to_tracks));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> genre_to_albums;
  genre_to_albums.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  genre_to_albums.push_back(std::make_pair(2u, std::vector<uint32_t>{2}));
  fs.addFile("/openpod/genre_to_albums.bin", buildRelation(genre_to_albums));

  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> genre_to_tracks;
  genre_to_tracks.push_back(std::make_pair(1u, std::vector<uint32_t>{1}));
  genre_to_tracks.push_back(std::make_pair(2u, std::vector<uint32_t>{2}));
  fs.addFile("/openpod/genre_to_tracks.bin", buildRelation(genre_to_tracks));

  std::vector<std::pair<uint32_t, uint32_t>> track_to_album;
  track_to_album.push_back(std::make_pair(1u, 1u));
  track_to_album.push_back(std::make_pair(2u, 2u));
  fs.addFile("/openpod/track_to_album.bin", buildTrackToAlbumIndex(track_to_album));

  std::vector<CoverIndexEntry> album_to_cover;
  album_to_cover.push_back({1u, 0u, 50u, 1});
  album_to_cover.push_back({2u, 50u, 60u, 1});
  fs.addFile("/openpod/album_to_cover.bin", buildAlbumCoverIndex(album_to_cover));
}

} // namespace openpod_test

#pragma once
#include <stdint.h>

// Section ids of /openpod/index.pack and /openpod/podcasts.pack. Keep in
// sync with `pub mod section` in syncer/crates/core/src/index/pack.rs.
namespace PackSection {
// Music (index.pack)
static const uint32_t ARTIST_NAMES = 1;
static const uint32_t ALBUM_NAMES = 2;
static const uint32_t GENRE_NAMES = 3;
static const uint32_t TRACK_NAMES = 4;
static const uint32_t PATHS = 5;
static const uint32_t DURATIONS = 6;
static const uint32_t TRACK_TO_ALBUM = 7;
static const uint32_t TRACK_TO_ARTIST = 8;
static const uint32_t ALBUM_TO_ARTIST = 9;
static const uint32_t ALBUM_TO_COVER = 10;
static const uint32_t ARTIST_TO_ALBUMS = 11;
static const uint32_t ARTIST_TO_TRACKS = 12;
static const uint32_t ALBUM_TO_TRACKS = 13;
static const uint32_t GENRE_TO_ALBUMS = 14;
static const uint32_t GENRE_TO_TRACKS = 15;
static const uint32_t GENRE_TO_ARTISTS = 16;
static const uint32_t SEARCH_TRIE = 17;
// Letter-start tables: LETTERS + list * 4 + letters, list 0..3 = artists,
// albums, genres, tracks, letters 1..3.
static const uint32_t LETTERS = 32;
inline uint32_t letters(uint32_t list, uint32_t letters) {
  return LETTERS + list * 4 + letters;
}

// Podcasts (podcasts.pack)
static const uint32_t POD_FOLDER_NAMES = 64;
static const uint32_t POD_FOLDER_PARENT = 65;
static const uint32_t POD_FOLDER_FLAGS = 66;
static const uint32_t POD_FOLDER_SPAN = 67;
static const uint32_t POD_FOLDER_CHILDREN = 68;
static const uint32_t POD_FOLDER_EPISODES = 69;
static const uint32_t POD_EPISODE_TITLES = 70;
static const uint32_t POD_EPISODE_PATHS = 71;
static const uint32_t POD_EPISODE_DURATIONS = 72;
static const uint32_t POD_EPISODE_FOLDER = 73;
static const uint32_t POD_EPISODE_DATE = 74;
static const uint32_t POD_EPISODE_NUMBER = 75;
static const uint32_t POD_FOLDER_TO_COVER = 76;
} // namespace PackSection

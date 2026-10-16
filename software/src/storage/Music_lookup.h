#pragma once
#include "../pinout.h"
#include "PSRAM_controller.hpp"
#include "PageStore.h"
#include "../state/podcast_source.h"
#include "types.h"
#include <Arduino.h>
#include <SdFat.h>
#include <stdint.h>
#include <string.h>

// Maximum results to return from searches
#define MAX_SEARCH_RESULTS 50
// Most names a list lookup loads at once (State keeps a window of this many
// around the visible rows; lists themselves can be any length).
#define LOOKUP_WINDOW 64

// The lookup tables are sections of /openpod/index.pack (and
// podcasts.pack), read through the PSRAM page cache (PageStore.h). Formats
// in syncer/crates/core/src/index/binary.rs; ids are dense, so every table
// is indexed by id directly.

// String table: [u32 count][u32 data_size][u32 offset]*(count+1)[strings]
typedef struct {
  PackRef sec;
  uint32_t entry_count; // Number of entries
  uint32_t data_size;   // Size of string data section
} index_header_t;

// Relation: [u32 count][u32 total][u32 start]*(count+1)[u32 target]*total,
// source id's targets at [start[id], start[id+1]).
typedef struct {
  PackRef sec;
  uint32_t entry_count;        // Number of source ids
  uint32_t total_target_count; // Total number of target ids
} relation_header_t;

// Dense fixed-width arrays, [u32 count][entry]*count, indexed by id:
// durations (u16 seconds), track->album / track->artist / album->artist
// (u32 ids), covers (9-byte album_cover_entry_t).
typedef struct {
  PackRef sec;
  uint32_t entry_count; // Number of entries
} duration_index_t;
typedef duration_index_t track_to_id_index_t;
typedef duration_index_t album_cover_index_t;

// One album's cover location within thumbs.bin (index/thumbs.rs
// cover_index). format byte: 0 = qoi, 1 = raw565. length == 0 means no cover
// was resolved for this album.
typedef struct {
  uint32_t offset;
  uint32_t length;
  uint8_t format;
} album_cover_entry_t;

// A string inside a string table section.
typedef struct {
  uint32_t offset; // in the section
  uint32_t length; // without the NUL
} lookup_result_t;

// Route management for navigation
struct Route {
  enum RouteType { ROOT, SEARCH_RESULTS, GENRES, ARTISTS, ALBUMS, TRACKS };
  RouteType type;
  uint32_t entityId;     // ID of current entity (genre_id, artist_id, etc.)
  char entityName[64];   // Display name for breadcrumb
  uint32_t currentPage;  // For pagination
  uint32_t totalResults; // Total items available
  bool hasMore;          // Whether more pages exist
};

#define MAX_ROUTE_DEPTH 8

class RouteManager {
private:
  Route routeStack[MAX_ROUTE_DEPTH];
  int stackDepth;

public:
  RouteManager() : stackDepth(0) {}

  void pushRoute(Route::RouteType type, uint32_t entityId = 0,
                 const char *entityName = nullptr) {
    if (stackDepth < MAX_ROUTE_DEPTH) {
      routeStack[stackDepth].type = type;
      routeStack[stackDepth].entityId = entityId;
      routeStack[stackDepth].currentPage = 0;
      routeStack[stackDepth].totalResults = 0;
      routeStack[stackDepth].hasMore = false;

      if (entityName) {
        strncpy(routeStack[stackDepth].entityName, entityName, 63);
        routeStack[stackDepth].entityName[63] = '\0';
      } else {
        routeStack[stackDepth].entityName[0] = '\0';
      }

      stackDepth++;
    }
  }

  bool popRoute() {
    if (stackDepth > 0) {
      stackDepth--;
      return true;
    }
    return false;
  }

  Route &getCurrentRoute() {
    static Route defaultRoute = {Route::ROOT, 0, "", 0, 0, false};
    if (stackDepth > 0) {
      return routeStack[stackDepth - 1];
    }
    return defaultRoute;
  }

  bool canGoBack() { return stackDepth > 1; }
  int getDepth() { return stackDepth; }
};

class MusicLookup : public PodcastSource {
private:
  bool initialized;
  bool index_missing = false;

  // SD card (its own SdFat: see notes/performance-and-memory.md step 2)
  SdFat sd;

  // Headers for all indexes and relationships
  relation_header_t genre_to_tracks_h;
  relation_header_t genre_to_albums_h;
  relation_header_t artist_to_tracks_h;
  relation_header_t artist_to_albums_h;
  relation_header_t album_to_tracks_h;

  index_header_t artist_index_h;
  index_header_t album_index_h;
  index_header_t genre_index_h;
  index_header_t track_index_h;
  index_header_t path_index_h;
  duration_index_t duration_index_h;
  track_to_id_index_t track_to_album_h;
  track_to_id_index_t track_to_artist_h;
  track_to_id_index_t album_to_artist_h;
  album_cover_index_t album_cover_h;
  PackRef search_trie;

  // Podcasts (optional: a card without podcast tables has no Podcasts
  // screen). See state/podcast_source.h.
  bool podcasts_loaded;
  uint32_t podcast_generation;
  index_header_t pod_folder_names_h;
  index_header_t pod_episode_titles_h;
  index_header_t pod_episode_paths_h;
  track_to_id_index_t pod_folder_parent_h;
  track_to_id_index_t pod_folder_flags_h;
  track_to_id_index_t pod_folder_span_h; // 2 values per folder
  track_to_id_index_t pod_episode_folder_h;
  track_to_id_index_t pod_episode_date_h;
  track_to_id_index_t pod_episode_number_h;
  relation_header_t pod_folder_children_h;
  relation_header_t pod_folder_episodes_h;
  duration_index_t pod_episode_durations_h;
  album_cover_index_t pod_folder_cover_h;
  // Opens podcasts.pack; false (no Podcasts screen) when it's missing, a
  // table is, or the meta file doesn't match.
  bool loadPodcasts();
  uint32_t readDense(const track_to_id_index_t &h, uint32_t i);

  // Section headers, checked against the section length.
  bool openStrings(uint8_t pack, uint32_t id, index_header_t &h);
  bool openRelation(uint8_t pack, uint32_t id, relation_header_t &h);
  bool openDense(uint8_t pack, uint32_t id, duration_index_t &h,
                 uint32_t entrySize);

  lookup_result_t getStringAtIndex(uint32_t index, const index_header_t &header);
  uint32_t getRelatedIds(uint32_t source_id, const relation_header_t &header,
                         uint32_t *results, uint32_t max_results,
                         uint32_t offset = 0);
  bool copyString(const index_header_t &header, const lookup_result_t &result,
                  char *&buffer_pos, uint32_t &buffer_remaining,
                  const char *&out);
  uint32_t getIndexRange(const index_header_t &header, char *buffer,
                         uint32_t buffer_size, const char **string_pointers,
                         uint32_t max_results, uint32_t offset);
  // Number of targets related to source_id (0 if it isn't in the relation),
  // without reading the target ids.
  uint32_t getRelatedCount(uint32_t source_id, const relation_header_t &header);
  bool getString(uint32_t id, const index_header_t &index_header, char *buffer,
                 uint32_t buffer_size);
  bool readCover(const album_cover_index_t &h, uint32_t id,
                 album_cover_entry_t &out);

  // Generic string loading function
  uint32_t getLookupStrings(uint32_t source_id,
                            const relation_header_t &relation_header,
                            const index_header_t &target_index_header,
                            char *buffer, uint32_t buffer_size,
                            const char **string_pointers, uint32_t max_results,
                            uint32_t offset = 0);

public:
  MusicLookup();
  ~MusicLookup();

  // Opens /openpod/index.pack (false: no library, the card needs a
  // re-sync) and podcasts.pack if there is one.
  bool init();
  // The card is readable but has no usable index.pack (indexed by an older
  // syncer, or never synced): it needs a re-sync.
  bool indexMissing() const { return index_missing; }
  // Utility
  SdFat &getSD() { return sd; }
  bool isInitialized() const { return initialized; }
  void printStats();

  // Path lookup. Podcast episodes (PODCAST_TRACK_BIT set) resolve through
  // the podcast tables here and in getTrackName / getTrackDurationSeconds.
  bool getTrackPath(uint32_t track_id, char *buffer, uint32_t buffer_size);

  // String lookups
  bool getArtistName(uint32_t artist_id, char *buffer, uint32_t buffer_size);
  bool getAlbumName(uint32_t album_id, char *buffer, uint32_t buffer_size);
  bool getGenreName(uint32_t genre_id, char *buffer, uint32_t buffer_size);
  bool getTrackName(uint32_t track_id, char *buffer, uint32_t buffer_size);

  // Duration lookup (seconds), O(1) direct index - returns 0 if unavailable
  uint32_t getTrackDurationSeconds(uint32_t track_id);

  // Album lookup for a track, O(1) direct index - returns UINT32_MAX if
  // track_id is out of range.
  uint32_t getAlbumIdForTrack(uint32_t track_id);

  // Primary artist for a track, O(1) direct index - returns UINT32_MAX if
  // track_id is out of range or the pack has no track->artist section.
  uint32_t getArtistIdForTrack(uint32_t track_id);

  // Artist for an album, O(1) direct index from the album->artist section.
  // Without it, falls back to the artist of the album's first track.
  // Returns UINT32_MAX if neither resolves.
  uint32_t getArtistIdForAlbum(uint32_t album_id);

  // Relationship sizes (0 if the id is unknown).
  uint32_t getAlbumCountForArtist(uint32_t artist_id);
  uint32_t getAlbumCountForGenre(uint32_t genre_id);
  uint32_t getTrackCountForAlbum(uint32_t album_id);
  uint32_t getTrackCountForArtist(uint32_t artist_id);
  uint32_t getTrackCountForGenre(uint32_t genre_id);

  // Entries in each index (the length of the full lists).
  uint32_t getArtistCount() const;
  uint32_t getAlbumCount() const;
  uint32_t getGenreCount() const;
  uint32_t getTrackCount() const;

  // Position of a track in its album's list order, -1 if it isn't there.
  int32_t getTrackIndexInAlbum(uint32_t album_id, uint32_t track_id);

  // Cover location for an album, O(1) direct index - returns false if
  // album_id is out of range or no cover was resolved for it (length == 0).
  bool getAlbumCoverEntry(uint32_t album_id, album_cover_entry_t &out);

  // Consolidated relationship lookups - new signature
  uint32_t getTracksByArtist(uint32_t artist_id, char *buffer,
                             uint32_t buffer_size, const char **string_pointers,
                             uint32_t max_results,
                             uint32_t offset = 0);
  uint32_t getAlbumsByArtist(uint32_t artist_id, char *buffer,
                             uint32_t buffer_size, const char **string_pointers,
                             uint32_t max_results,
                             uint32_t offset = 0);
  uint32_t getTracksByAlbum(uint32_t album_id, char *buffer,
                            uint32_t buffer_size, const char **string_pointers,
                            uint32_t max_results,
                             uint32_t offset = 0);
  uint32_t getTracksByGenre(uint32_t genre_id, char *buffer,
                            uint32_t buffer_size, const char **string_pointers,
                            uint32_t max_results,
                             uint32_t offset = 0);
  uint32_t getAlbumsByGenre(uint32_t genre_id, char *buffer,
                            uint32_t buffer_size, const char **string_pointers,
                            uint32_t max_results,
                             uint32_t offset = 0);

  // Get all items from an index (for browsing)
  uint32_t getAllArtists(char *buffer, uint32_t buffer_size,
                         const char **string_pointers, uint32_t max_results,
                         uint32_t offset = 0);
  uint32_t getAllAlbums(char *buffer, uint32_t buffer_size,
                        const char **string_pointers, uint32_t max_results,
                        uint32_t offset = 0);
  uint32_t getAllGenres(char *buffer, uint32_t buffer_size,
                        const char **string_pointers, uint32_t max_results,
                        uint32_t offset = 0);
  uint32_t getAllTracks(char *buffer, uint32_t buffer_size,
                        const char **string_pointers, uint32_t max_results,
                        uint32_t offset = 0);
  uint32_t getRelatedIdAtIndex(uint32_t source_id,
                               const relation_header_t &header, uint32_t index);
  uint32_t getIdAtIndex(const index_header_t &header, uint32_t index);
  // Fast scroll on a full list (list 0 artists, 1 albums, 2 genres, 3
  // tracks): where a click from position `pos` lands at `letters` (1-3)
  // letters, from the syncer's letter tables. dir > 0: the next bucket's
  // start (pos at the last one); dir < 0: this bucket's start, or the
  // previous one's when already there. -1 when the pack has no table.
  int32_t letterBucket(uint32_t list, uint32_t letters, uint32_t pos, int dir);

  // The search trie section (for MusicIndex::init).
  const PackRef &getSearchTrie() const { return search_trie; }
  uint32_t getArtistIdAtIndex(uint32_t index);
  uint32_t getAlbumIdAtIndex(uint32_t index);
  uint32_t getGenreIdAtIndex(uint32_t index);
  uint32_t getTrackIdAtIndex(uint32_t index);
  uint32_t getTrackIdByArtistAtIndex(uint32_t artistId, uint32_t index);
  uint32_t getTrackIdByAlbumAtIndex(uint32_t albumId, uint32_t index);
  uint32_t getTrackIdByGenreAtIndex(uint32_t genreId, uint32_t index);
  uint32_t getAlbumIdByArtistAtIndex(uint32_t artistId, uint32_t index);
  uint32_t getAlbumIdByGenreAtIndex(uint32_t genreId, uint32_t index);

  // Show art of a podcast folder (podcast_thumbs.bin), inherited from its
  // parent when it has none.
  bool getPodcastCoverEntry(uint32_t folder, album_cover_entry_t &out);

  // PodcastSource
  bool hasPodcasts() const override { return initialized && podcasts_loaded; }
  uint32_t podcastGeneration() const override { return podcast_generation; }
  uint32_t podcastFolderCount() const override {
    return podcasts_loaded ? pod_folder_names_h.entry_count : 0;
  }
  uint32_t podcastEpisodeCount() const override {
    return podcasts_loaded ? pod_episode_titles_h.entry_count : 0;
  }
  bool podcastFolderName(uint32_t folder, char *buf, uint32_t size) override;
  uint32_t podcastFolderParent(uint32_t folder) override;
  bool podcastFolderIsSeries(uint32_t folder) override;
  void podcastFolderSpan(uint32_t folder, uint32_t &first,
                         uint32_t &end) override;
  uint32_t podcastSubfolderCount(uint32_t folder) override;
  uint32_t podcastSubfolderAt(uint32_t folder, uint32_t index) override;
  uint32_t podcastOwnEpisodeCount(uint32_t folder) override;
  uint32_t podcastOwnEpisodeAt(uint32_t folder, uint32_t index) override;
  bool podcastEpisodeTitle(uint32_t episode, char *buf, uint32_t size) override;
  bool podcastEpisodePath(uint32_t episode, char *buf, uint32_t size) override;
  uint32_t podcastEpisodeDuration(uint32_t episode) override;
  uint32_t podcastEpisodeFolder(uint32_t episode) override;
  uint32_t podcastEpisodeDate(uint32_t episode) override;
  uint32_t podcastEpisodeNumber(uint32_t episode) override;
};
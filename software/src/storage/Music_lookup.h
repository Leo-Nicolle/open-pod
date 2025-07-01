#pragma once
#include "../pinout.h"
#include "PSRAM_controller.hpp"
#include "types.h"
#include <Arduino.h>
#include <SdFat.h>
#include <stdint.h>
#include <string.h>

// Maximum results to return from searches
#define MAX_SEARCH_RESULTS 50

// Structure for index headers (string indexes)
typedef struct {
  uint32_t baseOffset;  // Base address in PSRAM
  uint32_t entry_count; // Number of entries
  uint32_t data_size;   // Size of string data section
} index_header_t;

// Structure for relationship headers
typedef struct {
  uint32_t baseOffset;         // Base address in PSRAM
  uint32_t entry_count;        // Number of source entries
  uint32_t total_target_count; // Total number of target IDs
} relation_header_t;

// Structure for lookup results
typedef struct {
  uint32_t address; // PSRAM address of data
  uint32_t length;  // Length of data
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

class MusicLookup {
private:
  bool initialized;
  uint32_t psram_base_address;
  uint32_t psram_last_address;

  // File paths
  const char *genre_to_tracks_path = "/openpod/genre_to_tracks.bin";
  const char *genre_to_albums_path = "/openpod/genre_to_albums.bin";
  const char *artist_to_tracks_path = "/openpod/artist_to_tracks.bin";
  const char *artist_to_albums_path = "/openpod/artist_to_albums.bin";
  const char *album_to_tracks_path = "/openpod/album_to_tracks.bin";

  const char *artist_index_path = "/openpod/artist_index.bin";
  const char *album_index_path = "/openpod/album_index.bin";
  const char *genre_index_path = "/openpod/genre_index.bin";
  const char *track_index_path = "/openpod/track_index.bin";
  const char *path_index_path = "/openpod/path_index.bin";

  // SD card and file handling
  SdFat sd;
  FsFile file;

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

  // Private helper methods
  bool loadIndexFromSDCard(const char *filename, uint32_t &base_address,
                           index_header_t &header);
  bool loadRelationFromSDCard(const char *filename, uint32_t &base_address,
                              relation_header_t &header);
  bool loadDataFromSDCard(uint32_t base_address, uint32_t size);
  int32_t binarySearch(uint32_t id, uint32_t baseAddress, uint32_t count);
  uint32_t readLittleEndian32(FsFile &file);
  lookup_result_t getStringAtIndex(int32_t index, const index_header_t &header);
  uint32_t getRelatedIds(uint32_t source_id, const relation_header_t &header,
                         uint32_t *results, uint32_t max_results);
  bool getString(uint32_t id, const index_header_t &index_header, char *buffer,
                 uint32_t buffer_size);

  // Generic string loading function
  uint32_t getLookupStrings(uint32_t source_id,
                            const relation_header_t &relation_header,
                            const index_header_t &target_index_header,
                            char *buffer, uint32_t buffer_size,
                            const char **string_pointers, uint32_t max_results);

public:
  MusicLookup(uint32_t psram_base_addr = MUSIC_INDEX_BASE_ADDRESS);
  ~MusicLookup();

  bool init();
  void test();
  // Utility
  SdFat &getSD() { return sd; }
  bool isInitialized() const { return initialized; }
  void printStats();

  // Path lookup
  bool getTrackPath(uint32_t track_id, char *buffer, uint32_t buffer_size);

  // String lookups
  bool getArtistName(uint32_t artist_id, char *buffer, uint32_t buffer_size);
  bool getAlbumName(uint32_t album_id, char *buffer, uint32_t buffer_size);
  bool getGenreName(uint32_t genre_id, char *buffer, uint32_t buffer_size);
  bool getTrackName(uint32_t track_id, char *buffer, uint32_t buffer_size);

  // Consolidated relationship lookups - new signature
  uint32_t getTracksByArtist(uint32_t artist_id, char *buffer,
                             uint32_t buffer_size, const char **string_pointers,
                             uint32_t max_results);
  uint32_t getAlbumsByArtist(uint32_t artist_id, char *buffer,
                             uint32_t buffer_size, const char **string_pointers,
                             uint32_t max_results);
  uint32_t getTracksByAlbum(uint32_t album_id, char *buffer,
                            uint32_t buffer_size, const char **string_pointers,
                            uint32_t max_results);
  uint32_t getTracksByGenre(uint32_t genre_id, char *buffer,
                            uint32_t buffer_size, const char **string_pointers,
                            uint32_t max_results);
  uint32_t getAlbumsByGenre(uint32_t genre_id, char *buffer,
                            uint32_t buffer_size, const char **string_pointers,
                            uint32_t max_results);

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
  uint32_t getRelatedIdAtIndex(uint32_t source_id,
                               const relation_header_t &header, uint32_t index);
  uint32_t getIdAtIndex(const index_header_t &header, uint32_t index);
  uint32_t getLastPSRAMAddress();
  uint32_t getArtistIdAtIndex(uint32_t index);
  uint32_t getAlbumIdAtIndex(uint32_t index);
  uint32_t getGenreIdAtIndex(uint32_t index);
  uint32_t getTrackIdByArtistAtIndex(uint32_t artistId, uint32_t index);
  uint32_t getTrackIdByAlbumAtIndex(uint32_t albumId, uint32_t index);
  uint32_t getTrackIdByGenreAtIndex(uint32_t genreId, uint32_t index);
  uint32_t getAlbumIdByArtistAtIndex(uint32_t artistId, uint32_t index);
  uint32_t getAlbumIdByGenreAtIndex(uint32_t genreId, uint32_t index);
};
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
  uint32_t baseOffset;      // Base address in PSRAM
  uint32_t entry_count;     // Number of entries
  uint32_t data_size;       // Size of string data section
} index_header_t;

// Structure for relationship headers
typedef struct {
  uint32_t baseOffset;           // Base address in PSRAM
  uint32_t entry_count;          // Number of source entries
  uint32_t total_target_count;   // Total number of target IDs
} relation_header_t;

// Structure for lookup results
typedef struct {
  uint32_t address;   // PSRAM address of data
  uint32_t length;    // Length of data
} lookup_result_t;

// Structure for search results
typedef struct {
  uint32_t id;                    // ID of the result
  char name[64];                  // Name (optional, can be filled later)
} string_results_t;

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
  bool loadIndexFromSDCard(const char *filename, uint32_t &base_address, index_header_t &header);
  bool loadRelationFromSDCard(const char *filename, uint32_t &base_address, relation_header_t &header);
  bool loadDataFromSDCard(uint32_t base_address, uint32_t size);
  int32_t binarySearch(uint32_t id, uint32_t baseAddress, uint32_t count);
  uint32_t readLittleEndian32(FsFile &file);
  lookup_result_t getStringAtIndex(int32_t index, const index_header_t &header);
  uint32_t getRelatedIds(uint32_t source_id, const relation_header_t &header, uint32_t *results, uint32_t max_results);
  bool getString(uint32_t id, const index_header_t &index_header, char *buffer, uint32_t buffer_size);

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
  
  // Relationship lookups
  uint32_t getAlbumsByArtist(uint32_t artist_id, string_results_t *results, uint32_t max_results);
  uint32_t getTracksByArtist(uint32_t artist_id, string_results_t *results, uint32_t max_results);
  uint32_t getTracksByAlbum(uint32_t album_id, string_results_t *results, uint32_t max_results);
  uint32_t getTracksByGenre(uint32_t genre_id, string_results_t *results, uint32_t max_results);
  uint32_t getAlbumsByGenre(uint32_t genre_id, string_results_t *results, uint32_t max_results);
  uint32_t getLastPSRAMAddress();
};
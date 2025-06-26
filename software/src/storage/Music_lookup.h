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

class MusicLookup {
private:
  SPI_PSRAM *psram;
  bool initialized;
  const char *genre_to_tracks_path = "/openpod/genre_to_tracks.bin";
  const char *genre_to_artists_path = "/openpod/genre_to_artists.bin";
  const char *genre_to_albums_path = "/openpod/genre_to_albums.bin";
  const char *artist_to_tracks_path = "/openpod/artist_to_tracks.bin";
  const char *artist_to_albums_path = "/openpod/artist_to_albums.bin";
  const char *album_to_tracks_path = "/openpod/album_to_tracks.bin";

  const char *artist_index_path = "/openpod/artist_index.bin";
  const char *album_index_path = "/openpod/album_index.bin";
  const char *genre_index_path = "/openpod/genre_index.bin";
  const char *track_index_path = "/openpod/track_index.bin";
  const char *path_index_path = "/openpod/path_index.bin";

  SdFat sd; 
  File file;
  relation_header_t genre_to_tracks_h;
  relation_header_t genre_to_artists_h;
  relation_header_t genre_to_albums_h;
  relation_header_t artist_to_tracks_h;
  relation_header_t artist_to_albums_h;
  relation_header_t album_to_tracks_h;
  index_header_t artist_index_h;
  index_header_t album_index_h;
  index_header_t genre_index_h;
  index_header_t track_index_h;
  index_header_t path_index_h;
  uint32_t psram_base_address;
  bool initialized;

  bool loadIndexFromSDCard(const char *filename, uint32_t& base_address,
                           index_header_t &header);
  bool loadRelationFromSDCard(const char *filename, uint32_t& base_address,
                              relation_header_t &header);
  bool loadDataFromSDCard(uint32_t base_address);
  void readPathString(uint32_t offset, uint32_t length, char *buffer,
                      uint32_t buffer_size);
int32_t MusicLookup::binarySearch(uint32_t id, uint32_t baseAddres, uint32_t count);
  uint32_t readLittleEndian32(FsFile &file);

public:
  MusicLookup(uint32_t psram_base_addr = MUSIC_INDEX_BASE_ADDRESS);
  ~MusicLookup();
  bool init();
  SdFat &getSD() { return sd; }
  bool getTrackPath(uint32_t track_id, char *buffer, uint32_t buffer_size);
  uint32_t getArtistsByGenre(uint32_t genre_id, search_result_t *results,
                             uint32_t max_results);
  uint32_t getAlbumsByArtist(uint32_t artist_id, search_result_t *results,
                             uint32_t max_results);
  uint32_t getTracksByArtist(uint32_t artist_id, search_result_t *results,
                             uint32_t max_results);
  uint32_t getTracksByAlbum(uint32_t album_id, search_result_t *results,
                            uint32_t max_results);
  bool isPathIndexInitialized() const { return initialized; }
  void printPathIndexStats();
};
#include "Music_lookup.h"

MusicLookup::MusicLookup(uint32_t psram_base_addr)
    : psram_base_address(psram_base_addr), initialized(false) {
  memset(&genre_to_tracks_h, 0, sizeof(genre_to_tracks_h));
  memset(&genre_to_artists_h, 0, sizeof(genre_to_artists_h));
  memset(&genre_to_albums_h, 0, sizeof(genre_to_albums_h));
  memset(&artist_to_tracks_h, 0, sizeof(artist_to_tracks_h));
  memset(&artist_to_albums_h, 0, sizeof(artist_to_albums_h));
  memset(&album_to_tracks_h, 0, sizeof(album_to_tracks_h));
  memset(&artist_index_h, 0, sizeof(artist_index_h));
  memset(&album_index_h, 0, sizeof(album_index_h));
  memset(&genre_index_h, 0, sizeof(genre_index_h));
  memset(&track_index_h, 0, sizeof(track_index_h));
  memset(&path_index_h, 0, sizeof(path_index_h));
}

MusicLookup::~MusicLookup() {
  // Nothing to clean up - PSRAM controller is managed externally
}

bool MusicLookup::init() {
  if (!psram) {
    Serial.println("ERROR: PSRAM controller not provided for path index");
    return false;
  }
  Serial.println("=== PATH INDEX INIT ===");
  // Initialize SD card with default SPI configuration
  if (!sd.begin(SdSpiConfig(CARDCS, SHARED_SPI, SD_SCK_MHZ(25)))) {
    Serial.println("ERROR: SD card initialization failed for path index");
    return false;
  }

  uint32_t base_address = psram_base_address;
  loadRelationFromSDCard(genre_to_tracks_path, base_address, genre_to_tracks_h);
  loadRelationFromSDCard(genre_to_artists_path, base_address,
                         genre_to_artists_h);
  loadRelationFromSDCard(genre_to_albums_path, base_address, genre_to_albums_h);
  loadRelationFromSDCard(artist_to_tracks_path, base_address,
                         artist_to_tracks_h);
  loadRelationFromSDCard(artist_to_albums_path, base_address,
                         artist_to_albums_h);
  loadRelationFromSDCard(album_to_tracks_path, base_address, album_to_tracks_h);
  loadIndexFromSDCard(artist_index_path, base_address, artist_index_h);
  loadIndexFromSDCard(album_index_path, base_address, album_index_h);
  loadIndexFromSDCard(genre_index_path, base_address, genre_index_h);
  loadIndexFromSDCard(track_index_path, base_address, track_index_h);
  loadIndexFromSDCard(path_index_path, base_address, path_index_h);
  initialized = true;
  return true;
}

bool MusicLookup::getTrackPath(uint32_t track_id, char *buffer,
                               uint32_t buffer_size) {
  if (!initialized || !buffer || buffer_size == 0) {
    return false;
  }

  // // Binary search for track ID
  // int32_t index = binarySearchTrackId(track_id);
  // if (index < 0) {
  //   return false; // Track ID not found
  // }

  // // Read path offset for this track
  // uint32_t path_offset;
  // uint32_t offset_addr = path_offsets_offset + (index * 4);
  // psram->readData(offset_addr, (uint8_t *)&path_offset, 4);

  // // Calculate path length
  // uint32_t path_length;
  // if (index + 1 < path_header.track_count) {
  //   // Read next offset to calculate length
  //   uint32_t next_offset;
  //   uint32_t next_offset_addr = path_offsets_offset + ((index + 1) * 4);
  //   psram->readData(next_offset_addr, (uint8_t *)&next_offset, 4);
  //   path_length = next_offset - path_offset - 1; // -1 for null terminator
  // } else {
  //   // Last entry, calculate from total path data size
  //   path_length = path_header.path_data_size - path_offset - 1;
  // }

  // // Read the path string
  // readPathString(path_offset, path_length, buffer, buffer_size);

  return true;
}

uint32_t MusicLookup::readLittleEndian32(FsFile &file) {
  uint8_t bytes[4];
  file.read(bytes, 4);
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

bool MusicLookup::loadIndexFromSDCard(const char *filename,
                                      uint32_t &base_address,
                                      index_header_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    Serial.print("ERROR: Could not open path index file: ");
    Serial.println(filename);
    return false;
  }
  header.baseOffset = base_address;
  header.ids = readLittleEndian32(file);
  header.data_size = readLittleEndian32(file);
  bool res = loadDataFromSDCard(base_address);
  base_address += header.ids * 4 + header.data_size;
  file.close();
  return res;
}
bool MusicLookup::loadRelationFromSDCard(const char *filename,
                                         uint32_t &base_address,
                                         relation_header_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    Serial.print("ERROR: Could not open path index file: ");
    Serial.println(filename);
    return false;
  }
  header.baseOffset = base_address;
  header.sources = base_address + readLittleEndian32(file);
  header.targets = base_address + readLittleEndian32(file);
  bool res = loadDataFromSDCard(base_address);
  base_address += (header.sources + header.targets) * 4;
  file.close();
  return res;
}
bool MusicLookup::loadDataFromSDCard(uint32_t base_address) {

  // Read entire path index file to PSRAM
  const uint32_t CHUNK_SIZE = 4096;
  uint8_t *buffer = (uint8_t *)malloc(CHUNK_SIZE);
  if (!buffer) {
    Serial.println("ERROR: Failed to allocate buffer for path index");
    return false;
  }
  uint32_t file_size = file.size();
  uint32_t bytes_read = 0;
  uint32_t psram_addr = base_address;
  while (bytes_read < file_size) {
    uint32_t to_read = min(CHUNK_SIZE, file_size - bytes_read);
    uint32_t actual_read = file.read(buffer, to_read);

    if (actual_read == 0) {
      Serial.println("ERROR: Failed to read from path index file");
      free(buffer);
      return false;
    }

    psram->writeData(psram_addr, buffer, actual_read);
    psram_addr += actual_read;
    bytes_read += actual_read;
  }

  free(buffer);
  return true;
}

int32_t MusicLookup::relationLookup(uint32_t id, index_header_t index_header) {
  // Read the base offset and number of IDs from the index header
  uint32_t base_offset = index_header.baseOffset;
  uint32_t ids = index_header.ids;

  int32_t index = binarySearch(id, base_offset, ids);
  if(index < 0){
    return -1;
  };
  uint32_t result;
  uint32_t address = base_offset + (ids + index) * 4;
  psram.readData(address, (uint8_t *)&result, 4);
  return result;
}
lookup_result_t MusicLookup::indexLookup(uint32_t id,
                                         index_header_t index_header) {
  // Read the base offset and number of IDs from the index header
  uint32_t base_offset = index_header.baseOffset;
  uint32_t ids = index_header.ids;
  uint32_t data_size = index_header.data_size;
  lookup_result_t result = {0, 0};
  // binary search for the ID
  int32_t index = binarySearch(id, base_offset, ids);
  // fetch for the query result
  if(index < 0){
    return result; // Not found
  }

  uint32_t length;
  uint32_t address = base_offset + (ids + index)* 4; 
  psram.readData(address, (uint8_t *)&length, 4);
  result.address = address + (ids * 4);
  result.length = length;
}
// void MusicLookup::readPathString(uint32_t offset, uint32_t length, char *buffer,
//                                  uint32_t buffer_size) {
//   uint32_t copy_len = min(length, buffer_size - 1);
//   // TODO: Find the origin of this +4 offset
//   psram->readData(path_data_offset + offset + 4, (uint8_t *)buffer, copy_len);
//   buffer[copy_len] = '\0';
// }

int32_t MusicLookup::binarySearch(uint32_t id, uint32_t baseAddress, uint32_t count) {
  if (!initialized) {
    return -1;
  }

  uint32_t left = 0;
  uint32_t right = count - 1;

  while (left <= right) {
    uint32_t mid = (left + right) / 2;

    // Read track ID at mid position
    uint32_t mid_id;
    uint32_t addr = baseAddress + (mid * 4);
    psram->readData(addr, (uint8_t *)&mid_id, 4);

    if (mid_id == id) {
      return (int32_t)mid;
    } else if (mid_id < id) {
      left = mid + 1;
    } else {
      if (mid == 0)
        break; // Prevent underflow
      right = mid - 1;
    }
  }
  return -1; // Not found
}

// Relationship functions - placeholders for now
uint32_t MusicLookup::getArtistsByGenre(uint32_t genre_id,
                                        search_result_t *results,
                                        uint32_t max_results) {
  Serial.println("WARNING: getArtistsByGenre not yet implemented");
  return 0;
}

uint32_t MusicLookup::getAlbumsByArtist(uint32_t artist_id,
                                        search_result_t *results,
                                        uint32_t max_results) {
  Serial.println("WARNING: getAlbumsByArtist not yet implemented");
  return 0;
}

uint32_t MusicLookup::getTracksByArtist(uint32_t artist_id,
                                        search_result_t *results,
                                        uint32_t max_results) {
  Serial.println("WARNING: getTracksByArtist not yet implemented");
  return 0;
}

uint32_t MusicLookup::getTracksByAlbum(uint32_t album_id,
                                       search_result_t *results,
                                       uint32_t max_results) {
  Serial.println("WARNING: getTracksByAlbum not yet implemented");
  return 0;
}

void MusicLookup::printPathIndexStats() {
  if (!initialized) {
    Serial.println("Path index not initialized");
    return;
  }

  
}
#include "Music_lookup.h"

MusicLookup::MusicLookup(uint32_t psram_base_addr)
    : psram_base_address(psram_base_addr), initialized(false) {
  memset(&genre_to_tracks_h, 0, sizeof(genre_to_tracks_h));
  // memset(&genre_to_a, 0, sizeof(genre_to_artists_h));
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
  Serial.println("=== MUSIC LOOKUP INIT ===");
  
  // Initialize SD card with default SPI configuration
  if (!sd.begin(SdSpiConfig(CARDCS, SHARED_SPI, SD_SCK_MHZ(25)))) {
    Serial.println("ERROR: SD card initialization failed");
    return false;
  }

  uint32_t base_address = psram_base_address;
  
  // Load all indexes and relationships
  if (!loadIndexFromSDCard(artist_index_path, base_address, artist_index_h)) return false;
  if (!loadIndexFromSDCard(album_index_path, base_address, album_index_h)) return false;
  if (!loadIndexFromSDCard(genre_index_path, base_address, genre_index_h)) return false;
  if (!loadIndexFromSDCard(track_index_path, base_address, track_index_h)) return false;
  if (!loadIndexFromSDCard(path_index_path, base_address, path_index_h)) return false;
  
  if (!loadRelationFromSDCard(artist_to_albums_path, base_address, artist_to_albums_h)) return false;
  if (!loadRelationFromSDCard(artist_to_tracks_path, base_address, artist_to_tracks_h)) return false;
  if (!loadRelationFromSDCard(album_to_tracks_path, base_address, album_to_tracks_h)) return false;
  if (!loadRelationFromSDCard(genre_to_albums_path, base_address, genre_to_albums_h)) return false;
  if (!loadRelationFromSDCard(genre_to_tracks_path, base_address, genre_to_tracks_h)) return false;
  
  initialized = true;
  Serial.println("Music lookup initialized successfully");
  return true;
}

bool MusicLookup::getTrackPath(uint32_t track_id, char *buffer, uint32_t buffer_size) {
  if (!initialized || !buffer || buffer_size == 0) {
    return false;
  }

  // Binary search for track ID in path index
  int32_t index = binarySearch(track_id, path_index_h.baseOffset, path_index_h.entry_count);
  if (index < 0) {
    return false; // Track ID not found
  }

  // Get string data for this track
  lookup_result_t result = getStringAtIndex(index, path_index_h);
  if (result.length == 0) {
    return false;
  }

  // Read the path string
  uint32_t copy_len = min(result.length, buffer_size - 1);
  psram.readData(result.address, (uint8_t *)buffer, copy_len);
  buffer[copy_len] = '\0';
  
  return true;
}

bool MusicLookup::getString(uint32_t id, const index_header_t &index_header, char *buffer, uint32_t buffer_size) {
  if (!initialized || !buffer || buffer_size == 0) {
    return false;
  }

  int32_t index = binarySearch(id, index_header.baseOffset, index_header.entry_count);
  if (index < 0) {
    return false;
  }

  lookup_result_t result = getStringAtIndex(index, index_header);
  if (result.length == 0) {
    return false;
  }

  uint32_t copy_len = min(result.length, buffer_size - 1);
  psram.readData(result.address, (uint8_t *)buffer, copy_len);
  buffer[copy_len] = '\0';
  
  return true;
}

uint32_t MusicLookup::readLittleEndian32(FsFile &file) {
  uint8_t bytes[4];
  file.read(bytes, 4);
  return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
         ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

bool MusicLookup::loadIndexFromSDCard(const char *filename, uint32_t &base_address, index_header_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    Serial.print("ERROR: Could not open index file: ");
    Serial.println(filename);
    return false;
  }
  
  header.baseOffset = base_address;
  header.entry_count = readLittleEndian32(file);
  header.data_size = readLittleEndian32(file);
  
  // Calculate layout: [entry_count][data_size][ids...][offsets...][string_data...]
  uint32_t ids_size = header.entry_count * 4;
  uint32_t offsets_size = (header.entry_count + 1) * 4; // +1 for end marker
  uint32_t total_size = 8 + ids_size + offsets_size + header.data_size; // 8 = header
  
  // Seek back to beginning to load entire file
  file.seekSet(0);
  bool res = loadDataFromSDCard(base_address, total_size);
  base_address += total_size;
  
  file.close();
  return res;
}

bool MusicLookup::loadRelationFromSDCard(const char *filename, uint32_t &base_address, relation_header_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    Serial.print("ERROR: Could not open relation file: ");
    Serial.println(filename);
    return false;
  }
  
  header.baseOffset = base_address;
  header.entry_count = readLittleEndian32(file);
  header.total_target_count = readLittleEndian32(file);
  
  // Calculate layout: [entry_count][total_target_count][source_ids...][target_counts...][target_ids...]
  uint32_t source_ids_size = header.entry_count * 4;
  uint32_t target_counts_size = header.entry_count * 4;
  uint32_t target_ids_size = header.total_target_count * 4;
  uint32_t total_size = 8 + source_ids_size + target_counts_size + target_ids_size; // 8 = header
  
  // Seek back to beginning to load entire file
  file.seekSet(0);
  bool res = loadDataFromSDCard(base_address, total_size);
  base_address += total_size;
  
  file.close();
  return res;
}

bool MusicLookup::loadDataFromSDCard(uint32_t base_address, uint32_t size) {
  const uint32_t CHUNK_SIZE = 4096;
  uint8_t *buffer = (uint8_t *)malloc(CHUNK_SIZE);
  if (!buffer) {
    Serial.println("ERROR: Failed to allocate buffer");
    return false;
  }
  
  uint32_t bytes_read = 0;
  uint32_t psram_addr = base_address;
  
  while (bytes_read < size) {
    uint32_t to_read = min(CHUNK_SIZE, size - bytes_read);
    uint32_t actual_read = file.read(buffer, to_read);

    if (actual_read == 0) {
      Serial.println("ERROR: Failed to read from file");
      free(buffer);
      return false;
    }

    psram.writeData(psram_addr, buffer, actual_read);
    psram_addr += actual_read;
    bytes_read += actual_read;
  }

  free(buffer);
  return true;
}

lookup_result_t MusicLookup::getStringAtIndex(int32_t index, const index_header_t &header) {
  lookup_result_t result = {0, 0};
  
  if (index < 0 || index >= (int32_t)header.entry_count) {
    return result;
  }
  
  // Layout: [header(8)][ids(entry_count*4)][offsets((entry_count+1)*4)][string_data]
  uint32_t offsets_base = header.baseOffset + 8 + (header.entry_count * 4);
  uint32_t string_data_base = offsets_base + ((header.entry_count + 1) * 4);
  
  // Read start and end offsets
  uint32_t start_offset, end_offset;
  psram.readData(offsets_base + (index * 4), (uint8_t *)&start_offset, 4);
  psram.readData(offsets_base + ((index + 1) * 4), (uint8_t *)&end_offset, 4);
  
  result.address = string_data_base + start_offset;
  result.length = end_offset - start_offset - 1; // -1 for null terminator
  
  return result;
}

uint32_t MusicLookup::getRelatedIds(uint32_t source_id, const relation_header_t &header, uint32_t *results, uint32_t max_results) {
  if (!initialized || !results || max_results == 0) {
    return 0;
  }
  
  // Binary search for source ID
  uint32_t source_ids_base = header.baseOffset + 8; // Skip header
  int32_t index = binarySearch(source_id, source_ids_base, header.entry_count);
  if (index < 0) {
    return 0; // Not found
  }
  
  // Get target count for this source
  uint32_t target_counts_base = source_ids_base + (header.entry_count * 4);
  uint32_t target_count;
  psram.readData(target_counts_base + (index * 4), (uint8_t *)&target_count, 4);
  
  if (target_count == 0) {
    return 0;
  }
  
  // Calculate offset into target IDs array
  uint32_t target_offset = 0;
  for (int32_t i = 0; i < index; i++) {
    uint32_t count;
    psram.readData(target_counts_base + (i * 4), (uint8_t *)&count, 4);
    target_offset += count;
  }
  
  // Read target IDs
  uint32_t target_ids_base = target_counts_base + (header.entry_count * 4);
  uint32_t to_read = min(target_count, max_results);
  
  for (uint32_t i = 0; i < to_read; i++) {
    psram.readData(target_ids_base + ((target_offset + i) * 4), (uint8_t *)&results[i], 4);
  }
  
  return to_read;
}

int32_t MusicLookup::binarySearch(uint32_t id, uint32_t baseAddress, uint32_t count) {
  if (!initialized || count == 0) {
    return -1;
  }

  uint32_t left = 0;
  uint32_t right = count - 1;

  while (left <= right) {
    uint32_t mid = (left + right) / 2;

    // Read ID at mid position
    uint32_t mid_id;
    uint32_t addr = baseAddress + (mid * 4);
    psram.readData(addr, (uint8_t *)&mid_id, 4);

    if (mid_id == id) {
      return (int32_t)mid;
    } else if (mid_id < id) {
      left = mid + 1;
    } else {
      if (mid == 0) break; // Prevent underflow
      right = mid - 1;
    }
  }
  return -1; // Not found
}

// Relationship functions
uint32_t MusicLookup::getAlbumsByArtist(uint32_t artist_id, string_results_t *results, uint32_t max_results) {
  if (!results || max_results == 0) return 0;
  
  uint32_t album_ids[MAX_SEARCH_RESULTS];
  uint32_t count = getRelatedIds(artist_id, artist_to_albums_h, album_ids, min(max_results, (uint32_t)MAX_SEARCH_RESULTS));
  
  for (uint32_t i = 0; i < count; i++) {
    results[i].id = album_ids[i];
    results[i].name[0] = '\0'; // Will be filled if needed
  }
  
  return count;
}

uint32_t MusicLookup::getTracksByArtist(uint32_t artist_id, string_results_t *results, uint32_t max_results) {
  if (!results || max_results == 0) return 0;
  
  uint32_t track_ids[MAX_SEARCH_RESULTS];
  uint32_t count = getRelatedIds(artist_id, artist_to_tracks_h, track_ids, min(max_results,(uint32_t) MAX_SEARCH_RESULTS));
  
  for (uint32_t i = 0; i < count; i++) {
    results[i].id = track_ids[i];
    results[i].name[0] = '\0'; // Will be filled if needed
  }
  
  return count;
}

uint32_t MusicLookup::getTracksByAlbum(uint32_t album_id, string_results_t *results, uint32_t max_results) {
  if (!results || max_results == 0) return 0;
  
  uint32_t track_ids[MAX_SEARCH_RESULTS];
  uint32_t count = getRelatedIds(album_id, album_to_tracks_h, track_ids, min(max_results, (uint32_t)MAX_SEARCH_RESULTS));
  
  for (uint32_t i = 0; i < count; i++) {
    results[i].id = track_ids[i];
    results[i].name[0] = '\0'; // Will be filled if needed
  }
  
  return count;
}

uint32_t MusicLookup::getTracksByGenre(uint32_t genre_id, string_results_t *results, uint32_t max_results) {
  if (!results || max_results == 0) return 0;
  
  uint32_t track_ids[MAX_SEARCH_RESULTS];
  uint32_t count = getRelatedIds(genre_id, genre_to_tracks_h, track_ids, min(max_results,(uint32_t) MAX_SEARCH_RESULTS));
  
  for (uint32_t i = 0; i < count; i++) {
    results[i].id = track_ids[i];
    results[i].name[0] = '\0'; // Will be filled if needed
  }
  
  return count;
}

uint32_t MusicLookup::getAlbumsByGenre(uint32_t genre_id, string_results_t *results, uint32_t max_results) {
  if (!results || max_results == 0) return 0;
  
  uint32_t album_ids[MAX_SEARCH_RESULTS];
  uint32_t count = getRelatedIds(genre_id, genre_to_albums_h, album_ids, min(max_results, (uint32_t)MAX_SEARCH_RESULTS));
  
  for (uint32_t i = 0; i < count; i++) {
    results[i].id = album_ids[i];
    results[i].name[0] = '\0'; // Will be filled if needed
  }
  
  return count;
}

// String lookup functions
bool MusicLookup::getArtistName(uint32_t artist_id, char *buffer, uint32_t buffer_size) {
  return getString(artist_id, artist_index_h, buffer, buffer_size);
}

bool MusicLookup::getAlbumName(uint32_t album_id, char *buffer, uint32_t buffer_size) {
  return getString(album_id, album_index_h, buffer, buffer_size);
}

bool MusicLookup::getGenreName(uint32_t genre_id, char *buffer, uint32_t buffer_size) {
  return getString(genre_id, genre_index_h, buffer, buffer_size);
}

bool MusicLookup::getTrackName(uint32_t track_id, char *buffer, uint32_t buffer_size) {
  return getString(track_id, track_index_h, buffer, buffer_size);
}

void MusicLookup::printStats() {
  if (!initialized) {
    Serial.println("Music lookup not initialized");
    return;
  }

  Serial.println("=== MUSIC LOOKUP STATS ===");
  Serial.print("Artists: "); Serial.println(artist_index_h.entry_count);
  Serial.print("Albums: "); Serial.println(album_index_h.entry_count);
  Serial.print("Genres: "); Serial.println(genre_index_h.entry_count);
  Serial.print("Tracks: "); Serial.println(track_index_h.entry_count);
  Serial.print("Artist->Album relations: "); Serial.println(artist_to_albums_h.entry_count);
  Serial.print("Album->Track relations: "); Serial.println(album_to_tracks_h.entry_count);
  Serial.println("========================");
}
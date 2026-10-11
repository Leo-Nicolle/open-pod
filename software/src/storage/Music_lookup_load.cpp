#include "Music_lookup.h"
#include "SharedSpiDriver.h"
#include "../logging.h"

// MusicLookup start-up: copies the index/relation files from the SD card into
// PSRAM. The lookups that read them back are in Music_lookup.cpp.

bool MusicLookup::init() {
  LOG_CAT(INDEX_USAGE, "=== MUSIC LOOKUP INIT ===");

  // Initialize SD card with default SPI configuration
  if (!sd.begin(OPENPOD_SD_CONFIG(CARDCS))) {
    LOG_ERROR("SD card initialization failed");
    return false;
  }

  uint32_t current_address = psram_base_address;

  // Load all indexes and relationships
  if (!loadIndexFromSDCard(artist_index_path, current_address, artist_index_h))
    return false;
  if (!loadIndexFromSDCard(album_index_path, current_address, album_index_h))
    return false;
  if (!loadIndexFromSDCard(genre_index_path, current_address, genre_index_h))
    return false;
  if (!loadIndexFromSDCard(track_index_path, current_address, track_index_h))
    return false;
  if (!loadIndexFromSDCard(path_index_path, current_address, path_index_h))
    return false;
  if (!loadDurationIndexFromSDCard(duration_index_path, current_address,
                                   duration_index_h))
    return false;
  if (!loadTrackToIdIndexFromSDCard(track_to_album_path, current_address,
                                    track_to_album_h))
    return false;
  // Optional: older SD cards predate track_to_artist.bin. Without it Now
  // Playing just shows no artist line.
  if (!loadTrackToIdIndexFromSDCard(track_to_artist_path, current_address,
                                    track_to_artist_h))
    memset(&track_to_artist_h, 0, sizeof(track_to_artist_h));
  // Optional too: search falls back to the album's first track's artist.
  if (!loadTrackToIdIndexFromSDCard(album_to_artist_path, current_address,
                                    album_to_artist_h))
    memset(&album_to_artist_h, 0, sizeof(album_to_artist_h));
  if (!loadAlbumCoverIndexFromSDCard(album_to_cover_path, current_address,
                                     album_cover_h))
    return false;

  if (!loadRelationFromSDCard(artist_to_albums_path, current_address,
                              artist_to_albums_h))
    return false;
  LOG_CAT(INDEX_USAGE, "Loading artist to tracks relation");
  if (!loadRelationFromSDCard(artist_to_tracks_path, current_address,
                              artist_to_tracks_h))
    return false;
  LOG_CAT(INDEX_USAGE, "Loading album to tracks relation");
  if (!loadRelationFromSDCard(album_to_tracks_path, current_address,
                              album_to_tracks_h))
    return false;
  if (!loadRelationFromSDCard(genre_to_albums_path, current_address,
                              genre_to_albums_h))
    return false;
  if (!loadRelationFromSDCard(genre_to_tracks_path, current_address,
                              genre_to_tracks_h))
    return false;
  psram_last_address = current_address;
  initialized = true;
  LOG_INFO("Music lookup initialized successfully");
  test();
  return true;
}

void MusicLookup::test() {
  uint32_t id;
  uint32_t addr = 6292413 + 8;
  psram.readData(addr, (uint8_t *)&id, 4);
  LOG_CAT(INDEX_USAGE, "First id: %lu", (unsigned long)id);
  psram.readData(addr + 4, (uint8_t *)&id, 4);
  LOG_CAT(INDEX_USAGE, "Second id: %lu", (unsigned long)id);
}

bool MusicLookup::loadIndexFromSDCard(const char *filename,
                                      uint32_t &base_address,
                                      index_header_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    LOG_ERROR("Could not open index file: %s", filename);
    return false;
  }

  header.baseOffset = base_address;
  header.entry_count = readLittleEndian32(file);
  header.data_size = readLittleEndian32(file);

  // Calculate layout:
  // [entry_count][data_size][ids...][offsets...][string_data...]
  uint32_t ids_size = header.entry_count * 4;
  uint32_t offsets_size = (header.entry_count + 1) * 4; // +1 for end marker
  uint32_t total_size =
      8 + ids_size + offsets_size + header.data_size; // 8 = header

  // Seek back to beginning to load entire file
  file.seekSet(0);
  bool res = loadDataFromSDCard(base_address, total_size);
  base_address += total_size;

  file.close();
  return res;
}

bool MusicLookup::loadDurationIndexFromSDCard(const char *filename,
                                              uint32_t &base_address,
                                              duration_index_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    LOG_ERROR("Could not open duration index file: %s", filename);
    return false;
  }

  uint32_t entry_count = readLittleEndian32(file);

  // Layout: [entry_count][uint16 seconds]*entry_count - skip the 4-byte
  // count field so baseOffset points straight at the array for O(1)
  // indexed reads (no ids/offsets table like index_header_t's format).
  header.baseOffset = base_address + 4;
  header.entry_count = entry_count;

  uint32_t total_size = 4 + entry_count * sizeof(uint16_t);

  // Seek back to beginning to load entire file
  file.seekSet(0);
  bool res = loadDataFromSDCard(base_address, total_size);
  base_address += total_size;

  file.close();
  return res;
}

bool MusicLookup::loadTrackToIdIndexFromSDCard(const char *filename,
                                               uint32_t &base_address,
                                               track_to_id_index_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    LOG_ERROR("Could not open track-to-id index file: %s", filename);
    return false;
  }

  uint32_t entry_count = readLittleEndian32(file);

  // Layout: [entry_count][uint32 id]*entry_count - same shape as
  // loadDurationIndexFromSDCard, just 4-byte entries instead of 2-byte.
  header.baseOffset = base_address + 4;
  header.entry_count = entry_count;

  uint32_t total_size = 4 + entry_count * sizeof(uint32_t);

  file.seekSet(0);
  bool res = loadDataFromSDCard(base_address, total_size);
  base_address += total_size;

  file.close();
  return res;
}

bool MusicLookup::loadAlbumCoverIndexFromSDCard(const char *filename,
                                                uint32_t &base_address,
                                                album_cover_index_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    LOG_ERROR("Could not open album-to-cover index file: %s", filename);
    return false;
  }

  uint32_t entry_count = readLittleEndian32(file);

  // Layout: [entry_count][{u32 offset, u32 length, u8 format}]*entry_count -
  // 9 bytes per entry, matching indexer's exportAlbumCoverIndexToBinary.
  header.baseOffset = base_address + 4;
  header.entry_count = entry_count;

  const uint32_t ENTRY_SIZE = 9;
  uint32_t total_size = 4 + entry_count * ENTRY_SIZE;

  file.seekSet(0);
  bool res = loadDataFromSDCard(base_address, total_size);
  base_address += total_size;

  file.close();
  return res;
}

bool MusicLookup::loadRelationFromSDCard(const char *filename,
                                         uint32_t &base_address,
                                         relation_header_t &header) {
  if (!file.open(filename, O_RDONLY)) {
    LOG_ERROR("Could not open relation file: %s", filename);
    return false;
  }

  header.baseOffset = base_address;
  header.entry_count = readLittleEndian32(file);
  header.total_target_count = readLittleEndian32(file);

  // Calculate layout:
  // [entry_count][total_target_count][source_ids...][target_counts...][target_ids...]
  uint32_t source_ids_size = header.entry_count * 4;
  uint32_t target_counts_size = header.entry_count * 4;
  uint32_t target_ids_size = header.total_target_count * 4;
  uint32_t total_size =
      8 + source_ids_size + target_counts_size + target_ids_size; // 8 = header

  // Seek back to beginning to load entire file
  file.seekSet(0);
  bool res = loadDataFromSDCard(base_address, total_size);
  base_address += total_size;

  uint32_t id;
  uint32_t addr = header.baseOffset + 8;
  psram.readData(addr, (uint8_t *)&id, 4);
  LOG_CAT(INDEX_USAGE, "First id: %lu", (unsigned long)id);
  psram.readData(addr + 4, (uint8_t *)&id, 4);
  LOG_CAT(INDEX_USAGE, "Second id: %lu", (unsigned long)id);

  file.close();
  return res;
}

bool MusicLookup::loadDataFromSDCard(uint32_t base_address, uint32_t size) {
  const uint32_t CHUNK_SIZE = 4096;
  uint8_t *buffer = (uint8_t *)malloc(CHUNK_SIZE);
  if (!buffer) {
    LOG_ERROR("Failed to allocate buffer");
    return false;
  }

  uint32_t bytes_read = 0;
  uint32_t psram_addr = base_address;

  while (bytes_read < size) {
    uint32_t to_read = min(CHUNK_SIZE, size - bytes_read);
    uint32_t actual_read = file.read(buffer, to_read);

    if (actual_read == 0) {
      LOG_ERROR("Failed to read from file");
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

void MusicLookup::printStats() {
  if (!initialized) {
    LOG_WARN("Music lookup not initialized");
    return;
  }

  LOG_INFO("=== MUSIC LOOKUP STATS ===\n"
           "Artists: %lu\n"
           "Albums: %lu\n"
           "Genres: %lu\n"
           "Tracks: %lu\n"
           "Artist->Album relations: %lu\n"
           "Album->Track relations: %lu\n"
           "========================",
           (unsigned long)artist_index_h.entry_count,
           (unsigned long)album_index_h.entry_count,
           (unsigned long)genre_index_h.entry_count,
           (unsigned long)track_index_h.entry_count,
           (unsigned long)artist_to_albums_h.entry_count,
           (unsigned long)album_to_tracks_h.entry_count);
}

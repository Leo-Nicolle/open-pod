#include "Music_lookup.h"
#include "../sound/AudioYield.h"

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
  memset(&duration_index_h, 0, sizeof(duration_index_h));
  memset(&track_to_album_h, 0, sizeof(track_to_album_h));
  memset(&track_to_artist_h, 0, sizeof(track_to_artist_h));
  memset(&album_to_artist_h, 0, sizeof(album_to_artist_h));
  memset(&album_cover_h, 0, sizeof(album_cover_h));
}

MusicLookup::~MusicLookup() {
  // Nothing to clean up - PSRAM controller is managed externally
}

bool MusicLookup::getTrackPath(uint32_t track_id, char *buffer,
                               uint32_t buffer_size) {
  if (!initialized || !buffer || buffer_size == 0) {
    return false;
  }

  // Binary search for track ID in path index
  // (+8 skips the [entry_count][data_size] header before the id array)
  int32_t index = binarySearch(track_id, path_index_h.baseOffset + 8,
                               path_index_h.entry_count);
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

uint32_t MusicLookup::getTrackDurationSeconds(uint32_t track_id) {
  if (!initialized || track_id >= duration_index_h.entry_count) {
    return 0;
  }

  uint16_t seconds = 0;
  psram.readData(duration_index_h.baseOffset + track_id * sizeof(uint16_t),
                 (uint8_t *)&seconds, sizeof(uint16_t));
  return seconds;
}

uint32_t MusicLookup::getAlbumIdForTrack(uint32_t track_id) {
  if (!initialized || track_id >= track_to_album_h.entry_count) {
    return 0xFFFFFFFFu;
  }

  uint32_t albumId = 0;
  psram.readData(track_to_album_h.baseOffset + track_id * sizeof(uint32_t),
                 (uint8_t *)&albumId, sizeof(uint32_t));
  return albumId;
}

uint32_t MusicLookup::getArtistIdForTrack(uint32_t track_id) {
  if (!initialized || track_id >= track_to_artist_h.entry_count) {
    return 0xFFFFFFFFu;
  }

  uint32_t artistId = 0;
  psram.readData(track_to_artist_h.baseOffset + track_id * sizeof(uint32_t),
                 (uint8_t *)&artistId, sizeof(uint32_t));
  return artistId;
}

uint32_t MusicLookup::getArtistIdForAlbum(uint32_t album_id) {
  if (!initialized) {
    return 0xFFFFFFFFu;
  }
  if (album_id < album_to_artist_h.entry_count) {
    uint32_t artistId = 0;
    psram.readData(album_to_artist_h.baseOffset + album_id * sizeof(uint32_t),
                   (uint8_t *)&artistId, sizeof(uint32_t));
    return artistId;
  }
  uint32_t firstTrack = getTrackIdByAlbumAtIndex(album_id, 0);
  if (firstTrack == 0xFFFFFFFFu) {
    return 0xFFFFFFFFu;
  }
  return getArtistIdForTrack(firstTrack);
}

uint32_t MusicLookup::getAlbumCountForArtist(uint32_t artist_id) {
  return getRelatedCount(artist_id, artist_to_albums_h);
}

uint32_t MusicLookup::getAlbumCountForGenre(uint32_t genre_id) {
  return getRelatedCount(genre_id, genre_to_albums_h);
}

uint32_t MusicLookup::getTrackCountForAlbum(uint32_t album_id) {
  return getRelatedCount(album_id, album_to_tracks_h);
}

uint32_t MusicLookup::getTrackCountForArtist(uint32_t artist_id) {
  return getRelatedCount(artist_id, artist_to_tracks_h);
}

uint32_t MusicLookup::getTrackCountForGenre(uint32_t genre_id) {
  return getRelatedCount(genre_id, genre_to_tracks_h);
}

uint32_t MusicLookup::getArtistCount() const { return artist_index_h.entry_count; }
uint32_t MusicLookup::getAlbumCount() const { return album_index_h.entry_count; }
uint32_t MusicLookup::getGenreCount() const { return genre_index_h.entry_count; }
uint32_t MusicLookup::getTrackCount() const { return track_index_h.entry_count; }

int32_t MusicLookup::getTrackIndexInAlbum(uint32_t album_id, uint32_t track_id) {
  uint32_t ids[LOOKUP_WINDOW];
  for (uint32_t offset = 0;; offset += LOOKUP_WINDOW) {
    uint32_t n = getRelatedIds(album_id, album_to_tracks_h, ids, LOOKUP_WINDOW, offset);
    for (uint32_t i = 0; i < n; i++) {
      if (ids[i] == track_id) {
        return (int32_t)(offset + i);
      }
    }
    if (n < LOOKUP_WINDOW) {
      return -1;
    }
  }
}

bool MusicLookup::getAlbumCoverEntry(uint32_t album_id,
                                     album_cover_entry_t &out) {
  if (!initialized || album_id >= album_cover_h.entry_count) {
    return false;
  }

  // Read the three fields individually rather than into a padded C struct,
  // matching every other lookup here (avoids host/PSRAM struct-packing
  // mismatches).
  const uint32_t ENTRY_SIZE = 9;
  uint32_t addr = album_cover_h.baseOffset + album_id * ENTRY_SIZE;
  psram.readData(addr, (uint8_t *)&out.offset, sizeof(uint32_t));
  psram.readData(addr + 4, (uint8_t *)&out.length, sizeof(uint32_t));
  psram.readData(addr + 8, (uint8_t *)&out.format, sizeof(uint8_t));

  return out.length > 0;
}

bool MusicLookup::getString(uint32_t id, const index_header_t &index_header,
                            char *buffer, uint32_t buffer_size) {
  if (!initialized || !buffer || buffer_size == 0) {
    return false;
  }

  // (+8 skips the [entry_count][data_size] header before the id array)
  int32_t index = binarySearch(id, index_header.baseOffset + 8,
                               index_header.entry_count);
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

lookup_result_t MusicLookup::getStringAtIndex(int32_t index,
                                              const index_header_t &header) {
  lookup_result_t result = {0, 0};

  if (index < 0 || index >= (int32_t)header.entry_count) {
    return result;
  }

  // Layout:
  // [header(8)][ids(entry_count*4)][offsets((entry_count+1)*4)][string_data]
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

uint32_t MusicLookup::getRelatedCount(uint32_t source_id,
                                      const relation_header_t &header) {
  if (!initialized || header.entry_count == 0) {
    return 0;
  }
  uint32_t source_ids_base = header.baseOffset + 8; // Skip header
  int32_t index = binarySearch(source_id, source_ids_base, header.entry_count);
  if (index < 0) {
    return 0;
  }
  uint32_t target_counts_base = source_ids_base + (header.entry_count * 4);
  uint32_t target_count = 0;
  psram.readData(target_counts_base + (index * 4), (uint8_t *)&target_count, 4);
  return target_count;
}

uint32_t MusicLookup::getRelatedIds(uint32_t source_id,
                                    const relation_header_t &header,
                                    uint32_t *results, uint32_t max_results,
                                    uint32_t offset) {
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

  if (target_count <= offset) {
    return 0;
  }

  // Calculate offset into target IDs array
  uint32_t target_offset = 0;
  for (int32_t i = 0; i < index; i++) {
    if ((i & 63) == 63) {
      audioYield(); // one read per earlier source: thousands on a big library
    }
    uint32_t count;
    psram.readData(target_counts_base + (i * 4), (uint8_t *)&count, 4);
    target_offset += count;
  }

  // Read target IDs (one read: they are contiguous)
  uint32_t target_ids_base = target_counts_base + (header.entry_count * 4);
  uint32_t to_read = min(target_count - offset, max_results);
  psram.readData(target_ids_base + ((target_offset + offset) * 4),
                 (uint8_t *)results, to_read * 4);
  return to_read;
}

int32_t MusicLookup::binarySearch(uint32_t id, uint32_t baseAddress,
                                  uint32_t count) {
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
      if (mid == 0)
        break; // Prevent underflow
      right = mid - 1;
    }
  }
  return -1; // Not found
}

// Generic string loading function - consolidates all relationship lookups.
// Loads up to max_results (<= LOOKUP_WINDOW) names starting at the offset-th
// related id.
uint32_t MusicLookup::getLookupStrings(
    uint32_t source_id, const relation_header_t &relation_header,
    const index_header_t &target_index_header, char *buffer,
    uint32_t buffer_size, const char **string_pointers, uint32_t max_results,
    uint32_t offset) {
  if (!initialized || !buffer || !string_pointers || max_results == 0) {
    return 0;
  }

  uint32_t target_ids[LOOKUP_WINDOW];
  uint32_t id_count =
      getRelatedIds(source_id, relation_header, target_ids,
                    min(max_results, (uint32_t)LOOKUP_WINDOW), offset);

  char *buffer_pos = buffer;
  uint32_t buffer_remaining = buffer_size;
  uint32_t strings_loaded = 0;

  for (uint32_t i = 0; i < id_count; i++) {
    if ((i & 15) == 15) {
      audioYield(); // long lists take 200+ ms: keep the audio fifo topped up
    }
    // Binary search for this ID in the target index
    // (+8 skips the [entry_count][data_size] header before the id array)
    int32_t index = binarySearch(target_ids[i], target_index_header.baseOffset + 8,
                                 target_index_header.entry_count);
    lookup_result_t result = index >= 0 ? getStringAtIndex(index, target_index_header)
                                        : lookup_result_t{0, 0};
    if (!copyString(result, buffer_pos, buffer_remaining, string_pointers[strings_loaded])) {
      break; // buffer full
    }
    strings_loaded++;
  }

  return strings_loaded;
}

// Copies one string to buffer_pos (advancing it), "" for a missing one.
// Every slot gets a string, so list positions stay aligned with ids. False
// when the buffer is full.
bool MusicLookup::copyString(const lookup_result_t &result, char *&buffer_pos,
                             uint32_t &buffer_remaining, const char *&out) {
  uint32_t len = result.length;
  if (len + 1 > buffer_remaining) {
    return false;
  }
  out = buffer_pos;
  if (len > 0) {
    psram.readData(result.address, (uint8_t *)buffer_pos, len);
  }
  buffer_pos[len] = '\0';
  buffer_pos += len + 1;
  buffer_remaining -= len + 1;
  return true;
}

// Names of entries [offset, offset + max_results) of a string index.
uint32_t MusicLookup::getIndexRange(const index_header_t &header, char *buffer,
                                    uint32_t buffer_size,
                                    const char **string_pointers,
                                    uint32_t max_results, uint32_t offset) {
  if (!initialized || !buffer || !string_pointers || max_results == 0) {
    return 0;
  }
  uint32_t start_index = min(offset, header.entry_count);
  uint32_t end_index = min(start_index + max_results, header.entry_count);
  char *buffer_pos = buffer;
  uint32_t buffer_remaining = buffer_size;
  uint32_t strings_loaded = 0;
  for (uint32_t i = start_index; i < end_index; i++) {
    if (!copyString(getStringAtIndex(i, header), buffer_pos, buffer_remaining,
                    string_pointers[strings_loaded])) {
      break; // buffer full
    }
    strings_loaded++;
  }
  return strings_loaded;
}

// Consolidated relationship lookups using new signature
uint32_t MusicLookup::getTracksByArtist(uint32_t artist_id, char *buffer,
                                       uint32_t buffer_size,
                                       const char **string_pointers,
                                       uint32_t max_results, uint32_t offset) {
  return getLookupStrings(artist_id, artist_to_tracks_h, track_index_h, buffer, buffer_size,
                          string_pointers, max_results, offset);
}

uint32_t MusicLookup::getAlbumsByArtist(uint32_t artist_id, char *buffer,
                                       uint32_t buffer_size,
                                       const char **string_pointers,
                                       uint32_t max_results, uint32_t offset) {
  return getLookupStrings(artist_id, artist_to_albums_h, album_index_h, buffer, buffer_size,
                          string_pointers, max_results, offset);
}

uint32_t MusicLookup::getTracksByAlbum(uint32_t album_id, char *buffer,
                                       uint32_t buffer_size,
                                       const char **string_pointers,
                                       uint32_t max_results, uint32_t offset) {
  return getLookupStrings(album_id, album_to_tracks_h, track_index_h, buffer, buffer_size,
                          string_pointers, max_results, offset);
}

uint32_t MusicLookup::getTracksByGenre(uint32_t genre_id, char *buffer,
                                       uint32_t buffer_size,
                                       const char **string_pointers,
                                       uint32_t max_results, uint32_t offset) {
  return getLookupStrings(genre_id, genre_to_tracks_h, track_index_h, buffer, buffer_size,
                          string_pointers, max_results, offset);
}

uint32_t MusicLookup::getAlbumsByGenre(uint32_t genre_id, char *buffer,
                                       uint32_t buffer_size,
                                       const char **string_pointers,
                                       uint32_t max_results, uint32_t offset) {
  return getLookupStrings(genre_id, genre_to_albums_h, album_index_h, buffer, buffer_size,
                          string_pointers, max_results, offset);
}

// Get all items from an index (for browsing), from the offset-th one.
uint32_t MusicLookup::getAllArtists(char *buffer, uint32_t buffer_size,
                                    const char **string_pointers,
                                    uint32_t max_results, uint32_t offset) {
  return getIndexRange(artist_index_h, buffer, buffer_size, string_pointers,
                       max_results, offset);
}

uint32_t MusicLookup::getAllAlbums(char *buffer, uint32_t buffer_size,
                                   const char **string_pointers,
                                   uint32_t max_results, uint32_t offset) {
  return getIndexRange(album_index_h, buffer, buffer_size, string_pointers,
                       max_results, offset);
}

uint32_t MusicLookup::getAllGenres(char *buffer, uint32_t buffer_size,
                                   const char **string_pointers,
                                   uint32_t max_results, uint32_t offset) {
  return getIndexRange(genre_index_h, buffer, buffer_size, string_pointers,
                       max_results, offset);
}

uint32_t MusicLookup::getAllTracks(char *buffer, uint32_t buffer_size,
                                   const char **string_pointers,
                                   uint32_t max_results, uint32_t offset) {
  return getIndexRange(track_index_h, buffer, buffer_size, string_pointers,
                       max_results, offset);
}

// String lookup functions
bool MusicLookup::getArtistName(uint32_t artist_id, char *buffer,
                                uint32_t buffer_size) {
  return getString(artist_id, artist_index_h, buffer, buffer_size);
}

bool MusicLookup::getAlbumName(uint32_t album_id, char *buffer,
                               uint32_t buffer_size) {
  return getString(album_id, album_index_h, buffer, buffer_size);
}

bool MusicLookup::getGenreName(uint32_t genre_id, char *buffer,
                               uint32_t buffer_size) {
  return getString(genre_id, genre_index_h, buffer, buffer_size);
}

bool MusicLookup::getTrackName(uint32_t track_id, char *buffer,
                               uint32_t buffer_size) {
  return getString(track_id, track_index_h, buffer, buffer_size);
}
uint32_t MusicLookup::getLastPSRAMAddress() { return psram_last_address; }

uint32_t MusicLookup::getRelatedIdAtIndex(uint32_t source_id,
                                          const relation_header_t &header,
                                          uint32_t index) {
  // NOTE: source_id == 0 is a valid id here (the indexer assigns ids
  // starting at 0), so it must not be rejected - getRelatedIds() (the
  // list-building counterpart used to populate what's shown on screen)
  // has no such check, so excluding it here silently broke every lookup
  // for whichever artist/album/genre happened to get id 0 (e.g. the first
  // artist crawled): the list displayed correctly, but every selection
  // within it resolved to 0 regardless of which row was actually picked.
  //
  // For the same reason, failures return UINT32_MAX rather than 0 - 0 is a
  // real id, so it can't double as "not found" without colliding with it.
  if (!initialized) {
    return UINT32_MAX;
  }

  // Binary search for source ID
  uint32_t source_ids_base = header.baseOffset + 8; // Skip header
  int32_t source_index =
      binarySearch(source_id, source_ids_base, header.entry_count);
  if (source_index < 0) {
    return UINT32_MAX; // Source ID not found
  }

  // Get target count for this source
  uint32_t target_counts_base = source_ids_base + (header.entry_count * 4);
  uint32_t target_count;
  psram.readData(target_counts_base + (source_index * 4),
                 (uint8_t *)&target_count, 4);

  if (index >= target_count) {
    return UINT32_MAX; // Index out of range
  }

  // Calculate offset into target IDs array
  uint32_t target_offset = 0;
  for (int32_t i = 0; i < source_index; i++) {
    if ((i & 63) == 63) {
      audioYield(); // one read per earlier source: thousands on a big library
    }
    uint32_t count;
    psram.readData(target_counts_base + (i * 4), (uint8_t *)&count, 4);
    target_offset += count;
  }

  // Read the specific target ID at the requested index
  uint32_t target_ids_base = target_counts_base + (header.entry_count * 4);
  uint32_t target_id;
  psram.readData(target_ids_base + ((target_offset + index) * 4),
                 (uint8_t *)&target_id, 4);

  return target_id;
}

uint32_t MusicLookup::getIdAtIndex(const index_header_t &header,
                                   uint32_t index) {
  if (!initialized || index >= header.entry_count) {
    return UINT32_MAX; // Not found - 0 is a valid id, can't mean "not found"
  }

  uint32_t entity_id;
  uint32_t ids_base = header.baseOffset + 8; // Skip header
  psram.readData(ids_base + (index * 4), (uint8_t *)&entity_id, 4);
  return entity_id;
}

// Convenience functions - just call the core functions
uint32_t MusicLookup::getArtistIdAtIndex(uint32_t index) {
  return getIdAtIndex(artist_index_h, index);
}

uint32_t MusicLookup::getAlbumIdAtIndex(uint32_t index) {
  return getIdAtIndex(album_index_h, index);
}

uint32_t MusicLookup::getGenreIdAtIndex(uint32_t index) {
  return getIdAtIndex(genre_index_h, index);
}

uint32_t MusicLookup::getTrackIdAtIndex(uint32_t index) {
  return getIdAtIndex(track_index_h, index);
}

uint32_t MusicLookup::getTrackIdByArtistAtIndex(uint32_t artistId,
                                                uint32_t index) {
  return getRelatedIdAtIndex(artistId, artist_to_tracks_h, index);
}

uint32_t MusicLookup::getTrackIdByAlbumAtIndex(uint32_t albumId,
                                               uint32_t index) {
  return getRelatedIdAtIndex(albumId, album_to_tracks_h, index);
}

uint32_t MusicLookup::getTrackIdByGenreAtIndex(uint32_t genreId,
                                               uint32_t index) {
  return getRelatedIdAtIndex(genreId, genre_to_tracks_h, index);
}

uint32_t MusicLookup::getAlbumIdByArtistAtIndex(uint32_t artistId,
                                                uint32_t index) {
  return getRelatedIdAtIndex(artistId, artist_to_albums_h, index);
}

uint32_t MusicLookup::getAlbumIdByGenreAtIndex(uint32_t genreId,
                                               uint32_t index) {
  return getRelatedIdAtIndex(genreId, genre_to_albums_h, index);
}

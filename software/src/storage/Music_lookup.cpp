#include "Music_lookup.h"
#include "../sound/AudioYield.h"
#include "PackSections.h"

MusicLookup::MusicLookup() : initialized(false) {
  memset(&genre_to_tracks_h, 0, sizeof(genre_to_tracks_h));
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
  memset(&search_trie, 0, sizeof(search_trie));
  podcasts_loaded = false;
  podcast_generation = 0;
}

MusicLookup::~MusicLookup() {}

bool MusicLookup::getTrackPath(uint32_t track_id, char *buffer,
                               uint32_t buffer_size) {
  if (isPodcastTrack(track_id)) {
    return podcastEpisodePath(podcastEpisodeOf(track_id), buffer, buffer_size);
  }
  return getString(track_id, path_index_h, buffer, buffer_size);
}

uint32_t MusicLookup::getTrackDurationSeconds(uint32_t track_id) {
  if (isPodcastTrack(track_id)) {
    return podcastEpisodeDuration(podcastEpisodeOf(track_id));
  }
  if (!initialized || track_id >= duration_index_h.entry_count) {
    return 0;
  }
  uint16_t seconds = 0;
  pageStore.read(duration_index_h.sec, 4 + track_id * 2, &seconds, 2);
  return seconds;
}

uint32_t MusicLookup::getAlbumIdForTrack(uint32_t track_id) {
  if (!initialized || track_id >= track_to_album_h.entry_count) {
    return 0xFFFFFFFFu;
  }
  return pageStore.readU32(track_to_album_h.sec, 4 + track_id * 4);
}

uint32_t MusicLookup::getArtistIdForTrack(uint32_t track_id) {
  if (!initialized || track_id >= track_to_artist_h.entry_count) {
    return 0xFFFFFFFFu;
  }
  return pageStore.readU32(track_to_artist_h.sec, 4 + track_id * 4);
}

uint32_t MusicLookup::getArtistIdForAlbum(uint32_t album_id) {
  if (!initialized) {
    return 0xFFFFFFFFu;
  }
  if (album_id < album_to_artist_h.entry_count) {
    return pageStore.readU32(album_to_artist_h.sec, 4 + album_id * 4);
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

bool MusicLookup::readCover(const album_cover_index_t &h, uint32_t id,
                            album_cover_entry_t &out) {
  if (id >= h.entry_count) {
    return false;
  }
  // Field by field rather than into a padded C struct (9-byte entries).
  uint8_t e[9];
  pageStore.read(h.sec, 4 + id * 9, e, sizeof(e));
  memcpy(&out.offset, e, 4);
  memcpy(&out.length, e + 4, 4);
  out.format = e[8];
  return out.length > 0;
}

bool MusicLookup::getAlbumCoverEntry(uint32_t album_id,
                                     album_cover_entry_t &out) {
  return initialized && readCover(album_cover_h, album_id, out);
}

bool MusicLookup::getString(uint32_t id, const index_header_t &index_header,
                            char *buffer, uint32_t buffer_size) {
  if (!initialized || !buffer || buffer_size == 0 ||
      id >= index_header.entry_count) {
    return false;
  }
  lookup_result_t result = getStringAtIndex(id, index_header);
  if (result.length == 0) {
    return false;
  }
  uint32_t copy_len = min(result.length, buffer_size - 1);
  pageStore.read(index_header.sec, result.offset, buffer, copy_len);
  buffer[copy_len] = '\0';
  return true;
}

// String `index` of a table: [count][data_size][offset]*(count+1)[strings].
lookup_result_t MusicLookup::getStringAtIndex(uint32_t index,
                                              const index_header_t &header) {
  lookup_result_t result = {0, 0};
  if (index >= header.entry_count) {
    return result;
  }
  uint32_t se[2]; // start, end: one read
  pageStore.read(header.sec, 8 + index * 4, se, sizeof(se));
  if (se[1] <= se[0] || se[1] > header.data_size) {
    return result; // empty (or damaged)
  }
  result.offset = 8 + (header.entry_count + 1) * 4 + se[0];
  result.length = se[1] - se[0] - 1; // -1 for the NUL
  return result;
}

// Relations: [count][total][start]*(count+1)[target]*total.
uint32_t MusicLookup::getRelatedCount(uint32_t source_id,
                                      const relation_header_t &header) {
  if (!initialized || source_id >= header.entry_count) {
    return 0;
  }
  uint32_t se[2];
  pageStore.read(header.sec, 8 + source_id * 4, se, sizeof(se));
  return se[1] >= se[0] ? se[1] - se[0] : 0;
}

uint32_t MusicLookup::getRelatedIds(uint32_t source_id,
                                    const relation_header_t &header,
                                    uint32_t *results, uint32_t max_results,
                                    uint32_t offset) {
  if (!initialized || !results || max_results == 0 ||
      source_id >= header.entry_count) {
    return 0;
  }
  uint32_t se[2];
  pageStore.read(header.sec, 8 + source_id * 4, se, sizeof(se));
  if (se[1] < se[0] || se[1] - se[0] <= offset) {
    return 0;
  }
  uint32_t to_read = min(se[1] - se[0] - offset, max_results);
  uint32_t targets = 8 + (header.entry_count + 1) * 4;
  // One read: they are contiguous.
  if (!pageStore.read(header.sec, targets + (se[0] + offset) * 4, results,
                      to_read * 4)) {
    return 0;
  }
  return to_read;
}

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
      audioYield(); // cold pages read the card: keep the audio fifo topped up
    }
    lookup_result_t result = getStringAtIndex(target_ids[i], target_index_header);
    if (!copyString(target_index_header, result, buffer_pos, buffer_remaining,
                    string_pointers[strings_loaded])) {
      break; // buffer full
    }
    strings_loaded++;
  }

  return strings_loaded;
}

// Copies one string to buffer_pos (advancing it), "" for a missing one.
// Every slot gets a string, so list positions stay aligned with ids. False
// when the buffer is full.
bool MusicLookup::copyString(const index_header_t &header,
                             const lookup_result_t &result, char *&buffer_pos,
                             uint32_t &buffer_remaining, const char *&out) {
  uint32_t len = result.length;
  if (len + 1 > buffer_remaining) {
    return false;
  }
  out = buffer_pos;
  if (len > 0) {
    pageStore.read(header.sec, result.offset, buffer_pos, len);
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
    if (!copyString(header, getStringAtIndex(i, header), buffer_pos,
                    buffer_remaining, string_pointers[strings_loaded])) {
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
  if (isPodcastTrack(track_id)) {
    return podcastEpisodeTitle(podcastEpisodeOf(track_id), buffer, buffer_size);
  }
  return getString(track_id, track_index_h, buffer, buffer_size);
}

// source_id == 0 is a valid id, and failures return UINT32_MAX (0 is a real
// id, so it can't double as "not found").
uint32_t MusicLookup::getRelatedIdAtIndex(uint32_t source_id,
                                          const relation_header_t &header,
                                          uint32_t index) {
  uint32_t id;
  return getRelatedIds(source_id, header, &id, 1, index) == 1 ? id : UINT32_MAX;
}

// Ids are dense and in list order: the index-th entry of a table has id
// index.
uint32_t MusicLookup::getIdAtIndex(const index_header_t &header,
                                   uint32_t index) {
  if (!initialized || index >= header.entry_count) {
    return UINT32_MAX;
  }
  return index;
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

// Letter tables: [u32 count]{u32 start, u8 key[4]}*count, in list order.
int32_t MusicLookup::letterBucket(uint32_t list, uint32_t letters,
                                  uint32_t pos, int dir) {
  PackRef t;
  if (!initialized || list > 3 || letters < 1 || letters > 3 ||
      !pageStore.find(0, PackSection::letters(list, letters), t) ||
      t.length < 4) {
    return -1;
  }
  uint32_t n = pageStore.readU32(t, 0);
  if (n == 0 || 4 + (uint64_t)n * 8 > t.length) {
    return -1;
  }
  // The last bucket starting at or before pos.
  uint32_t lo = 0, hi = n - 1;
  while (lo < hi) {
    uint32_t mid = lo + (hi - lo + 1) / 2;
    if (pageStore.readU32(t, 4 + mid * 8) <= pos)
      lo = mid;
    else
      hi = mid - 1;
  }
  uint32_t start = pageStore.readU32(t, 4 + lo * 8);
  if (dir > 0) {
    return lo + 1 < n ? (int32_t)pageStore.readU32(t, 4 + (lo + 1) * 8)
                      : (int32_t)pos;
  }
  if (start < pos || lo == 0) {
    return (int32_t)min(start, pos);
  }
  return (int32_t)pageStore.readU32(t, 4 + (lo - 1) * 8);
}

// --- podcasts ------------------------------------------------------------------

uint32_t MusicLookup::readDense(const track_to_id_index_t &h, uint32_t i) {
  if (!podcasts_loaded || i >= h.entry_count) {
    return 0xFFFFFFFFu;
  }
  return pageStore.readU32(h.sec, 4 + i * 4);
}

bool MusicLookup::podcastFolderName(uint32_t folder, char *buf,
                                    uint32_t size) {
  return podcasts_loaded && getString(folder, pod_folder_names_h, buf, size);
}

uint32_t MusicLookup::podcastFolderParent(uint32_t folder) {
  return readDense(pod_folder_parent_h, folder);
}

bool MusicLookup::podcastFolderIsSeries(uint32_t folder) {
  uint32_t flags = readDense(pod_folder_flags_h, folder);
  return flags != 0xFFFFFFFFu && (flags & 1);
}

void MusicLookup::podcastFolderSpan(uint32_t folder, uint32_t &first,
                                    uint32_t &end) {
  first = readDense(pod_folder_span_h, folder * 2);
  end = readDense(pod_folder_span_h, folder * 2 + 1);
  if (first == 0xFFFFFFFFu || end == 0xFFFFFFFFu || end < first) {
    first = end = 0;
  }
}

uint32_t MusicLookup::podcastSubfolderCount(uint32_t folder) {
  return podcasts_loaded ? getRelatedCount(folder, pod_folder_children_h) : 0;
}

uint32_t MusicLookup::podcastSubfolderAt(uint32_t folder, uint32_t index) {
  return podcasts_loaded
             ? getRelatedIdAtIndex(folder, pod_folder_children_h, index)
             : 0xFFFFFFFFu;
}

uint32_t MusicLookup::podcastOwnEpisodeCount(uint32_t folder) {
  return podcasts_loaded ? getRelatedCount(folder, pod_folder_episodes_h) : 0;
}

uint32_t MusicLookup::podcastOwnEpisodeAt(uint32_t folder, uint32_t index) {
  // A folder's own episodes are the first ids of its span.
  if (index >= podcastOwnEpisodeCount(folder)) {
    return 0xFFFFFFFFu;
  }
  uint32_t first, end;
  podcastFolderSpan(folder, first, end);
  return first + index;
}

bool MusicLookup::podcastEpisodeTitle(uint32_t episode, char *buf,
                                      uint32_t size) {
  return podcasts_loaded && getString(episode, pod_episode_titles_h, buf, size);
}

bool MusicLookup::podcastEpisodePath(uint32_t episode, char *buf,
                                     uint32_t size) {
  return podcasts_loaded && getString(episode, pod_episode_paths_h, buf, size);
}

uint32_t MusicLookup::podcastEpisodeDuration(uint32_t episode) {
  if (!podcasts_loaded || episode >= pod_episode_durations_h.entry_count) {
    return 0;
  }
  uint16_t seconds = 0;
  pageStore.read(pod_episode_durations_h.sec, 4 + episode * 2, &seconds, 2);
  return seconds;
}

uint32_t MusicLookup::podcastEpisodeFolder(uint32_t episode) {
  return readDense(pod_episode_folder_h, episode);
}

uint32_t MusicLookup::podcastEpisodeDate(uint32_t episode) {
  uint32_t d = readDense(pod_episode_date_h, episode);
  return d == 0xFFFFFFFFu ? 0 : d;
}

uint32_t MusicLookup::podcastEpisodeNumber(uint32_t episode) {
  uint32_t n = readDense(pod_episode_number_h, episode);
  return n == 0xFFFFFFFFu ? 0 : n;
}

bool MusicLookup::getPodcastCoverEntry(uint32_t folder,
                                       album_cover_entry_t &out) {
  return podcasts_loaded && readCover(pod_folder_cover_h, folder, out);
}

#include "Music_lookup.h"
#include "SharedSpiDriver.h"
#include "../logging.h"
#include "PackSections.h"

// MusicLookup start-up: opens the index packs. The lookups that read them
// are in Music_lookup.cpp.

static const uint8_t MUSIC_PACK = 0;
static const uint8_t PODCAST_PACK = 1;

// Opening the packs reads their section tables only: the tables stay on the
// card and come into PSRAM a page at a time as they're read (PageStore.h).
bool MusicLookup::init() {
  LOG_CAT(INDEX_USAGE, "=== MUSIC LOOKUP INIT ===");
  initialized = false;
  podcasts_loaded = false;
  index_missing = false;

  // Audio_buffer::begin() usually mounted the card already (same SdFat): a
  // second begin() would reset it under the files the player has open.
  if (sd.fatType() == 0 && !sd.begin(OPENPOD_SD_CONFIG(CARDCS))) {
    LOG_ERROR("SD card initialization failed");
    return false;
  }

  pageStore.begin(INDEX_CACHE_BASE_ADDRESS, INDEX_CACHE_SIZE);
  if (pageStore.open(sd, "/opentone/index.pack", MUSIC_PACK) < 0) {
    LOG_ERROR("No /opentone/index.pack: the card needs a re-sync");
    index_missing = true;
    return false;
  }

  using namespace PackSection;
  const uint8_t P = MUSIC_PACK;
  bool ok = openStrings(P, ARTIST_NAMES, artist_index_h) &&
            openStrings(P, ALBUM_NAMES, album_index_h) &&
            openStrings(P, GENRE_NAMES, genre_index_h) &&
            openStrings(P, TRACK_NAMES, track_index_h) &&
            openStrings(P, PATHS, path_index_h) &&
            openDense(P, DURATIONS, duration_index_h, 2) &&
            openDense(P, TRACK_TO_ALBUM, track_to_album_h, 4) &&
            openDense(P, ALBUM_TO_COVER, album_cover_h, 9) &&
            openRelation(P, ARTIST_TO_ALBUMS, artist_to_albums_h) &&
            openRelation(P, ARTIST_TO_TRACKS, artist_to_tracks_h) &&
            openRelation(P, ALBUM_TO_TRACKS, album_to_tracks_h) &&
            openRelation(P, GENRE_TO_ALBUMS, genre_to_albums_h) &&
            openRelation(P, GENRE_TO_TRACKS, genre_to_tracks_h);
  if (!ok) {
    LOG_ERROR("index.pack is incomplete: the card needs a re-sync");
    pageStore.close(MUSIC_PACK);
    index_missing = true;
    return false;
  }
  // Optional: without them, Now Playing has no artist line and search
  // falls back to the album's first track's artist.
  if (!openDense(P, TRACK_TO_ARTIST, track_to_artist_h, 4))
    memset(&track_to_artist_h, 0, sizeof(track_to_artist_h));
  if (!openDense(P, ALBUM_TO_ARTIST, album_to_artist_h, 4))
    memset(&album_to_artist_h, 0, sizeof(album_to_artist_h));
  if (!pageStore.find(P, SEARCH_TRIE, search_trie))
    memset(&search_trie, 0, sizeof(search_trie));

  initialized = true; // the podcast loader reads back through getters
  podcasts_loaded = loadPodcasts();
  LOG_INFO("Music lookup: %lu tracks", (unsigned long)track_index_h.entry_count);
  return true;
}

bool MusicLookup::openStrings(uint8_t pack, uint32_t id, index_header_t &h) {
  if (!pageStore.find(pack, id, h.sec) || h.sec.length < 12) {
    return false;
  }
  h.entry_count = pageStore.readU32(h.sec, 0);
  h.data_size = pageStore.readU32(h.sec, 4);
  // Checked in 64 bits: a damaged header can't claim more than is there.
  uint64_t need = 8 + ((uint64_t)h.entry_count + 1) * 4 + h.data_size;
  return need <= h.sec.length;
}

bool MusicLookup::openRelation(uint8_t pack, uint32_t id,
                               relation_header_t &h) {
  if (!pageStore.find(pack, id, h.sec) || h.sec.length < 12) {
    return false;
  }
  h.entry_count = pageStore.readU32(h.sec, 0);
  h.total_target_count = pageStore.readU32(h.sec, 4);
  uint64_t need = 8 + ((uint64_t)h.entry_count + 1) * 4 +
                  (uint64_t)h.total_target_count * 4;
  return need <= h.sec.length;
}

bool MusicLookup::openDense(uint8_t pack, uint32_t id, duration_index_t &h,
                            uint32_t entrySize) {
  if (!pageStore.find(pack, id, h.sec) || h.sec.length < 4) {
    return false;
  }
  h.entry_count = pageStore.readU32(h.sec, 0);
  return 4 + (uint64_t)h.entry_count * entrySize <= h.sec.length;
}

bool MusicLookup::loadPodcasts() {
  // "OPPI" | u16 version | u16 0 | u32 episodes | u32 generation
  FsFile meta = sd.open("/opentone/podcast_meta.bin", O_RDONLY);
  if (!meta.isOpen()) {
    return false;
  }
  uint8_t m[16];
  bool read = meta.read(m, sizeof(m)) == sizeof(m);
  meta.close();
  uint32_t w[4];
  memcpy(w, m, sizeof(w));
  if (!read || w[0] != 0x4950504Fu /* "OPPI" */ || (w[1] & 0xFFFF) != 1) {
    LOG_ERROR("podcast_meta.bin: unknown format");
    return false;
  }
  uint32_t episodes = w[2];
  podcast_generation = w[3];

  if (pageStore.open(sd, "/opentone/podcasts.pack", PODCAST_PACK) < 0) {
    LOG_ERROR("No /opentone/podcasts.pack: no Podcasts screen");
    return false;
  }
  using namespace PackSection;
  const uint8_t P = PODCAST_PACK;
  bool ok = openStrings(P, POD_FOLDER_NAMES, pod_folder_names_h) &&
            openStrings(P, POD_EPISODE_TITLES, pod_episode_titles_h) &&
            openStrings(P, POD_EPISODE_PATHS, pod_episode_paths_h) &&
            openDense(P, POD_FOLDER_PARENT, pod_folder_parent_h, 4) &&
            openDense(P, POD_FOLDER_FLAGS, pod_folder_flags_h, 4) &&
            openDense(P, POD_FOLDER_SPAN, pod_folder_span_h, 4) &&
            openDense(P, POD_EPISODE_FOLDER, pod_episode_folder_h, 4) &&
            openDense(P, POD_EPISODE_DATE, pod_episode_date_h, 4) &&
            openDense(P, POD_EPISODE_NUMBER, pod_episode_number_h, 4) &&
            openRelation(P, POD_FOLDER_CHILDREN, pod_folder_children_h) &&
            openRelation(P, POD_FOLDER_EPISODES, pod_folder_episodes_h) &&
            openDense(P, POD_EPISODE_DURATIONS, pod_episode_durations_h, 2) &&
            openDense(P, POD_FOLDER_TO_COVER, pod_folder_cover_h, 9);
  if (!ok || pod_episode_titles_h.entry_count != episodes) {
    LOG_ERROR("Podcast tables incomplete: no Podcasts screen");
    pageStore.close(PODCAST_PACK);
    return false;
  }
  LOG_INFO("Podcasts: %lu episodes in %lu folders",
           (unsigned long)episodes,
           (unsigned long)pod_folder_names_h.entry_count);
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

#pragma once
#include <stdint.h>

// The podcast tables (openpod/podcast_*.bin, written by the syncer's
// podcasts module) as the podcast screens read them. MusicLookup implements
// it from PSRAM; tests use an in-memory one.
//
// Ids follow a depth-first walk in display order: folder 0 is the root
// ("Podcasts"), a folder's own episodes come first, then its subfolders
// (alphabetical). So a folder's own episodes are ids
// [span.first, span.first + ownEpisodes) and everything below it is
// [span.first, span.end): one linear scan rolls a folder's state up.

// Podcast episodes travel through the player as track ids with this bit
// set, so the music playback path (paths, durations, names, prefetch)
// works for them unchanged (see MusicLookup::getTrackPath).
#define PODCAST_TRACK_BIT 0x40000000u

inline bool isPodcastTrack(uint32_t trackId) {
  return trackId != 0xFFFFFFFFu && (trackId & PODCAST_TRACK_BIT);
}
inline uint32_t podcastTrackId(uint32_t episode) {
  return episode | PODCAST_TRACK_BIT;
}
inline uint32_t podcastEpisodeOf(uint32_t trackId) {
  return trackId & ~PODCAST_TRACK_BIT;
}

class PodcastSource {
public:
  static const uint32_t NONE = 0xFFFFFFFFu;

  virtual ~PodcastSource() {}

  virtual bool hasPodcasts() const = 0;
  // Must match podcast_state.bin's, or the state is left alone.
  virtual uint32_t podcastGeneration() const = 0;
  virtual uint32_t podcastFolderCount() const = 0;
  virtual uint32_t podcastEpisodeCount() const = 0;

  virtual bool podcastFolderName(uint32_t folder, char *buf, uint32_t size) = 0;
  // NONE for the root.
  virtual uint32_t podcastFolderParent(uint32_t folder) = 0;
  // Every episode title in it starts with "N/M".
  virtual bool podcastFolderIsSeries(uint32_t folder) = 0;
  // Episodes below the folder (own and in subfolders): [first, end).
  virtual void podcastFolderSpan(uint32_t folder, uint32_t &first,
                                 uint32_t &end) = 0;
  virtual uint32_t podcastSubfolderCount(uint32_t folder) = 0;
  virtual uint32_t podcastSubfolderAt(uint32_t folder, uint32_t index) = 0;
  // Its own episodes, in display order.
  virtual uint32_t podcastOwnEpisodeCount(uint32_t folder) = 0;
  virtual uint32_t podcastOwnEpisodeAt(uint32_t folder, uint32_t index) = 0;

  // Title without the "N/M" prefix of series episodes.
  virtual bool podcastEpisodeTitle(uint32_t episode, char *buf,
                                   uint32_t size) = 0;
  virtual bool podcastEpisodePath(uint32_t episode, char *buf,
                                  uint32_t size) = 0;
  virtual uint32_t podcastEpisodeDuration(uint32_t episode) = 0;
  virtual uint32_t podcastEpisodeFolder(uint32_t episode) = 0;
  // Days since 1970-01-01, 0 when unknown.
  virtual uint32_t podcastEpisodeDate(uint32_t episode) = 0;
  // n | total << 16, 0 when the episode isn't numbered.
  virtual uint32_t podcastEpisodeNumber(uint32_t episode) = 0;
};

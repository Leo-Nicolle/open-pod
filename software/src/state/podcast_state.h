#pragma once
#include <Arduino.h>
#include <SdFat.h>
#include <stdint.h>

// Each podcast episode's listening state: /opentone/podcast_state.bin, one
// fixed-size record per episode id (written by the syncer's podcasts
// module, which moves the records to the new ids on every sync).
//
//   header (16 B): "OPPS" | u16 version | u16 record size | u32 count
//                  | u32 generation
//   record (20 B): u64 key | u32 position_s | u32 last_played | u32 flags
//
// The records live in PSRAM; a change rewrites its own 12 bytes on the card
// in place. When the count or generation doesn't match the index (a sync
// interrupted between the two files) the state is ignored and never
// written: every episode shows as new until the next sync.
//
// Rules (.agents/opentone-podcasts-design/.../README.md §6): new = not
// played and position 0; in progress = not played, position > 0; played.
// last_played is a sequence number (no clock): the highest in-progress one
// is the "continue" row.
class PodcastState {
public:
  static const uint32_t HEADER = 16;
  static const uint32_t RECORD = 20;
  static const uint32_t FLAG_PLAYED = 1;
  // Leaving an episode with at most this much left counts as played (the
  // tail is credits, ads, the closing theme): 5% of it, within these bounds.
  static const uint32_t PLAYED_TAIL_MIN_SECONDS = 60;
  static const uint32_t PLAYED_TAIL_MAX_SECONDS = 180;
  static uint32_t playedTail(uint32_t duration);
  static const char *const PATH;

  enum Mark : uint8_t { MARK_NEW, MARK_PROGRESS, MARK_NONE };

  struct Entry {
    uint32_t position;
    uint32_t lastPlayed;
    uint32_t flags;
    bool played() const { return flags & FLAG_PLAYED; }
    Mark mark() const {
      return played() ? MARK_NONE : position > 0 ? MARK_PROGRESS : MARK_NEW;
    }
  };

  // A folder's episodes, rolled up: any new -> filled mark, else any in
  // progress -> half mark, else everything played -> none (dim title).
  struct Rollup {
    uint32_t episodes;
    uint32_t fresh;
    uint32_t inProgress;
    Mark mark() const {
      return fresh ? MARK_NEW : inProgress ? MARK_PROGRESS : MARK_NONE;
    }
  };

  PodcastState();

  // Copies the state file into PSRAM at psramBase (PODCAST_STATE_BASE_ADDRESS
  // on the device: PODCAST_STATE_SIZE bytes, so at most that / RECORD
  // episodes). `episodes` and `generation` come from the podcast index.
  // Returns whether it is usable.
  bool load(SdFat &sd, uint32_t psramBase, uint32_t episodes,
            uint32_t generation);
  bool isLoaded() const { return loaded; }
  uint32_t count() const { return records; }
  // PSRAM taken (0 when not loaded).
  uint32_t bytesUsed() const { return loaded ? records * RECORD : 0; }
  // Changes with every write, so views can cache what they computed.
  uint32_t version() const { return changes; }

  Entry get(uint32_t episode) const;

  // Remembers where playback is. `leaving`: the user paused, stopped or
  // switched episode (not a periodic save), so being within playedTail() of
  // the end marks it played.
  void savePosition(uint32_t episode, uint32_t position, uint32_t duration,
                    bool leaving);
  // Reached the end.
  void markFinished(uint32_t episode);
  // Mark as played keeps the position; mark as unplayed resets it to 0.
  void markPlayed(uint32_t episode, bool played);

  // Episodes [first, end).
  Rollup rollup(uint32_t first, uint32_t end) const;
  // The in-progress episode played last, or -1.
  int32_t continueEpisode() const;

private:
  SdFat *sd;
  bool loaded;
  uint32_t base;
  uint32_t records;
  uint32_t sequence; // highest last_played
  uint32_t changes;
  mutable uint32_t continueVersion;
  mutable int32_t continueCache;

  void put(uint32_t episode, const Entry &e);
};

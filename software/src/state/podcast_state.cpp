#include "podcast_state.h"
#include "../logging.h"
#include "../storage/PSRAM_controller.hpp"

const char *const PodcastState::PATH = "/openpod/podcast_state.bin";

static uint32_t le32(const uint8_t *b) {
  return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) |
         ((uint32_t)b[3] << 24);
}

static void putLe32(uint8_t *b, uint32_t v) {
  b[0] = v & 0xFF;
  b[1] = (v >> 8) & 0xFF;
  b[2] = (v >> 16) & 0xFF;
  b[3] = (v >> 24) & 0xFF;
}

PodcastState::PodcastState()
    : sd(nullptr), loaded(false), base(0), records(0), sequence(0),
      changes(0), continueVersion(0xFFFFFFFFu), continueCache(-1) {}

bool PodcastState::load(SdFat &card, uint32_t psramBase, uint32_t episodes,
                        uint32_t generation) {
  sd = &card;
  loaded = false;
  records = 0;
  sequence = 0;
  changes++;
  if (episodes > PODCAST_STATE_SIZE / RECORD) {
    LOG_ERROR("%lu podcast episodes: listening state holds %lu at most",
              (unsigned long)episodes,
              (unsigned long)(PODCAST_STATE_SIZE / RECORD));
    return false;
  }
  FsFile f;
  if (!f.open(PATH, O_RDONLY)) {
    LOG_WARN("No %s: podcast episodes all show as new", PATH);
    return false;
  }
  uint8_t h[HEADER];
  bool ok = f.read(h, HEADER) == HEADER && memcmp(h, "OPPS", 4) == 0 &&
            (h[4] | (h[5] << 8)) == 1 && (h[6] | (h[7] << 8)) == RECORD &&
            le32(h + 8) == episodes && le32(h + 12) == generation;
  if (!ok) {
    f.close();
    LOG_WARN("%s doesn't match the podcast index (interrupted sync?): "
             "left alone",
             PATH);
    return false;
  }
  // One sector at a time through the stack, as MusicLookup loads its tables.
  uint8_t buf[RECORD * 25];
  uint32_t total = episodes * RECORD;
  for (uint32_t done = 0; done < total;) {
    uint32_t n = min((uint32_t)sizeof(buf), total - done);
    if (f.read(buf, n) != n) {
      f.close();
      return false;
    }
    psram.writeData(psramBase + done, buf, n);
    for (uint32_t r = 0; r < n; r += RECORD) {
      sequence = max(sequence, le32(buf + r + 12));
    }
    done += n;
  }
  f.close();
  base = psramBase;
  records = episodes;
  loaded = true;
  return true;
}

PodcastState::Entry PodcastState::get(uint32_t episode) const {
  Entry e = {0, 0, 0};
  if (!loaded || episode >= records) {
    return e;
  }
  uint8_t b[12];
  psram.readData(base + episode * RECORD + 8, b, sizeof(b));
  e.position = le32(b);
  e.lastPlayed = le32(b + 4);
  e.flags = le32(b + 8);
  return e;
}

void PodcastState::put(uint32_t episode, const Entry &e) {
  if (!loaded || episode >= records) {
    return;
  }
  uint8_t b[12];
  putLe32(b, e.position);
  putLe32(b + 4, e.lastPlayed);
  putLe32(b + 8, e.flags);
  psram.writeData(base + episode * RECORD + 8, b, sizeof(b));
  changes++;
  FsFile f;
  if (sd && f.open(PATH, O_RDWR)) {
    f.seekSet(HEADER + episode * RECORD + 8);
    if (f.write(b, sizeof(b)) != sizeof(b)) {
      LOG_ERROR("Could not save the podcast state");
    }
    f.close();
  } else {
    LOG_ERROR("Could not open %s to save the podcast state", PATH);
  }
}

uint32_t PodcastState::playedTail(uint32_t duration) {
  uint32_t tail = duration / 20;
  return tail < PLAYED_TAIL_MIN_SECONDS   ? PLAYED_TAIL_MIN_SECONDS
         : tail > PLAYED_TAIL_MAX_SECONDS ? PLAYED_TAIL_MAX_SECONDS
                                          : tail;
}

void PodcastState::savePosition(uint32_t episode, uint32_t position,
                                uint32_t duration, bool leaving) {
  Entry e = get(episode);
  e.position = position;
  e.lastPlayed = ++sequence;
  if (leaving && duration > 0 && position + playedTail(duration) >= duration) {
    e.flags |= FLAG_PLAYED;
  }
  put(episode, e);
}

void PodcastState::markFinished(uint32_t episode) {
  Entry e = get(episode);
  e.flags |= FLAG_PLAYED;
  e.lastPlayed = ++sequence;
  put(episode, e);
}

void PodcastState::markPlayed(uint32_t episode, bool played) {
  Entry e = get(episode);
  if (played) {
    e.flags |= FLAG_PLAYED;
  } else {
    e.flags &= ~FLAG_PLAYED;
    e.position = 0;
  }
  put(episode, e);
}

PodcastState::Rollup PodcastState::rollup(uint32_t first, uint32_t end) const {
  Rollup r = {0, 0, 0};
  if (!loaded) {
    r.episodes = r.fresh = end > first ? end - first : 0; // all new
    return r;
  }
  end = min(end, records);
  r.episodes = end > first ? end - first : 0;
  uint8_t buf[RECORD * 16];
  for (uint32_t i = first; i < end;) {
    uint32_t n = min((uint32_t)16, end - i);
    psram.readData(base + i * RECORD, buf, n * RECORD);
    for (uint32_t k = 0; k < n; k++) {
      const uint8_t *rec = buf + k * RECORD;
      if (le32(rec + 16) & FLAG_PLAYED)
        continue;
      if (le32(rec + 8) > 0)
        r.inProgress++;
      else
        r.fresh++;
    }
    i += n;
  }
  return r;
}

int32_t PodcastState::continueEpisode() const {
  if (!loaded) {
    return -1;
  }
  if (continueVersion == changes) {
    return continueCache;
  }
  int32_t best = -1;
  uint32_t bestSeq = 0;
  uint8_t buf[RECORD * 16];
  for (uint32_t i = 0; i < records;) {
    uint32_t n = min((uint32_t)16, records - i);
    psram.readData(base + i * RECORD, buf, n * RECORD);
    for (uint32_t k = 0; k < n; k++) {
      const uint8_t *rec = buf + k * RECORD;
      uint32_t seq = le32(rec + 12);
      if (!(le32(rec + 16) & FLAG_PLAYED) && le32(rec + 8) > 0 &&
          (best < 0 || seq > bestSeq)) {
        best = (int32_t)(i + k);
        bestSeq = seq;
      }
    }
    i += n;
  }
  continueCache = best;
  continueVersion = changes;
  return best;
}

#pragma once
#include "podcast_source.h"
#include "podcast_state.h"
#include "router.h"
#include <Arduino.h>

// Text the podcast screens show (README §2, §3).
namespace podfmt {
// "28:40", "1:02:03"
void duration(uint32_t seconds, char *out, size_t size);
// "−34:13" (ASCII minus: the fonts are ASCII)
void timeLeft(uint32_t seconds, char *out, size_t size);
// "12 Oct"; "" for an unknown date (0)
void shortDate(uint32_t days, char *out, size_t size);
// "Mon, 12 Oct 2026"; "" for an unknown date
void longDate(uint32_t days, char *out, size_t size);
} // namespace podfmt

// One row of the podcast screens: a state mark at x=10, an optional series
// number column, the title, and the status right-aligned at x=304.
struct PodRow {
  enum Kind : uint8_t {
    CONTINUE, // Podcasts root: the in-progress episode played last
    FOLDER,
    EPISODE,
    ACTION, // episode actions (hold Center)
    INFO,   // read-only: Published, Length (the cursor skips them)
  };
  enum Action : uint8_t {
    ACT_NONE,
    ACT_RESUME,
    ACT_PLAY_START,
    ACT_PLAY,
    ACT_MARK_PLAYED,
    ACT_MARK_UNPLAYED,
  };
  enum Tone : uint8_t { TONE_DIMMER, TONE_ACCENT };

  uint8_t kind;
  uint8_t action;
  uint8_t mark; // PodcastState::Mark (MARK_NONE for actions)
  uint8_t tone; // status color
  bool dim;     // played: DIM title
  bool chevron;
  bool hasProgress;
  uint16_t progress; // underline width, 0-320
  uint32_t id;       // folder or episode id
  char title[96];
  char number[10]; // "1/10", series episodes only
  char status[24];
};

// The podcast screens' model: builds their rows from the podcast tables
// and the listening state, moves the cursor and runs the row actions. The
// selection lives in the route (selectedIndex / topVisibleIndex), so back()
// returns to it. No display code: it runs (and is tested) on the host.
class PodcastMenu {
public:
  static const int VISIBLE_ROWS = 7;

  enum Screen : uint8_t { NONE, ROOT, FOLDER, ACTIONS };

  // What the input layer does after a click.
  struct Nav {
    enum Kind : uint8_t { STAY, PUSH, BACK, PLAY } kind;
    Route_t::RouteType route; // PUSH
    uint32_t entity;          // PUSH: folder / episode id
    const char *title;        // PUSH
    int selected;             // PUSH: the new screen's first cursor row
    uint32_t episode;         // PLAY
    bool fromStart;           // PLAY: ignore the saved position
  };

  PodcastMenu(PodcastSource &source, PodcastState &state);

  static Screen screenFor(Route_t::RouteType type);
  static bool isPodcastRoute(Route_t::RouteType type) {
    return screenFor(type) != NONE;
  }

  // Lays out the route's rows (when the route or the listening state
  // changed) and clamps its selection. Cheap; call before drawing.
  void sync(Route_t &route);

  Screen screen() const { return scr; }
  const char *title() const { return heading; }
  int rowCount() const { return count; }
  PodRow row(int i) const;
  int selected() const { return sel; }
  int first() const { return top; }
  // No podcasts on the card: a hint instead of rows.
  bool empty() const { return scr != ACTIONS && count == 0; }

  // Cursor; negative clicks move down. Returns true if it moved.
  bool scroll(Route_t &route, int clicks);
  // Center on the selected row.
  Nav click(Route_t &route);
  // Center held on the selected row: an episode's actions.
  Nav hold(Route_t &route);

  // The row a folder's list opens on: its newest (dated) or next
  // (series) unfinished episode, else the first row.
  static int initialRow(PodcastSource &source, PodcastState &state,
                        uint32_t folder);

  // Content hash of screen chunk `chunk` (0 = header title), so the view
  // redraws only the chunks that changed.
  uint32_t chunkHash(int chunk) const;

private:
  PodcastSource &src;
  PodcastState &st;
  Screen scr;
  uint32_t entity;
  uint32_t laidOutVersion;
  char heading[64];
  int count;
  int sel;
  int top;
  // Rows: [continue] [subfolders] [own episodes] on folder screens.
  int contRows;
  int subRows;
  int32_t contEpisode;
  // ACTIONS screen rows.
  uint8_t actions[5];
  char navTitle[64];

  // Folder roll-ups, recomputed when the listening state changes.
  struct CachedRollup {
    uint32_t folder;
    PodcastState::Rollup r;
  };
  static const int ROLLUP_CACHE = 16;
  mutable CachedRollup rollups[ROLLUP_CACHE];
  mutable int rollupCount;
  mutable uint32_t rollupVersion;
  PodcastState::Rollup rollupOf(uint32_t folder) const;

  void layout(const Route_t &route);
  bool selectable(int i) const;
  void clamp(Route_t &route);
  void buildFolderRow(PodRow &r, uint32_t folder) const;
  void buildEpisodeRow(PodRow &r, uint32_t episode) const;
  void buildActionRow(PodRow &r, int i) const;
  Nav push(Route_t::RouteType type, uint32_t id, int selected);
  static Nav stay();
};

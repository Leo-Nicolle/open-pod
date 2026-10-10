#pragma once
#include "../storage/Music_index.h"
#include "../storage/Music_lookup.h"
#include <Arduino.h>

// The search screen's model: the query typed on the letter ribbon, the
// ranked hits for it, and the display strings for the rows on screen.
// No display code, so it runs (and is tested) on the host.
//
// Two modes: TYPING (the ribbon is up, scroll moves its cursor and 6 result
// rows show under it) and BROWSING (the ribbon drops, scroll moves the
// highlight over 7 rows).
class SearchModel {
public:
  enum Mode : uint8_t { TYPING, BROWSING };

  // A trie hit, deduplicated and ranked. type is a SearchType.
  struct Hit {
    uint32_t id;
    uint8_t type;
    uint8_t relevance;
    uint8_t nameLen;
  };

  static const int MAX_QUERY = 32;
  static const int MAX_HITS = 100;
  static const int MAX_ROWS = 7; // browsing capacity; typing shows 6
  static const int RIBBON_LEN = 36;
  static const int TITLE_LEN = 48;
  static const int SUB_LEN = 40;

  // The ribbon's characters, in order: A-Z then 0-9.
  static const char RIBBON[RIBBON_LEN + 1];

  SearchModel();

  // The trie to query (MusicIndex is loaded after MusicLookup at boot).
  void setIndex(MusicIndex *index) { this->index = index; }

  // Fresh, empty search in TYPING mode with the cursor on 'A'.
  void reset();

  // Ribbon cursor, clamped to the ribbon. Returns true if it moved.
  bool moveCursor(int clicks);

  // Query edits. Each re-runs the search and scrolls the list back to the
  // top. Return true if the query changed.
  bool append(MusicLookup &lookup);    // the ribbon letter under the cursor
  bool backspace(MusicLookup &lookup); // drop the last character
  bool space(MusicLookup &lookup);     // not first, not after another space

  // Mode switches. enterResults needs at least one hit.
  bool enterResults();
  bool exitResults();

  // Moves the highlight (BROWSING only); first follows to keep it visible.
  // Returns true if the selection moved.
  bool scroll(int clicks, MusicLookup &lookup);

  // Accessors for the view and State
  Mode getMode() const { return mode; }
  const char *getQuery() const { return query; }
  int getCursor() const { return cursor; }
  char getCursorChar() const { return RIBBON[cursor]; }
  int getHitCount() const { return hitCount; }
  const Hit &getHit(int i) const { return hits[i]; }
  int getSelected() const { return selected; }
  int getFirst() const { return first; }
  int getCapacity() const { return mode == TYPING ? MAX_ROWS - 1 : MAX_ROWS; }
  bool hasSelection() const { return mode == BROWSING && hitCount > 0; }
  const Hit *getSelectedHit() const {
    return hasSelection() ? &hits[selected] : nullptr;
  }

  // Display strings for the hit at index first + row (row < MAX_ROWS).
  const char *getRowTitle(int row) const { return rowTitle[row]; }
  const char *getRowSub(int row) const { return rowSub[row]; }

  // Lowercases, and drops a leading article ("the ", "le ", ...) once
  // something follows it, matching the indexer's normalizeString(). An
  // article alone is searched as the bare word ("the " -> "the").
  static void normalizeQuery(const char *in, char *out, size_t outSize);

  // Deduplicates raw trie hits by (type, id), keeping the best relevance,
  // and sorts them by relevance (high first), then shorter name first.
  // Returns the number of hits written to out.
  static int rankHits(const search_result_t *raw, int rawCount, Hit *out,
                      int maxOut);

private:
  MusicIndex *index;
  Mode mode;
  char query[MAX_QUERY + 1];
  int cursor;

  Hit hits[MAX_HITS];
  int hitCount;
  int selected;
  int first;

  // Row strings for hits [rowsFirst, rowsFirst + MAX_ROWS).
  char rowTitle[MAX_ROWS][TITLE_LEN];
  char rowSub[MAX_ROWS][SUB_LEN];
  int rowsFirst;
  int rowsCount;

  void runQuery(MusicLookup &lookup);
  void fillRows(MusicLookup &lookup);
  void describeHit(const Hit &hit, MusicLookup &lookup, char *title,
                   char *sub);
};

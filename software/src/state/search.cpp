#include "search.h"
#include "../sound/AudioYield.h"

// Out-of-class definitions: C++11 needs them once the constants are bound to
// references (min/constrain).
const int SearchModel::MAX_QUERY;
const int SearchModel::MAX_HITS;
const int SearchModel::MAX_ROWS;
const int SearchModel::RIBBON_LEN;
const int SearchModel::TITLE_LEN;
const int SearchModel::SUB_LEN;

const char SearchModel::RIBBON[RIBBON_LEN + 1] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

// Raw trie hits before dedup. Static rather than on the stack: 2 KB.
static search_result_t rawHits[SearchModel::MAX_HITS];

SearchModel::SearchModel()
    : index(nullptr), entry(RIBBON, MAX_QUERY, false) {
  reset();
}

void SearchModel::reset() {
  mode = TYPING;
  entry.reset();
  hitCount = 0;
  selected = 0;
  first = 0;
  rowsFirst = 0;
  rowsCount = 0;
  for (int i = 0; i < MAX_ROWS; i++) {
    rowTitle[i][0] = '\0';
    rowSub[i][0] = '\0';
  }
}

bool SearchModel::moveCursor(int clicks) { return entry.moveCursor(clicks); }

bool SearchModel::append(MusicLookup &lookup) {
  // The ribbon shows capitals; the index is lowercase.
  char c = entry.cursorChar();
  if (!entry.appendChar((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c))
    return false;
  runQuery(lookup);
  return true;
}

bool SearchModel::backspace(MusicLookup &lookup) {
  if (!entry.backspace())
    return false;
  runQuery(lookup);
  return true;
}

bool SearchModel::space(MusicLookup &lookup) {
  if (!entry.space())
    return false;
  runQuery(lookup);
  return true;
}

bool SearchModel::enterResults() {
  if (mode == BROWSING || hitCount == 0)
    return false;
  mode = BROWSING;
  selected = first;
  return true;
}

bool SearchModel::exitResults() {
  if (mode == TYPING)
    return false;
  mode = TYPING;
  return true;
}

bool SearchModel::scroll(int clicks, MusicLookup &lookup) {
  if (mode != BROWSING || hitCount == 0)
    return false;
  int next = constrain(selected + clicks, 0, hitCount - 1);
  if (next == selected)
    return false;
  selected = next;
  int cap = getCapacity();
  if (selected < first)
    first = selected;
  else if (selected >= first + cap)
    first = selected - cap + 1;
  fillRows(lookup);
  return true;
}

void SearchModel::normalizeQuery(const char *in, char *out, size_t outSize) {
  if (outSize == 0)
    return;
  size_t n = 0;
  for (; in[n] && n < outSize - 1; n++) {
    char c = in[n];
    out[n] = (c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c;
  }
  out[n] = '\0';

  static const char *const ARTICLES[] = {"the", "a",  "an",  "le", "la",
                                         "les", "un", "une", "des"};
  for (const char *article : ARTICLES) {
    size_t al = strlen(article);
    if (strncmp(out, article, al) != 0 || out[al] != ' ')
      continue;
    if (out[al + 1] == '\0') {
      out[al] = '\0'; // "the " -> "the": the word itself is indexed
    } else {
      memmove(out, out + al + 1, strlen(out + al + 1) + 1);
    }
    break;
  }
}

int SearchModel::rankHits(const search_result_t *raw, int rawCount, Hit *out,
                          int maxOut) {
  int count = 0;
  for (int i = 0; i < rawCount; i++) {
    uint8_t relevance = (uint8_t)min(raw[i].relevance, (uint32_t)255);
    int dup = -1;
    for (int j = 0; j < count; j++) {
      if (out[j].id == raw[i].id && out[j].type == raw[i].type) {
        dup = j;
        break;
      }
    }
    if (dup >= 0) {
      if (relevance > out[dup].relevance)
        out[dup].relevance = relevance;
      continue;
    }
    if (count >= maxOut)
      continue;
    out[count].id = raw[i].id;
    out[count].type = (uint8_t)raw[i].type;
    out[count].relevance = relevance;
    out[count].nameLen = (uint8_t)min(raw[i].name_length, (uint32_t)255);
    count++;
  }

  // Insertion sort: stable, and count is at most MAX_HITS.
  for (int i = 1; i < count; i++) {
    Hit h = out[i];
    int j = i - 1;
    while (j >= 0 && (out[j].relevance < h.relevance ||
                      (out[j].relevance == h.relevance &&
                       out[j].nameLen > h.nameLen))) {
      out[j + 1] = out[j];
      j--;
    }
    out[j + 1] = h;
  }
  return count;
}

void SearchModel::runQuery(MusicLookup &lookup) {
  hitCount = 0;
  selected = 0;
  first = 0;
  rowsCount = 0;

  char normalized[MAX_QUERY + 1];
  normalizeQuery(entry.text(), normalized, sizeof(normalized));
  if (normalized[0] != '\0' && index && index->isInitialized()) {
    uint32_t n = index->searchByPrefix(normalized, rawHits, MAX_HITS);
    hitCount = rankHits(rawHits, (int)n, hits, MAX_HITS);
  }
  if (hitCount == 0)
    mode = TYPING; // nothing to browse
  fillRows(lookup);
}

void SearchModel::fillRows(MusicLookup &lookup) {
  // Keep the strings of rows still on screen, look up only the new ones
  // (one wheel click scrolls a single row in).
  char oldTitle[MAX_ROWS][TITLE_LEN];
  char oldSub[MAX_ROWS][SUB_LEN];
  memcpy(oldTitle, rowTitle, sizeof(rowTitle));
  memcpy(oldSub, rowSub, sizeof(rowSub));
  int oldFirst = rowsFirst;
  int oldCount = rowsCount;

  rowsFirst = first;
  rowsCount = min(MAX_ROWS, hitCount - first);
  int lookups = 0;
  for (int row = 0; row < MAX_ROWS; row++) {
    int hitIndex = first + row;
    if (row >= rowsCount) {
      rowTitle[row][0] = '\0';
      rowSub[row][0] = '\0';
      continue;
    }
    int oldRow = hitIndex - oldFirst;
    if (oldRow >= 0 && oldRow < oldCount) {
      memcpy(rowTitle[row], oldTitle[oldRow], TITLE_LEN);
      memcpy(rowSub[row], oldSub[oldRow], SUB_LEN);
      continue;
    }
    describeHit(hits[hitIndex], lookup, rowTitle[row], rowSub[row]);
    if ((++lookups & 1) == 0)
      audioYield(); // two string lookups per row: keep the audio fifo topped up
  }
}

void SearchModel::describeHit(const Hit &hit, MusicLookup &lookup,
                              char *title, char *sub) {
  title[0] = '\0';
  sub[0] = '\0';
  uint32_t count = 0;
  switch (hit.type) {
  case SEARCH_TRACK: {
    lookup.getTrackName(hit.id, title, TITLE_LEN);
    uint32_t artistId = lookup.getArtistIdForTrack(hit.id);
    if (artistId != UINT32_MAX)
      lookup.getArtistName(artistId, sub, SUB_LEN);
    break;
  }
  case SEARCH_ALBUM: {
    lookup.getAlbumName(hit.id, title, TITLE_LEN);
    uint32_t artistId = lookup.getArtistIdForAlbum(hit.id);
    if (artistId != UINT32_MAX)
      lookup.getArtistName(artistId, sub, SUB_LEN);
    break;
  }
  case SEARCH_ARTIST:
    lookup.getArtistName(hit.id, title, TITLE_LEN);
    count = lookup.getAlbumCountForArtist(hit.id);
    snprintf(sub, SUB_LEN, "%u album%s", (unsigned)count, count == 1 ? "" : "s");
    break;
  case SEARCH_GENRE:
    lookup.getGenreName(hit.id, title, TITLE_LEN);
    count = lookup.getAlbumCountForGenre(hit.id);
    snprintf(sub, SUB_LEN, "%u album%s", (unsigned)count, count == 1 ? "" : "s");
    break;
  }
}

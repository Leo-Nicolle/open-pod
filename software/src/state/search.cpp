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

SearchModel::SearchModel()
    : index(nullptr), entry(RIBBON, MAX_QUERY, false), lookup(nullptr) {
  reset();
}

void SearchModel::reset() {
  mode = TYPING;
  entry.reset();
  hitCount = 0;
  selected = 0;
  first = 0;
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
  this->lookup = &lookup;
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

void SearchModel::addHit(const search_result_t &raw, Hit *out, int &count,
                         int maxOut) {
  uint8_t relevance = (uint8_t)min(raw.relevance, (uint32_t)255);
  for (int j = 0; j < count; j++) {
    if (out[j].id == raw.id && out[j].type == raw.type) {
      if (relevance > out[j].relevance)
        out[j].relevance = relevance;
      return;
    }
  }
  if (count >= maxOut)
    return;
  out[count].id = raw.id;
  out[count].type = (uint8_t)raw.type;
  out[count].relevance = relevance;
  out[count].nameLen = (uint8_t)min(raw.name_length, (uint32_t)255);
  count++;
}

void SearchModel::sortHits(Hit *out, int count) {
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
}

int SearchModel::rankHits(const search_result_t *raw, int rawCount, Hit *out,
                          int maxOut) {
  int count = 0;
  for (int i = 0; i < rawCount; i++)
    addHit(raw[i], out, count, maxOut);
  sortHits(out, count);
  return count;
}

void SearchModel::collectHit(const search_result_t &raw, void *self) {
  SearchModel *s = (SearchModel *)self;
  addHit(raw, s->hits, s->hitCount, MAX_HITS);
}

void SearchModel::runQuery(MusicLookup &lookup) {
  hitCount = 0;
  selected = 0;
  first = 0;
  this->lookup = &lookup;

  char normalized[MAX_QUERY + 1];
  normalizeQuery(entry.text(), normalized, sizeof(normalized));
  if (normalized[0] != '\0' && index && index->isInitialized()) {
    // Deduped as the trie is walked; the walk still stops after MAX_HITS
    // raw hits, duplicates included.
    index->searchByPrefix(normalized, collectHit, this, MAX_HITS);
    sortHits(hits, hitCount);
  }
  if (hitCount == 0)
    mode = TYPING; // nothing to browse
}

void SearchModel::describeRow(int row, char *title, char *sub) const {
  title[0] = '\0';
  sub[0] = '\0';
  int hitIndex = first + row;
  if (row < 0 || row >= MAX_ROWS || hitIndex >= hitCount || !lookup)
    return;
  describeHit(hits[hitIndex], *lookup, title, sub);
  audioYield(); // two string lookups: keep the audio fifo topped up
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

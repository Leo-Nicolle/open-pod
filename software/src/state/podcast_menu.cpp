#include "podcast_menu.h"
#include <stdio.h>
#include <string.h>

// --- formatting ------------------------------------------------------------------

namespace podfmt {

static const char *const MONTHS[12] = {"Jan", "Feb", "Mar", "Apr",
                                       "May", "Jun", "Jul", "Aug",
                                       "Sep", "Oct", "Nov", "Dec"};
static const char *const WEEKDAYS[7] = {"Sun", "Mon", "Tue", "Wed",
                                        "Thu", "Fri", "Sat"};

// Howard Hinnant's civil_from_days, for days since 1970-01-01.
static void civil(uint32_t days, int &y, unsigned &m, unsigned &d) {
  long z = (long)days + 719468;
  long era = z / 146097;
  unsigned doe = (unsigned)(z - era * 146097);
  unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
  long yy = (long)yoe + era * 400;
  unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
  unsigned mp = (5 * doy + 2) / 153;
  d = doy - (153 * mp + 2) / 5 + 1;
  m = mp < 10 ? mp + 3 : mp - 9;
  y = (int)(yy + (m <= 2));
}

void duration(uint32_t s, char *out, size_t size) {
  if (s >= 3600)
    snprintf(out, size, "%lu:%02lu:%02lu", (unsigned long)(s / 3600),
             (unsigned long)(s / 60 % 60), (unsigned long)(s % 60));
  else
    snprintf(out, size, "%lu:%02lu", (unsigned long)(s / 60),
             (unsigned long)(s % 60));
}

void timeLeft(uint32_t s, char *out, size_t size) {
  char d[16];
  duration(s, d, sizeof(d));
  snprintf(out, size, "-%s", d);
}

void shortDate(uint32_t days, char *out, size_t size) {
  if (!days) {
    out[0] = '\0';
    return;
  }
  int y;
  unsigned m, d;
  civil(days, y, m, d);
  snprintf(out, size, "%u %s", d, MONTHS[m - 1]);
}

void longDate(uint32_t days, char *out, size_t size) {
  if (!days) {
    out[0] = '\0';
    return;
  }
  int y;
  unsigned m, d;
  civil(days, y, m, d);
  snprintf(out, size, "%s, %u %s %d", WEEKDAYS[(days + 4) % 7], d,
           MONTHS[m - 1], y);
}

} // namespace podfmt

// --- menu ---------------------------------------------------------------------

// ACTIONS screen row codes: PodRow::Action, then the read-only rows.
static const uint8_t INFO_PUBLISHED = 100;
static const uint8_t INFO_LENGTH = 101;

PodcastMenu::PodcastMenu(PodcastSource &source, PodcastState &state)
    : src(source), st(state), scr(NONE), entity(0),
      laidOutVersion(0xFFFFFFFFu), count(0), sel(0), top(0), contRows(0),
      subRows(0), contEpisode(-1), rollupCount(0),
      rollupVersion(0xFFFFFFFFu) {
  heading[0] = '\0';
  navTitle[0] = '\0';
  memset(actions, 0, sizeof(actions));
}

PodcastMenu::Screen PodcastMenu::screenFor(Route_t::RouteType type) {
  switch (type) {
  case Route_t::PODCASTS:
    return ROOT;
  case Route_t::PODCAST_FOLDER:
    return FOLDER;
  case Route_t::PODCAST_ACTIONS:
    return ACTIONS;
  default:
    return NONE;
  }
}

PodcastState::Rollup PodcastMenu::rollupOf(uint32_t folder) const {
  if (rollupVersion != st.version()) {
    rollupVersion = st.version();
    rollupCount = 0;
  }
  for (int i = 0; i < rollupCount; i++)
    if (rollups[i].folder == folder)
      return rollups[i].r;
  uint32_t first, end;
  src.podcastFolderSpan(folder, first, end);
  PodcastState::Rollup r = st.rollup(first, end);
  int slot = rollupCount < ROLLUP_CACHE ? rollupCount++ : (int)(folder % ROLLUP_CACHE);
  rollups[slot].folder = folder;
  rollups[slot].r = r;
  return r;
}

void PodcastMenu::layout(const Route_t &route) {
  scr = screenFor(route.type);
  entity = route.type == Route_t::PODCASTS ? 0 : route.entityId;
  laidOutVersion = st.version();
  contRows = subRows = count = 0;
  contEpisode = -1;
  heading[0] = '\0';
  if (scr == NONE || !src.hasPodcasts()) {
    snprintf(heading, sizeof(heading), "Podcasts");
    return;
  }
  if (scr == ACTIONS) {
    src.podcastEpisodeTitle(entity, heading, sizeof(heading));
    PodcastState::Mark mark = st.get(entity).mark();
    int n = 0;
    if (mark == PodcastState::MARK_PROGRESS) {
      actions[n++] = PodRow::ACT_RESUME;
      actions[n++] = PodRow::ACT_PLAY_START;
      actions[n++] = PodRow::ACT_MARK_PLAYED;
    } else {
      actions[n++] = PodRow::ACT_PLAY;
      actions[n++] = mark == PodcastState::MARK_NONE ? PodRow::ACT_MARK_UNPLAYED
                                                     : PodRow::ACT_MARK_PLAYED;
    }
    if (src.podcastEpisodeDate(entity))
      actions[n++] = INFO_PUBLISHED;
    actions[n++] = INFO_LENGTH;
    count = n;
    return;
  }
  if (scr == ROOT) {
    snprintf(heading, sizeof(heading), "Podcasts");
    contEpisode = st.continueEpisode();
    contRows = contEpisode >= 0 ? 1 : 0;
  } else {
    src.podcastFolderName(entity, heading, sizeof(heading));
  }
  subRows = (int)src.podcastSubfolderCount(entity);
  count = contRows + subRows + (int)src.podcastOwnEpisodeCount(entity);
}

void PodcastMenu::sync(Route_t &route) {
  Screen s = screenFor(route.type);
  uint32_t e = route.type == Route_t::PODCASTS ? 0 : route.entityId;
  if (s != scr || e != entity || laidOutVersion != st.version())
    layout(route);
  clamp(route);
}

bool PodcastMenu::selectable(int i) const {
  if (i < 0 || i >= count)
    return false;
  return !(scr == ACTIONS && actions[i] >= INFO_PUBLISHED);
}

void PodcastMenu::clamp(Route_t &route) {
  sel = constrain(route.selectedIndex, 0, max(0, count - 1));
  // Never rest on a read-only row: step back to the nearest selectable one.
  while (sel > 0 && !selectable(sel))
    sel--;
  top = constrain(route.topVisibleIndex, 0, max(0, count - VISIBLE_ROWS));
  if (sel < top)
    top = sel;
  if (sel >= top + VISIBLE_ROWS)
    top = sel - VISIBLE_ROWS + 1;
  route.selectedIndex = sel;
  route.topVisibleIndex = top;
}

bool PodcastMenu::scroll(Route_t &route, int clicks) {
  sync(route);
  // Negative clicks scroll down, as in the other lists.
  int s = sel;
  int step = clicks < 0 ? 1 : -1;
  for (int n = clicks > 0 ? clicks : -clicks; n > 0; n--) {
    int next = s + step;
    while (next >= 0 && next < count && !selectable(next))
      next += step;
    if (next < 0 || next >= count)
      break;
    s = next;
  }
  if (s == sel)
    return false;
  route.selectedIndex = s;
  clamp(route);
  return true;
}

void PodcastMenu::buildFolderRow(PodRow &r, uint32_t folder) const {
  r.kind = PodRow::FOLDER;
  r.id = folder;
  r.chevron = true;
  src.podcastFolderName(folder, r.title, sizeof(r.title));
  PodcastState::Rollup ru = rollupOf(folder);
  r.mark = ru.mark();
  r.dim = ru.episodes > 0 && r.mark == PodcastState::MARK_NONE;
  if (ru.fresh) {
    snprintf(r.status, sizeof(r.status), "%lu new", (unsigned long)ru.fresh);
    r.tone = PodRow::TONE_ACCENT;
  } else {
    snprintf(r.status, sizeof(r.status), "%lu ep", (unsigned long)ru.episodes);
  }
}

void PodcastMenu::buildEpisodeRow(PodRow &r, uint32_t episode) const {
  r.kind = PodRow::EPISODE;
  r.id = episode;
  src.podcastEpisodeTitle(episode, r.title, sizeof(r.title));
  PodcastState::Entry e = st.get(episode);
  r.mark = e.mark();
  r.dim = e.played();
  uint32_t length = src.podcastEpisodeDuration(episode);
  uint32_t number = src.podcastEpisodeNumber(episode);
  if (number) {
    snprintf(r.number, sizeof(r.number), "%lu/%lu",
             (unsigned long)(number & 0xFFFF), (unsigned long)(number >> 16));
    podfmt::duration(length, r.status, sizeof(r.status));
  } else {
    podfmt::shortDate(src.podcastEpisodeDate(episode), r.status,
                      sizeof(r.status));
    if (!r.status[0])
      podfmt::duration(length, r.status, sizeof(r.status));
  }
  if (r.mark == PodcastState::MARK_PROGRESS && length > 0) {
    r.hasProgress = true;
    r.progress = (uint16_t)min((uint32_t)320, (320 * e.position + length / 2) / length);
  }
}

void PodcastMenu::buildActionRow(PodRow &r, int i) const {
  uint8_t a = actions[i];
  r.id = entity;
  if (a >= INFO_PUBLISHED) {
    r.kind = PodRow::INFO;
    if (a == INFO_PUBLISHED) {
      snprintf(r.title, sizeof(r.title), "Published");
      podfmt::longDate(src.podcastEpisodeDate(entity), r.status,
                       sizeof(r.status));
    } else {
      snprintf(r.title, sizeof(r.title), "Length");
      podfmt::duration(src.podcastEpisodeDuration(entity), r.status,
                       sizeof(r.status));
    }
    return;
  }
  r.kind = PodRow::ACTION;
  r.action = a;
  static const char *const LABELS[] = {"",     "Resume",         "Play from start",
                                       "Play", "Mark as played", "Mark as unplayed"};
  snprintf(r.title, sizeof(r.title), "%s", LABELS[a]);
  if (a == PodRow::ACT_RESUME)
    podfmt::duration(st.get(entity).position, r.status, sizeof(r.status));
}

PodRow PodcastMenu::row(int i) const {
  PodRow r;
  memset(&r, 0, sizeof(r));
  r.mark = PodcastState::MARK_NONE;
  if (i < 0 || i >= count)
    return r;
  if (scr == ACTIONS) {
    buildActionRow(r, i);
    return r;
  }
  if (i < contRows) {
    buildEpisodeRow(r, (uint32_t)contEpisode);
    PodcastState::Entry e = st.get((uint32_t)contEpisode);
    uint32_t length = src.podcastEpisodeDuration((uint32_t)contEpisode);
    r.kind = PodRow::CONTINUE;
    r.number[0] = '\0';
    r.mark = PodcastState::MARK_PROGRESS;
    r.dim = false;
    r.tone = PodRow::TONE_ACCENT;
    podfmt::timeLeft(length > e.position ? length - e.position : 0, r.status,
                     sizeof(r.status));
    return r;
  }
  i -= contRows;
  if (i < subRows) {
    buildFolderRow(r, src.podcastSubfolderAt(entity, (uint32_t)i));
    return r;
  }
  buildEpisodeRow(r, src.podcastOwnEpisodeAt(entity, (uint32_t)(i - subRows)));
  return r;
}

int PodcastMenu::initialRow(PodcastSource &source, PodcastState &state,
                            uint32_t folder) {
  int subs = (int)source.podcastSubfolderCount(folder);
  uint32_t n = source.podcastOwnEpisodeCount(folder);
  for (uint32_t i = 0; i < n; i++) {
    if (!state.get(source.podcastOwnEpisodeAt(folder, i)).played())
      return subs + (int)i;
  }
  return 0;
}

PodcastMenu::Nav PodcastMenu::stay() {
  Nav n;
  memset(&n, 0, sizeof(n));
  n.kind = Nav::STAY;
  return n;
}

PodcastMenu::Nav PodcastMenu::push(Route_t::RouteType type, uint32_t id,
                                   int selected) {
  Nav n = stay();
  n.kind = Nav::PUSH;
  n.route = type;
  n.entity = id;
  n.selected = selected;
  if (type == Route_t::PODCAST_ACTIONS)
    src.podcastEpisodeTitle(id, navTitle, sizeof(navTitle));
  else
    src.podcastFolderName(id, navTitle, sizeof(navTitle));
  n.title = navTitle;
  return n;
}

PodcastMenu::Nav PodcastMenu::click(Route_t &route) {
  sync(route);
  if (!selectable(sel))
    return stay();
  PodRow r = row(sel);
  Nav n = stay();
  switch (r.kind) {
  case PodRow::FOLDER:
    return push(Route_t::PODCAST_FOLDER, r.id, initialRow(src, st, r.id));
  case PodRow::CONTINUE:
  case PodRow::EPISODE:
    n.kind = Nav::PLAY;
    n.episode = r.id;
    // A played episode starts over; a new or started one resumes.
    n.fromStart = r.mark == PodcastState::MARK_NONE;
    return n;
  case PodRow::ACTION:
    switch (r.action) {
    case PodRow::ACT_RESUME:
    case PodRow::ACT_PLAY_START:
    case PodRow::ACT_PLAY:
      n.kind = Nav::PLAY;
      n.episode = r.id;
      n.fromStart = r.action != PodRow::ACT_RESUME;
      return n;
    case PodRow::ACT_MARK_PLAYED:
    case PodRow::ACT_MARK_UNPLAYED:
      st.markPlayed(r.id, r.action == PodRow::ACT_MARK_PLAYED);
      n.kind = Nav::BACK;
      return n;
    }
    break;
  }
  return n;
}

PodcastMenu::Nav PodcastMenu::hold(Route_t &route) {
  sync(route);
  if (scr == ACTIONS || !selectable(sel))
    return stay();
  PodRow r = row(sel);
  if (r.kind != PodRow::EPISODE && r.kind != PodRow::CONTINUE)
    return stay();
  return push(Route_t::PODCAST_ACTIONS, r.id, 0);
}

static uint32_t fnv(uint32_t h, const void *data, size_t n) {
  const uint8_t *p = (const uint8_t *)data;
  for (size_t i = 0; i < n; i++)
    h = (h ^ p[i]) * 16777619u;
  return h;
}

uint32_t PodcastMenu::chunkHash(int chunk) const {
  uint32_t h = 2166136261u;
  if (chunk == 0)
    return fnv(h, heading, strlen(heading));
  // Every row chunk carries the scrollbar column.
  h = fnv(h, &top, sizeof(top));
  h = fnv(h, &count, sizeof(count));
  int i = top + chunk - 1;
  if (i >= count)
    return fnv(h, &chunk, sizeof(chunk)) ^ (empty() ? 0x55u : 0);
  PodRow r = row(i);
  bool hi = i == sel;
  h = fnv(h, &hi, sizeof(hi));
  h = fnv(h, &r.kind, 8); // kind .. hasProgress
  h = fnv(h, &r.progress, sizeof(r.progress));
  h = fnv(h, r.title, strlen(r.title));
  h = fnv(h, r.number, strlen(r.number));
  h = fnv(h, r.status, strlen(r.status));
  return h;
}

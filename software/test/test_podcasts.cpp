// Podcasts: the tables the syncer writes (syncer/crates/core/src/podcasts),
// the listening state the device keeps, the screens' model, and playback.
#include "../src/state/app_toast.h"
#include "../src/state/podcast_menu.h"
#include "../src/state/podcast_state.h"
#include "../src/state/state.h"
#include "../src/storage/Music_lookup.h"
#include "test_helpers.h"
#include <cstring>
#include <doctest.h>

namespace {

using openpod_test::FakeFs;
using openpod_test::pushU32;

const uint32_t GENERATION = 7;
const uint32_t NONE = 0xFFFFFFFFu;

std::vector<uint8_t> dense(const std::vector<uint32_t> &values) {
  std::vector<uint8_t> out;
  pushU32(out, (uint32_t)values.size());
  for (uint32_t v : values)
    pushU32(out, v);
  return out;
}

std::vector<uint8_t> strings(const std::vector<std::string> &values) {
  std::vector<std::pair<uint32_t, std::string>> entries;
  for (size_t i = 0; i < values.size(); i++)
    entries.push_back(std::make_pair((uint32_t)i, values[i]));
  return openpod_test::buildStringIndex(entries);
}

// The tree, ids in display order (as the syncer numbers them):
//   0 Podcasts
//   1   France Culture
//   2     Les chemins (dated): episodes 0 (12 Oct), 1 (10 Oct), 2 (9 Oct)
//   3     Vargas (series):     episodes 3 (1/3), 4 (2/3), 5 (3/3)
const uint32_t OCT12 = 20738; // 2026-10-12, a Monday

void registerPodcasts(uint32_t stateGeneration = GENERATION) {
  openpod_test::registerSampleCatalog();
  auto &fs = FakeFs::get();
  fs.addFile("/openpod/podcast_folder_names.bin",
             strings({"Podcasts", "France Culture", "Les chemins", "Vargas"}));
  fs.addFile("/openpod/podcast_folder_parent.bin", dense({NONE, 0, 1, 1}));
  fs.addFile("/openpod/podcast_folder_flags.bin", dense({0, 0, 0, 1}));
  fs.addFile("/openpod/podcast_folder_span.bin",
             dense({0, 6, 0, 6, 0, 3, 3, 6}));
  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> children;
  children.push_back(std::make_pair(0u, std::vector<uint32_t>{1}));
  children.push_back(std::make_pair(1u, std::vector<uint32_t>{2, 3}));
  children.push_back(std::make_pair(2u, std::vector<uint32_t>{}));
  children.push_back(std::make_pair(3u, std::vector<uint32_t>{}));
  fs.addFile("/openpod/podcast_folder_children.bin",
             openpod_test::buildRelation(children));
  std::vector<std::pair<uint32_t, std::vector<uint32_t>>> episodes;
  episodes.push_back(std::make_pair(0u, std::vector<uint32_t>{}));
  episodes.push_back(std::make_pair(1u, std::vector<uint32_t>{}));
  episodes.push_back(std::make_pair(2u, std::vector<uint32_t>{0, 1, 2}));
  episodes.push_back(std::make_pair(3u, std::vector<uint32_t>{3, 4, 5}));
  fs.addFile("/openpod/podcast_folder_episodes.bin",
             openpod_test::buildRelation(episodes));
  fs.addFile("/openpod/podcast_episode_titles.bin",
             strings({"Tchekhov", "Simone Weil", "Arendt", "Le hetre",
                      "Marc, Mathias et Lucien", "Fin"}));
  fs.addFile("/openpod/podcast_episode_paths.bin",
             strings({"Podcasts/FC/Chemins/a.mp3", "Podcasts/FC/Chemins/b.mp3",
                      "Podcasts/FC/Chemins/c.mp3", "Podcasts/FC/Vargas/1.mp3",
                      "Podcasts/FC/Vargas/2.mp3", "Podcasts/FC/Vargas/3.mp3"}));
  std::vector<std::pair<uint32_t, uint16_t>> durations;
  for (uint32_t i = 0; i < 6; i++)
    durations.push_back(std::make_pair(i, (uint16_t)(i < 3 ? 3480 : 1740)));
  fs.addFile("/openpod/podcast_episode_durations.bin",
             openpod_test::buildDurationIndex(durations));
  fs.addFile("/openpod/podcast_episode_folder.bin", dense({2, 2, 2, 3, 3, 3}));
  fs.addFile("/openpod/podcast_episode_date.bin",
             dense({OCT12, OCT12 - 2, OCT12 - 3, 0, 0, 0}));
  fs.addFile("/openpod/podcast_episode_number.bin",
             dense({0, 0, 0, 1 | 3 << 16, 2 | 3 << 16, 3 | 3 << 16}));
  std::vector<uint8_t> covers;
  pushU32(covers, 4);
  for (int i = 0; i < 4; i++) {
    pushU32(covers, i == 2 ? 51200 : 0);
    pushU32(covers, 51200);
    covers.push_back(1);
  }
  fs.addFile("/openpod/podcast_folder_to_cover.bin", covers);

  std::vector<uint8_t> meta = {'O', 'P', 'P', 'I', 1, 0, 0, 0};
  pushU32(meta, 6);
  pushU32(meta, GENERATION);
  fs.addFile("/openpod/podcast_meta.bin", meta);

  std::vector<uint8_t> st = {'O', 'P', 'P', 'S', 1, 0, 20, 0};
  pushU32(st, 6);
  pushU32(st, stateGeneration);
  for (uint32_t i = 0; i < 6; i++) {
    pushU32(st, 0x1000 + i); // key, low half
    pushU32(st, 0);          // key, high half
    pushU32(st, 0);          // position
    pushU32(st, 0);          // last played
    pushU32(st, 0);          // flags
  }
  fs.addFile(PodcastState::PATH, st);
}

// Record `episode`'s (position, last played, flags) as on the card.
void cardRecord(uint32_t episode, uint32_t &pos, uint32_t &seq,
                uint32_t &flags) {
  const std::vector<uint8_t> &b = *FakeFs::get().files[PodcastState::PATH];
  auto at = [&](size_t o) {
    return (uint32_t)b[o] | (uint32_t)b[o + 1] << 8 | (uint32_t)b[o + 2] << 16 |
           (uint32_t)b[o + 3] << 24;
  };
  size_t o = 16 + episode * 20 + 8;
  pos = at(o);
  seq = at(o + 4);
  flags = at(o + 8);
}

struct Fixture {
  MusicLookup lookup;
  PodcastState listening;
  SdFat sd;

  explicit Fixture(uint32_t stateGeneration = GENERATION) {
    openpod_test::resetAll();
    registerPodcasts(stateGeneration);
    REQUIRE(lookup.init());
    listening.load(sd, lookup.getLastPSRAMAddress(), lookup.podcastEpisodeCount(),
                   lookup.podcastGeneration());
  }
};

Route_t route(Route_t::RouteType type, uint32_t entity = 0, int selected = 0) {
  Route_t r;
  memset(&r, 0, sizeof(r));
  r.type = type;
  r.entityId = entity;
  r.selectedIndex = selected;
  return r;
}

} // namespace

TEST_CASE("Podcasts - formatting") {
  char b[32];
  podfmt::duration(1720, b, sizeof(b));
  CHECK(std::string(b) == "28:40");
  podfmt::duration(3723, b, sizeof(b));
  CHECK(std::string(b) == "1:02:03");
  podfmt::timeLeft(2053, b, sizeof(b));
  CHECK(std::string(b) == "-34:13");
  podfmt::shortDate(OCT12, b, sizeof(b));
  CHECK(std::string(b) == "12 Oct");
  podfmt::longDate(OCT12, b, sizeof(b));
  CHECK(std::string(b) == "Mon, 12 Oct 2026");
  podfmt::longDate(0, b, sizeof(b));
  CHECK(std::string(b) == "");
}

TEST_CASE("Podcasts - MusicLookup reads the podcast tables") {
  Fixture f;
  MusicLookup &l = f.lookup;
  REQUIRE(l.hasPodcasts());
  CHECK(l.podcastGeneration() == GENERATION);
  CHECK(l.podcastFolderCount() == 4);
  CHECK(l.podcastEpisodeCount() == 6);
  char b[64];
  CHECK(l.podcastFolderName(1, b, sizeof(b)));
  CHECK(std::string(b) == "France Culture");
  CHECK(l.podcastFolderParent(0) == NONE);
  CHECK(l.podcastFolderParent(3) == 1);
  CHECK(l.podcastFolderIsSeries(3));
  CHECK_FALSE(l.podcastFolderIsSeries(2));
  CHECK(l.podcastSubfolderCount(1) == 2);
  CHECK(l.podcastSubfolderAt(1, 1) == 3);
  CHECK(l.podcastOwnEpisodeCount(3) == 3);
  CHECK(l.podcastOwnEpisodeAt(3, 1) == 4);
  CHECK(l.podcastEpisodeNumber(4) == (2 | 3 << 16));
  CHECK(l.podcastEpisodeDate(0) == OCT12);
  album_cover_entry_t cover;
  CHECK(l.getPodcastCoverEntry(2, cover));
  CHECK(cover.offset == 51200);

  // Episodes play through the music path with the podcast bit set.
  CHECK(l.getTrackPath(podcastTrackId(4), b, sizeof(b)));
  CHECK(std::string(b) == "Podcasts/FC/Vargas/2.mp3");
  CHECK(l.getTrackName(podcastTrackId(0), b, sizeof(b)));
  CHECK(std::string(b) == "Tchekhov");
  CHECK(l.getTrackDurationSeconds(podcastTrackId(3)) == 1740);
  // Music is untouched.
  CHECK(l.getTrackPath(1, b, sizeof(b)));
  CHECK(std::string(b) == "/music/artist_one/track_one.flac");
}

TEST_CASE("Podcasts - a card without podcast tables still loads") {
  openpod_test::resetAll();
  openpod_test::registerSampleCatalog();
  MusicLookup l;
  REQUIRE(l.init());
  CHECK_FALSE(l.hasPodcasts());
  CHECK(l.podcastEpisodeCount() == 0);
}

TEST_CASE("Podcasts - listening state rules and in-place writes") {
  Fixture f;
  PodcastState &s = f.listening;
  REQUIRE(s.isLoaded());
  CHECK(s.get(0).mark() == PodcastState::MARK_NEW);

  s.savePosition(0, 1427, 3480, false);
  CHECK(s.get(0).mark() == PodcastState::MARK_PROGRESS);
  uint32_t pos, seq, flags;
  cardRecord(0, pos, seq, flags);
  CHECK(pos == 1427);
  CHECK(seq == 1);
  CHECK(flags == 0);
  // The key stays where it was.
  CHECK((*FakeFs::get().files[PodcastState::PATH])[16] == 0x00);
  CHECK((*FakeFs::get().files[PodcastState::PATH])[17] == 0x10);

  // Pausing with less than a minute left counts as played.
  s.savePosition(1, 3430, 3480, true);
  CHECK(s.get(1).played());
  // Not when it's only the periodic save.
  s.savePosition(2, 3430, 3480, false);
  CHECK_FALSE(s.get(2).played());

  // Mark as played keeps the position, unplayed resets it.
  s.markPlayed(0, true);
  CHECK(s.get(0).played());
  CHECK(s.get(0).position == 1427);
  s.markPlayed(0, false);
  CHECK(s.get(0).mark() == PodcastState::MARK_NEW);
  cardRecord(0, pos, seq, flags);
  CHECK(pos == 0);
  CHECK(flags == 0);

  PodcastState::Rollup r = s.rollup(0, 3);
  CHECK(r.episodes == 3);
  CHECK(r.fresh == 1);
  CHECK(r.inProgress == 1);
  CHECK(r.mark() == PodcastState::MARK_NEW);
  CHECK(s.rollup(1, 2).mark() == PodcastState::MARK_NONE);

  // The continue row: the in-progress episode played last.
  CHECK(s.continueEpisode() == 2);
  s.savePosition(4, 60, 1740, false);
  CHECK(s.continueEpisode() == 4);
}

TEST_CASE("Podcasts - a state file from another sync is left alone") {
  Fixture f(GENERATION + 1);
  CHECK_FALSE(f.listening.isLoaded());
  CHECK(f.listening.get(0).mark() == PodcastState::MARK_NEW);
  f.listening.savePosition(0, 100, 3480, true);
  uint32_t pos, seq, flags;
  cardRecord(0, pos, seq, flags);
  CHECK(pos == 0);
  CHECK(f.listening.rollup(0, 6).fresh == 6);
}

TEST_CASE("Podcasts - menu rows") {
  Fixture f;
  PodcastMenu m(f.lookup, f.listening);

  Route_t root = route(Route_t::PODCASTS);
  m.sync(root);
  CHECK(std::string(m.title()) == "Podcasts");
  CHECK(m.rowCount() == 1);
  PodRow r = m.row(0);
  CHECK(r.kind == PodRow::FOLDER);
  CHECK(std::string(r.title) == "France Culture");
  CHECK(std::string(r.status) == "6 new");
  CHECK(r.mark == PodcastState::MARK_NEW);
  CHECK(r.chevron);

  // Something in progress: the continue row comes first.
  f.listening.savePosition(0, 1427, 3480, false);
  m.sync(root);
  CHECK(m.rowCount() == 2);
  r = m.row(0);
  CHECK(r.kind == PodRow::CONTINUE);
  CHECK(std::string(r.title) == "Tchekhov");
  CHECK(std::string(r.status) == "-34:13");
  CHECK(r.tone == PodRow::TONE_ACCENT);
  CHECK(r.hasProgress);
  CHECK(r.progress == 131);

  Route_t show = route(Route_t::PODCAST_FOLDER, 2);
  m.sync(show);
  CHECK(std::string(m.title()) == "Les chemins");
  CHECK(m.rowCount() == 3);
  r = m.row(0);
  CHECK(std::string(r.status) == "12 Oct");
  CHECK(r.mark == PodcastState::MARK_PROGRESS);
  CHECK(r.number[0] == '\0');

  f.listening.markPlayed(3, true);
  Route_t series = route(Route_t::PODCAST_FOLDER, 3);
  m.sync(series);
  r = m.row(0);
  CHECK(std::string(r.number) == "1/3");
  CHECK(std::string(r.title) == "Le hetre");
  CHECK(std::string(r.status) == "29:00");
  CHECK(r.dim);
  CHECK(r.mark == PodcastState::MARK_NONE);

  // Folder roll-ups follow the state.
  Route_t fc = route(Route_t::PODCAST_FOLDER, 1);
  m.sync(fc);
  CHECK(std::string(m.row(1).status) == "2 new");
  f.listening.markPlayed(4, true);
  f.listening.markPlayed(5, true);
  m.sync(fc);
  r = m.row(1);
  CHECK(std::string(r.status) == "3 ep");
  CHECK(r.dim);
  CHECK(r.mark == PodcastState::MARK_NONE);
}

TEST_CASE("Podcasts - menu navigation and actions") {
  Fixture f;
  PodcastMenu m(f.lookup, f.listening);
  f.listening.markPlayed(0, true);

  // A folder opens on its first unfinished episode.
  Route_t fc = route(Route_t::PODCAST_FOLDER, 1);
  PodcastMenu::Nav nav = m.click(fc);
  CHECK(nav.kind == PodcastMenu::Nav::PUSH);
  CHECK(nav.route == Route_t::PODCAST_FOLDER);
  CHECK(nav.entity == 2);
  CHECK(std::string(nav.title) == "Les chemins");
  CHECK(nav.selected == 1);

  Route_t show = route(Route_t::PODCAST_FOLDER, 2, 1);
  m.sync(show);
  CHECK(m.scroll(show, -1)); // down
  CHECK(show.selectedIndex == 2);
  CHECK_FALSE(m.scroll(show, -1)); // last row
  CHECK(m.scroll(show, 2));        // up, twice
  CHECK(show.selectedIndex == 0);

  // A played episode plays from the start.
  nav = m.click(show);
  CHECK(nav.kind == PodcastMenu::Nav::PLAY);
  CHECK(nav.episode == 0);
  CHECK(nav.fromStart);

  // Hold Center: the episode's actions.
  f.listening.savePosition(1, 1427, 3480, false);
  show.selectedIndex = 1;
  nav = m.hold(show);
  CHECK(nav.kind == PodcastMenu::Nav::PUSH);
  CHECK(nav.route == Route_t::PODCAST_ACTIONS);
  CHECK(nav.entity == 1);

  Route_t actions = route(Route_t::PODCAST_ACTIONS, 1);
  m.sync(actions);
  CHECK(std::string(m.title()) == "Simone Weil");
  REQUIRE(m.rowCount() == 5);
  CHECK(std::string(m.row(0).title) == "Resume");
  CHECK(std::string(m.row(0).status) == "23:47");
  CHECK(std::string(m.row(1).title) == "Play from start");
  CHECK(std::string(m.row(2).title) == "Mark as played");
  CHECK(m.row(3).kind == PodRow::INFO);
  CHECK(std::string(m.row(3).status) == "Sat, 10 Oct 2026");
  CHECK(std::string(m.row(4).status) == "58:00");
  // The cursor skips the read-only rows.
  m.scroll(actions, -5);
  CHECK(actions.selectedIndex == 2);

  nav = m.click(actions);
  CHECK(nav.kind == PodcastMenu::Nav::BACK);
  CHECK(f.listening.get(1).played());
  CHECK(f.listening.get(1).position == 1427);

  // A played episode: Play / Mark as unplayed.
  m.sync(actions);
  CHECK(m.rowCount() == 4);
  CHECK(std::string(m.row(1).title) == "Mark as unplayed");

  // New episode on a series: no Published row (no date).
  Route_t newOne = route(Route_t::PODCAST_ACTIONS, 5);
  m.sync(newOne);
  CHECK(m.rowCount() == 3);
  CHECK(std::string(m.row(0).title) == "Play");
}

TEST_CASE("Podcasts - playback resumes, saves and ends") {
  Fixture f;
  State state;
  state.setPodcasts(&f.lookup, &f.listening);
  uint8_t toasts = appToast.serial();

  // Resume: from the saved position minus 5 s, with a toast.
  f.listening.savePosition(4, 1427, 1740, false);
  state.startPodcast(4, false, f.lookup);
  CHECK(state.getCurrentRoute().type == Route_t::NOW_PLAYING);
  CHECK(state.isPlayingPodcast());
  CHECK(state.getPlayingTrackId() == (int)podcastTrackId(4));
  CHECK(state.getPlaybackPosition() == 1422);
  CHECK(appToast.serial() == (uint8_t)(toasts + 1));
  CHECK(std::string(appToast.last().text) == "Resumed at 23:47");
  CHECK(appToast.last().plain);

  // Every 30 s of progress, the position is saved; pausing saves too.
  state.setTrackDuration(1740);
  state.updateProgress(1440);
  CHECK(f.listening.get(4).position == 1427);
  state.updateProgress(1452);
  CHECK(f.listening.get(4).position == 1452);
  state.updateProgress(1460);
  state.togglePlayback();
  CHECK(f.listening.get(4).position == 1460);
  state.togglePlayback();

  // The series goes on with the next number.
  uint32_t next = 0;
  CHECK(state.getNextTrackId(f.lookup, next));
  CHECK(next == podcastTrackId(5));
  state.notifyTrackEnded(f.lookup);
  CHECK(f.listening.get(4).played());
  CHECK(state.getPlayingTrackId() == (int)podcastTrackId(5));
  CHECK(std::string(appToast.last().text) == "Finished 2/3 \xB7 playing 3/3");
  CHECK(appToast.last().tone == BtModel::TOAST_SUCCESS);

  // The last number: playback stops.
  state.notifyTrackEnded(f.lookup);
  CHECK(f.listening.get(5).played());
  CHECK_FALSE(state.isPlayingPodcast());
}

TEST_CASE("Podcasts - a dated episode stops and its list comes back") {
  Fixture f;
  State state;
  state.setPodcasts(&f.lookup, &f.listening);
  // Podcasts > Les chemins, cursor on the newest episode, played.
  state.pushMenuRoute(Route_t::PODCASTS, 0, "Podcasts", 0);
  state.pushMenuRoute(Route_t::PODCAST_FOLDER, 2, "Les chemins", 0);
  state.startPodcast(0, true, f.lookup);
  REQUIRE(state.getCurrentRoute().type == Route_t::NOW_PLAYING);
  uint32_t next = 0;
  CHECK_FALSE(state.getNextTrackId(f.lookup, next)); // dated: no chaining

  state.notifyTrackEnded(f.lookup);
  CHECK(f.listening.get(0).played());
  CHECK_FALSE(state.getIsPlaying());
  CHECK(state.getCurrentRoute().type == Route_t::PODCAST_FOLDER);
  CHECK(state.getCurrentRoute().selectedIndex == 1); // next unfinished
  CHECK(std::string(appToast.last().text) == "Finished \xB7 marked as played");
}

TEST_CASE("Podcasts - the played tail is 5% of the episode, 1 to 3 min") {
  CHECK(PodcastState::playedTail(600) == 60);
  CHECK(PodcastState::playedTail(1680) == 84);
  CHECK(PodcastState::playedTail(3600) == 180);
  CHECK(PodcastState::playedTail(7200) == 180);

  Fixture f;
  // A periodic save in the tail doesn't mark it; leaving there does.
  f.listening.savePosition(0, 3400, 3480, false);
  CHECK_FALSE(f.listening.get(0).played());
  f.listening.savePosition(0, 3400, 3480, true);
  CHECK(f.listening.get(0).played());
  // Further from the end: in progress.
  f.listening.savePosition(1, 3200, 3480, true);
  CHECK(f.listening.get(1).mark() == PodcastState::MARK_PROGRESS);
}

TEST_CASE("Podcasts - next/previous walk the folder's list") {
  Fixture f;
  State state;
  state.setPodcasts(&f.lookup, &f.listening);
  f.listening.savePosition(1, 600, 3480, false);

  // Les chemins: 0, 1, 2. Next in the tail: the left one counts as played,
  // a started one resumes.
  state.startPodcast(0, true, f.lookup);
  state.setTrackDuration(3480);
  state.updateProgress(3400);
  CHECK(state.nextTrack(f.lookup));
  CHECK(f.listening.get(0).played());
  CHECK(state.getPlayingTrackId() == (int)podcastTrackId(1));
  CHECK(state.getPlaybackPosition() == 595);
  CHECK(std::string(appToast.last().text) == "Resumed at 10:00");

  CHECK(state.nextTrack(f.lookup));
  CHECK(state.getPlayingTrackId() == (int)podcastTrackId(2));
  CHECK(f.listening.get(1).position == 595);
  CHECK_FALSE(state.nextTrack(f.lookup)); // end of the list
  CHECK(state.getPlayingTrackId() == (int)podcastTrackId(2));

  // Previous: at the start, the episode before; later on, a restart.
  CHECK(state.previousTrack(f.lookup));
  CHECK(state.getPlayingTrackId() == (int)podcastTrackId(1));
  state.updateProgress(700);
  CHECK(state.previousTrack(f.lookup));
  CHECK(state.getPlayingTrackId() == (int)podcastTrackId(1));
  CHECK(state.getPlaybackPosition() == 0);
}

#include "app.h"
#include "../logging.h"
#include "../sound/AudioProfiler.h"
#include "../state/fast_scroll.h"

// Test bench console (software/bench/ drives it over the debug serial, see
// docs/tools/bench.md). Commands arrive from app/console.cpp without their
// leading '#'; every answer is one line starting with "#ok <cmd>" or
// "#err <cmd>", so the host can pick them out of the regular logs.
//
// Input commands (click/long/scroll) go through the wheel's event queue, so
// they take exactly the path of a finger. They answer once the event was
// handled and the UI has nothing left to draw (no animation, no navigation
// transition), with the time that took.
//
// Gesture commands (spin/hold/gesture) go one level lower: a virtual finger
// stands in for the wheel's sensors (ClickWheel::setVirtual), so debounce,
// the scroll lift-off hold, long-press timing and the fast-scroll window all
// run as with a real finger. The gesture is played here, on the board, so its
// timing doesn't depend on the USB link. They answer like the input commands,
// timed from the gesture's end. Every wheel event the UI handles, whatever
// made it, is echoed as "#ev <type> ..." for the host to see.
//
// The profiler counts the console as BENCH overhead, except the work a
// command runs on the spot (a search, a seek): that goes to UI or AUDIO.
#if OPENPOD_BENCH

#ifndef OPENPOD_GIT_REV
#define OPENPOD_GIT_REV "unknown"
#endif

extern "C" char *sbrk(int incr);

static const uint32_t WAIT_TIMEOUT_MS = 10000;

// The input command waiting for the UI to settle (name[0] == 0: none).
static struct
{
  char name[8];
  uint32_t startUs;
  uint32_t startMs;
  uint32_t loops;  // main-loop passes since the injection
  bool playing;    // a gesture is still being played (not timed yet)
  uint16_t events; // wheel events handled since the command
} pending = {"", 0, 0, 0, false, 0};

static const char *const BUTTON_NAMES[] = {"select", "menu", "next", "play",
                                           "prev"};
static const ClickWheel::Button BUTTONS[] = {
    ClickWheel::Button::Center, ClickWheel::Button::Top,
    ClickWheel::Button::Right, ClickWheel::Button::Bottom,
    ClickWheel::Button::Left};

// Boot timings, printed at the end of setup() and again on "#boot" (for a
// host that connects after the board booted, like the docs' live page).
static struct
{
  uint32_t ms, lookupMs, searchIndexMs;
} bootInfo = {0, 0, 0};

static void printBoot(const char *prefix)
{
  Serial.printf("%s ms=%lu lookup_ms=%lu search_index_ms=%lu rev=%s\n", prefix,
                (unsigned long)bootInfo.ms, (unsigned long)bootInfo.lookupMs,
                (unsigned long)bootInfo.searchIndexMs, OPENPOD_GIT_REV);
}

static bool parseButton(const char *name, ClickWheel::Button &out)
{
  for (size_t i = 0; i < sizeof(BUTTONS) / sizeof(BUTTONS[0]); i++)
  {
    if (!strcmp(name, BUTTON_NAMES[i]))
    {
      out = BUTTONS[i];
      return true;
    }
  }
  return false;
}

static void startPending(const char *name)
{
  strncpy(pending.name, name, sizeof(pending.name) - 1);
  pending.name[sizeof(pending.name) - 1] = 0;
  pending.startUs = micros();
  pending.startMs = millis();
  pending.playing = false;
  pending.events = 0;
  pending.loops = 0;
}

static uint32_t freeRam()
{
  char top;
  return (uint32_t)(&top - sbrk(0));
}

// Escapes nothing: names never contain '"' in the bench dataset, and the
// host only needs them to check where it is.
// The highlighted row's label: a search result's title on the search screen,
// otherwise the list element ("" on screens drawn from their own model).
static const char *selectedLabel()
{
  SearchModel &search = state.getSearch();
  if (state.getCurrentRoute().type == Route_t::SEARCH_RESULTS)
  {
    static char title[SearchModel::TITLE_LEN];
    static char sub[SearchModel::SUB_LEN];
    title[0] = '\0';
    if (search.hasSelection())
      search.describeRow(search.getSelected() - search.getFirst(), title, sub);
    return title;
  }
  const char *const *visible = state.getVisibleElements();
  int row = state.getRelativeSelectedIndex();
  if (visible && row >= 0 && row < ELEMENTS_PER_SCREEN && visible[row] &&
      visible[row][0])
    return visible[row];
  // Long lists (all tracks, ...) are drawn from the list renderer's own cache
  // and can leave the state's rows empty: name the selected entity instead.
  static char name[64];
  name[0] = 0;
  uint32_t id = state.getSelectedEntityId(musicLookup);
  if (id == UINT32_MAX)
    return "";
  switch (state.getCurrentRoute().type)
  {
  case Route_t::TRACKS:
    musicLookup.getTrackName(id, name, sizeof(name));
    break;
  case Route_t::ALBUMS:
    musicLookup.getAlbumName(id, name, sizeof(name));
    break;
  case Route_t::ARTISTS:
    musicLookup.getArtistName(id, name, sizeof(name));
    break;
  case Route_t::GENRES:
    musicLookup.getGenreName(id, name, sizeof(name));
    break;
  default:
    break;
  }
  return name;
}

static void printState()
{
  Route_t &route = state.getCurrentRoute();
  // The highlighted row's fast-scroll key at 3 letters (the overlay shows
  // its first `letters`).
  const char *label = selectedLabel();
  char key[4];
  fastScrollKey(label, 3, key);
  const char *playing = state.getPlayingTrackName();
  Serial.printf("#ok state route=%d sel=%d total=%d top=%d playing=%d "
                "track=%d pos=%d dur=%d vol=%d repeat=%d search=%d hits=%d "
                "fast=%d letters=%d seek=%d screen=%d anim=%d miss=%lu "
                "query=\"%s\" header=\"%s\" now=\"%s\" item=\"%s\" "
                "key=\"%s\"\n",
                (int)route.type, state.getSelectedIndex(),
                state.getTotalElements(), state.getTopVisibleIndex(),
                state.getIsPlaying() ? 1 : 0, state.getPlayingTrackId(),
                (int)player.audioPlayer.getDecodeTime(),
                (int)player.audioPlayer.getDurationSeconds(),
                state.getVolume(), state.getRepeatTrack() ? 1 : 0,
                (int)state.getSearch().getMode(),
                state.getSearch().getHitCount(),
                state.getFastScrollActive() ? 1 : 0,
                state.getFastScrollLetters(),
                state.getSeekModeActive() ? 1 : 0, screenOff ? 0 : 1,
                isNavigationBusy() ? 1 : 0,
                (unsigned long)pageStore.stats().misses,
                state.getSearch().getQuery(), state.getHeaderTitle(),
                playing ? playing : "", label, key);
}

static void printDataset()
{
  FsFile f = musicLookup.getSD().open("/opentone/bench.json", O_RDONLY);
  if (!f)
  {
    Serial.printf("#err dataset no /opentone/bench.json\n");
    return;
  }
  char buf[256];
  int n = f.read(buf, sizeof(buf) - 1);
  f.close();
  if (n < 0)
    n = 0;
  buf[n] = 0;
  for (int i = 0; i < n; i++)
  {
    if (buf[i] == '\n' || buf[i] == '\r')
      buf[i] = ' ';
  }
  Serial.printf("#ok dataset %s tracks_loaded=%lu albums_loaded=%lu "
                "artists_loaded=%lu genres_loaded=%lu\n",
                buf, (unsigned long)musicLookup.getTrackCount(),
                (unsigned long)musicLookup.getAlbumCount(),
                (unsigned long)musicLookup.getArtistCount(),
                (unsigned long)musicLookup.getGenreCount());
}

// Types `text` on the search screen like a user would: moves the letter
// ribbon to each character, then appends it (which runs the query and
// redraws the results). Answers with the time spent appending.
static void typeText(const char *text)
{
  SearchModel &search = state.getSearch();
  if (state.getCurrentRoute().type != Route_t::SEARCH_RESULTS ||
      search.getMode() != SearchModel::TYPING)
  {
    Serial.printf("#err type not on the search screen\n");
    return;
  }
  const char *chars = search.getEntry().chars();
  uint32_t work = audioprof::now();
  uint32_t totalUs = 0, maxUs = 0;
  int typed = 0;
  for (const char *p = text; *p; p++)
  {
    char c = toupper((unsigned char)*p);
    uint32_t t0 = micros();
    if (c == ' ')
    {
      state.searchSpace(musicLookup);
    }
    else
    {
      const char *at = strchr(chars, c);
      if (!at)
        continue; // not on the ribbon
      int delta = (int)(at - chars) - search.getCursor();
      // Negative clicks move the ribbon forward (see State::searchScroll).
      if (delta)
        state.searchScroll(-delta, musicLookup);
      t0 = micros();
      state.searchAppend(musicLookup);
    }
    uint32_t dt = micros() - t0;
    totalUs += dt;
    if (dt > maxUs)
      maxUs = dt;
    typed++;
  }
  audioprof::moveTo(audioprof::UI_UPDATE, work);
  Serial.printf("#ok type us=%lu max_us=%lu chars=%d hits=%d\n",
                (unsigned long)totalUs, (unsigned long)maxUs, typed,
                search.getHitCount());
}

static void clearQuery()
{
  int n = 0;
  uint32_t work = audioprof::now();
  while (state.getSearch().getEntry().length() > 0 && n++ < 64)
    state.searchBackspace(musicLookup);
  audioprof::moveTo(audioprof::UI_UPDATE, work);
  Serial.printf("#ok clear\n");
}

// --- Virtual finger ------------------------------------------------------

// One gesture keyframe: the finger's angle (unwrapped degrees, so a spin can
// go past 360; NO_FINGER = lifted) and the buttons held, from `at` ms after
// the start. The angle is interpolated between two touching keyframes.
struct Key
{
  uint32_t at;
  int32_t angle;
  uint8_t buttons;
};
static const int32_t NO_FINGER = INT32_MIN;
static const int MAX_KEYS = 24;
static const uint32_t MAX_GESTURE_MS = 20000;
// ClickWheel reads a turn the short way round, so one loop pass must move
// the finger less than 180 deg. Loop passes stretch to ~50 ms while the UI
// redraws a screen: 2000 deg/s keeps a step under 100 deg. A real flick is
// about 2 turns/s (720 deg/s).
static const uint32_t MAX_DEG_PER_S = 2000;
// The virtual finger hands the wheel back to the sensors after this long
// lifted with no button held.
static const uint32_t VIRTUAL_RELEASE_MS = 1500;

static struct
{
  Key keys[MAX_KEYS];
  int count;
  uint32_t startMs;
  bool playing;
  // What was last applied (by a gesture, or live by #finger / #btn).
  int32_t angle;
  uint8_t buttons;
  uint32_t idleSinceMs;
  uint32_t lastEchoMs;
  int16_t lastEcho;
} finger = {{}, 0, 0, false, NO_FINGER, 0, 0, 0, -1};

static int16_t wrapAngle(int32_t a)
{
  int32_t w = a % 360;
  return (int16_t)(w < 0 ? w + 360 : w);
}

static void applyFinger(int32_t angle, uint8_t buttons)
{
  finger.angle = angle;
  finger.buttons = buttons;
  wheel.setVirtual(angle == NO_FINGER ? -1 : wrapAngle(angle), buttons);
  finger.idleSinceMs = millis();
}

// "select+menu" -> button mask; false on an unknown name.
static bool parseButtons(const char *text, uint8_t &mask)
{
  mask = 0;
  char buf[48];
  strncpy(buf, text, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  char *save = nullptr;
  for (char *tok = strtok_r(buf, "+", &save); tok;
       tok = strtok_r(nullptr, "+", &save))
  {
    ClickWheel::Button b;
    if (!parseButton(tok, b))
      return false;
    mask |= 1 << (uint8_t)b;
  }
  return true;
}

static bool addKey(uint32_t at, int32_t angle, uint8_t buttons)
{
  if (finger.count >= MAX_KEYS)
    return false;
  finger.keys[finger.count++] = {at, angle, buttons};
  return true;
}

// Checks the keyframes and starts playing them as command `cmd`.
static void startGesture(const char *cmd)
{
  const Key *k = finger.keys;
  if (!finger.count || k[finger.count - 1].at > MAX_GESTURE_MS)
  {
    Serial.printf("#err %s needs 1 to %d keyframes within %lu ms\n", cmd,
                  MAX_KEYS, (unsigned long)MAX_GESTURE_MS);
    return;
  }
  for (int i = 1; i < finger.count; i++)
  {
    if (k[i].at < k[i - 1].at)
    {
      Serial.printf("#err %s keyframes out of order\n", cmd);
      return;
    }
    if (k[i].angle == NO_FINGER || k[i - 1].angle == NO_FINGER)
      continue;
    uint32_t deg = (uint32_t)abs(k[i].angle - k[i - 1].angle);
    uint32_t dt = k[i].at - k[i - 1].at;
    if (deg && deg * 1000 > MAX_DEG_PER_S * dt)
    {
      Serial.printf("#err %s faster than %lu deg/s\n", cmd,
                    (unsigned long)MAX_DEG_PER_S);
      return;
    }
  }
  startPending(cmd);
  pending.playing = true;
  finger.playing = true;
  finger.startMs = millis();
}

// "#spin <deg> <ms> [startDeg] [stillMs]": touch at startDeg, turn by deg
// (positive = counter-clockwise = scroll up, like #scroll) at a steady speed
// over ms, stay still stillMs (default 100, so the 80 ms lift-off hold lets
// the last clicks through), lift.
static void spin(const char *arg)
{
  long deg = 0, ms = 0, startDeg = 0, stillMs = 100;
  if (sscanf(arg, "%ld %ld %ld %ld", &deg, &ms, &startDeg, &stillMs) < 2 ||
      ms <= 0 || stillMs < 0)
  {
    Serial.printf("#err spin usage: spin <deg> <ms> [startDeg] [stillMs]\n");
    return;
  }
  const uint32_t TOUCH_MS = 30; // finger down before it moves
  finger.count = 0;
  addKey(0, startDeg, 0);
  addKey(TOUCH_MS, startDeg, 0);
  addKey(TOUCH_MS + ms, startDeg + deg, 0);
  addKey(TOUCH_MS + ms + stillMs, startDeg + deg, 0);
  addKey(TOUCH_MS + ms + stillMs, NO_FINGER, 0);
  startGesture("spin");
}

// "#hold <button[+button]> <ms>": holds the button(s) for ms (a click under
// 800 ms, a long press from 800, very long from 2000).
static void hold(const char *arg)
{
  char names[48] = "";
  long ms = 0;
  uint8_t mask = 0;
  if (sscanf(arg, "%47s %ld", names, &ms) < 2 || ms <= 0 ||
      !parseButtons(names, mask) || !mask)
  {
    Serial.printf("#err hold usage: hold <button[+button]> <ms>\n");
    return;
  }
  finger.count = 0;
  addKey(0, NO_FINGER, mask);
  addKey(ms, NO_FINGER, 0);
  startGesture("hold");
}

// "#gesture <ms>:<deg|up>[+button...] ...": raw keyframes, e.g.
// "0:90 200:180 260:195 260:up" (a spin with a twitch at lift-off) or
// "0:up+select 1000:up". Angles are unwrapped degrees.
static void gesture(const char *arg)
{
  finger.count = 0;
  char buf[160];
  strncpy(buf, arg, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = 0;
  char *save = nullptr;
  for (char *tok = strtok_r(buf, " ", &save); tok;
       tok = strtok_r(nullptr, " ", &save))
  {
    char *colon = strchr(tok, ':');
    if (!colon)
    {
      Serial.printf("#err gesture bad keyframe '%s'\n", tok);
      return;
    }
    *colon = 0;
    char *plus = strchr(colon + 1, '+');
    uint8_t mask = 0;
    if (plus)
    {
      *plus = 0;
      if (!parseButtons(plus + 1, mask))
      {
        Serial.printf("#err gesture unknown button in '%s'\n", plus + 1);
        return;
      }
    }
    int32_t angle = strcmp(colon + 1, "up") ? atol(colon + 1) : NO_FINGER;
    if (!addKey((uint32_t)atol(tok), angle, mask))
    {
      Serial.printf("#err gesture more than %d keyframes\n", MAX_KEYS);
      return;
    }
  }
  startGesture("gesture");
}

void benchInputTick()
{
  if (finger.playing)
  {
    uint32_t t = millis() - finger.startMs;
    const Key *k = finger.keys;
    int i = 0;
    while (i + 1 < finger.count && k[i + 1].at <= t)
      i++;
    int32_t angle = k[i].angle;
    if (i + 1 < finger.count && angle != NO_FINGER &&
        k[i + 1].angle != NO_FINGER)
    {
      // k[i + 1].at > t >= k[i].at here, so no division by zero.
      angle += (int32_t)((int64_t)(k[i + 1].angle - k[i].angle) *
                         (int64_t)(t - k[i].at) /
                         (int64_t)(k[i + 1].at - k[i].at));
    }
    applyFinger(angle, k[i].buttons);
    if (t >= k[finger.count - 1].at)
    {
      // Played: from here on, time how long the UI takes to settle.
      finger.playing = false;
      pending.playing = false;
      pending.startUs = micros();
      pending.startMs = millis();
      pending.loops = 0;
    }
  }
  else if (wheel.isVirtual() && finger.angle == NO_FINGER && !finger.buttons &&
           millis() - finger.idleSinceMs > VIRTUAL_RELEASE_MS)
  {
    wheel.clearVirtual(); // back to the real wheel
  }

  // Where the finger is (real or virtual), for the host's wheel view, at
  // most ~30 times a second. -1 = lifted.
  int16_t a = wheel.currentAngle();
  if (a != finger.lastEcho && millis() - finger.lastEchoMs >= 33)
  {
    finger.lastEcho = a;
    finger.lastEchoMs = millis();
    Serial.printf("#ev finger a=%d t=%lu\n", a, (unsigned long)millis());
  }
}

void benchOnWheelEvent(const ClickWheel::Event &e)
{
  // ClickWheel::EventType order.
  static const char *const TYPES[] = {"down", "up",     "click", "long",
                                      "verylong", "scroll", "touch", "untouch"};
  uint8_t type = (uint8_t)e.type;
  if (type >= sizeof(TYPES) / sizeof(TYPES[0]))
    return;
  pending.events++;
  unsigned long t = millis();
  if (e.type == ClickWheel::EventType::Scroll)
    Serial.printf("#ev scroll v=%d t=%lu\n", e.value, t);
  else if (e.button < ClickWheel::Button::Count)
    Serial.printf("#ev %s b=%s t=%lu\n", TYPES[type],
                  BUTTON_NAMES[(uint8_t)e.button], t);
  else
    Serial.printf("#ev %s t=%lu\n", TYPES[type], t);
}

// Every scroll's speed and gear, and each shift down (v=0: on a pause or
// lift-off): the scroll tuning page plots them.
void benchOnScrollSpeed(int letters, uint16_t clicksPerSecond)
{
  Serial.printf("#ev fast l=%d v=%u t=%lu\n", letters,
                (unsigned)clicksPerSecond, millis());
}

// "#scrollcfg [s1=<c/s>] [s2=] [s3=] [win=<clicks>] [dwell=<ms>]": sets the
// fast-scroll gears (state/fast_scroll.h) and prints them.
static void scrollConfig(const char *arg)
{
  ScrollTuning &t = scrollTuning;
  char key[8];
  unsigned value;
  int n = 0;
  while (sscanf(arg, " %7[a-z0-9]=%u%n", key, &value, &n) == 2)
  {
    arg += n;
    if (!strcmp(key, "s1"))
      t.shift1 = (uint16_t)value;
    else if (!strcmp(key, "s2"))
      t.shift2 = (uint16_t)value;
    else if (!strcmp(key, "s3"))
      t.shift3 = (uint16_t)value;
    else if (!strcmp(key, "win") && value > 0 && value < 25)
      t.windowClicks = (uint16_t)value;
    else if (!strcmp(key, "dwell") && value > 0)
      t.dwellMs = (uint16_t)value;
    else
    {
      Serial.printf("#err scrollcfg unknown %s\n", key);
      return;
    }
  }
  Serial.printf("#ok scrollcfg s1=%u s2=%u s3=%u win=%u dwell=%u\n",
                t.shift1, t.shift2, t.shift3, t.windowClicks, t.dwellMs);
}

void benchCommand(const char *line)
{
  char cmd[16] = "";
  int consumed = 0;
  if (sscanf(line, "%15s %n", cmd, &consumed) < 1)
    return;
  const char *arg = line + consumed;

  if (pending.name[0] && strcmp(cmd, "state") && strcmp(cmd, "stats"))
  {
    Serial.printf("#err %s busy with %s\n", cmd, pending.name);
    return;
  }

  ClickWheel::Button button;
  if (!strcmp(cmd, "ping"))
  {
    Serial.printf("#ok ping\n");
  }
  else if (!strcmp(cmd, "version"))
  {
    Serial.printf("#ok version rev=%s\n", OPENPOD_GIT_REV);
  }
  else if (!strcmp(cmd, "boot"))
  {
    printBoot("#ok boot");
  }
  else if (!strcmp(cmd, "dataset"))
  {
    printDataset();
  }
  else if (!strcmp(cmd, "state"))
  {
    printState();
  }
  else if (!strcmp(cmd, "scrollcfg"))
  {
    scrollConfig(arg);
  }
  else if (!strcmp(cmd, "cache"))
  {
    // Index page cache (storage/PageStore.h); "#cache reset" zeroes it.
    if (!strcmp(arg, "reset"))
      pageStore.resetStats();
    const PageStore::Stats &st = pageStore.stats();
    Serial.printf("#ok cache slots=%u page=%lu hits=%lu misses=%lu miss_us=%lu "
                  "seek_us=%lu\n",
                  (unsigned)pageStore.slotCount(),
                  (unsigned long)PageStore::PAGE_SIZE, (unsigned long)st.hits,
                  (unsigned long)st.misses, (unsigned long)st.missUs,
                  (unsigned long)st.seekUs);
  }
  else if (!strcmp(cmd, "stats"))
  {
    Serial.printf("#ok stats uptime_ms=%lu free_ram=%lu stack_free_min=%lu "
                  "stack_used_max=%lu\n",
                  (unsigned long)millis(), (unsigned long)freeRam(),
                  (unsigned long)stackFreeMin(), (unsigned long)stackUsedMax());
  }
  else if (!strcmp(cmd, "click") || !strcmp(cmd, "long"))
  {
    if (!parseButton(arg, button))
    {
      Serial.printf("#err %s unknown button '%s'\n", cmd, arg);
      return;
    }
    ClickWheel::EventType type = cmd[0] == 'c'
                                     ? ClickWheel::EventType::Click
                                     : ClickWheel::EventType::LongPress;
    startPending(cmd);
    wheel.inject({type, button, 0});
  }
  else if (!strcmp(cmd, "scroll"))
  {
    int clicks = atoi(arg);
    if (clicks == 0)
    {
      Serial.printf("#err scroll needs a non-zero click count\n");
      return;
    }
    startPending(cmd);
    wheel.inject({ClickWheel::EventType::Scroll, ClickWheel::Button::Count,
                  (int16_t)clicks});
  }
  else if (!strcmp(cmd, "spin"))
  {
    spin(arg);
  }
  else if (!strcmp(cmd, "hold"))
  {
    hold(arg);
  }
  else if (!strcmp(cmd, "gesture"))
  {
    gesture(arg);
  }
  else if (!strcmp(cmd, "finger"))
  {
    // Live finger from the host: "#finger <deg>" or "#finger up".
    applyFinger(strcmp(arg, "up") ? atol(arg) : NO_FINGER, finger.buttons);
    Serial.printf("#ok finger\n");
  }
  else if (!strcmp(cmd, "btn"))
  {
    // Live button from the host: "#btn <button> <1|0>" (press/release).
    char name[16] = "";
    int down = 0;
    if (sscanf(arg, "%15s %d", name, &down) < 2 || !parseButton(name, button))
    {
      Serial.printf("#err btn usage: btn <button> <1|0>\n");
      return;
    }
    uint8_t bit = 1 << (uint8_t)button;
    applyFinger(finger.angle,
                down ? finger.buttons | bit : finger.buttons & ~bit);
    Serial.printf("#ok btn\n");
  }
  else if (!strcmp(cmd, "type"))
  {
    typeText(arg);
  }
  else if (!strcmp(cmd, "clear"))
  {
    clearQuery();
  }
  else if (!strcmp(cmd, "repeat"))
  {
    // "#repeat 1": the playing track loops (the background track during a
    // bench run). "#repeat 0" goes back to playing through the list.
    state.setRepeatTrack(atoi(arg) != 0);
    Serial.printf("#ok repeat on=%d\n", state.getRepeatTrack() ? 1 : 0);
  }
  else if (!strcmp(cmd, "seek"))
  {
    uint32_t work = audioprof::now();
    uint32_t t0 = micros();
    bool ok = player.audioPlayer.seekToSeconds((uint32_t)atoi(arg));
    uint32_t dt = micros() - t0;
    audioprof::moveTo(audioprof::AUDIO_LOOP, work);
    if (ok)
      Serial.printf("#ok seek us=%lu\n", (unsigned long)dt);
    else
      Serial.printf("#err seek failed\n");
  }
  else
  {
    Serial.printf("#err %s unknown command\n", cmd);
  }
}

// Whether the wheel's debounced buttons have caught up with the virtual
// finger's: a gesture ending on a release is played before the wheel
// reports it (20 ms debounce), and its up/click must come before the answer.
static bool buttonsSettled()
{
  if (!wheel.isVirtual())
    return true;
  for (uint8_t b = 0; b < ClickWheel::BUTTON_COUNT; b++)
  {
    bool want = finger.buttons & (1 << b);
    if (wheel.isPressed((ClickWheel::Button)b) != want)
      return false;
  }
  return true;
}

void benchTick(bool uiBusy)
{
  if (!pending.name[0] || pending.playing)
    return;
  pending.loops++;
  bool settled = pending.loops >= 2 && wheel.pendingEvents() == 0 &&
                 buttonsSettled() && !uiBusy && !isNavigationBusy() &&
                 !state.getIsAnimating();
  if (settled)
  {
    Serial.printf("#ok %s us=%lu events=%u\n", pending.name,
                  (unsigned long)(micros() - pending.startUs), pending.events);
    pending.name[0] = 0;
  }
  else if (millis() - pending.startMs > WAIT_TIMEOUT_MS)
  {
    Serial.printf("#err %s timeout\n", pending.name);
    pending.name[0] = 0;
  }
}

void benchBootDone(uint32_t lookupMs, uint32_t searchIndexMs)
{
  bootInfo.ms = millis();
  bootInfo.lookupMs = lookupMs;
  bootInfo.searchIndexMs = searchIndexMs;
  printBoot("#boot");
}

#else

void benchCommand(const char *line)
{
  Serial.printf("#err bench disabled (build with -e bench)\n");
}
void benchTick(bool) {}
void benchBootDone(uint32_t, uint32_t) {}
void benchInputTick() {}
void benchOnWheelEvent(const ClickWheel::Event &) {}
void benchOnScrollSpeed(int, uint16_t) {}

#endif

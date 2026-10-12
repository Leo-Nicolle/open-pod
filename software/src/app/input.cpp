#include "app.h"
#include "../logging.h"

// Tracks whether a route-transition animation is currently playing, kept in
// sync via EVENT_ANIMATION_STARTED/FINISHED (see onNavigationStateEvent
// below) rather than polled from state.getIsAnimating() each tick.
static bool navigationBusy = false;

void onNavigationStateEvent(int eventType, void *eventData,
                            EventTarget *source)
{
  if (eventType == EVENT_ANIMATION_STARTED)
  {
    navigationBusy = true;
  }
  else if (eventType == EVENT_ANIMATION_FINISHED)
  {
    navigationBusy = false;
  }
}

bool isNavigationBusy() { return navigationBusy; }

// Now Playing seek-mode: active while the countdown below hasn't elapsed.
static unsigned long seekModeLastInteraction = 0;
static const unsigned long SEEK_MODE_DELAY = 3000; // 3 seconds
static const int SEEK_SECONDS_PER_CLICK = 3;

// Scrolling while seeking only previews a target position (moves the
// displayed time/progress bar); the actual file seek is deferred until
// scrolling pauses, so a long scrub doesn't re-seek the file on every tick.
static bool seekPending = false;
static int pendingSeekTarget = 0; // seconds, valid only while seekPending
static unsigned long lastSeekScrollTime = 0;
static const unsigned long SEEK_COMMIT_DELAY = 400; // ms of no scrolling

// Seek acceleration: the step doubles every SEEK_ACCEL_CLICKS clicks of one
// continuous scrub (24 clicks/turn), capped so the whole track spans
// SEEK_FULL_SWEEP_CLICKS clicks. A pause or a direction change starts a new,
// fine-grained scrub. Short tracks hit the cap at once, so songs keep the
// fixed SEEK_SECONDS_PER_CLICK step. Starting values - tune on hardware.
static const int SEEK_ACCEL_CLICKS = 8;
static const int SEEK_FULL_SWEEP_CLICKS = 60;
static const unsigned long SEEK_SCRUB_GAP_MS = 300;
static int seekScrubClicks = 0;
static int seekScrubDir = 0;

// Fast scrolling jumps a whole screen per tick; on short lists that overshoots,
// so it is only enabled when the list spans more than this many screens.
static const int FAST_SCROLL_MIN_PAGES = 3;

// Fast-scroll letter overlay (same long-list condition as above): shown once
// FAST_SCROLL_MIN_CLICKS wheel clicks land within FAST_SCROLL_WINDOW_MS (or a
// single multi-click tick), hidden FAST_SCROLL_HIDE_DELAY ms after the wheel
// slows down. Starting values - tune on hardware.
static const unsigned long FAST_SCROLL_WINDOW_MS = 300;
static const int FAST_SCROLL_MIN_CLICKS = 6;
static const unsigned long FAST_SCROLL_HIDE_DELAY = 600;
static unsigned long fastScrollWindowStart = 0;
static int fastScrollWindowClicks = 0;
static unsigned long lastFastScrollTime = 0;

static void onButtonClick(ClickWheel::Button button);
static void onButtonLongPress(ClickWheel::Button button);
static void onButtonVeryLongPress(ClickWheel::Button button);
static void onScroll(int scrollClicks);
static int seekStepSeconds(int scrollClicks, int duration);
static void onSearchButtonClick(ClickWheel::Button button);
static void onMenuButtonClick(ClickWheel::Button button);
static void onPodcastButtonClick(ClickWheel::Button button);
static void trackFastScroll(int scrollClicks);
static void commitPendingSeek();

void handleWheelInput()
{
  ClickWheel::Event e;
  while (wheel.pollEvent(e))
  {
    if (screenOff)
    {
      // Only a button wakes the screen, and that press does nothing else,
      // so a blind press never skips or pauses. Ignored touch/scroll don't
      // count as activity (nor throttle the audio refill).
      bool isPress = e.type == ClickWheel::EventType::Click ||
                     e.type == ClickWheel::EventType::LongPress ||
                     e.type == ClickWheel::EventType::VeryLongPress;
      if (isPress)
      {
        display.setDisplayOn(true);
        screenOff = false;
        lastUserInput = millis();
        haptics.buttonClick();
        LOG_CAT(USER_INPUT, "💡 Screen on");
      }
      continue;
    }
    lastUserInput = millis();
    player.audioPlayer.noteUserActivity();
    switch (e.type)
    {
    case ClickWheel::EventType::Click:
      haptics.buttonClick();
      onButtonClick(e.button);
      break;
    case ClickWheel::EventType::LongPress:
      onButtonLongPress(e.button);
      break;
    case ClickWheel::EventType::VeryLongPress:
      onButtonVeryLongPress(e.button);
      break;
    case ClickWheel::EventType::Scroll:
      haptics.scrollTick(e.value);
      onScroll(e.value);
      break;
    case ClickWheel::EventType::TouchStart:
      LOG_CAT(USER_INPUT, "👆 Wheel touch start");
      break;
    case ClickWheel::EventType::TouchEnd:
      LOG_CAT(USER_INPUT, "👋 Wheel touch end");
      break;
    default:
      break;
    }
  }
}

// Clicks fire on release (and not at all once a long press fired), so a
// center hold never also triggers a select.
//
// A route change (goToSelected/back) kicks off an 800ms slide animation
// that owns shared state (OpenPodUIEngine::lastDrawnOffset) for its whole
// duration. Starting a second one before the first finishes runs two
// animations against that same shared state at once and corrupts the
// display buffer / can hang the MCU - so drop navigation input while a
// transition is still playing (tracked via EVENT_ANIMATION_STARTED/
// FINISHED, see onNavigationStateEvent), rather than queuing/stacking it.
static void onButtonClick(ClickWheel::Button button)
{
  LOG_CAT(USER_INPUT, "🔘 Click: %s", ClickWheel::buttonName(button));

  if (state.getCurrentRoute().type == Route_t::RouteType::SEARCH_RESULTS)
  {
    onSearchButtonClick(button);
    return;
  }
  if (BtMenu::isMenuRoute(state.getCurrentRoute().type))
  {
    onMenuButtonClick(button);
    return;
  }
  if (PodcastMenu::isPodcastRoute(state.getCurrentRoute().type))
  {
    onPodcastButtonClick(button);
    return;
  }

  bool onNowPlaying =
      state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING;
  bool seeking = onNowPlaying && state.getSeekModeActive();

  switch (button)
  {
  case ClickWheel::Button::Center:
    // Enter menu / navigate forward, or toggle seek-mode on Now Playing
    if (onNowPlaying)
    {
      if (seeking)
      {
        commitPendingSeek(); // don't lose an in-flight scrub on exit
        state.exitSeekMode();
      }
      else
      {
        state.enterSeekMode();
        seekModeLastInteraction = millis();
        seekPending = false; // start each seek session from the real position
      }
    }
    else if (!navigationBusy)
    {
      state.goToSelected(musicLookup);
    }
    break;

  case ClickWheel::Button::Top:
    // Exit seek-mode if active, otherwise go back one level
    if (seeking)
    {
      commitPendingSeek(); // don't lose an in-flight scrub on exit
      state.exitSeekMode();
    }
    else if (!navigationBusy)
    {
      state.back(musicLookup);
    }
    break;

  case ClickWheel::Button::Bottom:
    if (onNowPlaying)
    {
      state.togglePlayback();
    }
    break;

  case ClickWheel::Button::Left:
  case ClickWheel::Button::Right:
    if (onNowPlaying)
    {
      if (seeking)
      {
        // The track is changing: drop the scrub instead of seeking into it
        seekPending = false;
        state.exitSeekMode();
      }
      if (button == ClickWheel::Button::Left)
      {
        // The periodic progress poll lags up to a second (and seek-mode
        // shows a preview position), so decide restart-vs-previous on the
        // real decode time. A restart resets it to 0, so a quick second
        // click (double-click) lands on the previous track.
        state.updateProgress((int)player.audioPlayer.getDecodeTime());
        state.previousTrack(musicLookup);
      }
      else
      {
        state.nextTrack(musicLookup);
      }
    }
    break;

  default:
    break;
  }
}

// Search screen. Typing: Center adds the ribbon letter, Left deletes one,
// Right adds a space, Bottom drops into the results, Top leaves search.
// Browsing results: Center opens the highlighted one, Top goes back to
// typing.
static void onSearchButtonClick(ClickWheel::Button button)
{
  bool typing = state.getSearch().getMode() == SearchModel::TYPING;
  switch (button)
  {
  case ClickWheel::Button::Center:
    if (typing)
      state.searchAppend(musicLookup);
    else if (!navigationBusy)
      state.goToSelected(musicLookup);
    break;
  case ClickWheel::Button::Top:
    if (!typing)
      state.searchExitResults();
    else if (!navigationBusy)
      state.back(musicLookup);
    break;
  case ClickWheel::Button::Left:
    if (typing)
      state.searchBackspace(musicLookup);
    break;
  case ClickWheel::Button::Right:
    if (typing)
      state.searchSpace(musicLookup);
    break;
  case ClickWheel::Button::Bottom:
    if (typing)
      state.searchEnterResults();
    break;
  default:
    break;
  }
}

static void applyMenuNav(const BtMenu::Nav &nav)
{
  switch (nav.kind)
  {
  case BtMenu::Nav::PUSH:
    state.pushMenuRoute(nav.route, nav.slot, nav.title, nav.selected);
    break;
  case BtMenu::Nav::BACK:
    state.back(musicLookup);
    break;
  case BtMenu::Nav::BACK_TO_LIST:
    state.backTo(Route_t::BT_LIST, musicLookup);
    break;
  default:
    break;
  }
}

// Settings and the Bluetooth screens. Center runs the highlighted row, Top
// goes back. On the rename screen (same keys as search): Center adds the
// ribbon letter, Left deletes one, Right adds a space, Bottom (play) saves,
// Top cancels. The UI picks up every change on its next refresh.
static void onMenuButtonClick(ClickWheel::Button button)
{
  Route_t &route = state.getCurrentRoute();
  btMenu.sync(route, millis());
  bool renaming = route.type == Route_t::BT_RENAME;
  switch (button)
  {
  case ClickWheel::Button::Center:
    if (renaming)
      btMenu.renameAppend();
    else if (!navigationBusy)
      applyMenuNav(btMenu.click(route, millis()));
    break;
  case ClickWheel::Button::Top:
    if (!navigationBusy)
      state.back(musicLookup);
    break;
  case ClickWheel::Button::Left:
    if (renaming)
      btMenu.renameBackspace();
    break;
  case ClickWheel::Button::Right:
    if (renaming)
      btMenu.renameSpace();
    break;
  case ClickWheel::Button::Bottom:
    if (renaming && !navigationBusy)
      applyMenuNav(btMenu.renameSave());
    break;
  default:
    break;
  }
}

static void applyPodcastNav(const PodcastMenu::Nav &nav)
{
  switch (nav.kind)
  {
  case PodcastMenu::Nav::PUSH:
    state.pushMenuRoute(nav.route, nav.entity, nav.title, nav.selected);
    break;
  case PodcastMenu::Nav::BACK:
    state.back(musicLookup);
    break;
  case PodcastMenu::Nav::PLAY:
    state.startPodcast(nav.episode, nav.fromStart, musicLookup);
    break;
  default:
    break;
  }
}

// The podcast screens: Center opens a folder or plays an episode (or runs
// an action), Top goes back. Hold Center is in onButtonLongPress.
static void onPodcastButtonClick(ClickWheel::Button button)
{
  Route_t &route = state.getCurrentRoute();
  switch (button)
  {
  case ClickWheel::Button::Center:
    if (!navigationBusy)
      applyPodcastNav(podcastMenu.click(route));
    break;
  case ClickWheel::Button::Top:
    if (!navigationBusy)
      state.back(musicLookup);
    break;
  default:
    break;
  }
}

static void onButtonLongPress(ClickWheel::Button button)
{
  // On a podcast episode, holding Center opens its actions.
  if (button == ClickWheel::Button::Center && !navigationBusy &&
      PodcastMenu::isPodcastRoute(state.getCurrentRoute().type))
  {
    PodcastMenu::Nav nav = podcastMenu.hold(state.getCurrentRoute());
    if (nav.kind != PodcastMenu::Nav::STAY)
    {
      applyPodcastNav(nav);
      return;
    }
  }
  if (button == ClickWheel::Button::Center && !navigationBusy)
  {
    LOG_CAT(USER_INPUT, "⏭️ Jumping to Now Playing");
    state.goToNowPlaying(musicLookup);
  }
}

static void onButtonVeryLongPress(ClickWheel::Button button)
{
  if (button == ClickWheel::Button::Center)
  {
    LOG_INFO("🔁 Recalibrating wheel baseline...");
    wheel.takeBaseline();
  }
}

// Scroll wheel: seek (seek-mode) / volume (Now Playing) / list navigation
static void onScroll(int scrollClicks)
{
  LOG_CAT(USER_INPUT, "🔄 Scroll: %d", scrollClicks);

  bool onNowPlaying =
      state.getCurrentRoute().type == Route_t::RouteType::NOW_PLAYING;
  if (onNowPlaying && state.getSeekModeActive())
  {
    // Scroll direction is reversed vs. list navigation: scrolling "up"
    // seeks backward, "down" seeks forward.
    int base = seekPending ? pendingSeekTarget
                           : (int)player.audioPlayer.getDecodeTime();
    int duration = (int)player.audioPlayer.getDurationSeconds();
    pendingSeekTarget = constrain(
        base - scrollClicks * seekStepSeconds(scrollClicks, duration), 0,
        duration);
    seekPending = true;
    state.updateProgress(pendingSeekTarget); // preview only, no file seek yet
    seekModeLastInteraction = millis();
    lastSeekScrollTime = millis();
  }
  else if (onNowPlaying)
  {
    // Reversed vs. list navigation, same as seeking above.
    for (int i = 0; i < abs(scrollClicks); i++)
    {
      if (scrollClicks < 0)
      {
        state.increaseVolume();
      }
      else
      {
        state.decreaseVolume();
      }
    }
  }
  else if (state.getCurrentRoute().type == Route_t::RouteType::SEARCH_RESULTS)
  {
    state.searchScroll(scrollClicks, musicLookup);
  }
  else if (PodcastMenu::isPodcastRoute(state.getCurrentRoute().type))
  {
    podcastMenu.scroll(state.getCurrentRoute(), scrollClicks);
  }
  else if (BtMenu::isMenuRoute(state.getCurrentRoute().type))
  {
    Route_t &route = state.getCurrentRoute();
    if (route.type == Route_t::BT_RENAME)
      btMenu.renameMove(-scrollClicks); // forward = down, as in search
    else
    {
      btMenu.sync(route, millis());
      btMenu.scroll(route, scrollClicks);
    }
  }
  else
  {
    // Before moving, so the scroll event already renders the overlay.
    trackFastScroll(scrollClicks);
    if (abs(scrollClicks) > 1 &&
        state.getTotalElements() > FAST_SCROLL_MIN_PAGES * ELEMENTS_PER_SCREEN)
    {
      if (scrollClicks < 0)
      {
        state.pageDown(); // Fast scroll down
      }
      else
      {
        state.pageUp(); // Fast scroll up
      }
    }
    else
    {
      // Handle multiple clicks at once for smooth scrolling
      for (int i = 0; i < abs(scrollClicks); i++)
      {
        if (scrollClicks < 0)
        {
          state.scrollDown();
        }
        else
        {
          state.scrollUp();
        }
      }
    }
  }
}

// Seconds per click for this scroll event (see SEEK_ACCEL_CLICKS). Call
// before lastSeekScrollTime is updated.
static int seekStepSeconds(int scrollClicks, int duration)
{
  int dir = scrollClicks < 0 ? -1 : 1;
  if (millis() - lastSeekScrollTime > SEEK_SCRUB_GAP_MS || dir != seekScrubDir)
    seekScrubClicks = 0;
  seekScrubDir = dir;
  int shift = min(seekScrubClicks / SEEK_ACCEL_CLICKS, 8);
  seekScrubClicks += abs(scrollClicks);
  int cap = max(SEEK_SECONDS_PER_CLICK, duration / SEEK_FULL_SWEEP_CLICKS);
  return min(SEEK_SECONDS_PER_CLICK << shift, cap);
}

// List scrolling: counts clicks in a FAST_SCROLL_WINDOW_MS window and shows
// the letter overlay on long lists once the wheel spins fast enough.
static void trackFastScroll(int scrollClicks)
{
  if (state.getTotalElements() <= FAST_SCROLL_MIN_PAGES * ELEMENTS_PER_SCREEN)
    return;
  unsigned long now = millis();
  if (now - fastScrollWindowStart > FAST_SCROLL_WINDOW_MS)
  {
    fastScrollWindowStart = now;
    fastScrollWindowClicks = 0;
  }
  fastScrollWindowClicks += abs(scrollClicks);
  if (abs(scrollClicks) > 1 || fastScrollWindowClicks >= FAST_SCROLL_MIN_CLICKS)
  {
    lastFastScrollTime = now;
    state.setFastScroll(true);
  }
}

void checkFastScrollTimeout()
{
  if (state.getFastScrollActive() &&
      millis() - lastFastScrollTime >= FAST_SCROLL_HIDE_DELAY)
  {
    state.setFastScroll(false);
  }
}

static void commitPendingSeek()
{
  if (seekPending)
  {
    player.audioPlayer.seekToSeconds((uint32_t)pendingSeekTarget);
    seekPending = false;
  }
}

void checkSeekModeTimeout()
{
  if (!state.getSeekModeActive())
  {
    return;
  }
  // Commit the actual file seek once scrolling has paused, rather than on
  // every scroll tick, so a long scrub doesn't repeatedly re-seek the file.
  if (seekPending && millis() - lastSeekScrollTime >= SEEK_COMMIT_DELAY)
  {
    commitPendingSeek();
  }
  if (millis() - seekModeLastInteraction > SEEK_MODE_DELAY)
  {
    commitPendingSeek(); // don't lose an in-flight scrub on idle-exit
    state.exitSeekMode();
  }
}

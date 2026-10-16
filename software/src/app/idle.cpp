#include "app.h"
#include "../logging.h"

// Idle: jump to Now Playing (only while playing), then blank the screen.
// While blank, only a button press wakes it (and is swallowed); touch and
// scroll are ignored.
unsigned long lastUserInput = 0;
static const unsigned long IDLE_NOW_PLAYING_DELAY = 5000;
static const unsigned long IDLE_SCREEN_OFF_DELAY = 15000;
bool screenOff = false;

// Audio progress tracking
static unsigned long lastProgressUpdate = 0;
static const unsigned long PROGRESS_UPDATE_INTERVAL = 1000; // 1 second

// Called every loop: lastUserInput only moves on input, so once on Now
// Playing (goToNowPlaying no-ops there) the jump fires once per idle period.
void checkIdle()
{
#if OPENPOD_BENCH
  // The bench drives the UI blind: no screen-off (whose waking press does
  // nothing) and no jump to Now Playing in the middle of a scenario.
  return;
#endif
  unsigned long idle = millis() - lastUserInput;
  if (!screenOff && idle >= IDLE_SCREEN_OFF_DELAY)
  {
    display.setDisplayOn(false);
    screenOff = true;
    LOG_CAT(USER_INPUT, "🌙 Screen off");
  }
  else if (idle >= IDLE_NOW_PLAYING_DELAY && state.getIsPlaying() &&
           !isNavigationBusy() &&
           state.getCurrentRoute().type != Route_t::RouteType::NOW_PLAYING)
  {
    LOG_CAT(USER_INPUT, "⏭️ Idle: jumping to Now Playing");
    state.goToNowPlaying(musicLookup);
  }
}

void updateAudioProgress()
{
  // Update progress periodically if playing. Skip while seek-mode is
  // active: the scroll handler already previews the pending target
  // position (bar + timestamp), and this periodic poll would otherwise
  // fight it - overwriting the preview with the real, not-yet-committed
  // decode time every second and making the bar jump back and forth.
  if (state.getIsPlaying() && !state.getSeekModeActive() &&
      !player.isResuming())
  {
    unsigned long now = millis();
    if (now - lastProgressUpdate >= PROGRESS_UPDATE_INTERVAL)
    {
      lastProgressUpdate = now;

      int currentPosition = player.audioPlayer.getDecodeTime();
      state.updateProgress(currentPosition); // This triggers
                                             // EVENT_PROGRESS_UPDATED
    }
  }
}

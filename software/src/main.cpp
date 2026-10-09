#include "Adafruit_MPR121.h"
#include "ClickWheel.hpp"
#include "Haptics.hpp"
#include "WheelTrace.hpp"
#include "pinout.h"
#include "rendering/ILI9341_GFX.h"
#include "sound/player.h"
#include "sound/AudioProfiler.h"
#include "sound/Spi1Bus.h"
#include "sound/LinkBench.hpp"
#include "state/state.h"
#include "storage/AlbumArt.h"
#include "storage/Music_index.h"
#include "storage/Music_lookup.h"
#include "ui/ui_engine.hpp"
#include <Arduino.h>
#include <Wire.h>
MusicIndex musicIndex;
MusicLookup musicLookup;
AlbumArt albumArt;
// Display and UI instances
ILI9341_GFX display;
PodPlayer player;
OpenPodUIEngine ui(&display);

// ClickWheel instances
TwoWire Wire3(PB4, PA8);
Adafruit_MPR121 cap = Adafruit_MPR121();
ClickWheel wheel(&cap);
Haptics haptics;
WheelTrace trace;

// Control settings
bool enableTeleplot = false;
bool enableTrace = false;
// Runs ILI9341_GFX::runDisplayDiagnostics() right after display.begin() and
// before the ClickWheel/MPR121 init (which halts setup() if that hardware
// isn't present) — lets the screen be bring-up tested on its own. See
// runDisplayDiagnostics() for what each phase checks.
bool enableDisplayDiagnostics = false;
// Prints, once a second, the buffer monitor line and the audio profiler line
// (time spent per link: SD, PSRAM, VS1053 clocking vs DREQ wait, UI; worst
// main-loop gap). Off by default: it's ~200 bytes of Serial per second.
bool enableAudioMonitor = true;
// Runs the SD / PSRAM / VS1053 link throughput bench at boot (see
// LinkBench.hpp), then continues normally.
bool enableLinkBench = false;
// The haptic motor's start-up kick draws a current spike from the 3.3V
// rail; turn this off to check whether it's what browns out the board.
bool enableHaptics = false;

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
unsigned long lastUIUpdate = 0;
const unsigned long UI_UPDATE_INTERVAL = 50; // 20Hz

// Now Playing seek-mode: active while the countdown below hasn't elapsed.
unsigned long seekModeLastInteraction = 0;
const unsigned long SEEK_MODE_DELAY = 3000; // 3 seconds
const int SEEK_SECONDS_PER_CLICK = 3;

// Scrolling while seeking only previews a target position (moves the
// displayed time/progress bar); the actual file seek is deferred until
// scrolling pauses, so a long scrub doesn't re-seek the file on every tick.
bool seekPending = false;
int pendingSeekTarget = 0; // seconds, valid only while seekPending
unsigned long lastSeekScrollTime = 0;
const unsigned long SEEK_COMMIT_DELAY = 400; // ms of no scrolling

// Fast scrolling jumps a whole screen per tick; on short lists that overshoots,
// so it is only enabled when the list spans more than this many screens.
const int FAST_SCROLL_MIN_PAGES = 3;

// Audio progress tracking
unsigned long lastProgressUpdate = 0;
const unsigned long PROGRESS_UPDATE_INTERVAL = 1000; // 1 second

// Function declarations
void handleWheelInput();
void onButtonClick(ClickWheel::Button button);
void onButtonLongPress(ClickWheel::Button button);
void onButtonVeryLongPress(ClickWheel::Button button);
void onScroll(int scrollClicks);
void checkSeekModeTimeout();
void commitPendingSeek();
void drawTraceVisualization();
bool shouldUpdateTelemetry();
void sendTelemetry();
void demoAnimations();
void updateAudioProgress();

// Why the MCU last (re)started, from RCC_CSR, then clears the flags. A fault
// hangs on this core rather than resetting, so an unexpected reboot is a
// real reset: pin (NRST / ST-Link), power (POR / brown-out) or software.
void printResetCause()
{
  uint32_t csr = RCC->CSR;
  Serial.print("Reset cause:");
  if (csr & RCC_CSR_LPWRRSTF) Serial.print(" low-power");
  if (csr & RCC_CSR_WWDGRSTF) Serial.print(" window-watchdog");
  if (csr & RCC_CSR_IWDGRSTF) Serial.print(" watchdog");
  if (csr & RCC_CSR_SFTRSTF) Serial.print(" software");
  if (csr & RCC_CSR_PORRSTF) Serial.print(" power-on");
  if (csr & RCC_CSR_PINRSTF) Serial.print(" reset-pin");
  if (csr & RCC_CSR_BORRSTF) Serial.print(" brown-out");
  Serial.println();
  RCC->CSR |= RCC_CSR_RMVF;
}

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    delay(10);

  Serial.println("=== OpenPod with ClickWheel (Event-Driven) ===");
  printResetCause();

  state.addEventListener(onNavigationStateEvent);

  audioprof::begin();

  // Initialize hardware
  player.setup();
  player.audioPlayer.setDebugMonitorEnabled(enableAudioMonitor);
  musicLookup.init();
  albumArt.init(musicLookup.getSD());
  if (enableLinkBench)
  {
    runLinkBench(player.audioPlayer, musicLookup);
  }
  display.begin();
  Serial.println("✅ Display initialized!");
  delay(100);
  if (enableDisplayDiagnostics)
  {
    display.runDisplayDiagnostics();
  }

  display.setupScroll(0, SCREEN_WIDTH, 0);

  Wire3.begin();
  // Fast mode: both the MPR121 and the DRV2605L support 400kHz, and it cuts
  // the per-loop wheel reads and haptic phase writes by ~4x.
  Wire3.setClock(400000);

  if (!cap.begin(MPR121_ADDR, &Wire3))
  {
    Serial.println("❌ MPR121 not found!");
    while (1)
      delay(1000);
  }

  if (!wheel.begin())
  {
    Serial.println("❌ ClickWheel initialization failed!");
    while (1)
      delay(1000);
  }
  wheel.setIrqPin(MPR121_IRQ);
  wheel.setButtonPins(BTN_CENTER, BTN_TOP, BTN_RIGHT, BTN_BOTTOM, BTN_LEFT);

  Serial.println("✅ ClickWheel initialized!");

  // Haptics are optional: the pod stays usable without the DRV2605L.
  if (enableHaptics && haptics.begin(&Wire3))
  {
    Serial.println("✅ Haptics initialized!");
  }
  else
  {
    Serial.println("❌ DRV2605L not found, haptics disabled");
  }
  Serial.println("Keep hands off wheel for baseline...");
  wheel.takeBaseline();
  wheel.setSensitivities(0.896, 1.0, 0.950, 0.913);
  // --- MusicIndex initialization and artist/track search ---
  if (!musicIndex.init(musicLookup.getLastPSRAMAddress()))
  {
    Serial.println("❌ Failed to initialize MusicIndex!");
  }
  delay(100);
  ui.begin();
  state.loadCurrentRouteData(musicLookup);

  Serial.println("🎮 Ready! Controls:");
  Serial.println("- Scroll: Navigate list / adjust volume / seek");
  Serial.println("- Center press: Select / enter Now Playing seek-mode");
  Serial.println("- Bottom press: Play/pause (Now Playing)");
  Serial.println("- Top press: Back / exit seek-mode");
  Serial.println("- Left/Right press: Previous/next track (Now Playing)");
  Serial.println("- Center hold 0.8s: Jump to Now Playing");
  Serial.println("- Center hold 2s: Recalibrate wheel");
  Serial.println("=====================================");
}

void loop()
{
  // Handle calibration mode
  if (wheel.isCalibrating())
  {
    spi1bus::Lock busLock; // I2C traffic, see wheel.update() below
    wheel.updateCalibration();
    return;
  }

  // Update wheel state
  {
    // The wheel's I2C traffic (MPR121) holds the SPI1 lock so the VS1053
    // feeder never sends during it: on the breadboard, I2C SCL (PA8) sits
    // next to the VS1053's XDCS (PA9), and overlapping traffic corrupted the
    // audio stream (glitches while touching the wheel). The feeder catches
    // up as soon as the lock is released.
    spi1bus::Lock busLock;
    wheel.update();
  }

  // Handle input
  handleWheelInput();
  checkSeekModeTimeout();

  audioprof::markLoop();
  if (navigationBusy)
  {
    player.audioPlayer.noteUserActivity(); // keep transitions smooth too
  }

  // Update UI (handles animations and event processing)
  uint32_t uiStart = audioprof::now();
  ui.update();
  audioprof::add(audioprof::UI_UPDATE, uiStart);

  // Feed audio data to the VS1053 (this is what actually makes sound)
  uint32_t audioStart = audioprof::now();
  player.loop();
  audioprof::add(audioprof::AUDIO_LOOP, audioStart);
  if (enableAudioMonitor)
  {
    audioprof::report();
  }

  // Update audio progress periodically
  updateAudioProgress();

  // Optional trace visualization
  if (enableTrace)
  {
    drawTraceVisualization();
  }

  // Optional telemetry
  if (enableTeleplot && shouldUpdateTelemetry())
  {
    sendTelemetry();
  }

  // No fixed delay: audioPlayer.loop() bounds its own feed work, and the
  // clickwheel/UI are polled every iteration. A delay(20) here only added to
  // the gap between VS1053 feeds (starving its FIFO).
}

void handleWheelInput()
{
  ClickWheel::Event e;
  while (wheel.pollEvent(e))
  {
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
      Serial.println("👆 Wheel touch start");
      break;
    case ClickWheel::EventType::TouchEnd:
      Serial.println("👋 Wheel touch end");
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
void onButtonClick(ClickWheel::Button button)
{
  Serial.print("🔘 Click: ");
  Serial.println(ClickWheel::buttonName(button));

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

void onButtonLongPress(ClickWheel::Button button)
{
  if (button == ClickWheel::Button::Center && !navigationBusy)
  {
    Serial.println("⏭️ Jumping to Now Playing");
    state.goToNowPlaying(musicLookup);
  }
}

void onButtonVeryLongPress(ClickWheel::Button button)
{
  if (button == ClickWheel::Button::Center)
  {
    Serial.println("🔁 Recalibrating wheel baseline...");
    wheel.takeBaseline();
  }
}

// Scroll wheel: seek (seek-mode) / volume (Now Playing) / list navigation
void onScroll(int scrollClicks)
{
  Serial.print("🔄 Scroll: ");
  Serial.println(scrollClicks);

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
        base - scrollClicks * SEEK_SECONDS_PER_CLICK, 0, duration);
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
  else if (abs(scrollClicks) > 1 &&
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

void commitPendingSeek()
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

void updateAudioProgress()
{
  // Update progress periodically if playing. Skip while seek-mode is
  // active: the scroll handler already previews the pending target
  // position (bar + timestamp), and this periodic poll would otherwise
  // fight it - overwriting the preview with the real, not-yet-committed
  // decode time every second and making the bar jump back and forth.
  if (state.getIsPlaying() && !state.getSeekModeActive())
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

void drawTraceVisualization()
{
  int angle = wheel.getWheelAngle();
  if (angle != -1)
  {
    trace.add(angle);
  }

  display.fillScreen(0x0000); // Black background
  trace.draw(display, 70, 120, 55);
  trace.drawDebugHUD(display, wheel, 140, 15);
}

bool shouldUpdateTelemetry()
{
  unsigned long now = millis();
  if (now - lastUIUpdate >= UI_UPDATE_INTERVAL)
  {
    lastUIUpdate = now;
    return true;
  }
  return false;
}

void sendTelemetry()
{
  // Send telemetry data for debugging
  return;
  Serial.print(">delta_top:");
  Serial.println(wheel.getDelta(ClickWheel::TOP));
  Serial.print(">delta_right:");
  Serial.println(wheel.getDelta(ClickWheel::RIGHT));
  Serial.print(">delta_bottom:");
  Serial.println(wheel.getDelta(ClickWheel::BOTTOM));
  Serial.print(">delta_left:");
  Serial.println(wheel.getDelta(ClickWheel::LEFT));

  Serial.print(">wheel_touched:");
  Serial.println(wheel.isWheelTouched() ? 200 : 0);
  Serial.print(">buttons_pressed:");
  Serial.println(wheel.pressedMask());
  Serial.print(">scroll_degrees:");
  Serial.println(wheel.getScrollDegrees());

  // Add state telemetry
  // Serial.print(">selected_track:");
  // Serial.println(state.getSelectedIndex());
  Serial.print(">playing_track:");
  Serial.println(state.getPlayingTrackIndex());
  Serial.print(">is_playing:");
  Serial.println(state.getIsPlaying() ? 100 : 0);
  Serial.print(">ui_state:");
  // Serial.println(state.getCurrentShowing());
}

void demoAnimations()
{
  Serial.println("🎬 Demo: Event-driven transitions...");

  Serial.println("✅ Demo complete - UI is now event-driven!");
}

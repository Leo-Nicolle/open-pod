#include "Adafruit_MPR121.h"
#include "bluetooth/Bluetooth.hpp"
#include "bluetooth/BtConfig.h"
#include "app/app.h"
#include "logging.h"
#include "pinout.h"
#include "sound/startup_sound.h"
#include "sound/AudioProfiler.h"
#include "sound/Spi1Bus.h"
#include "sound/LinkBench.hpp"
#include "storage/AlbumArt.h"
#include "storage/Music_index.h"
#include "storage/Music_lookup.h"
// The only includer of the UI engine, see app/app.h.
#include "ui/ui_engine.hpp"
#include "ui/boot_screen.hpp"
#include <Arduino.h>
#include <Wire.h>
// Debug/log switches (LOG_*, DEBUG_*, ENABLE_HAPTICS) are build flags, see
// logging.h.

MusicIndex musicIndex;
MusicLookup musicLookup;
AlbumArt albumArt;
// Display and UI instances
ILI9341_GFX display;
PodPlayer player;
OpenPodUIEngine ui(&display);
BootScreen boot(&display);

// ClickWheel instances
TwoWire Wire3(PB4, PA8);
Adafruit_MPR121 cap = Adafruit_MPR121();
ClickWheel wheel(&cap);
Haptics haptics;
// Bluetooth: the module's state (BtModel), its UART driver, the settings
// saved on the SD card, and the Settings/Bluetooth screens' model.
BtModel btModel;
Bluetooth bluetooth(btModel);
BtConfig btConfig;
BtMenu btMenu(btModel);
PodcastState podcastState;
PodcastMenu podcastMenu(musicLookup, podcastState);
// Boot step timings, reported to the test bench (app/bench.cpp).
static uint32_t lookupInitMs = 0;
static uint32_t searchIndexInitMs = 0;

void onConsoleAtLine(const char *line) { bluetooth.forwardConsoleLine(line); }

void setup()
{
  // Hold the VS1053 out of reset from the first instruction: left floating
  // until player.setup() (a few hundred ms in), XRESET can dip and reset the
  // chip, which thumps in the headphones (see VS1053_driver::begin()).
  digitalWrite(MP3_RESET, HIGH);
  pinMode(MP3_RESET, OUTPUT);

  Serial.begin(115200);
  while (!Serial)
    delay(10);

  LOG_INFO("=== OpenPod with ClickWheel (Event-Driven) ===");
  printResetCause();

  // First, so the module boots (~1.5 s) and reconnects to the last device
  // while the rest of the init runs; bluetooth.loop() finishes its setup.
  bluetooth.begin();
  // Headphones gone (off, out of range): pause, like unplugging wired ones.
  btModel.onLinkLost = [] {
    if (state.getIsPlaying())
      state.togglePlayback();
  };

  state.addEventListener(onNavigationStateEvent); // see app/input.cpp

  // Screen first: the title screen plays while the rest of the init runs in
  // its idle gaps (see ui/boot_screen.hpp).
  display.begin();
  LOG_INFO("✅ Display initialized!");
  delay(100);
  if (DEBUG_DISPLAY_DIAGNOSTICS)
  {
    display.runDisplayDiagnostics();
  }

  display.setupScroll(0, SCREEN_WIDTH, 0);
  boot.begin();

  boot.runStep([] {
    audioprof::begin();
    player.setup();
    // The VS1053 also streams to the Bluetooth module over I2S. Before the
    // startup sound, so it plays there too.
    player.audioPlayer.getDriver().setI2SOutput(true);
    player.audioPlayer.setDebugMonitorEnabled(LOG_ENABLED(AUDIO_BUFFER));
    // From flash, so it plays without an SD card. The clip fits the audio
    // fifo whole, so the feeder interrupt plays it out even while a long
    // init step blocks the loop.
    if (!player.audioPlayer.playClip(STARTUP_SOUND_MP3, STARTUP_SOUND_LEN))
    {
      LOG_ERROR("Startup sound not played");
    }
  });
  // Ends the clip's stream once it's sent (the hold below waits for that).
  boot.setIdle([] {
    player.audioPlayer.loop();
    bluetooth.loop();
  });

  boot.runStep([] {
    uint32_t t0 = millis();
    // Index pages load through the audio buffer's 4 KB chunk buffer: 8x
    // fewer card reads per page than 512 bytes at a time.
    pageStore.setScratch(player.audioPlayer.buffer().scratch(),
                         Audio_buffer::SCRATCH_SIZE);
    if (!musicLookup.init() && musicLookup.indexMissing())
      appToast.show(BtModel::TOAST_ERROR, "Library needs a ", "re-sync");
    lookupInitMs = millis() - t0;
    albumArt.init(musicLookup.getSD());
    // The listening state has its own PSRAM region; the index pages are
    // cached after it (PSRAM_controller.hpp).
    if (musicLookup.hasPodcasts())
      podcastState.load(musicLookup.getSD(), PODCAST_STATE_BASE_ADDRESS,
                        musicLookup.podcastEpisodeCount(),
                        musicLookup.podcastGeneration());
    state.setPodcasts(musicLookup.hasPodcasts() ? &musicLookup : nullptr,
                      &podcastState);
    btMenu.setStorageAvailable(btConfig.begin(musicLookup.getSD(), btModel));
    if (DEBUG_LINK_BENCH)
    {
      runLinkBench(player.audioPlayer, musicLookup);
    }
  });

  boot.runStep([] {
    Wire3.begin();
    // Fast mode: both the MPR121 and the DRV2605L support 400kHz, and it cuts
    // the per-loop wheel reads and haptic phase writes by ~4x.
    Wire3.setClock(400000);

    if (!cap.begin(MPR121_ADDR, &Wire3))
    {
      LOG_ERROR("MPR121 not found!");
      while (1)
        delay(1000);
    }

    if (!wheel.begin())
    {
      LOG_ERROR("ClickWheel initialization failed!");
      while (1)
        delay(1000);
    }
    wheel.setIrqPin(MPR121_IRQ);
    LOG_INFO("✅ ClickWheel initialized!");
  });

  boot.runStep([] {
    // Haptics are optional: the pod stays usable without the DRV2605L.
    if (ENABLE_HAPTICS && haptics.begin(&Wire3))
    {
      LOG_INFO("✅ Haptics initialized!");
    }
    else
    {
      LOG_WARN("DRV2605L not found, haptics disabled");
    }
  });

  boot.runStep([] {
    LOG_INFO("Keep hands off wheel for baseline...");
    wheel.takeBaseline();
    wheel.setSensitivities(0.896, 1.0, 0.950, 0.913);
  });

  boot.runStep([] {
    // --- MusicIndex initialization and artist/track search ---
    uint32_t t0 = millis();
    bool indexOk = musicIndex.init(musicLookup.getSearchTrie());
    searchIndexInitMs = millis() - t0;
    if (!indexOk)
    {
      LOG_ERROR("Failed to initialize MusicIndex!");
    }
    state.getSearch().setIndex(&musicIndex);
  });

  boot.runStep([] {
    ui.begin();
    state.loadCurrentRouteData(musicLookup);
  });

  // The title screen stays until the startup sound has played; a skip cuts it.
  boot.finish(ui, [] {
    if (!player.audioPlayer.isClipPlaying())
      return false;
    if (!boot.wasSkipped())
      return true;
    player.audioPlayer.stopPlaying();
    return false;
  });

  // The boot screen read the buttons directly; hand them to the wheel only
  // once released, so a press that skipped the intro isn't replayed as a click.
  boot.waitForRelease(1000);
  wheel.setButtonPins(BTN_CENTER, BTN_TOP, BTN_RIGHT, BTN_BOTTOM, BTN_LEFT);

  LOG_INFO("🎮 Ready! Controls:\n"
           "- Scroll: Navigate list / adjust volume / seek\n"
           "- Center press: Select / enter Now Playing seek-mode\n"
           "- Bottom press: Play/pause (Now Playing)\n"
           "- Top press: Back / exit seek-mode\n"
           "- Left/Right press: Previous/next track (Now Playing)\n"
           "- Center hold 0.8s: Jump to Now Playing\n"
           "- Center hold 2s: Recalibrate wheel\n"
           "- Idle 5s: Now Playing (if playing), 15s: screen off\n"
           "- Screen off: any button wakes it (scroll ignored)\n"
           "=====================================");
  lastUserInput = millis();
  benchBootDone(lookupInitMs, searchIndexInitMs);
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
  uint32_t loopStart = audioprof::now();

  // Update wheel state
  {
    audioprof::Span span(audioprof::WHEEL_UPDATE);
    // The wheel's I2C traffic (MPR121) holds the SPI1 lock so the VS1053
    // feeder never sends during it: on the breadboard, I2C SCL (PA8) sits
    // next to the VS1053's XDCS (PA9), and overlapping traffic corrupted the
    // audio stream (glitches while touching the wheel). The feeder catches
    // up as soon as the lock is released.
    spi1bus::Lock busLock;
    benchInputTick();
    wheel.update();
  }

  // Handle input
  {
    audioprof::Span span(audioprof::INPUT_HANDLING);
    handleWheelInput();
    checkSeekModeTimeout();
    checkFastScrollTimeout();
    checkIdle();
  }

  audioprof::markLoop();
  if (isNavigationBusy())
  {
    player.audioPlayer.noteUserActivity(); // keep transitions smooth too
  }

  // Update UI (handles animations and event processing)
  {
    audioprof::Span span(audioprof::UI_UPDATE);
    ui.update();
  }
  {
    audioprof::Span span(audioprof::BENCH_IO);
    benchTick(ui.isBusy());
  }

  // Feed audio data to the VS1053 (this is what actually makes sound)
  {
    audioprof::Span span(audioprof::AUDIO_LOOP);
    player.loop();
  }

  {
    audioprof::Span span(audioprof::MISC_WORK);
    // Update audio progress periodically
    updateAudioProgress();
  }

  {
    audioprof::Span span(audioprof::BENCH_IO);
    pollConsole();
  }
  {
    audioprof::Span span(audioprof::BT_LOOP);
    bluetooth.loop();
    btConfig.saveIfDirty(btModel);
  }

  {
    audioprof::Span span(audioprof::MISC_WORK);
    // Optional trace visualization
    if (DEBUG_WHEEL_TRACE)
    {
      drawTraceVisualization();
    }

    // Optional telemetry
    if (LOG_ENABLED(TELEMETRY) && shouldUpdateTelemetry())
    {
      sendTelemetry();
    }
  }

  audioprof::add(audioprof::MAIN_LOOP, loopStart);
  // Last, outside every span: it counts its own time in the next window.
  if (LOG_ENABLED(AUDIO_BUFFER))
  {
    audioprof::report();
  }

  // No fixed delay: audioPlayer.loop() bounds its own feed work, and the
  // clickwheel/UI are polled every iteration. A delay(20) here only added to
  // the gap between VS1053 feeds (starving its FIFO).
}

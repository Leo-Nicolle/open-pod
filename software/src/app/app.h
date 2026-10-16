#pragma once
// Glue between main.cpp (setup/loop) and the app/ files it calls into.
//
// main.cpp defines the hardware/global objects below and is the only
// translation unit that includes ui/ui_engine.hpp (and the other ui/*.hpp
// components): those headers define member functions outside their class
// without `inline`, so a second includer would break the link. Nothing in
// app/ touches the UI engine directly - it reacts to state events instead.
#include "../ClickWheel.hpp"
#include "../Haptics.hpp"
#include "../rendering/ILI9341_GFX.h"
#include "../sound/player.h"
#include "../state/bt_menu.h"
#include "../state/podcast_menu.h"
#include "../state/state.h"

extern ILI9341_GFX display;
extern PodPlayer player;
extern ClickWheel wheel;
extern Haptics haptics;
extern BtMenu btMenu;
extern PodcastMenu podcastMenu;
extern PodcastState podcastState;

// Idle tracking (app/idle.cpp), also moved/read by the input handlers.
extern unsigned long lastUserInput;
extern bool screenOff;

// input.cpp: wheel events -> navigation / playback / seek / volume.
void onNavigationStateEvent(int eventType, void *eventData,
                            EventTarget *source);
bool isNavigationBusy();
void handleWheelInput();
void checkSeekModeTimeout();
void checkFastScrollTimeout();

// idle.cpp
void checkIdle();
void updateAudioProgress();

// diagnostics.cpp
void printResetCause();
void drawTraceVisualization();
bool shouldUpdateTelemetry();
void sendTelemetry();

// console.cpp: reads the debug serial; "AT..." lines go to
// onConsoleAtLine() (main.cpp, the Bluetooth module), "#..." to the bench.
void pollConsole();
void onConsoleAtLine(const char *line);

// bench.cpp: test bench commands (only with -DOPENPOD_BENCH=1, see
// docs/tools/bench.md). benchTick() runs every loop, after the UI update.
void benchCommand(const char *line);
void benchTick(bool uiBusy);
void benchBootDone(uint32_t lookupMs, uint32_t searchIndexMs);
// Plays the virtual finger's gesture; runs every loop, before wheel.update().
void benchInputTick();
// Echoes a wheel event the UI is about to handle ("#ev ..." lines).
void benchOnWheelEvent(const ClickWheel::Event &e);

#pragma once
// Serial logging, switched on/off at build time from platformio.ini
// build_flags (e.g. -DLOG_LEVEL=1 -DLOG_UI=1). Disabled calls compile away
// (the conditions are constants) but still get their arguments type-checked.
//
// Log flags
// ---------
//   LOG_LEVEL         0 = silent (also mutes every category below)
//                     1 = errors and warnings
//                     2 = + info: boot progress, track changes, init
//                         summaries, on-demand dumps (printStats(),
//                         dumpRegisters(), display diagnostics, link bench,
//                         wheel calibration).                   default 2
//   LOG_AUDIO_BUFFER  Once-a-second buffer monitor + audio profiler line
//                     (time per link: SD, PSRAM, VS1053, UI; worst loop gap),
//                     track start timing, prefetch adopt, end-fill. ~200
//                     bytes of Serial per second.               default 1
//   LOG_VS1053        VS1053 clock / SPI speed / FLAC plugin setup details.
//                                                               default 0
//   LOG_INDEX_USAGE   MusicIndex / MusicLookup loading details (header
//                     sizes, load progress, relations).         default 0
//   LOG_USER_INPUT    Wheel clicks, scrolls, touches, long presses, screen
//                     on/off and idle jumps.                    default 0
//   LOG_UI            UI engine event chatter: scroll positions, list
//                     pages, route changes, animations.         default 0
//   LOG_TELEMETRY     Teleplot ">name:value" lines (wheel deltas, buttons,
//                     playback) at 20 Hz.                       default 0
//
// Debug features (not logs, but switched the same way)
// ---------------------------------------------------
//   DEBUG_WHEEL_TRACE          Draws the wheel touch trace + electrode HUD
//                              over the UI every loop.          default 0
//   DEBUG_DISPLAY_DIAGNOSTICS  Runs ILI9341_GFX::runDisplayDiagnostics()
//                              right after display.begin(), before the
//                              MPR121 init (which halts setup() if absent),
//                              so the screen can be bring-up tested alone.
//                                                               default 0
//   DEBUG_LINK_BENCH           Runs the SD / PSRAM / VS1053 throughput bench
//                              (sound/LinkBench.hpp) at boot.   default 0
//   ENABLE_HAPTICS             The haptic motor's start-up kick draws a
//                              current spike from the 3.3V rail; set to 0 to
//                              check whether it's what browns out the board.
//                                                               default 1
//
// Macros (printf-style; the format must be a string literal, a newline is
// appended). Output is ANSI-colored: errors red, warnings yellow, category
// tags cyan. Note: nano newlib's printf has no %f - print floats scaled to
// integers.
//   LOG_ERROR(fmt, ...)       "[E] ..."             LOG_LEVEL >= 1
//   LOG_WARN(fmt, ...)        "[W] ..."             LOG_LEVEL >= 1
//   LOG_INFO(fmt, ...)        "..."                 LOG_LEVEL >= 2
//   LOG_CAT(CAT, fmt, ...)    "[tag] ..."           LOG_LEVEL >= 1 && LOG_<CAT>
//                             e.g. LOG_CAT(UI, "Page changed")
//   LOG_ENABLED(CAT)          true when LOG_CAT(CAT, ...) would print, to
//                             guard work done only for logging.
//   LOG_RAW(cond, fmt, ...)   no prefix, no newline, LOG_LEVEL >= 1 && cond:
//                             for lines built in pieces and Teleplot output.
#include <Arduino.h>

#ifndef LOG_LEVEL
#define LOG_LEVEL 2
#endif
#ifndef LOG_AUDIO_BUFFER
#define LOG_AUDIO_BUFFER 1
#endif
#ifndef LOG_VS1053
#define LOG_VS1053 0
#endif
#ifndef LOG_INDEX_USAGE
#define LOG_INDEX_USAGE 0
#endif
#ifndef LOG_USER_INPUT
#define LOG_USER_INPUT 0
#endif
#ifndef LOG_UI
#define LOG_UI 0
#endif
#ifndef LOG_TELEMETRY
#define LOG_TELEMETRY 0
#endif

#ifndef DEBUG_WHEEL_TRACE
#define DEBUG_WHEEL_TRACE 0
#endif
#ifndef DEBUG_DISPLAY_DIAGNOSTICS
#define DEBUG_DISPLAY_DIAGNOSTICS 0
#endif
#ifndef DEBUG_LINK_BENCH
#define DEBUG_LINK_BENCH 0
#endif
#ifndef ENABLE_HAPTICS
#define ENABLE_HAPTICS 1
#endif

// Category tags, looked up by LOG_CAT(CAT, ...) as LOG_TAG_<CAT>.
#define LOG_TAG_AUDIO_BUFFER "audio"
#define LOG_TAG_VS1053 "vs1053"
#define LOG_TAG_INDEX_USAGE "index"
#define LOG_TAG_USER_INPUT "input"
#define LOG_TAG_UI "ui"
#define LOG_TAG_TELEMETRY "tele"

#define LOG_ANSI_RESET "\033[0m"
#define LOG_ANSI_RED "\033[31m"
#define LOG_ANSI_YELLOW "\033[33m"
#define LOG_ANSI_CYAN "\033[36m"

// Print::printf() isn't declared with the printf format attribute, so the
// compiler can't check the arguments against the format. logFormatCheck() is:
// it's never called (if (false) - the arguments aren't evaluated twice), only
// type-checked.
__attribute__((format(printf, 1, 2))) inline void logFormatCheck(const char *,
                                                                 ...) {}

#define LOG_PRINTF_IF(cond, fmt, ...)                                          \
  do {                                                                         \
    if (false)                                                                 \
      logFormatCheck(fmt, ##__VA_ARGS__);                                      \
    if (cond)                                                                  \
      Serial.printf(fmt, ##__VA_ARGS__);                                       \
  } while (0)

#define LOG_ERROR(fmt, ...)                                                    \
  LOG_PRINTF_IF(LOG_LEVEL >= 1,                                                \
                LOG_ANSI_RED "[E] " fmt LOG_ANSI_RESET "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)                                                     \
  LOG_PRINTF_IF(LOG_LEVEL >= 1,                                                \
                LOG_ANSI_YELLOW "[W] " fmt LOG_ANSI_RESET "\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)                                                     \
  LOG_PRINTF_IF(LOG_LEVEL >= 2, fmt "\n", ##__VA_ARGS__)
#define LOG_ENABLED(cat) (LOG_LEVEL >= 1 && LOG_##cat)
#define LOG_CAT(cat, fmt, ...)                                                 \
  LOG_PRINTF_IF(LOG_ENABLED(cat),                                   \
                LOG_ANSI_CYAN "[" LOG_TAG_##cat "] " LOG_ANSI_RESET fmt "\n",  \
                ##__VA_ARGS__)
#define LOG_RAW(cond, fmt, ...)                                                \
  LOG_PRINTF_IF(LOG_LEVEL >= 1 && (cond), fmt, ##__VA_ARGS__)

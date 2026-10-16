#include "app.h"
#include "../logging.h"
#if DEBUG_WHEEL_TRACE
#include "../WheelTrace.hpp"

static WheelTrace trace;
#endif

// Teleplot rate.
static unsigned long lastTelemetryUpdate = 0;
static const unsigned long TELEMETRY_INTERVAL = 50; // 20Hz

// Why the MCU last (re)started, from RCC_CSR, then clears the flags. A fault
// hangs on this core rather than resetting, so an unexpected reboot is a
// real reset: pin (NRST / ST-Link), power (POR / brown-out) or software.
void printResetCause()
{
  uint32_t csr = RCC->CSR;
  LOG_INFO("Reset cause:%s%s%s%s%s%s%s",
           (csr & RCC_CSR_LPWRRSTF) ? " low-power" : "",
           (csr & RCC_CSR_WWDGRSTF) ? " window-watchdog" : "",
           (csr & RCC_CSR_IWDGRSTF) ? " watchdog" : "",
           (csr & RCC_CSR_SFTRSTF) ? " software" : "",
           (csr & RCC_CSR_PORRSTF) ? " power-on" : "",
           (csr & RCC_CSR_PINRSTF) ? " reset-pin" : "",
           (csr & RCC_CSR_BORRSTF) ? " brown-out" : "");
  RCC->CSR |= RCC_CSR_RMVF;
}

#if DEBUG_WHEEL_TRACE
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
#endif

bool shouldUpdateTelemetry()
{
  unsigned long now = millis();
  if (now - lastTelemetryUpdate >= TELEMETRY_INTERVAL)
  {
    lastTelemetryUpdate = now;
    return true;
  }
  return false;
}

// Teleplot lines (">name:value"), unprefixed so Teleplot can parse them.
void sendTelemetry()
{
  LOG_RAW(LOG_TELEMETRY, ">delta_top:%d\n", wheel.getDelta(ClickWheel::TOP));
  LOG_RAW(LOG_TELEMETRY, ">delta_right:%d\n", wheel.getDelta(ClickWheel::RIGHT));
  LOG_RAW(LOG_TELEMETRY, ">delta_bottom:%d\n",
          wheel.getDelta(ClickWheel::BOTTOM));
  LOG_RAW(LOG_TELEMETRY, ">delta_left:%d\n", wheel.getDelta(ClickWheel::LEFT));

  LOG_RAW(LOG_TELEMETRY, ">wheel_touched:%d\n", wheel.isWheelTouched() ? 200 : 0);
  LOG_RAW(LOG_TELEMETRY, ">buttons_pressed:%u\n", wheel.pressedMask());
  // No %f in nano printf: degrees as an integer.
  LOG_RAW(LOG_TELEMETRY, ">scroll_degrees:%d\n", (int)wheel.getScrollDegrees());

  LOG_RAW(LOG_TELEMETRY, ">playing_track:%d\n", state.getPlayingTrackIndex());
  LOG_RAW(LOG_TELEMETRY, ">is_playing:%d\n", state.getIsPlaying() ? 100 : 0);
}

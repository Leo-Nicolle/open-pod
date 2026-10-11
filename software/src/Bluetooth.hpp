#pragma once
#include "logging.h"
#include "pinout.h"
#include <Arduino.h>

// FSC-BT1035 Bluetooth module as an A2DP source: the VS1053's I2S output
// (VS1053_driver::setI2SOutput()) goes straight into the module, which streams
// it to headphones/speakers. Wiring: docs/BT-pinout.md. AT commands:
// datasheets/FSC-BT1035_programming_user_guide.pdf.
//
// The module stores its settings and reconnects to the last device by itself
// at power-on, so this class only:
//   - powers it on (SYS_CTRL high, kept high),
//   - waits for it to answer, then makes sure its audio input is I2S (slave,
//     48 kHz, 16 bit) - writing the settings only when they differ,
//   - logs its events (connection state, device, codec),
//   - forwards lines typed on the debug serial that start with "AT" to it,
//     to pair a new device by hand (AT+SCAN=1, AT+A2DPCONN=<MAC>).
// Everything is non-blocking: call loop() every main-loop iteration.
class Bluetooth {
public:
  enum A2dpState : uint8_t {
    A2DP_UNSUPPORTED,
    A2DP_STANDBY,
    A2DP_CONNECTING,
    A2DP_CONNECTED,
    A2DP_STREAMING,
    A2DP_PAUSED,
  };

  void begin() {
    // SYS_CTRL has no internal pull. High > 20 ms powers the module on; it
    // ignores the pin once booted, so it just stays high.
    digitalWrite(BT_SYS_CTRL, HIGH);
    pinMode(BT_SYS_CTRL, OUTPUT);
    uart.begin(115200);
    phase = Phase::Booting;
    phaseStartMs = lastSendMs = millis();
  }

  void loop() {
    readModule();
    readConsole();

    uint32_t now = millis();
    switch (phase) {
    case Phase::Booting:
      // Boot takes ~1.5 s; ping until it answers.
      if (now - phaseStartMs > BOOT_TIMEOUT_MS) {
        LOG_WARN("Bluetooth module not answering, check its power and UART");
        phase = Phase::Failed;
      } else if (now - lastSendMs > 300) {
        send("AT");
      }
      break;
    case Phase::Configuring:
      if (awaiting && now - lastSendMs > 1000) {
        LOG_WARN("Bluetooth: no answer to a config command, skipping it");
        awaiting = false;
        nextConfigStep();
      }
      if (!awaiting)
        runConfigStep();
      break;
    case Phase::Ready:
    case Phase::Failed:
    case Phase::Off:
      break;
    }
  }

  A2dpState a2dpState() const { return a2dp; }
  bool ready() const { return phase == Phase::Ready; }

private:
  enum class Phase : uint8_t { Off, Booting, Configuring, Ready, Failed };

  // One stored setting: queried, then written only when it differs (they
  // live in the module's flash).
  struct Setting {
    const char *name; // AT+<name>, answered with +<name>=<value>
    int wanted;
  };
  // AUXCFG 3 = audio input from I2S. I2SCFG 3 = enabled, slave, 48 kHz,
  // standard I2S, 16 bit: matches the VS1053 as master at 48 kHz.
  static constexpr Setting SETTINGS[] = {{"AUXCFG", 3}, {"I2SCFG", 3}};
  static constexpr uint8_t N_SETTINGS = sizeof(SETTINGS) / sizeof(SETTINGS[0]);
  static const uint32_t BOOT_TIMEOUT_MS = 8000;

  Uart uart{BT_UART_RX, BT_UART_TX};
  Phase phase = Phase::Off;
  uint32_t phaseStartMs = 0;
  uint32_t lastSendMs = 0;
  A2dpState a2dp = A2DP_UNSUPPORTED;

  // Configuring: setting index, whether it was queried / written yet, and the
  // value the query returned (-1: none).
  uint8_t configStep = 0;
  bool configQueried = false;
  bool configWritten = false;
  bool awaiting = false; // a command is waiting for OK/ERROR
  int queriedValue = -1;

  char rxLine[96];
  uint8_t rxLen = 0;
  char consoleLine[64];
  uint8_t consoleLen = 0;

  void send(const char *cmd) {
    LOG_CAT(BLUETOOTH, "< %s", cmd);
    uart.print(cmd);
    uart.print("\r\n");
    lastSendMs = millis();
  }

  void runConfigStep() {
    if (configStep >= N_SETTINGS) {
      phase = Phase::Ready;
      LOG_INFO("✅ Bluetooth ready (I2S input)");
      return;
    }
    const Setting &s = SETTINGS[configStep];
    char cmd[24];
    if (!configQueried) {
      snprintf(cmd, sizeof(cmd), "AT+%s", s.name);
      configQueried = true;
    } else {
      snprintf(cmd, sizeof(cmd), "AT+%s=%d", s.name, s.wanted);
      configWritten = true;
      LOG_INFO("Bluetooth: setting %s to %d (was %d)", s.name, s.wanted,
               queriedValue);
    }
    send(cmd);
    awaiting = true;
  }

  void nextConfigStep() {
    configStep++;
    configQueried = configWritten = false;
    queriedValue = -1;
  }

  // OK/ERROR for the command in flight.
  void onCommandDone(bool ok) {
    if (phase == Phase::Booting) {
      if (ok) {
        LOG_INFO("Bluetooth module answering after %lu ms",
                 (unsigned long)(millis() - phaseStartMs));
        phase = Phase::Configuring;
        configStep = 0;
        configQueried = configWritten = false;
        queriedValue = -1;
        awaiting = false;
      }
      return;
    }
    if (phase != Phase::Configuring || !awaiting)
      return;
    awaiting = false;
    const Setting &s = SETTINGS[configStep];
    if (!ok) {
      LOG_WARN("Bluetooth: AT+%s failed", s.name);
      nextConfigStep();
    } else if (configWritten || queriedValue == s.wanted) {
      nextConfigStep();
    }
    // else: the query answered another value (or none): runConfigStep()
    // writes it.
  }

  void onLine(const char *line) {
    LOG_CAT(BLUETOOTH, "> %s", line);
    if (!strcmp(line, "OK")) {
      onCommandDone(true);
      return;
    }
    if (!strcmp(line, "ERROR")) {
      onCommandDone(false);
      return;
    }
    if (line[0] != '+')
      return;
    const char *eq = strchr(line, '=');
    if (!eq)
      return;
    const char *value = eq + 1;
    size_t keyLen = eq - line - 1;

    if (phase == Phase::Configuring && configStep < N_SETTINGS &&
        keyLen == strlen(SETTINGS[configStep].name) &&
        !strncmp(line + 1, SETTINGS[configStep].name, keyLen)) {
      queriedValue = atoi(value);
    } else if (!strncmp(line, "+A2DPSTAT=", 10)) {
      static const char *const NAMES[] = {"unsupported", "standby",
                                          "connecting",  "connected",
                                          "streaming",   "paused"};
      int st = atoi(value);
      if (st >= 0 && st <= A2DP_PAUSED) {
        a2dp = (A2dpState)st;
        LOG_INFO("Bluetooth A2DP: %s", NAMES[st]);
      }
    } else if (!strncmp(line, "+A2DPDEV=", 9)) {
      LOG_INFO("Bluetooth connected to %s", value); // MAC,name
    } else if (!strncmp(line, "+A2DPENC=", 9)) {
      static const char *const CODECS[] = {"?",       "SBC",     "aptX",
                                           "aptX HD", "aptX LL", "aptX Adaptive"};
      int c = atoi(value);
      LOG_INFO("Bluetooth codec: %s", c >= 1 && c <= 5 ? CODECS[c] : "?");
    } else if (!strncmp(line, "+SCAN=", 6)) {
      LOG_INFO("Bluetooth scan: %s", value); // index,type,MAC,RSSI,len,name,class
    } else if (!strncmp(line, "+PAIRED=", 8)) {
      LOG_INFO("Bluetooth paired with %s", value);
    }
  }

  void readModule() {
    while (uart.available()) {
      char c = uart.read();
      if (c == '\r' || c == '\n') {
        if (rxLen) {
          rxLine[rxLen] = 0;
          onLine(rxLine);
          rxLen = 0;
        }
      } else if (rxLen < sizeof(rxLine) - 1) {
        rxLine[rxLen++] = c;
      }
    }
  }

  // Lines typed on the debug serial starting with "AT" go to the module as
  // is (with its answers logged by onLine()); anything else is dropped.
  void readConsole() {
    while (Serial.available()) {
      char c = Serial.read();
      if (c == '\r' || c == '\n') {
        consoleLine[consoleLen] = 0;
        if (consoleLen >= 2 && (consoleLine[0] == 'A' || consoleLine[0] == 'a') &&
            (consoleLine[1] == 'T' || consoleLine[1] == 't')) {
          LOG_INFO("Bluetooth < %s", consoleLine);
          uart.print(consoleLine);
          uart.print("\r\n");
        }
        consoleLen = 0;
      } else if (c == '\b' || c == 0x7F) {
        if (consoleLen)
          consoleLen--;
      } else if (consoleLen < sizeof(consoleLine) - 1) {
        consoleLine[consoleLen++] = c;
      }
    }
  }
};

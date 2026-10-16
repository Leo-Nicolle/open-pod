#pragma once
#include "../logging.h"
#include "../pinout.h"
#include "BtModel.h"
#include <Arduino.h>

// FSC-BT1035 Bluetooth module as an A2DP source: the VS1053's I2S output
// (VS1053_driver::setI2SOutput()) goes straight into the module, which streams
// it to headphones/speakers. Wiring: notes/BT-pinout.md. AT commands:
// datasheets/FSC-BT1035_programming_user_guide.pdf.
//
// This class owns the UART:
//   - powers the module on (SYS_CTRL high, kept high),
//   - waits for it to answer, then makes sure its audio input is I2S (slave,
//     48 kHz, 16 bit) - writing the settings only when they differ,
//   - then sends the commands BtModel queues (scan, connect, ...), one at a
//     time, each waiting for its OK/ERROR,
//   - hands every line the module sends to BtModel,
//   - forwards lines typed on the debug serial that start with "AT" to it
//     (forwardConsoleLine(), fed by app/console.cpp).
// Everything is non-blocking: call loop() every main-loop iteration.
class Bluetooth {
public:
  explicit Bluetooth(BtModel &model) : model(model) {}

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
      if (awaiting && now - lastSendMs > COMMAND_TIMEOUT_MS) {
        LOG_WARN("Bluetooth: no answer to a config command, skipping it");
        awaiting = false;
        nextConfigStep();
      }
      if (!awaiting)
        runConfigStep();
      break;
    case Phase::Ready:
      if (awaiting && now - lastSendMs > COMMAND_TIMEOUT_MS) {
        LOG_WARN("Bluetooth: no answer to %s", inFlight);
        awaiting = false;
        model.onCommandDone(false, now);
      }
      if (!awaiting && model.nextCommand(inFlight, sizeof(inFlight))) {
        send(inFlight);
        awaiting = true;
      }
      break;
    case Phase::Failed:
    case Phase::Off:
      break;
    }
    model.tick(now);
  }

  bool ready() const { return phase == Phase::Ready; }

  // A line typed on the debug serial that starts with "AT" goes to the module
  // as is (its answers are logged by onLine()).
  void forwardConsoleLine(const char *line) {
    LOG_INFO("Bluetooth < %s", line);
    uart.print(line);
    uart.print("\r\n");
  }

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
  static const uint32_t COMMAND_TIMEOUT_MS = 1000;

  BtModel &model;
  Uart uart{BT_UART_RX, BT_UART_TX};
  Phase phase = Phase::Off;
  uint32_t phaseStartMs = 0;
  uint32_t lastSendMs = 0;

  // Configuring: setting index, whether it was queried / written yet, and the
  // value the query returned (-1: none).
  uint8_t configStep = 0;
  bool configQueried = false;
  bool configWritten = false;
  bool awaiting = false; // a command is waiting for OK/ERROR
  int queriedValue = -1;
  char inFlight[BtModel::CMD_LEN];

  char rxLine[96];
  uint8_t rxLen = 0;

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
      model.onModuleReady();
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
    if (!awaiting)
      return;
    awaiting = false;
    if (phase == Phase::Ready) {
      if (!ok)
        LOG_WARN("Bluetooth: %s failed", inFlight);
      model.onCommandDone(ok, millis());
      return;
    }
    if (phase != Phase::Configuring)
      return;
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
    if (phase == Phase::Configuring && configStep < N_SETTINGS &&
        line[0] == '+') {
      const char *name = SETTINGS[configStep].name;
      size_t n = strlen(name);
      if (!strncmp(line + 1, name, n) && line[n + 1] == '=')
        queriedValue = atoi(line + n + 2);
    }
    model.onLine(line, millis());
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
};

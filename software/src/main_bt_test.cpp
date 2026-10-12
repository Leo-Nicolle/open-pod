// FSC-BT1035 bring-up sketch: powers the module through SYS_CTRL, prints
// everything it says on its UART, probes it with a few AT commands, then turns
// into a serial bridge between the PC and the module.
//
// Build and run: pio run -e bt_test -t upload -t monitor (platformio.ini).
// The player itself (main.cpp) drives the module with bluetooth/Bluetooth.hpp.
// Wiring: notes/BT-pinout.md.
//
// Type in the monitor:
//   AT+VER         any line not starting with '!' goes to the module + "\r\n"
//   !on / !off     SYS_CTRL high / low
//   !cycle         SYS_CTRL low 1 s, then high (power cycle)
//   !probe         resend the probe commands
//   !scan          scan for nearby Bluetooth (BR/EDR) devices, ~10 s
//   !conn 2        A2DP-connect to device #2 of the last scan
//   !bauds         try the probe at every common baud rate
//   !baud 9600     change the STM32 side of the BT UART
//   !hex           toggle the hex dump of what the module sends
//   !pin           read the levels of the BT pins
//
// Audio over I2S (VS1053 GPIO4/6/7 -> module PCM_SYNC/CLK/IN):
//   !i2s           module input = I2S slave 48 kHz 16 bit, VS1053 I2S on
//   !tone          VS1053 sine test on/off
//   !play          loop the startup sound (MP3 from flash), !stop to stop
//   !vol 40        VS1053 volume, 0 = loudest, 254 = quietest
//   !i2scheck      sample the VS1053 I2S pins: are they toggling?
//
// AT commands: datasheets/FSC-BT1035_programming_user_guide.pdf
#include "pinout.h"
#include "sound/VS1053_driver.h"
#include "sound/startup_sound.h"
#include <Arduino.h>

Uart BT(BT_UART_RX, BT_UART_TX);

static uint32_t btBaud = 115200;
static bool hexDump = false;
static uint32_t rxBytes = 0;

static const char *const PROBES[] = {"AT", "AT+VER", "AT+NAME", "AT+ADDR"};
static const uint32_t BAUDS[] = {115200, 9600, 19200, 38400, 57600,
                                 230400, 460800, 921600};

// What the module sends, printed one line at a time: the text, then (with
// !hex) the raw bytes, so non-printable garbage (wrong baud) is visible.
static char rxLine[128];
static uint8_t rxLen = 0;
static uint32_t lastRxMs = 0;

// Devices found by the last AT+SCAN=1, for !conn.
struct ScanResult
{
  char mac[13];
  int rssi;
  char name[33];
};
static ScanResult found[16];
static uint8_t nFound = 0;

static void printScanResults()
{
  Serial.printf("--- %u device(s) ---\n", nFound);
  for (uint8_t i = 0; i < nFound; i++)
    Serial.printf("  #%u  %s  %4d dBm  %s\n", i + 1, found[i].mac,
                  found[i].rssi, found[i].name);
  if (nFound)
    Serial.println("Connect with !conn <#>");
}

// +SCAN=<index>,<addr type>,<MAC>,<RSSI>,<name length>,<name>,<class>
// The guide ends a scan with +SCAN=E, but this firmware (V6.1.1) only reports
// +DEVSTAT=9 (scanning) then +DEVSTAT=1 when it's done.
static bool scanning = false;

static void parseScanLine(const char *line)
{
  if (scanning && (!strcmp(line, "+SCAN=E") ||
                   (!strncmp(line, "+DEVSTAT=", 9) && strcmp(line + 9, "9"))))
  {
    scanning = false;
    printScanResults();
    return;
  }
  if (strncmp(line, "+SCAN=", 6))
    return;
  const char *p = line + 6;
  char mac[13];
  int index, type, rssi, nameLen, consumed;
  if (sscanf(p, "%d,%d,%12[0-9A-Fa-f],%d,%d,%n", &index, &type, mac, &rssi,
             &nameLen, &consumed) < 5)
    return;
  ScanResult *r = nullptr;
  for (uint8_t i = 0; i < nFound; i++)
    if (!strcmp(found[i].mac, mac))
      r = &found[i];
  if (!r && nFound < sizeof(found) / sizeof(found[0]))
    r = &found[nFound++];
  if (!r)
    return;
  strcpy(r->mac, mac);
  r->rssi = rssi;
  // The name can contain commas, so it's cut by its length, not parsed.
  size_t n = min((size_t)nameLen, sizeof(r->name) - 1);
  n = min(n, strlen(p + consumed));
  memcpy(r->name, p + consumed, n);
  r->name[n] = 0;
}

static void flushRxLine()
{
  if (rxLen == 0)
    return;
  rxLine[rxLen] = 0;
  parseScanLine(rxLine);
  Serial.printf("[%7lu] BT> ", (unsigned long)millis());
  for (uint8_t i = 0; i < rxLen; i++)
  {
    char c = rxLine[i];
    Serial.print(c >= 32 && c < 127 ? c : '.');
  }
  if (hexDump)
  {
    Serial.print("   |");
    for (uint8_t i = 0; i < rxLen; i++)
      Serial.printf(" %02X", (uint8_t)rxLine[i]);
  }
  Serial.println();
  rxLen = 0;
}

static void pollBT()
{
  while (BT.available())
  {
    uint8_t c = BT.read();
    rxBytes++;
    lastRxMs = millis();
    if (c == '\n' || c == '\r')
    {
      flushRxLine();
      continue;
    }
    rxLine[rxLen++] = c;
    if (rxLen == sizeof(rxLine) - 1)
      flushRxLine();
  }
  // A line without terminator (prompt, garbage): print it after a pause.
  if (rxLen && millis() - lastRxMs > 50)
    flushRxLine();
}

// Waits while still printing what the module sends.
static void listen(uint32_t ms)
{
  uint32_t start = millis();
  while (millis() - start < ms)
    pollBT();
}

static void sendBT(const char *cmd)
{
  Serial.printf("[%7lu] BT< %s\n", (unsigned long)millis(), cmd);
  BT.print(cmd);
  BT.print("\r\n");
}

static void setBaud(uint32_t baud)
{
  btBaud = baud;
  BT.end();
  BT.begin(btBaud);
  Serial.printf("BT UART at %lu baud\n", (unsigned long)btBaud);
}

static void printPins()
{
  // BT_TX idles high once the module runs its UART; low or floating while off.
  Serial.printf("SYS_CTRL (PA15) out=%d | BT_TX -> PB7 reads %d\n",
                digitalRead(BT_SYS_CTRL), digitalRead(BT_UART_RX));
}

static void sysCtrl(bool on)
{
  digitalWrite(BT_SYS_CTRL, on ? HIGH : LOW);
  Serial.printf("SYS_CTRL %s\n", on ? "HIGH (power on)" : "LOW");
}

static void probe()
{
  for (const char *cmd : PROBES)
  {
    sendBT(cmd);
    listen(500);
  }
}

static void scanBauds()
{
  for (uint32_t baud : BAUDS)
  {
    setBaud(baud);
    uint32_t before = rxBytes;
    sendBT("AT");
    listen(400);
    if (rxBytes != before)
      Serial.printf("  -> %lu bytes back at %lu baud\n",
                    (unsigned long)(rxBytes - before), (unsigned long)baud);
  }
  setBaud(115200);
}

// The module must have the A2DP Source profile (AT+PROFILE bit 6) to connect
// to headphones or speakers: print the profile mask before scanning.
static void scanDevices()
{
  nFound = 0;
  scanning = true;
  sendBT("AT+PROFILE");
  listen(300);
  Serial.println("Scanning, put the receiver in pairing mode...");
  sendBT("AT+SCAN=1");
}

static void connectDevice(int n)
{
  if (n < 1 || n > nFound)
  {
    Serial.println("No such device, run !scan first");
    return;
  }
  Serial.printf("Connecting to %s (%s)\n", found[n - 1].name, found[n - 1].mac);
  if (scanning)
  {
    sendBT("AT+SCAN=0");
    listen(200);
  }
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "AT+A2DPCONN=%s", found[n - 1].mac);
  sendBT(cmd);
}

// ---- VS1053 as the I2S source ----

VS1053_driver vs(MP3_RESET, MP3CS, MP3XDCS, MP3DREQ);
static bool vsOk = false;
static bool toneOn = false;
static uint32_t playPos = 0; // next MP3 byte to send, while playing
static bool playing = false;

static void vsVolume(uint8_t v)
{
  vs.setVolume(v, v);
  Serial.printf("VS1053 volume %u\n", v);
}

// VS1053 datasheet 11.14: GPIO4-7 as outputs, then I2S_CONFIG = enable at
// 48 kHz. As I2S master at 48 kHz, SCLK = 12.288 MHz / 8 = 1.536 MHz, i.e.
// 16-bit words: that's the module's AT+I2SCFG=3 (slave, 48 kHz, 16 bit, I2S).
// MCLK stays off: the module doesn't need it.
static void enableI2S()
{
  sendBT("AT+AUXCFG=3"); // audio input: I2S
  listen(300);
  sendBT("AT+I2SCFG=3");
  listen(300);
  if (!vsOk)
  {
    Serial.println("VS1053 not found, I2S output not enabled");
    return;
  }
  vs.writeRegister(VS1053_REG_WRAMADDR, 0xC017);
  vs.writeRegister(VS1053_REG_WRAM, 0x00F0);
  vs.writeRegister(VS1053_REG_WRAMADDR, 0xC040);
  vs.writeRegister(VS1053_REG_WRAM, 0x0004);
  Serial.println("VS1053 I2S on (48 kHz master)");
}

// Reads the VS1053's own pin levels (GPIO_IDATA, 0xC018) many times over SCI.
// With I2S running, LROUT (GPIO4) and SCLK (GPIO6) must be seen both high and
// low, and SDATA (GPIO7) too while sound plays. A pin stuck at one level is
// either not driven or shorted/loaded by the wiring.
static void checkI2S()
{
  if (!vsOk)
  {
    Serial.println("no VS1053");
    return;
  }
  vs.writeRegister(VS1053_REG_WRAMADDR, 0xC040);
  uint16_t cfg = vs.readRegister(VS1053_REG_WRAM);
  vs.writeRegister(VS1053_REG_WRAMADDR, 0xC017);
  uint16_t ddr = vs.readRegister(VS1053_REG_WRAM);
  Serial.printf("I2S_CONFIG=0x%X (want 0x4) GPIO_DDR=0x%X (want 0xF0)\n", cfg,
                ddr);
  uint16_t seenHigh = 0, seenLow = 0;
  for (int i = 0; i < 300; i++)
  {
    vs.writeRegister(VS1053_REG_WRAMADDR, 0xC018);
    uint16_t v = vs.readRegister(VS1053_REG_WRAM);
    seenHigh |= v;
    seenLow |= ~v;
    delayMicroseconds(37); // not a multiple of the I2S periods
  }
  static const struct
  {
    uint8_t bit;
    const char *name;
  } pins[] = {{4, "GPIO4 LROUT -> PCM_SYNC (7)"},
              {6, "GPIO6 SCLK  -> PCM_CLK  (4)"},
              {7, "GPIO7 SDATA -> PCM_IN   (5)"}};
  for (auto &pin : pins)
  {
    bool h = seenHigh & (1 << pin.bit), l = seenLow & (1 << pin.bit);
    Serial.printf("  %s: %s\n", pin.name,
                  h && l ? "toggling" : h ? "stuck HIGH" : "stuck LOW");
  }
}

static void stopAudio()
{
  if (playing)
  {
    // Flush the decoder with zeros so the end of the clip plays out.
    static const uint8_t zeros[32] = {0};
    for (int i = 0; i < 2048 / 32; i++)
      vs.sendData(zeros, sizeof(zeros));
    playing = false;
  }
  if (toneOn)
  {
    static const uint8_t stop[8] = {0x45, 0x78, 0x69, 0x74, 0, 0, 0, 0};
    vs.sendData(stop, sizeof(stop));
    vs.writeRegister(VS1053_REG_MODE, VS1053_MODE_SM_SDINEW);
    toneOn = false;
  }
}

// Sine test (datasheet 9.12.1): 0x44 = 22.05 kHz table, step 4 -> ~690 Hz.
static void toggleTone()
{
  if (toneOn)
  {
    stopAudio();
    Serial.println("tone off");
    return;
  }
  stopAudio();
  vs.writeRegister(VS1053_REG_MODE,
                   VS1053_MODE_SM_SDINEW | VS1053_MODE_SM_TESTS);
  static const uint8_t start[8] = {0x53, 0xEF, 0x6E, 0x44, 0, 0, 0, 0};
  vs.sendData(start, sizeof(start));
  toneOn = true;
  Serial.println("tone on (~690 Hz)");
}

// Feeds the MP3 32 bytes at a time while DREQ says there's room, so the BT
// UART keeps being polled.
static void pollPlayback()
{
  while (playing && vs.dreqHigh())
  {
    uint32_t n = min((uint32_t)32, STARTUP_SOUND_LEN - playPos);
    vs.sendData(STARTUP_SOUND_MP3 + playPos, n);
    playPos += n;
    if (playPos >= STARTUP_SOUND_LEN)
      playPos = 0; // loop: the decoder resyncs on the next frame header
  }
}

static void localCommand(const char *cmd)
{
  Serial.printf("[%7lu] CMD %s\n", (unsigned long)millis(), cmd);
  if (!strcmp(cmd, "!on"))
    sysCtrl(true);
  else if (!strcmp(cmd, "!off"))
    sysCtrl(false);
  else if (!strcmp(cmd, "!cycle"))
  {
    sysCtrl(false);
    listen(1000);
    sysCtrl(true);
  }
  else if (!strcmp(cmd, "!probe"))
    probe();
  else if (!strcmp(cmd, "!scan"))
    scanDevices();
  else if (!strncmp(cmd, "!conn ", 6))
    connectDevice(atoi(cmd + 6));
  else if (!strcmp(cmd, "!bauds"))
    scanBauds();
  else if (!strncmp(cmd, "!baud ", 6))
    setBaud(strtoul(cmd + 6, nullptr, 10));
  else if (!strcmp(cmd, "!hex"))
  {
    hexDump = !hexDump;
    Serial.printf("hex dump %s\n", hexDump ? "on" : "off");
  }
  else if (!strcmp(cmd, "!pin"))
    printPins();
  else if (!strcmp(cmd, "!i2scheck"))
    checkI2S();
  else if (!strcmp(cmd, "!i2s"))
    enableI2S();
  else if (!strcmp(cmd, "!tone"))
    toggleTone();
  else if (!strcmp(cmd, "!play"))
  {
    stopAudio();
    playPos = 0;
    playing = vsOk;
    Serial.println(vsOk ? "playing startup sound in a loop" : "no VS1053");
  }
  else if (!strcmp(cmd, "!stop"))
  {
    stopAudio();
    Serial.println("stopped");
  }
  else if (!strncmp(cmd, "!vol ", 5))
    vsVolume(atoi(cmd + 5));
  else
    Serial.println("? !on !off !cycle !probe !scan !conn <#> !bauds !baud <n> "
                   "!hex !pin !i2s !tone !play !stop !vol <n> !i2scheck");
}

// Lines typed in the monitor: '!' commands run here, the rest go to the module.
static void pollPC()
{
  static char line[128];
  static uint8_t len = 0;
  while (Serial.available())
  {
    char c = Serial.read();
    if (c == '\r' || c == '\n')
    {
      if (len == 0)
        continue;
      line[len] = 0;
      len = 0;
      if (line[0] == '!')
        localCommand(line);
      else
        sendBT(line);
    }
    else if (c == '\b' || c == 0x7F) // backspace: the monitor sends it raw
    {
      if (len)
        len--;
    }
    else if (len < sizeof(line) - 1)
      line[len++] = c;
  }
}

void setup()
{
  // Keep the other SPI devices deselected and the VS1053 out of reset (a
  // floating XRESET thumps in the headphones, see main.cpp).
  digitalWrite(MP3_RESET, HIGH);
  pinMode(MP3_RESET, OUTPUT);
  for (uint32_t cs : {CARDCS, MP3CS, MP3XDCS, PSRAM_CS})
  {
    digitalWrite(cs, HIGH);
    pinMode(cs, OUTPUT);
  }

  // SYS_CTRL has no internal pull: drive it low right away (module off).
  digitalWrite(BT_SYS_CTRL, LOW);
  pinMode(BT_SYS_CTRL, OUTPUT);
  pinMode(BT_UART_RX, INPUT); // read the module's TX level before the UART

  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== FSC-BT1035 bring-up ===");
  printPins();

  // Off long enough to be a real power-off, then the > 20 ms power-on pulse.
  // SYS_CTRL stays high afterwards: the firmware ignores it once booted.
  delay(500);
  BT.begin(btBaud);
  sysCtrl(true);
  Serial.println("Listening 3 s for boot messages...");
  listen(3000);
  printPins();
  Serial.printf("%lu bytes received during boot\n", (unsigned long)rxBytes);

  probe();
  if (rxBytes == 0)
  {
    Serial.println("No answer at 115200, scanning baud rates...");
    scanBauds();
  }
  if (rxBytes == 0)
    Serial.println("Still nothing: check power (VBAT_IN, VDD_IO, GND), "
                   "SYS_CTRL, and TX/RX not swapped (!pin, !cycle)");

  vsOk = vs.begin(1000000);
  Serial.printf("VS1053 %s\n", vsOk ? "found" : "NOT found");
  if (vsOk)
    vsVolume(40); // begin() leaves it nearly muted

  Serial.println("Bridge mode. Type AT commands, or !help");
}

void loop()
{
  pollPC();
  pollBT();
  pollPlayback();
}

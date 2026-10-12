#pragma once
#include <Arduino.h>

// What the FSC-BT1035 tells us, and what we ask it to do, without the UART:
// Bluetooth.hpp feeds it the module's lines and sends the commands it
// queues. No display or hardware code, so it runs (and is tested) on the
// host. AT commands: datasheets/FSC-BT1035_programming_user_guide.pdf.
//
// The module keeps its paired list (8 devices) and reconnects to the last
// device by itself. What it can't keep, we do: an alias and a preferred
// codec per device (BtConfig saves them on the SD card). The codec setting
// (AT+A2DPCFG) is global in the module, so it is rewritten before each
// connection to match the device's preference.
//
// Devices live in fixed slots: a slot index stays valid until the device is
// dropped, and pin() keeps the one a screen shows from being dropped.
class BtModel {
public:
  static const int MAX_DEVICES = 12; // the module pairs at most 8
  static const int NAME_LEN = 25;  // broadcast name, 24 chars
  static const int ALIAS_LEN = 25; // alias, 24 chars
  static const int CMD_LEN = 32; // longest: AT+A2DPCONN=<12 hex digits>
  static const int MAX_COMMANDS = 8;
  static const int TOAST_LEN = 64;
  static const uint32_t SCAN_MS = 10000;
  static const uint32_t CONNECT_TIMEOUT_MS = 15000;
  static const uint32_t CODEC_DEBOUNCE_MS = 1000;
  static const uint32_t RSSI_POLL_MS = 5000;
  static const uint32_t FAILED_SHOW_MS = 2000;

  enum Link : uint8_t { LINK_IDLE, LINK_CONNECTING, LINK_CONNECTED };
  enum Icon : uint8_t { ICON_OFF, ICON_ON, ICON_CONNECTING, ICON_CONNECTED };
  // Codec preference (and codec in use). AUTO lets the module pick among
  // every aptX variant; the others allow only that one (SBC is always on).
  enum Codec : uint8_t {
    CODEC_AUTO,
    CODEC_SBC,
    CODEC_APTX,
    CODEC_APTX_HD,
    CODEC_APTX_LL,
    CODEC_APTX_AD,
    CODEC_COUNT
  };
  enum ToastTone : uint8_t { TOAST_SUCCESS, TOAST_NEUTRAL, TOAST_ERROR };

  struct Device {
    bool used;
    char macText[18]; // 00:1A:7D:DA:71:13 (macOf() for the module's form)
    char name[NAME_LEN];
    char alias[ALIAS_LEN];
    bool paired;
    bool connected;
    bool seen; // found by the current or last scan
    bool rssiValid;
    int8_t rssi;
    uint32_t rssiAtMs;
    uint8_t codecPref;         // Codec
    uint8_t codecsUnsupported; // bit per Codec: asked for, got another one
    uint8_t codecInUse;        // Codec; CODEC_AUTO = not connected/unknown
    bool listed;               // in the paired list being read (AT+PLIST)
    uint8_t cmdRefs;           // queued commands naming it: not dropped
  };

  struct Toast {
    uint8_t tone; // ToastTone
    char text[TOAST_LEN];
    // Where the device name sits in text (drawn bold), length 0 if none.
    uint8_t nameStart;
    uint8_t nameLen;
    // No Bluetooth icon: the text starts at x=8 (podcast toasts).
    bool plain;
  };

  BtModel();

  // --- Module side -------------------------------------------------------
  // One line from the module, without its CR/LF.
  void onLine(const char *line, uint32_t now);
  // OK/ERROR for the command last taken by nextCommand().
  void onCommandDone(bool ok, uint32_t now);
  // The command to send next, if any; it stays in flight until
  // onCommandDone(). Returns false when the queue is empty.
  bool nextCommand(char *out, size_t size);
  // The module is up: read its paired list, link state and codec setting.
  void onModuleReady();
  // Timers: scan end, connect timeout, codec debounce, RSSI polling.
  void tick(uint32_t now);

  // --- User side ---------------------------------------------------------
  void setRadio(bool on);
  void startScan(uint32_t now);
  void stopScan();
  void connect(int slot, uint32_t now);
  void cancelConnect();
  void disconnect();
  void forget(int slot);
  // Saved right away (configDirty), applied 1 s after the last change: a
  // connected device reconnects once with the new setting.
  void setCodecPref(int slot, uint8_t codec, uint32_t now);
  // Empty clears it.
  void setAlias(int slot, const char *alias);
  // The slot a screen shows: kept when the table is full, and its RSSI is
  // polled every 5 s while it's connected. -1 for none.
  void pin(int slot);

  // --- State -------------------------------------------------------------
  bool radioOn() const { return radio; }
  Link link() const { return linkState; }
  Icon icon() const;
  bool scanning() const { return scanActive; }
  uint32_t scanRemainingMs(uint32_t now) const;
  // The slot connecting or connected to, -1 if none.
  int linkSlot() const { return activeSlot; }
  // True for FAILED_SHOW_MS after a connection to `slot` failed.
  bool connectFailed(int slot, uint32_t now) const;

  const Device &device(int slot) const { return devices[slot]; }
  bool isListed(int slot) const; // shown in the device list
  const char *displayName(int slot) const;
  // Listed slots in list order: connected, paired, then the others by
  // signal strength. Returns how many were written.
  int orderedSlots(int *out, int max) const;
  int findSlot(const char *mac) const; // 12 hex digits
  // The device's MAC as the module writes it (12 hex digits).
  static void macOf(const Device &d, char out[13]);
  // Next codec after `codec` for this device, skipping the ones it was
  // seen not to support.
  uint8_t nextCodec(int slot, uint8_t codec) const;
  static const char *codecName(uint8_t codec);
  // 1-3 bars for an RSSI (dBm).
  static int signalBars(int rssi);

  // Bumped on every change the screens show.
  uint32_t revision() const { return rev; }

  // The latest feedback message; toastSerial() changes with each new one.
  uint8_t toastSerial() const { return toastCount; }
  const Toast &lastToast() const { return toast; }
  // Tests: copies a toast raised since the last call.
  bool takeToast(Toast &out);

  // Unexpected link loss (headphones off, out of range): the player pauses.
  void (*onLinkLost)() = nullptr;

  // --- Saved settings (alias, codec per device) ---------------------------
  bool configDirty() const { return cfgDirty; }
  // The config file, one "MAC\tcodec\talias\n" line per call: `slot` is
  // the cursor (start at 0). Returns false after the last line.
  bool configLine(int &slot, char *out, size_t size) const;
  void markConfigSaved() { cfgDirty = false; }
  // The whole file at once (tests), and markConfigSaved(). Returns the
  // length.
  int serializeConfig(char *out, size_t size);
  // One line of that file (without its newline).
  void loadConfigLine(const char *line);

private:
  Device devices[MAX_DEVICES];
  int pinned;

  bool radio;
  Link linkState;
  int activeSlot;          // connecting/connected slot
  bool userConnecting;     // a connect we asked for, not yet answered
  bool userDisconnecting;  // a link drop we asked for
  bool codecReconnecting;  // dropped to apply a codec, reconnect follows
  bool connectedToastDue;  // waiting for the device name (+A2DPDEV)
  uint32_t connectedAtMs;
  uint32_t connectStartMs;
  int failedSlot;
  uint32_t failedAtMs;
  int requestedCodecSlot; // device whose preference the last connect used
  uint8_t requestedCodec;

  bool scanActive;
  uint32_t scanStartMs;

  int codecPendingSlot;
  uint32_t codecApplyAtMs;
  uint32_t lastRssiPollMs;
  int lastCfgMask; // last AT+A2DPCFG value, -1 unknown

  // Commands are queued as {type, argument} and written out by
  // nextCommand(): the argument is a device slot (kept until then), the
  // A2DPCFG mask, or 0/1.
  enum Cmd : uint8_t {
    CMD_NONE,
    CMD_DEVSTAT,
    CMD_CFG_QUERY,
    CMD_CFG_SET,
    CMD_PLIST,
    CMD_PLIST_DEL,
    CMD_A2DPSTAT,
    CMD_A2DPENC,
    CMD_CONN,
    CMD_CONN_LAST,
    CMD_DISC,
    CMD_BTEN,
    CMD_SCAN,
    CMD_RSSI,
  };
  struct Command {
    uint8_t type;
    int8_t arg;
  };
  Command queue[MAX_COMMANDS];
  uint8_t qHead, qCount;
  uint8_t inFlight; // Cmd

  Toast toast;
  uint8_t toastCount, toastTaken;

  bool cfgDirty;
  uint32_t rev;

  void enqueue(uint8_t type, int arg = 0);
  static bool takesSlot(uint8_t type) {
    return type == CMD_PLIST_DEL || type == CMD_CONN || type == CMD_RSSI;
  }
  void changed() { rev++; }
  void raiseToast(uint8_t tone, const char *before, const char *name,
                  const char *after);

  int upsert(const char *mac);
  void drop(int slot);
  void dropUnused();
  void setLink(Link next, uint32_t now);
  void markConnected(int slot);
  void failConnect(uint32_t now);
  void queueConnect(int slot);
  static int codecMask(uint8_t codec);
  bool hasConfig(const Device &d) const;

  void parseScan(const char *value, uint32_t now);
  void parsePaired(const char *value);
  void parseA2dpDev(const char *value);
};

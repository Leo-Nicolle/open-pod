#pragma once
#include "../bluetooth/BtModel.h"
#include "router.h"
#include "text_entry.h"
#include <Arduino.h>

// One row of Settings and the Bluetooth screens (see
// .agents/bluetooth-design/.../README.md, "Row kit"): a label on the left,
// and on the right, from left to right, an optional note, value, signal bars
// and switch/chevron, right-aligned at x=304.
struct MenuRow {
  enum Kind : uint8_t {
    NORMAL,
    READONLY,    // the cursor skips it, label dimmer
    DISABLED,    // the cursor skips it, label and note greyed
    DESTRUCTIVE, // label in the destructive color
    FAILED,      // "Couldn't connect": error fill for 2 s
  };
  enum Trail : uint8_t { TRAIL_NONE, TRAIL_CHEVRON, TRAIL_SWITCH_ON, TRAIL_SWITCH_OFF };
  // Status text color: normal / on the highlighted row.
  enum Tone : uint8_t {
    TONE_ACCENT, // accent / cream
    TONE_DIM,    // dim / text
    TONE_DIMMER, // dimmer / text
    TONE_CREAM,  // cream on both
    TONE_COUNT,  // dim / cream: the scan countdown
    TONE_DISABLED,
  };
  enum Action : uint8_t {
    ACT_NONE,
    ACT_OPEN_BT,
    ACT_TOGGLE_RADIO,
    ACT_SCAN,
    ACT_OPEN_DEVICE,
    ACT_CONNECT,
    ACT_DISCONNECT,
    ACT_CANCEL_CONNECT,
    ACT_CODEC,
    ACT_RENAME,
    ACT_FORGET,
    ACT_FORGET_CANCEL,
    ACT_FORGET_CONFIRM,
  };

  uint8_t kind;
  uint8_t action;
  int8_t slot; // device slot, -1 if none
  const char *label;
  uint8_t dots; // 0: none, else 1-3 lit (Scanning / Connecting)
  const char *note;
  uint8_t noteTone;
  bool noteMono; // dBm, countdown: IBM Plex Mono in the design
  char value[14]; // "-127 dBm", "10s", "aptX Adaptive"
  uint8_t valueTone;
  bool valueMono;
  int8_t bars; // 0: none, else 1-3
  uint8_t trail;
  uint16_t progress; // bottom progress line width (scan), 0: none
};

// The Settings and Bluetooth screens' model: builds their rows from BtModel,
// moves the cursor (skipping read-only rows) and runs the row actions. The
// selection lives in the route (selectedIndex / topVisibleIndex), so back()
// returns to it. No display code, so it runs (and is tested) on the host.
class BtMenu {
public:
  static const int MAX_ROWS = 2 + BtModel::MAX_DEVICES;
  static const int VISIBLE_ROWS = 7;
  static const int ALIAS_MAX = BtModel::ALIAS_LEN - 1;
  static const uint32_t ANIM_STEP_MS = 400; // scan/connect dots

  enum Screen : uint8_t { NONE, SETTINGS, LIST, DEVICE, FORGET, RENAME };
  enum Hint : uint8_t { HINT_NONE, HINT_RADIO_OFF, HINT_NO_DEVICES };

  // What the input layer does after a click.
  struct Nav {
    enum Kind : uint8_t { STAY, PUSH, BACK, BACK_TO_LIST } kind;
    Route_t::RouteType route; // PUSH
    int slot;
    const char *title;
    int selected; // the new screen's initial cursor row
  };

  explicit BtMenu(BtModel &bt);

  // Aliases are saved on the SD card: without one, Rename is disabled.
  void setStorageAvailable(bool available) { storage = available; }

  static Screen screenFor(Route_t::RouteType type);
  static bool isMenuRoute(Route_t::RouteType type) {
    return screenFor(type) != NONE;
  }

  // Rebuilds the rows for `route` and keeps its selection on the same row
  // (by device and action) when rows move. Cheap; call before drawing.
  void sync(Route_t &route, uint32_t now);

  Screen screen() const { return scr; }
  const char *title() const;
  // The header name of a Settings/Bluetooth route, as push() gave it (for
  // State::setMenuRouteNames()).
  void routeName(const Route_t &route, char *buf, uint32_t size) const;
  int rowCount() const { return count; }
  // Row i, built from the model's current state.
  MenuRow row(int i) const;
  int selected() const { return sel; }
  int first() const { return top; }
  Hint hint() const;
  // Screen chunk of the first row: 1, or 4 on the forget confirmation.
  int listTopChunk() const { return scr == FORGET ? 4 : 1; }
  int deviceSlot() const { return slot; }
  const TextEntry &entry() const { return text; }
  // The device's display name ("" without one).
  const char *deviceName() const;
  // The rename screen's "empty name restores ..." device name.
  const char *broadcastName() const;

  // Cursor. Returns true if it moved.
  bool scroll(Route_t &route, int clicks);
  // Center on the selected row.
  Nav click(Route_t &route, uint32_t now);

  // Rename screen.
  bool renameMove(int clicks) { return text.moveCursor(clicks); }
  bool renameAppend() { return text.append(); }
  bool renameBackspace() { return text.backspace(); }
  bool renameSpace() { return text.space(); }
  Nav renameSave();

  // Content hash of screen chunk `chunk` (0 = header title), so the view
  // redraws only the chunks that changed.
  uint32_t chunkHash(int chunk) const;

private:
  BtModel &bt;
  bool storage;
  Screen scr;
  int slot;
  uint32_t at; // the time of the last sync (animations, timeouts)

  // Which rows the screen has; row(i) builds them.
  enum RowId : uint8_t {
    R_SET_BT,
    R_SET_DISPLAY,
    R_SET_AUDIO,
    R_SET_SYSTEM,
    R_SET_RESET,
    R_RADIO,
    R_SCAN,
    R_DEVICE,
    R_LINK, // Connect / Connecting / Couldn't connect / Disconnect
    R_CODEC,
    R_INUSE,
    R_SIGNAL,
    R_RENAME,
    R_FORGET,
    R_FORGET_CANCEL,
    R_FORGET_CONFIRM,
  };
  uint8_t ids[MAX_ROWS];
  int8_t slots[MAX_ROWS];
  int count;
  int sel;
  int top;
  TextEntry text;

  void addId(uint8_t id, int forSlot = -1);
  void listIds();
  bool selectable(int i) const;
  int rowKey(int i) const;
  int findKey(int key) const;
  void clampSelection(Route_t &route);
  int initialSelection(Screen s, int forSlot) const;
  Nav push(Route_t::RouteType type, int forSlot, const char *title);
};

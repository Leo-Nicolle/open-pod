#include "bt_menu.h"
#include <stdio.h>

// Out-of-class definitions: C++11 needs them once the constants are bound to
// references (min/constrain).
const int BtMenu::MAX_ROWS;
const int BtMenu::VISIBLE_ROWS;
const int BtMenu::ALIAS_MAX;
const uint32_t BtMenu::ANIM_STEP_MS;

namespace {

// The rename ribbon: lowercase letters then digits, wrapping.
const char RENAME_CHARS[] = "abcdefghijklmnopqrstuvwxyz0123456789";

uint32_t fnv(uint32_t h, const void *data, size_t len) {
  const uint8_t *p = (const uint8_t *)data;
  for (size_t i = 0; i < len; i++) {
    h ^= p[i];
    h *= 16777619u;
  }
  return h;
}

uint32_t fnvStr(uint32_t h, const char *s) {
  return s ? fnv(h, s, strlen(s) + 1) : fnv(h, "", 1);
}

uint32_t fnvInt(uint32_t h, int v) { return fnv(h, &v, sizeof(v)); }

} // namespace

BtMenu::BtMenu(BtModel &bt)
    : bt(bt), storage(false), scr(NONE), slot(-1), at(0), count(0), sel(0),
      top(0),
      text(RENAME_CHARS, ALIAS_MAX, true) {}

BtMenu::Screen BtMenu::screenFor(Route_t::RouteType type) {
  switch (type) {
  case Route_t::SETTINGS:
    return SETTINGS;
  case Route_t::BT_LIST:
    return LIST;
  case Route_t::BT_DEVICE:
    return DEVICE;
  case Route_t::BT_FORGET:
    return FORGET;
  case Route_t::BT_RENAME:
    return RENAME;
  default:
    return NONE;
  }
}

// --- Rows --------------------------------------------------------------------
// sync() only lists which rows a screen has (ids/slots); row(i) builds one
// when it's needed, so the menu holds no row text.

void BtMenu::addId(uint8_t id, int forSlot) {
  if (count < MAX_ROWS) {
    ids[count] = id;
    slots[count] = forSlot;
    count++;
  }
}

static MenuRow makeRow(const char *label, uint8_t action,
                       uint8_t kind = MenuRow::NORMAL) {
  MenuRow r;
  memset(&r, 0, sizeof(r));
  r.kind = kind;
  r.action = action;
  r.slot = -1;
  r.label = label;
  return r;
}

void BtMenu::listIds() {
  count = 0;
  switch (scr) {
  case SETTINGS:
    for (uint8_t id = R_SET_BT; id <= R_SET_RESET; id++)
      addId(id);
    break;
  case LIST: {
    addId(R_RADIO);
    if (!bt.radioOn())
      break; // every other row is hidden
    addId(R_SCAN);
    int order[BtModel::MAX_DEVICES];
    int n = bt.orderedSlots(order, BtModel::MAX_DEVICES);
    for (int k = 0; k < n; k++)
      addId(R_DEVICE, order[k]);
    break;
  }
  case DEVICE: {
    if (slot < 0 || !bt.device(slot).used)
      break;
    const BtModel::Device &d = bt.device(slot);
    bool connected =
        bt.linkSlot() == slot && bt.link() == BtModel::LINK_CONNECTED;
    addId(R_LINK, slot);
    if (d.paired || connected)
      addId(R_CODEC, slot);
    if (connected)
      addId(R_INUSE, slot);
    addId(R_SIGNAL, slot);
    addId(R_RENAME, slot);
    if (d.paired)
      addId(R_FORGET, slot);
    break;
  }
  case FORGET:
    addId(R_FORGET_CANCEL, slot);
    addId(R_FORGET_CONFIRM, slot);
    break;
  default:
    break;
  }
}

MenuRow BtMenu::row(int i) const {
  if (i < 0 || i >= count)
    return makeRow("", MenuRow::ACT_NONE, MenuRow::READONLY);
  int s = slots[i];
  uint32_t now = at;
  switch (ids[i]) {
  case R_SET_BT: {
    MenuRow r = makeRow("Bluetooth", MenuRow::ACT_OPEN_BT);
    r.trail = MenuRow::TRAIL_CHEVRON;
    if (!bt.radioOn()) {
      r.note = "Off";
      r.noteTone = MenuRow::TONE_DIM;
    } else if (bt.link() == BtModel::LINK_CONNECTED && bt.linkSlot() >= 0) {
      r.note = bt.displayName(bt.linkSlot());
      r.noteTone = MenuRow::TONE_ACCENT;
    } else {
      r.note = "On";
      r.noteTone = MenuRow::TONE_DIM;
    }
    return r;
  }
  case R_SET_DISPLAY:
  case R_SET_AUDIO:
  case R_SET_SYSTEM: {
    // Placeholders for now.
    static const char *const NAMES[] = {"Display", "Audio", "System"};
    MenuRow r = makeRow(NAMES[ids[i] - R_SET_DISPLAY], MenuRow::ACT_NONE);
    r.trail = MenuRow::TRAIL_CHEVRON;
    return r;
  }
  case R_SET_RESET:
    return makeRow("Reset", MenuRow::ACT_NONE, MenuRow::DESTRUCTIVE);

  case R_RADIO: {
    MenuRow r = makeRow("Bluetooth", MenuRow::ACT_TOGGLE_RADIO);
    r.trail = bt.radioOn() ? MenuRow::TRAIL_SWITCH_ON : MenuRow::TRAIL_SWITCH_OFF;
    return r;
  }
  case R_SCAN: {
    if (!bt.scanning())
      return makeRow("Scan for devices", MenuRow::ACT_SCAN);
    uint32_t remaining = bt.scanRemainingMs(now);
    // Steps with the dots, so the row redraws every ANIM_STEP_MS, not
    // every millisecond.
    uint32_t stepped = remaining / ANIM_STEP_MS * ANIM_STEP_MS;
    MenuRow r = makeRow("Scanning", MenuRow::ACT_SCAN);
    r.dots = (now / ANIM_STEP_MS) % 3 + 1;
    r.note = "Stop";
    r.noteTone = MenuRow::TONE_DIM;
    snprintf(r.value, sizeof(r.value), "%lus",
             (unsigned long)((remaining + 999) / 1000));
    r.valueTone = MenuRow::TONE_COUNT;
    r.valueMono = true;
    r.progress = (uint16_t)(320UL * stepped / BtModel::SCAN_MS);
    return r;
  }
  case R_DEVICE: {
    const BtModel::Device &d = bt.device(s);
    MenuRow r = makeRow(bt.displayName(s), MenuRow::ACT_OPEN_DEVICE);
    r.slot = s;
    if (s == bt.linkSlot() && bt.link() == BtModel::LINK_CONNECTED) {
      r.note = "Connected";
      r.noteTone = MenuRow::TONE_ACCENT;
    } else if (s == bt.linkSlot() && bt.link() == BtModel::LINK_CONNECTING) {
      r.note = "Connecting";
      r.noteTone = MenuRow::TONE_DIM;
    } else if (d.paired) {
      r.note = "Paired";
      r.noteTone = MenuRow::TONE_DIMMER;
    } else if (d.rssiValid) {
      r.bars = BtModel::signalBars(d.rssi);
    }
    return r;
  }

  case R_LINK: {
    bool mine = bt.linkSlot() == s;
    if (mine && bt.link() == BtModel::LINK_CONNECTED)
      return makeRow("Disconnect", MenuRow::ACT_DISCONNECT);
    if (mine && bt.link() == BtModel::LINK_CONNECTING) {
      MenuRow r = makeRow("Connecting", MenuRow::ACT_CANCEL_CONNECT);
      r.dots = (now / ANIM_STEP_MS) % 3 + 1;
      r.note = "Cancel";
      r.noteTone = MenuRow::TONE_DIM;
      return r;
    }
    if (bt.connectFailed(s, now)) {
      MenuRow r = makeRow("Couldn't connect", MenuRow::ACT_CONNECT,
                          MenuRow::FAILED);
      r.note = "is it on?";
      r.noteTone = MenuRow::TONE_CREAM;
      return r;
    }
    return makeRow("Connect", MenuRow::ACT_CONNECT);
  }
  case R_CODEC: {
    MenuRow r = makeRow("Codec", MenuRow::ACT_CODEC);
    snprintf(r.value, sizeof(r.value), "%s",
             BtModel::codecName(bt.device(s).codecPref));
    r.valueTone = MenuRow::TONE_DIM;
    return r;
  }
  case R_INUSE: {
    const BtModel::Device &d = bt.device(s);
    MenuRow r = makeRow("In use", MenuRow::ACT_NONE, MenuRow::READONLY);
    r.note = d.codecInUse != BtModel::CODEC_AUTO
                 ? BtModel::codecName(d.codecInUse)
                 : "...";
    r.noteTone = MenuRow::TONE_ACCENT;
    return r;
  }
  case R_SIGNAL: {
    const BtModel::Device &d = bt.device(s);
    bool connected =
        bt.linkSlot() == s && bt.link() == BtModel::LINK_CONNECTED;
    MenuRow r = makeRow("Signal", MenuRow::ACT_NONE, MenuRow::READONLY);
    if (d.rssiValid) {
      if (!connected) {
        r.note = "last scan";
        r.noteTone = MenuRow::TONE_DIMMER;
      }
      snprintf(r.value, sizeof(r.value), "%d dBm", (int)d.rssi);
      r.valueTone = MenuRow::TONE_DIM;
      r.valueMono = true;
      r.bars = BtModel::signalBars(d.rssi);
    } else {
      snprintf(r.value, sizeof(r.value), "--");
      r.valueTone = MenuRow::TONE_DIMMER;
    }
    return r;
  }
  case R_RENAME: {
    if (!storage) {
      MenuRow r = makeRow("Rename...", MenuRow::ACT_NONE, MenuRow::DISABLED);
      r.note = "needs SD card";
      r.noteTone = MenuRow::TONE_DISABLED;
      return r;
    }
    const BtModel::Device &d = bt.device(s);
    MenuRow r = makeRow("Rename...", MenuRow::ACT_RENAME);
    if (d.alias[0] && d.name[0]) {
      r.note = d.name; // the device's own name, under the alias
      r.noteTone = MenuRow::TONE_DIMMER;
    }
    return r;
  }
  case R_FORGET:
    return makeRow("Forget device", MenuRow::ACT_FORGET, MenuRow::DESTRUCTIVE);
  case R_FORGET_CANCEL:
    return makeRow("Cancel", MenuRow::ACT_FORGET_CANCEL);
  case R_FORGET_CONFIRM:
    return makeRow("Forget", MenuRow::ACT_FORGET_CONFIRM, MenuRow::DESTRUCTIVE);
  default:
    return makeRow("", MenuRow::ACT_NONE, MenuRow::READONLY);
  }
}

// --- Selection ---------------------------------------------------------------

bool BtMenu::selectable(int i) const {
  if (i < 0 || i >= count)
    return false;
  if (ids[i] == R_INUSE || ids[i] == R_SIGNAL)
    return false; // read-only
  return !(ids[i] == R_RENAME && !storage);
}

// Identifies a row across rebuilds: its device and which row it is.
int BtMenu::rowKey(int i) const { return (slots[i] + 1) * 64 + ids[i]; }

int BtMenu::findKey(int key) const {
  for (int i = 0; i < count; i++)
    if (rowKey(i) == key)
      return i;
  return -1;
}

void BtMenu::clampSelection(Route_t &route) {
  sel = constrain(sel, 0, max(0, count - 1));
  if (!selectable(sel)) {
    int s = sel;
    while (s < count && !selectable(s))
      s++;
    if (s >= count) {
      s = sel;
      while (s > 0 && !selectable(s))
        s--;
    }
    sel = s;
  }
  int visible = scr == FORGET ? 2 : VISIBLE_ROWS;
  top = constrain(top, 0, max(0, count - visible));
  if (sel < top)
    top = sel;
  if (sel >= top + visible)
    top = sel - visible + 1;
  route.selectedIndex = sel;
  route.topVisibleIndex = top;
}

int BtMenu::initialSelection(Screen s, int forSlot) const {
  switch (s) {
  case LIST:
    return bt.radioOn() ? 1 : 0; // "Scan for devices"
  case DEVICE:
    // Connected: on Codec, the likeliest change.
    return forSlot >= 0 && bt.linkSlot() == forSlot &&
                   bt.link() == BtModel::LINK_CONNECTED
               ? 1
               : 0;
  default:
    return 0; // the forget confirmation starts on Cancel
  }
}

void BtMenu::sync(Route_t &route, uint32_t now) {
  Screen next = screenFor(route.type);
  int nextSlot = (next == DEVICE || next == FORGET || next == RENAME)
                     ? (int)route.entityId
                     : -1;
  if (nextSlot >= BtModel::MAX_DEVICES)
    nextSlot = -1;
  // The row under the cursor, to find it again if rows move.
  int key = -1;
  if (next == scr && nextSlot == slot && route.selectedIndex == sel &&
      selectable(sel))
    key = rowKey(sel);

  scr = next;
  slot = nextSlot;
  at = now;
  listIds();

  sel = route.selectedIndex;
  top = route.topVisibleIndex;
  if (key >= 0) {
    int at = findKey(key);
    if (at >= 0)
      sel = at;
  }
  clampSelection(route);
  bt.pin(slot);
}

bool BtMenu::scroll(Route_t &route, int clicks) {
  // Negative clicks scroll down, as in the other lists.
  int dir = clicks < 0 ? 1 : -1;
  int moved = sel;
  for (int n = abs(clicks); n > 0; n--) {
    int s = moved + dir;
    while (s >= 0 && s < count && !selectable(s))
      s += dir;
    if (s < 0 || s >= count)
      break;
    moved = s;
  }
  if (moved == sel)
    return false;
  sel = moved;
  clampSelection(route);
  return true;
}

// --- Actions -----------------------------------------------------------------

BtMenu::Nav BtMenu::push(Route_t::RouteType type, int forSlot,
                         const char *title) {
  Nav nav = {Nav::PUSH, type, forSlot, title,
             initialSelection(screenFor(type), forSlot)};
  return nav;
}

BtMenu::Nav BtMenu::click(Route_t &route, uint32_t now) {
  Nav stay = {Nav::STAY, route.type, slot, nullptr, 0};
  if (!selectable(sel))
    return stay;
  const MenuRow r = row(sel);
  switch (r.action) {
  case MenuRow::ACT_OPEN_BT:
    return push(Route_t::BT_LIST, -1, "Bluetooth");
  case MenuRow::ACT_TOGGLE_RADIO:
    bt.setRadio(!bt.radioOn());
    break;
  case MenuRow::ACT_SCAN:
    if (bt.scanning())
      bt.stopScan();
    else
      bt.startScan(now);
    break;
  case MenuRow::ACT_OPEN_DEVICE:
    return push(Route_t::BT_DEVICE, r.slot, bt.displayName(r.slot));
  case MenuRow::ACT_CONNECT:
    bt.connect(slot, now);
    break;
  case MenuRow::ACT_DISCONNECT:
    bt.disconnect();
    break;
  case MenuRow::ACT_CANCEL_CONNECT:
    bt.cancelConnect();
    break;
  case MenuRow::ACT_CODEC:
    bt.setCodecPref(slot, bt.nextCodec(slot, bt.device(slot).codecPref), now);
    break;
  case MenuRow::ACT_RENAME: {
    const BtModel::Device &d = bt.device(slot);
    text.reset(d.alias[0] ? d.alias : d.name);
    return push(Route_t::BT_RENAME, slot, "Rename");
  }
  case MenuRow::ACT_FORGET:
    return push(Route_t::BT_FORGET, slot, "Forget device?");
  case MenuRow::ACT_FORGET_CANCEL: {
    Nav back = {Nav::BACK, route.type, slot, nullptr, 0};
    return back;
  }
  case MenuRow::ACT_FORGET_CONFIRM: {
    bt.forget(slot);
    Nav back = {Nav::BACK_TO_LIST, Route_t::BT_LIST, -1, nullptr, 0};
    return back;
  }
  default:
    break;
  }
  return stay;
}

BtMenu::Nav BtMenu::renameSave() {
  if (slot >= 0) {
    char name[BtModel::ALIAS_LEN];
    strncpy(name, text.text(), sizeof(name) - 1);
    name[sizeof(name) - 1] = '\0';
    int len = strlen(name);
    while (len > 0 && name[len - 1] == ' ')
      name[--len] = '\0';
    // Its own name isn't an alias.
    if (!strcmp(name, bt.device(slot).name))
      name[0] = '\0';
    bt.setAlias(slot, name);
  }
  Nav back = {Nav::BACK, Route_t::BT_RENAME, slot, nullptr, 0};
  return back;
}

// --- View helpers ------------------------------------------------------------

const char *BtMenu::title() const {
  switch (scr) {
  case SETTINGS:
    return "Settings";
  case DEVICE:
    return deviceName()[0] ? deviceName() : "Bluetooth";
  case FORGET:
    return "Forget device?";
  case RENAME:
    return "Rename";
  default:
    return "Bluetooth";
  }
}

const char *BtMenu::deviceName() const {
  return slot >= 0 && bt.device(slot).used ? bt.displayName(slot) : "";
}

const char *BtMenu::broadcastName() const {
  if (slot < 0)
    return "";
  const BtModel::Device &d = bt.device(slot);
  return d.name[0] ? d.name : d.macText;
}

BtMenu::Hint BtMenu::hint() const {
  if (scr != LIST)
    return HINT_NONE;
  if (!bt.radioOn())
    return HINT_RADIO_OFF;
  if (count <= 2 && !bt.scanning())
    return HINT_NO_DEVICES;
  return HINT_NONE;
}

uint32_t BtMenu::chunkHash(int chunk) const {
  uint32_t h = fnvInt(2166136261u, scr);
  h = fnvInt(h, chunk);
  if (chunk == 0)
    return fnvStr(h, title());

  if (scr == RENAME) {
    if (chunk == 1)
      return fnvInt(h, text.cursor());
    if (chunk >= 2 && chunk <= 5) {
      h = fnvStr(h, text.text());
      return fnvStr(h, broadcastName());
    }
    return h; // the empty band and the footer never change
  }

  if (scr == FORGET && chunk < listTopChunk())
    return fnvStr(h, deviceName());

  // Scrollbar: each row draws its slice of it.
  h = fnvInt(h, count);
  h = fnvInt(h, top);
  h = fnvInt(h, hint());

  int i = top + chunk - listTopChunk();
  if (i < 0 || i >= count)
    return h;
  const MenuRow r = row(i);
  h = fnvInt(h, i == sel);
  h = fnv(h, &r.kind, 3); // kind, action, slot
  h = fnvStr(h, r.label);
  h = fnvInt(h, r.dots);
  h = fnvStr(h, r.note);
  h = fnvInt(h, r.noteTone);
  h = fnvStr(h, r.value);
  h = fnvInt(h, r.valueTone);
  h = fnvInt(h, r.bars);
  h = fnvInt(h, r.trail);
  return fnvInt(h, r.progress);
}

#include "BtModel.h"
#include <stdio.h>

// Out-of-class definitions: C++11 needs them once the constants are bound to
// references (min/constrain).
const int BtModel::MAX_DEVICES;
const int BtModel::NAME_LEN;
const int BtModel::ALIAS_LEN;
const int BtModel::CMD_LEN;
const int BtModel::MAX_COMMANDS;
const int BtModel::TOAST_LEN;
const uint32_t BtModel::SCAN_MS;
const uint32_t BtModel::CONNECT_TIMEOUT_MS;
const uint32_t BtModel::CODEC_DEBOUNCE_MS;
const uint32_t BtModel::RSSI_POLL_MS;
const uint32_t BtModel::FAILED_SHOW_MS;

namespace {

const char *skipSpaces(const char *s) {
  while (*s == ' ')
    s++;
  return s;
}

bool startsWith(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

// Copies the 12 hex digits at s into mac (uppercased). Returns the position
// after them, or nullptr if there aren't 12.
const char *readMac(const char *s, char *mac) {
  s = skipSpaces(s);
  for (int i = 0; i < 12; i++) {
    char c = s[i];
    if (c >= 'a' && c <= 'f')
      c = c - 'a' + 'A';
    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')))
      return nullptr;
    mac[i] = c;
  }
  mac[12] = '\0';
  return s + 12;
}

void copyText(char *out, size_t size, const char *in, size_t len) {
  in = skipSpaces(in);
  size_t n = strlen(in);
  if (len < n)
    n = len;
  if (n > size - 1)
    n = size - 1;
  memcpy(out, in, n);
  // No trailing spaces either.
  while (n > 0 && out[n - 1] == ' ')
    n--;
  out[n] = '\0';
}

} // namespace

BtModel::BtModel()
    : pinned(-1), radio(true), linkState(LINK_IDLE), activeSlot(-1),
      userConnecting(false), userDisconnecting(false),
      codecReconnecting(false), connectedToastDue(false), connectedAtMs(0),
      connectStartMs(0), failedSlot(-1), failedAtMs(0),
      requestedCodecSlot(-1), requestedCodec(CODEC_AUTO), scanActive(false),
      scanStartMs(0), codecPendingSlot(-1), codecApplyAtMs(0),
      lastRssiPollMs(0), lastCfgMask(-1), qHead(0), qCount(0),
      inFlight(CMD_NONE), toastCount(0), toastTaken(0), cfgDirty(false),
      rev(0) {
  memset(devices, 0, sizeof(devices));
  memset(queue, 0, sizeof(queue));
  memset(&toast, 0, sizeof(toast));
}

// --- Command queue -----------------------------------------------------------

void BtModel::enqueue(uint8_t type, int arg) {
  if (qCount >= MAX_COMMANDS)
    return; // the module is not answering; drop rather than block
  queue[(qHead + qCount) % MAX_COMMANDS] = {type, (int8_t)arg};
  qCount++;
  if (takesSlot(type))
    devices[arg].cmdRefs++;
}

bool BtModel::nextCommand(char *out, size_t size) {
  if (inFlight != CMD_NONE || qCount == 0)
    return false;
  Command c = queue[qHead];
  qHead = (qHead + 1) % MAX_COMMANDS;
  qCount--;
  inFlight = c.type;
  char mac[13] = "";
  if (takesSlot(c.type)) {
    Device &d = devices[c.arg];
    macOf(d, mac);
    if (d.cmdRefs && --d.cmdRefs == 0)
      dropUnused();
  }
  switch (c.type) {
  case CMD_DEVSTAT:
    snprintf(out, size, "AT+DEVSTAT");
    break;
  case CMD_CFG_QUERY:
    snprintf(out, size, "AT+A2DPCFG");
    break;
  case CMD_CFG_SET:
    snprintf(out, size, "AT+A2DPCFG=%d", c.arg);
    break;
  case CMD_PLIST:
    snprintf(out, size, "AT+PLIST");
    break;
  case CMD_PLIST_DEL:
    snprintf(out, size, "AT+PLIST=%s", mac);
    break;
  case CMD_A2DPSTAT:
    snprintf(out, size, "AT+A2DPSTAT");
    break;
  case CMD_A2DPENC:
    snprintf(out, size, "AT+A2DPENC");
    break;
  case CMD_CONN:
    snprintf(out, size, "AT+A2DPCONN=%s", mac);
    break;
  case CMD_CONN_LAST:
    snprintf(out, size, "AT+A2DPCONN"); // the last device
    break;
  case CMD_DISC:
    snprintf(out, size, "AT+A2DPDISC");
    break;
  case CMD_BTEN:
    snprintf(out, size, "AT+BTEN=%d", c.arg);
    break;
  case CMD_SCAN:
    snprintf(out, size, "AT+SCAN=%d", c.arg);
    break;
  case CMD_RSSI:
    snprintf(out, size, "AT+RSSI=%s", mac);
    break;
  default:
    inFlight = CMD_NONE;
    return false;
  }
  return true;
}

void BtModel::onCommandDone(bool ok, uint32_t now) {
  if (inFlight == CMD_CONN && !ok && userConnecting)
    failConnect(now);
  else if (inFlight == CMD_CFG_SET && !ok)
    lastCfgMask = -1; // unknown: rewrite it next time
  inFlight = CMD_NONE;
}

void BtModel::onModuleReady() {
  enqueue(CMD_DEVSTAT);
  enqueue(CMD_CFG_QUERY);
  enqueue(CMD_PLIST);
  enqueue(CMD_A2DPSTAT);
}

// --- Module lines ------------------------------------------------------------

void BtModel::onLine(const char *line, uint32_t now) {
  if (line[0] != '+')
    return;
  const char *eq = strchr(line, '=');
  if (!eq)
    return;
  const char *value = eq + 1;

  if (startsWith(line, "+A2DPSTAT=")) {
    int st = atoi(value);
    setLink(st >= 3 ? LINK_CONNECTED : st == 2 ? LINK_CONNECTING : LINK_IDLE,
            now);
  } else if (startsWith(line, "+A2DPDEV=")) {
    parseA2dpDev(value);
  } else if (startsWith(line, "+A2DPENC=")) {
    // 1 SBC, 2 aptX, 3 aptX HD, 4 aptX LL, 5 aptX Adaptive
    static const uint8_t CODECS[] = {CODEC_AUTO,    CODEC_SBC,
                                     CODEC_APTX,    CODEC_APTX_HD,
                                     CODEC_APTX_LL, CODEC_APTX_AD};
    int e = atoi(value);
    if (activeSlot >= 0 && e >= 1 && e <= 5) {
      Device &d = devices[activeSlot];
      d.codecInUse = CODECS[e];
      // Asked for one codec and got another: the device doesn't have it.
      if (requestedCodecSlot == activeSlot && requestedCodec > CODEC_SBC) {
        if (d.codecInUse != requestedCodec)
          d.codecsUnsupported |= 1 << requestedCodec;
        else
          d.codecsUnsupported &= ~(1 << requestedCodec);
      }
      changed();
    }
  } else if (startsWith(line, "+A2DPCFG=")) {
    lastCfgMask = atoi(value);
  } else if (startsWith(line, "+SCAN=")) {
    if (!strcmp(value, "E")) {
      if (scanActive) {
        scanActive = false;
        raiseToast(TOAST_NEUTRAL, "Scan finished", "", "");
        changed();
      }
    } else {
      parseScan(value, now);
    }
  } else if (startsWith(line, "+DEVSTAT=")) {
    // bit 0 powered, bit 3 scanning BR/EDR. This firmware (V6.1.1) ends a
    // scan with +DEVSTAT, not +SCAN=E.
    int st = atoi(value);
    bool on = st & 1;
    if (on != radio) {
      radio = on;
      if (!on) {
        linkState = LINK_IDLE;
        activeSlot = -1;
        for (int i = 0; i < MAX_DEVICES; i++)
          devices[i].connected = false;
        scanActive = false;
      }
      changed();
    }
    bool scanBit = st & 8;
    if (scanActive && !scanBit && now - scanStartMs > 500) {
      scanActive = false;
      raiseToast(TOAST_NEUTRAL, "Scan finished", "", "");
      changed();
    } else if (!scanActive && scanBit) {
      scanActive = true; // started from the debug console
      scanStartMs = now;
      changed();
    }
  } else if (startsWith(line, "+PLIST=")) {
    if (!strcmp(value, "E")) {
      for (int i = 0; i < MAX_DEVICES; i++) {
        Device &d = devices[i];
        if (d.used && d.paired && !d.listed)
          d.paired = false;
        d.listed = false;
      }
      dropUnused();
      changed();
    } else {
      parsePaired(value);
    }
  } else if (startsWith(line, "+PAIRED=")) {
    enqueue(CMD_PLIST);
  } else if (startsWith(line, "+RSSI=")) {
    char mac[13];
    const char *p = readMac(value, mac);
    int slot = p ? findSlot(mac) : -1;
    if (slot >= 0 && *p == ',') {
      Device &d = devices[slot];
      d.rssi = constrain(atoi(p + 1), -127, 0);
      d.rssiValid = true;
      d.rssiAtMs = now;
      changed();
    }
  }
}

void BtModel::parseScan(const char *value, uint32_t now) {
  // <index>,<addr type>,<MAC>,<RSSI>,<name length>,<name>,<class>
  char mac[13];
  int index, type, rssi, nameLen, consumed = 0;
  if (sscanf(value, "%d,%d,%12[0-9A-Fa-f],%d,%d,%n", &index, &type, mac,
             &rssi, &nameLen, &consumed) < 5 ||
      consumed == 0)
    return;
  if (type != 2 || strlen(mac) != 12)
    return; // BLE
  char upper[13];
  if (!readMac(mac, upper))
    return;
  int slot = upsert(upper);
  if (slot < 0)
    return;
  Device &d = devices[slot];
  // The name can contain commas, so it's cut by its length, not parsed.
  if (nameLen > 0)
    copyText(d.name, sizeof(d.name), value + consumed, nameLen);
  d.rssi = constrain(rssi, -127, 0);
  d.rssiValid = true;
  d.rssiAtMs = now;
  d.seen = true;
  changed();
}

void BtModel::parsePaired(const char *value) {
  // <index>,<MAC>{,<name>}, sometimes with a space after the commas
  const char *comma = strchr(value, ',');
  char mac[13];
  const char *p = comma ? readMac(comma + 1, mac) : nullptr;
  if (!p)
    return;
  int slot = upsert(mac);
  if (slot < 0)
    return;
  Device &d = devices[slot];
  d.paired = true;
  d.listed = true;
  if (*p == ',' && *skipSpaces(p + 1))
    copyText(d.name, sizeof(d.name), p + 1, NAME_LEN);
  changed();
}

void BtModel::parseA2dpDev(const char *value) {
  // <MAC>{,<name>}
  char mac[13];
  const char *p = readMac(value, mac);
  if (!p)
    return;
  int slot = upsert(mac);
  if (slot < 0)
    return;
  Device &d = devices[slot];
  if (*p == ',' && *skipSpaces(p + 1))
    copyText(d.name, sizeof(d.name), p + 1, NAME_LEN);
  activeSlot = slot;
  if (linkState == LINK_CONNECTED)
    markConnected(slot);
  changed();
}

// --- Link state --------------------------------------------------------------

void BtModel::setLink(Link next, uint32_t now) {
  Link prev = linkState;
  if (next == prev)
    return;

  if (next == LINK_IDLE && codecReconnecting) {
    // The drop we asked for to apply a codec (or to switch devices): the
    // connect queued behind it follows, so stay "connecting".
    codecReconnecting = false;
    for (int i = 0; i < MAX_DEVICES; i++) {
      devices[i].connected = false;
      devices[i].codecInUse = CODEC_AUTO;
    }
    linkState = LINK_CONNECTING;
    changed();
    return;
  }

  linkState = next;
  changed();

  if (next == LINK_CONNECTED) {
    userConnecting = false;
    failedSlot = -1;
    connectedAtMs = now;
    connectedToastDue = true;
    if (activeSlot >= 0)
      markConnected(activeSlot);
    enqueue(CMD_A2DPENC);
    return;
  }
  if (next == LINK_CONNECTING)
    return;

  // Dropped to idle.
  const char *name = activeSlot >= 0 ? displayName(activeSlot) : "device";
  if (prev == LINK_CONNECTED) {
    if (userDisconnecting) {
      raiseToast(TOAST_NEUTRAL, "Disconnected", "", "");
    } else {
      raiseToast(TOAST_ERROR, "Lost ", name, " \xB7 paused");
      if (onLinkLost)
        onLinkLost();
    }
  } else if (userConnecting) {
    failConnect(now);
  }
  userDisconnecting = false;
  connectedToastDue = false;
  for (int i = 0; i < MAX_DEVICES; i++) {
    devices[i].connected = false;
    devices[i].codecInUse = CODEC_AUTO;
  }
  activeSlot = -1;
  dropUnused();
}

void BtModel::markConnected(int slot) {
  for (int i = 0; i < MAX_DEVICES; i++)
    devices[i].connected = (i == slot);
  activeSlot = slot;
  if (connectedToastDue) {
    connectedToastDue = false;
    raiseToast(TOAST_SUCCESS, "Connected to ", displayName(slot), "");
  }
}

void BtModel::failConnect(uint32_t now) {
  userConnecting = false;
  codecReconnecting = false;
  failedSlot = activeSlot;
  failedAtMs = now;
  activeSlot = -1;
  linkState = LINK_IDLE;
  raiseToast(TOAST_ERROR, "Couldn't connect", "", "");
  changed();
}

bool BtModel::connectFailed(int slot, uint32_t now) const {
  return slot >= 0 && slot == failedSlot && now - failedAtMs < FAILED_SHOW_MS;
}

// --- User actions ------------------------------------------------------------

int BtModel::codecMask(uint8_t codec) {
  // AT+A2DPCFG bits: 1 aptX, 2 aptX LL, 3 aptX HD, 4 aptX Adaptive
  switch (codec) {
  case CODEC_SBC:
    return 0;
  case CODEC_APTX:
    return 1 << 1;
  case CODEC_APTX_LL:
    return 1 << 2;
  case CODEC_APTX_HD:
    return 1 << 3;
  case CODEC_APTX_AD:
    return 1 << 4;
  default:
    return (1 << 1) | (1 << 2) | (1 << 3) | (1 << 4);
  }
}

void BtModel::queueConnect(int slot) {
  Device &d = devices[slot];
  int mask = codecMask(d.codecPref);
  if (mask != lastCfgMask) {
    enqueue(CMD_CFG_SET, mask);
    lastCfgMask = mask;
  }
  requestedCodecSlot = slot;
  requestedCodec = d.codecPref;
  enqueue(CMD_CONN, slot);
}

void BtModel::connect(int slot, uint32_t now) {
  if (!radio || slot < 0 || slot >= MAX_DEVICES || !devices[slot].used)
    return;
  if (linkState != LINK_IDLE && activeSlot == slot)
    return;
  if (linkState != LINK_IDLE) {
    codecReconnecting = true; // switching devices: no "Disconnected"
    enqueue(CMD_DISC);
  }
  queueConnect(slot);
  userConnecting = true;
  activeSlot = slot;
  connectStartMs = now;
  failedSlot = -1;
  linkState = LINK_CONNECTING;
  changed();
}

void BtModel::cancelConnect() {
  if (linkState != LINK_CONNECTING)
    return;
  userConnecting = false;
  codecReconnecting = false;
  userDisconnecting = true;
  enqueue(CMD_DISC);
  linkState = LINK_IDLE;
  activeSlot = -1;
  changed();
}

void BtModel::disconnect() {
  if (linkState != LINK_CONNECTED)
    return;
  userDisconnecting = true;
  enqueue(CMD_DISC);
}

void BtModel::setRadio(bool on) {
  if (on == radio)
    return;
  radio = on;
  if (on) {
    enqueue(CMD_BTEN, 1);
    enqueue(CMD_CONN_LAST); // the last device, like at power-on
  } else {
    enqueue(CMD_BTEN, 0);
    linkState = LINK_IDLE;
    activeSlot = -1;
    userConnecting = false;
    codecReconnecting = false;
    connectedToastDue = false;
    scanActive = false;
    for (int i = 0; i < MAX_DEVICES; i++)
      devices[i].connected = false;
  }
  changed();
}

void BtModel::startScan(uint32_t now) {
  if (!radio || scanActive)
    return;
  // A fresh list: devices from an older scan go, paired ones keep their
  // last signal ("last scan").
  for (int i = 0; i < MAX_DEVICES; i++)
    devices[i].seen = false;
  dropUnused();
  enqueue(CMD_SCAN, 1);
  scanActive = true;
  scanStartMs = now;
  changed();
}

void BtModel::stopScan() {
  if (!scanActive)
    return;
  enqueue(CMD_SCAN, 0);
  scanActive = false;
  changed();
}

uint32_t BtModel::scanRemainingMs(uint32_t now) const {
  if (!scanActive)
    return 0;
  uint32_t elapsed = now - scanStartMs;
  return elapsed >= SCAN_MS ? 0 : SCAN_MS - elapsed;
}

void BtModel::forget(int slot) {
  if (slot < 0 || slot >= MAX_DEVICES || !devices[slot].used)
    return;
  Device &d = devices[slot];
  char name[NAME_LEN];
  strncpy(name, displayName(slot), sizeof(name) - 1);
  name[sizeof(name) - 1] = '\0';
  if (activeSlot == slot && linkState != LINK_IDLE) {
    enqueue(CMD_DISC);
    linkState = LINK_IDLE; // quietly: the toast says "Forgot"
    activeSlot = -1;
    userConnecting = false;
    d.connected = false;
  }
  enqueue(CMD_PLIST_DEL, slot);
  d.paired = false;
  d.alias[0] = '\0';
  d.codecPref = CODEC_AUTO;
  d.codecsUnsupported = 0;
  cfgDirty = true;
  raiseToast(TOAST_SUCCESS, "Forgot ", name, "");
  dropUnused();
  changed();
}

void BtModel::setCodecPref(int slot, uint8_t codec, uint32_t now) {
  if (slot < 0 || slot >= MAX_DEVICES || codec >= CODEC_COUNT)
    return;
  Device &d = devices[slot];
  if (d.codecPref == codec)
    return;
  d.codecPref = codec;
  cfgDirty = true;
  changed();
  if (activeSlot == slot && linkState == LINK_CONNECTED) {
    codecPendingSlot = slot;
    codecApplyAtMs = now + CODEC_DEBOUNCE_MS;
  }
}

void BtModel::setAlias(int slot, const char *alias) {
  if (slot < 0 || slot >= MAX_DEVICES)
    return;
  Device &d = devices[slot];
  copyText(d.alias, sizeof(d.alias), alias ? alias : "", ALIAS_LEN);
  cfgDirty = true;
  raiseToast(TOAST_SUCCESS, "Name saved", "", "");
  changed();
}

void BtModel::pin(int slot) {
  if (slot == pinned)
    return;
  pinned = slot;
  lastRssiPollMs = 0;
  dropUnused();
}

void BtModel::tick(uint32_t now) {
  if (scanActive && now - scanStartMs > SCAN_MS + 3000) {
    // No end event: stop it ourselves.
    enqueue(CMD_SCAN, 0);
    scanActive = false;
    raiseToast(TOAST_NEUTRAL, "Scan finished", "", "");
    changed();
  }

  if (userConnecting && linkState == LINK_CONNECTING &&
      now - connectStartMs > CONNECT_TIMEOUT_MS) {
    enqueue(CMD_DISC);
    failConnect(now);
  }

  if (connectedToastDue && now - connectedAtMs > 1000) {
    connectedToastDue = false; // never got the device's name
    raiseToast(TOAST_SUCCESS, "Connected", "", "");
  }

  if (codecPendingSlot >= 0 && (int32_t)(now - codecApplyAtMs) >= 0) {
    int slot = codecPendingSlot;
    codecPendingSlot = -1;
    if (activeSlot == slot && linkState == LINK_CONNECTED) {
      codecReconnecting = true;
      enqueue(CMD_DISC);
      queueConnect(slot);
      userConnecting = true;
      connectStartMs = now;
      linkState = LINK_CONNECTING;
      changed();
    }
  }

  if (pinned >= 0 && pinned == activeSlot && linkState == LINK_CONNECTED &&
      qCount == 0 &&
      (lastRssiPollMs == 0 || now - lastRssiPollMs >= RSSI_POLL_MS)) {
    enqueue(CMD_RSSI, pinned);
    lastRssiPollMs = now ? now : 1;
  }
}

// --- Device table ------------------------------------------------------------

void BtModel::macOf(const Device &d, char out[13]) {
  for (int i = 0; i < 6; i++) {
    out[2 * i] = d.macText[3 * i];
    out[2 * i + 1] = d.macText[3 * i + 1];
  }
  out[12] = '\0';
}

int BtModel::findSlot(const char *mac) const {
  char m[13];
  for (int i = 0; i < MAX_DEVICES; i++) {
    if (!devices[i].used)
      continue;
    macOf(devices[i], m);
    if (!strcmp(m, mac))
      return i;
  }
  return -1;
}

bool BtModel::hasConfig(const Device &d) const {
  return d.alias[0] || d.codecPref != CODEC_AUTO;
}

int BtModel::upsert(const char *mac) {
  int slot = findSlot(mac);
  if (slot >= 0)
    return slot;
  for (int i = 0; i < MAX_DEVICES && slot < 0; i++)
    if (!devices[i].used)
      slot = i;
  if (slot < 0) {
    // Full: replace the weakest device we know nothing else about.
    for (int i = 0; i < MAX_DEVICES; i++) {
      const Device &d = devices[i];
      if (d.paired || d.connected || i == pinned || i == activeSlot ||
          hasConfig(d) || d.cmdRefs)
        continue;
      if (slot < 0 || !d.rssiValid ||
          (devices[slot].rssiValid && d.rssi < devices[slot].rssi))
        slot = i;
    }
    if (slot < 0)
      return -1;
  }
  Device &d = devices[slot];
  memset(&d, 0, sizeof(d));
  d.used = true;
  snprintf(d.macText, sizeof(d.macText), "%.2s:%.2s:%.2s:%.2s:%.2s:%.2s", mac,
           mac + 2, mac + 4, mac + 6, mac + 8, mac + 10);
  d.codecPref = CODEC_AUTO;
  return slot;
}

void BtModel::drop(int slot) { memset(&devices[slot], 0, sizeof(Device)); }

void BtModel::dropUnused() {
  for (int i = 0; i < MAX_DEVICES; i++) {
    const Device &d = devices[i];
    if (d.used && !d.paired && !d.connected && !d.seen && !hasConfig(d) &&
        !d.cmdRefs && i != pinned && i != activeSlot)
      drop(i);
  }
}

bool BtModel::isListed(int slot) const {
  const Device &d = devices[slot];
  return d.used && (d.paired || d.connected || d.seen || slot == activeSlot);
}

const char *BtModel::displayName(int slot) const {
  const Device &d = devices[slot];
  if (d.alias[0])
    return d.alias;
  if (d.name[0])
    return d.name;
  return d.macText;
}

int BtModel::orderedSlots(int *out, int max) const {
  int n = 0;
  // Connected (or connecting) first
  if (activeSlot >= 0 && devices[activeSlot].used && n < max)
    out[n++] = activeSlot;
  // Then paired, in the module's order
  for (int i = 0; i < MAX_DEVICES && n < max; i++)
    if (i != activeSlot && devices[i].used && devices[i].paired)
      out[n++] = i;
  // Then scan results by signal, strongest first
  int firstUnpaired = n;
  for (int i = 0; i < MAX_DEVICES && n < max; i++) {
    const Device &d = devices[i];
    if (i == activeSlot || !d.used || d.paired || !d.seen)
      continue;
    int at = n++;
    while (at > firstUnpaired && devices[out[at - 1]].rssi < d.rssi) {
      out[at] = out[at - 1];
      at--;
    }
    out[at] = i;
  }
  return n;
}

BtModel::Icon BtModel::icon() const {
  if (!radio)
    return ICON_OFF;
  if (linkState == LINK_CONNECTED)
    return ICON_CONNECTED;
  if (linkState == LINK_CONNECTING)
    return ICON_CONNECTING;
  return ICON_ON;
}

uint8_t BtModel::nextCodec(int slot, uint8_t codec) const {
  uint8_t unsupported = devices[slot].codecsUnsupported;
  for (int i = 1; i <= CODEC_COUNT; i++) {
    uint8_t c = (codec + i) % CODEC_COUNT;
    if (!(unsupported & (1 << c)))
      return c;
  }
  return codec;
}

const char *BtModel::codecName(uint8_t codec) {
  static const char *const NAMES[CODEC_COUNT] = {
      "Auto", "SBC", "aptX", "aptX HD", "aptX LL", "aptX Adaptive"};
  return codec < CODEC_COUNT ? NAMES[codec] : "?";
}

int BtModel::signalBars(int rssi) {
  if (rssi >= -60)
    return 3;
  if (rssi >= -75)
    return 2;
  return 1;
}

// --- Toasts ------------------------------------------------------------------

void BtModel::raiseToast(uint8_t tone, const char *before, const char *name,
                         const char *after) {
  toast.tone = tone;
  snprintf(toast.text, sizeof(toast.text), "%s%s%s", before, name, after);
  size_t start = strlen(before);
  size_t len = strlen(name);
  if (start > sizeof(toast.text) - 1)
    start = sizeof(toast.text) - 1;
  if (start + len > sizeof(toast.text) - 1)
    len = sizeof(toast.text) - 1 - start;
  toast.nameStart = start;
  toast.nameLen = len;
  toastCount++;
}

bool BtModel::takeToast(Toast &out) {
  if (toastTaken == toastCount)
    return false;
  out = toast;
  toastTaken = toastCount;
  return true;
}

// --- Saved settings ----------------------------------------------------------

bool BtModel::configLine(int &slot, char *out, size_t size) const {
  for (; slot < MAX_DEVICES; slot++) {
    const Device &d = devices[slot];
    if (!d.used || !hasConfig(d))
      continue;
    char mac[13];
    macOf(d, mac);
    snprintf(out, size, "%s\t%u\t%s\n", mac, (unsigned)d.codecPref,
             d.alias);
    slot++;
    return true;
  }
  return false;
}

int BtModel::serializeConfig(char *out, size_t size) {
  size_t n = 0;
  out[0] = '\0';
  int slot = 0;
  while (n + 1 < size && configLine(slot, out + n, size - n))
    n += strlen(out + n);
  markConfigSaved();
  return (int)n;
}

void BtModel::loadConfigLine(const char *line) {
  char mac[13];
  const char *p = readMac(line, mac);
  if (!p || *p != '\t')
    return;
  char *end;
  long codec = strtol(p + 1, &end, 10);
  if (end == p + 1)
    return;
  int slot = upsert(mac);
  if (slot < 0)
    return;
  Device &d = devices[slot];
  d.codecPref = codec >= 0 && codec < CODEC_COUNT ? codec : CODEC_AUTO;
  if (*end == '\t')
    copyText(d.alias, sizeof(d.alias), end + 1, ALIAS_LEN);
  changed();
}

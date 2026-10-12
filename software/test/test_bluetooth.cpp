#include <doctest.h>
#include <string>
#include <vector>

#include "../src/bluetooth/BtModel.h"
#include "../src/state/text_entry.h"

namespace {

// Drains the model's command queue, answering OK to each.
std::vector<std::string> drain(BtModel &bt, uint32_t now = 0) {
  std::vector<std::string> out;
  char cmd[BtModel::CMD_LEN];
  while (bt.nextCommand(cmd, sizeof(cmd))) {
    out.push_back(cmd);
    bt.onCommandDone(true, now);
  }
  return out;
}

bool has(const std::vector<std::string> &cmds, const std::string &cmd) {
  for (const auto &c : cmds)
    if (c == cmd)
      return true;
  return false;
}

std::string name(const BtModel &bt, int slot) { return bt.displayName(slot); }

// A model that has read its paired list: Sony (paired) and JBL (paired).
void withPaired(BtModel &bt) {
  bt.onLine("+PLIST=1,1C5CF226D773, Sony WH-1000XM4", 0);
  bt.onLine("+PLIST=2, A0BC30075421, JBL Charge 4", 0);
  bt.onLine("+PLIST=E", 0);
}

int ordered(const BtModel &bt, std::vector<int> &out) {
  int slots[BtModel::MAX_DEVICES];
  int n = bt.orderedSlots(slots, BtModel::MAX_DEVICES);
  out.assign(slots, slots + n);
  return n;
}

} // namespace

TEST_SUITE("Bluetooth model") {
  TEST_CASE("Paired list fills the table, spaces after commas are trimmed") {
    BtModel bt;
    withPaired(bt);
    int sony = bt.findSlot("1C5CF226D773");
    int jbl = bt.findSlot("A0BC30075421");
    REQUIRE(sony >= 0);
    REQUIRE(jbl >= 0);
    CHECK(bt.device(sony).paired);
    CHECK(name(bt, sony) == "Sony WH-1000XM4");
    CHECK(name(bt, jbl) == "JBL Charge 4");
    CHECK(std::string(bt.device(jbl).macText) == "A0:BC:30:07:54:21");

    // Read again without Sony: it's no longer paired, and dropped.
    bt.onLine("+PLIST=1,A0BC30075421,JBL Charge 4", 0);
    bt.onLine("+PLIST=E", 0);
    CHECK(bt.findSlot("1C5CF226D773") < 0);
    CHECK(bt.device(jbl).paired);
  }

  TEST_CASE("Scan results: name cut by its length, sorted by signal") {
    BtModel bt;
    bt.startScan(0);
    CHECK(bt.scanning());
    CHECK(has(drain(bt), "AT+SCAN=1"));
    bt.onLine("+SCAN=1,2,DC0D30000057,-80,10,Far, Away!,240408", 100);
    bt.onLine("+SCAN=2,2,DC0D30000012,-50,4,Near,240404", 200);
    bt.onLine("+SCAN=3,0,DC0D30000099,-40,3,BLE,0", 300); // LE: ignored
    std::vector<int> list;
    REQUIRE(ordered(bt, list) == 2);
    CHECK(name(bt, list[0]) == "Near");
    CHECK(name(bt, list[1]) == "Far, Away!");
    CHECK(bt.device(list[0]).rssi == -50);
    CHECK(BtModel::signalBars(-50) == 3);
    CHECK(BtModel::signalBars(-70) == 2);
    CHECK(BtModel::signalBars(-80) == 1);

    // This firmware ends a scan with +DEVSTAT (9 -> 1), not +SCAN=E.
    bt.onLine("+DEVSTAT=9", 400);
    CHECK(bt.scanning());
    bt.onLine("+DEVSTAT=1", 9000);
    CHECK_FALSE(bt.scanning());
    BtModel::Toast t;
    REQUIRE(bt.takeToast(t));
    CHECK(std::string(t.text) == "Scan finished");
    CHECK(t.tone == BtModel::TOAST_NEUTRAL);
  }

  TEST_CASE("List order: connected, paired, then scan results") {
    BtModel bt;
    withPaired(bt);
    bt.startScan(0);
    bt.onLine("+SCAN=1,2,DC0D30000057,-45,5,Other,240408", 10);
    bt.onLine("+A2DPDEV=A0BC30075421,JBL Charge 4", 20);
    bt.onLine("+A2DPSTAT=3", 20);
    std::vector<int> list;
    REQUIRE(ordered(bt, list) == 3);
    CHECK(name(bt, list[0]) == "JBL Charge 4");
    CHECK(bt.device(list[0]).connected);
    CHECK(name(bt, list[1]) == "Sony WH-1000XM4");
    CHECK(name(bt, list[2]) == "Other");
  }

  TEST_CASE("Auto-reconnect at boot: icon and success toast") {
    BtModel bt;
    CHECK(bt.icon() == BtModel::ICON_ON);
    bt.onLine("+A2DPSTAT=2", 0);
    CHECK(bt.icon() == BtModel::ICON_CONNECTING);
    bt.onLine("+A2DPSTAT=3", 10);
    bt.onLine("+A2DPDEV=1C5CF226D773,Sony WH-1000XM4", 20);
    CHECK(bt.icon() == BtModel::ICON_CONNECTED);
    BtModel::Toast t;
    REQUIRE(bt.takeToast(t));
    CHECK(t.tone == BtModel::TOAST_SUCCESS);
    CHECK(std::string(t.text) == "Connected to Sony WH-1000XM4");
    CHECK(std::string(t.text + t.nameStart, t.nameLen) == "Sony WH-1000XM4");
    CHECK(has(drain(bt), "AT+A2DPENC"));
    bt.onLine("+A2DPENC=3", 30);
    CHECK(bt.device(bt.linkSlot()).codecInUse == BtModel::CODEC_APTX_HD);
  }

  TEST_CASE("Unexpected drop pauses and shows an error toast") {
    static int lost = 0;
    lost = 0;
    BtModel bt;
    bt.onLinkLost = [] { lost++; };
    bt.onLine("+A2DPDEV=1C5CF226D773,Sony", 0);
    bt.onLine("+A2DPSTAT=3", 0);
    BtModel::Toast t;
    bt.takeToast(t);
    bt.onLine("+A2DPSTAT=1", 100);
    CHECK(lost == 1);
    REQUIRE(bt.takeToast(t));
    CHECK(t.tone == BtModel::TOAST_ERROR);
    CHECK(std::string(t.text) == "Lost Sony \xB7 paused");
    CHECK(bt.icon() == BtModel::ICON_ON);
  }

  TEST_CASE("User disconnect: neutral toast, no pause") {
    static int lost = 0;
    lost = 0;
    BtModel bt;
    bt.onLinkLost = [] { lost++; };
    bt.onLine("+A2DPDEV=1C5CF226D773,Sony", 0);
    bt.onLine("+A2DPSTAT=3", 0);
    BtModel::Toast t;
    bt.takeToast(t);
    bt.disconnect();
    CHECK(has(drain(bt), "AT+A2DPDISC"));
    bt.onLine("+A2DPSTAT=1", 100);
    CHECK(lost == 0);
    REQUIRE(bt.takeToast(t));
    CHECK(std::string(t.text) == "Disconnected");
  }

  TEST_CASE("Connect writes the codec setting only when it changes") {
    BtModel bt;
    withPaired(bt);
    bt.onLine("+A2DPCFG=30", 0); // Auto, as read at boot
    int sony = bt.findSlot("1C5CF226D773");
    bt.connect(sony, 0);
    CHECK(bt.link() == BtModel::LINK_CONNECTING);
    auto cmds = drain(bt);
    CHECK(cmds.size() == 1);
    CHECK(cmds[0] == "AT+A2DPCONN=1C5CF226D773");
    bt.onLine("+A2DPSTAT=1", 10); // gave up
    CHECK(bt.connectFailed(sony, 100));
    CHECK_FALSE(bt.connectFailed(sony, 100 + BtModel::FAILED_SHOW_MS));
    BtModel::Toast t;
    REQUIRE(bt.takeToast(t));
    CHECK(std::string(t.text) == "Couldn't connect");

    bt.setCodecPref(sony, BtModel::CODEC_APTX_HD, 0);
    CHECK(bt.configDirty());
    bt.connect(sony, 200);
    cmds = drain(bt);
    REQUIRE(cmds.size() == 2);
    CHECK(cmds[0] == "AT+A2DPCFG=8");
    CHECK(cmds[1] == "AT+A2DPCONN=1C5CF226D773");
  }

  TEST_CASE("Codec cycling while connected reconnects once, after 1 s") {
    BtModel bt;
    withPaired(bt);
    bt.onLine("+A2DPCFG=30", 0);
    int sony = bt.findSlot("1C5CF226D773");
    bt.onLine("+A2DPDEV=1C5CF226D773,Sony", 0);
    bt.onLine("+A2DPSTAT=3", 0);
    drain(bt);

    bt.setCodecPref(sony, BtModel::CODEC_SBC, 1000);
    bt.tick(1500);
    bt.setCodecPref(sony, BtModel::CODEC_APTX, 1600);
    bt.tick(2000);
    CHECK(drain(bt).empty()); // still waiting
    bt.setCodecPref(sony, BtModel::CODEC_APTX_LL, 2100);
    bt.tick(3200);
    auto cmds = drain(bt);
    REQUIRE(cmds.size() == 3);
    CHECK(cmds[0] == "AT+A2DPDISC");
    CHECK(cmds[1] == "AT+A2DPCFG=4");
    CHECK(cmds[2] == "AT+A2DPCONN=1C5CF226D773");

    // The drop is ours: no toast, no pause, still "connecting".
    BtModel::Toast t;
    bt.takeToast(t);
    bt.onLine("+A2DPSTAT=1", 3300);
    CHECK(bt.link() == BtModel::LINK_CONNECTING);
    CHECK_FALSE(bt.takeToast(t));
    bt.onLine("+A2DPSTAT=3", 3400);
    bt.onLine("+A2DPDEV=1C5CF226D773,Sony", 3400);
    // Asked for aptX LL, got SBC: LL is skipped from now on.
    bt.onLine("+A2DPENC=1", 3500);
    CHECK(bt.nextCodec(sony, BtModel::CODEC_APTX) == BtModel::CODEC_APTX_HD);
    CHECK(bt.nextCodec(sony, BtModel::CODEC_APTX_HD) == BtModel::CODEC_APTX_AD);
    CHECK(bt.nextCodec(sony, BtModel::CODEC_APTX_AD) == BtModel::CODEC_AUTO);
  }

  TEST_CASE("Forget unpairs, clears the saved settings and toasts") {
    BtModel bt;
    withPaired(bt);
    int jbl = bt.findSlot("A0BC30075421");
    bt.setAlias(jbl, "kitchen");
    CHECK(name(bt, jbl) == "kitchen");
    BtModel::Toast t;
    REQUIRE(bt.takeToast(t));
    CHECK(std::string(t.text) == "Name saved");
    bt.forget(jbl);
    CHECK(has(drain(bt), "AT+PLIST=A0BC30075421"));
    REQUIRE(bt.takeToast(t));
    CHECK(std::string(t.text) == "Forgot kitchen");
    CHECK(bt.findSlot("A0BC30075421") < 0);
  }

  TEST_CASE("Config round trip") {
    BtModel bt;
    withPaired(bt);
    bt.setAlias(bt.findSlot("A0BC30075421"), "kitchen");
    bt.setCodecPref(bt.findSlot("1C5CF226D773"), BtModel::CODEC_APTX_HD, 0);
    char buf[256];
    bt.serializeConfig(buf, sizeof(buf));
    CHECK_FALSE(bt.configDirty());
    CHECK(std::string(buf) ==
          "1C5CF226D773\t3\t\nA0BC30075421\t0\tkitchen\n");

    BtModel loaded;
    loaded.loadConfigLine("1C5CF226D773\t3\t");
    loaded.loadConfigLine("A0BC30075421\t0\tkitchen");
    loaded.loadConfigLine("garbage");
    withPaired(loaded);
    CHECK(name(loaded, loaded.findSlot("A0BC30075421")) == "kitchen");
    CHECK(loaded.device(loaded.findSlot("1C5CF226D773")).codecPref ==
          BtModel::CODEC_APTX_HD);
    CHECK_FALSE(loaded.configDirty());
  }

  TEST_CASE("Radio off: icon off, links dropped quietly") {
    BtModel bt;
    bt.onLine("+A2DPDEV=1C5CF226D773,Sony", 0);
    bt.onLine("+A2DPSTAT=3", 0);
    BtModel::Toast t;
    bt.takeToast(t);
    bt.setRadio(false);
    CHECK(bt.icon() == BtModel::ICON_OFF);
    CHECK(has(drain(bt), "AT+BTEN=0"));
    bt.onLine("+A2DPSTAT=1", 10);
    CHECK_FALSE(bt.takeToast(t));
    bt.setRadio(true);
    auto cmds = drain(bt);
    CHECK(has(cmds, "AT+BTEN=1"));
    CHECK(has(cmds, "AT+A2DPCONN"));
  }

  TEST_CASE("RSSI is polled for the pinned connected device") {
    BtModel bt;
    bt.onLine("+A2DPDEV=1C5CF226D773,Sony", 0);
    bt.onLine("+A2DPSTAT=3", 0);
    drain(bt);
    int sony = bt.findSlot("1C5CF226D773");
    bt.pin(sony);
    bt.tick(100);
    CHECK(has(drain(bt), "AT+RSSI=1C5CF226D773"));
    bt.tick(2000);
    CHECK(drain(bt).empty());
    bt.onLine("+RSSI=1C5CF226D773,-54", 2100);
    CHECK(bt.device(sony).rssi == -54);
    bt.tick(5200);
    CHECK(has(drain(bt), "AT+RSSI=1C5CF226D773"));
  }
}

TEST_SUITE("Text entry") {
  TEST_CASE("Clamped ribbon (search)") {
    TextEntry e("ABC", 4, false);
    CHECK(e.cursorChar() == 'A');
    CHECK_FALSE(e.moveCursor(-1));
    CHECK(e.moveCursor(5));
    CHECK(e.cursorChar() == 'C');
    CHECK_FALSE(e.space()); // not first
    CHECK(e.append());
    CHECK(e.space());
    CHECK_FALSE(e.space()); // not twice
    CHECK(e.append());
    CHECK(e.append());
    CHECK_FALSE(e.append()); // full
    CHECK(std::string(e.text()) == "C CC");
    CHECK(e.backspace());
    CHECK(std::string(e.text()) == "C C");
  }

  TEST_CASE("Wrapping ribbon (rename), initial text cut to the max") {
    TextEntry e("abc0", 24, true);
    CHECK(e.moveCursor(-1));
    CHECK(e.cursorChar() == '0');
    CHECK(e.moveCursor(1));
    CHECK(e.cursorChar() == 'a');
    CHECK(e.moveCursor(9)); // 9 % 4 = 1
    CHECK(e.cursorChar() == 'b');
    e.reset("a name that is far too long to fit");
    CHECK(e.length() == 24);
    CHECK(e.cursorChar() == 'a');
  }
}

#include "../src/state/bt_menu.h"
#include "../src/state/state.h"
#include "test_helpers.h"

namespace {

Route_t menuRoute(Route_t::RouteType type, int slot = 0, int selected = 0) {
  Route_t r;
  memset(&r, 0, sizeof(r));
  r.type = type;
  r.entityId = slot;
  r.selectedIndex = selected;
  return r;
}

std::string label(const BtMenu &m, int i) { return m.row(i).label; }

} // namespace

TEST_SUITE("Bluetooth menu") {
  TEST_CASE("Settings: Bluetooth row shows Off / On / the connected device") {
    BtModel bt;
    BtMenu menu(bt);
    Route_t route = menuRoute(Route_t::SETTINGS);
    menu.sync(route, 0);
    REQUIRE(menu.rowCount() == 5);
    CHECK(label(menu, 0) == "Bluetooth");
    CHECK(std::string(menu.row(0).note) == "On");
    CHECK(menu.row(0).trail == MenuRow::TRAIL_CHEVRON);
    CHECK(menu.row(4).kind == MenuRow::DESTRUCTIVE);

    bt.onLine("+A2DPDEV=1C5CF226D773,Sony", 0);
    bt.onLine("+A2DPSTAT=3", 0);
    menu.sync(route, 0);
    CHECK(std::string(menu.row(0).note) == "Sony");
    CHECK(menu.row(0).noteTone == MenuRow::TONE_ACCENT);

    BtMenu::Nav nav = menu.click(route, 0);
    CHECK(nav.kind == BtMenu::Nav::PUSH);
    CHECK(nav.route == Route_t::BT_LIST);
    CHECK(nav.selected == 1); // "Scan for devices"
  }

  TEST_CASE("List: radio off, empty, scanning, populated") {
    BtModel bt;
    BtMenu menu(bt);
    Route_t route = menuRoute(Route_t::BT_LIST, 0, 1);

    bt.setRadio(false);
    menu.sync(route, 0);
    CHECK(menu.rowCount() == 1);
    CHECK(menu.hint() == BtMenu::HINT_RADIO_OFF);
    CHECK(menu.row(0).trail == MenuRow::TRAIL_SWITCH_OFF);
    CHECK(menu.selected() == 0);

    bt.setRadio(true);
    menu.sync(route, 0);
    CHECK(menu.rowCount() == 2);
    CHECK(menu.hint() == BtMenu::HINT_NO_DEVICES);

    // Center on "Scan for devices"
    menu.scroll(route, -1);
    CHECK(menu.selected() == 1);
    menu.click(route, 1000);
    CHECK(bt.scanning());
    menu.sync(route, 1000);
    CHECK(label(menu, 1) == "Scanning");
    CHECK(std::string(menu.row(1).value) == "10s");
    CHECK(menu.row(1).progress == 320);
    CHECK(menu.hint() == BtMenu::HINT_NONE);
    menu.sync(route, 1000 + 4100);
    CHECK(std::string(menu.row(1).value) == "6s");
    CHECK(menu.row(1).progress == 320 * 5600 / 10000);

    bt.onLine("+SCAN=1,2,DC0D30000057,-80,3,Far,240408", 2000);
    bt.onLine("+SCAN=2,2,DC0D30000012,-50,4,Near,240404", 2000);
    bt.onLine("+PLIST=1,1C5CF226D773,Sony", 2000);
    bt.onLine("+PLIST=E", 2000);
    menu.sync(route, 2000);
    REQUIRE(menu.rowCount() == 5);
    CHECK(label(menu, 2) == "Sony");
    CHECK(std::string(menu.row(2).note) == "Paired");
    CHECK(label(menu, 3) == "Near");
    CHECK(menu.row(3).bars == 3);
    CHECK(label(menu, 4) == "Far");
    CHECK(menu.row(4).bars == 1);
  }

  TEST_CASE("List: the cursor follows its device when the list reorders") {
    BtModel bt;
    BtMenu menu(bt);
    bt.onLine("+PLIST=1,1C5CF226D773,Sony", 0);
    bt.onLine("+PLIST=2,A0BC30075421,JBL", 0);
    bt.onLine("+PLIST=E", 0);
    Route_t route = menuRoute(Route_t::BT_LIST, 0, 1);
    menu.sync(route, 0);
    menu.scroll(route, -2); // -> JBL
    CHECK(label(menu, menu.selected()) == "JBL");
    uint32_t sonyRowBefore = menu.chunkHash(3);

    bt.onLine("+A2DPDEV=A0BC30075421,JBL", 10);
    bt.onLine("+A2DPSTAT=3", 10);
    menu.sync(route, 10);
    CHECK(label(menu, 2) == "JBL"); // connected first
    CHECK(menu.selected() == 2);
    CHECK(route.selectedIndex == 2);
    CHECK(menu.chunkHash(3) != sonyRowBefore); // Sony moved down
  }

  TEST_CASE("Device screen: connected rows, cursor on Codec, read-only skipped") {
    BtModel bt;
    BtMenu menu(bt);
    menu.setStorageAvailable(true);
    bt.onLine("+PLIST=1,1C5CF226D773,Sony", 0);
    bt.onLine("+PLIST=E", 0);
    bt.onLine("+A2DPDEV=1C5CF226D773,Sony", 0);
    bt.onLine("+A2DPSTAT=3", 0);
    bt.onLine("+A2DPENC=3", 0);
    bt.onLine("+RSSI=1C5CF226D773,-54", 0);
    int sony = bt.findSlot("1C5CF226D773");

    Route_t list = menuRoute(Route_t::BT_LIST, 0, 2);
    menu.sync(list, 0);
    BtMenu::Nav nav = menu.click(list, 0);
    REQUIRE(nav.kind == BtMenu::Nav::PUSH);
    CHECK(nav.route == Route_t::BT_DEVICE);
    CHECK(nav.slot == sony);
    CHECK(nav.selected == 1);

    Route_t route = menuRoute(Route_t::BT_DEVICE, sony, nav.selected);
    menu.sync(route, 0);
    REQUIRE(menu.rowCount() == 6);
    CHECK(label(menu, 0) == "Disconnect");
    CHECK(label(menu, 1) == "Codec");
    CHECK(std::string(menu.row(1).value) == "Auto");
    CHECK(label(menu, 2) == "In use");
    CHECK(std::string(menu.row(2).note) == "aptX HD");
    CHECK(label(menu, 3) == "Signal");
    CHECK(std::string(menu.row(3).value) == "-54 dBm");
    CHECK(label(menu, 4) == "Rename...");
    CHECK(label(menu, 5) == "Forget device");
    CHECK(std::string(menu.title()) == "Sony");

    menu.scroll(route, -1); // skips In use and Signal
    CHECK(menu.selected() == 4);
    menu.scroll(route, 1);
    CHECK(menu.selected() == 1);

    // Codec cycles in place.
    menu.click(route, 0);
    menu.sync(route, 0);
    CHECK(std::string(menu.row(1).value) == "SBC");
  }

  TEST_CASE("Device screen: not connected, connect failed, no SD card") {
    BtModel bt;
    BtMenu menu(bt);
    bt.onLine("+SCAN=1,2,DC0D30000057,-71,3,JBL,240408", 0);
    int jbl = bt.findSlot("DC0D30000057");
    Route_t route = menuRoute(Route_t::BT_DEVICE, jbl);
    menu.sync(route, 0);
    // Unpaired: Connect, Signal, Rename
    REQUIRE(menu.rowCount() == 3);
    CHECK(label(menu, 0) == "Connect");
    CHECK(std::string(menu.row(1).note) == "last scan");
    CHECK(menu.row(2).kind == MenuRow::DISABLED);
    CHECK(std::string(menu.row(2).note) == "needs SD card");

    menu.click(route, 100);
    menu.sync(route, 100);
    CHECK(label(menu, 0) == "Connecting");
    CHECK(std::string(menu.row(0).note) == "Cancel");
    bt.onLine("+A2DPSTAT=1", 200);
    menu.sync(route, 300);
    CHECK(label(menu, 0) == "Couldn't connect");
    CHECK(menu.row(0).kind == MenuRow::FAILED);
    menu.sync(route, 300 + BtModel::FAILED_SHOW_MS);
    CHECK(label(menu, 0) == "Connect");
  }

  TEST_CASE("Rename: starts from the name, saving its own name clears the alias") {
    BtModel bt;
    BtMenu menu(bt);
    menu.setStorageAvailable(true);
    bt.onLine("+PLIST=1,1C5CF226D773,Sony", 0);
    bt.onLine("+PLIST=E", 0);
    int sony = bt.findSlot("1C5CF226D773");
    Route_t device = menuRoute(Route_t::BT_DEVICE, sony);
    menu.sync(device, 0);
    while (label(menu, menu.selected()) != "Rename...")
      REQUIRE(menu.scroll(device, -1));
    BtMenu::Nav nav = menu.click(device, 0);
    REQUIRE(nav.route == Route_t::BT_RENAME);

    Route_t rename = menuRoute(Route_t::BT_RENAME, sony);
    menu.sync(rename, 0);
    CHECK(std::string(menu.entry().text()) == "Sony");
    CHECK(menu.entry().cursorChar() == 'a');
    uint32_t ribbon = menu.chunkHash(1);
    uint32_t textChunk = menu.chunkHash(3);
    menu.renameMove(-1); // wraps to '9'
    CHECK(menu.entry().cursorChar() == '9');
    CHECK(menu.chunkHash(1) != ribbon);
    CHECK(menu.chunkHash(3) == textChunk);

    menu.renameSpace();
    menu.renameAppend();
    CHECK(menu.renameSave().kind == BtMenu::Nav::BACK);
    CHECK(name(bt, sony) == "Sony 9");

    menu.sync(device, 0);
    menu.click(device, 0); // reopen: starts from the alias
    menu.sync(rename, 0);
    for (int i = 0; i < 2; i++)
      menu.renameBackspace();
    menu.renameSave();
    CHECK(std::string(bt.device(sony).alias).empty());
  }

  TEST_CASE("Forget: Cancel first, Forget goes back to the list") {
    BtModel bt;
    BtMenu menu(bt);
    bt.onLine("+PLIST=1,1C5CF226D773,Sony", 0);
    bt.onLine("+PLIST=E", 0);
    int sony = bt.findSlot("1C5CF226D773");
    Route_t route = menuRoute(Route_t::BT_FORGET, sony);
    menu.sync(route, 0);
    CHECK(menu.listTopChunk() == 4);
    CHECK(label(menu, menu.selected()) == "Cancel");
    CHECK(menu.click(route, 0).kind == BtMenu::Nav::BACK);
    menu.scroll(route, -1);
    BtMenu::Nav nav = menu.click(route, 0);
    CHECK(nav.kind == BtMenu::Nav::BACK_TO_LIST);
    CHECK(has(drain(bt), "AT+PLIST=1C5CF226D773"));
  }

  TEST_CASE("State: Settings from the root menu, menu pushes and backTo") {

    openpod_test::resetAll();
    openpod_test::registerStateTestCatalog();
    State state;
    MusicLookup lookup;
    REQUIRE(lookup.init());
    state.loadCurrentRouteData(lookup);
    while (std::string(state.getCurrentTrackName()) != "Settings")
      state.scrollDown();
    state.goToSelected(lookup);
    CHECK(state.getCurrentRoute().type == Route_t::SETTINGS);
    CHECK(std::string(state.getHeaderTitle()) == "Settings");

    state.pushMenuRoute(Route_t::BT_LIST, 0, "Bluetooth", 1);
    state.pushMenuRoute(Route_t::BT_DEVICE, 3, "Sony", 0);
    state.pushMenuRoute(Route_t::BT_FORGET, 3, "Forget device?", 0);
    CHECK(state.getCurrentRoute().entityId == 3);
    REQUIRE(state.backTo(Route_t::BT_LIST, lookup));
    CHECK(state.getCurrentRoute().type == Route_t::BT_LIST);
    CHECK(state.getCurrentRoute().selectedIndex == 1);
    REQUIRE(state.back(lookup));
    REQUIRE(state.back(lookup));
    CHECK(state.getCurrentRoute().type == Route_t::ROOT);
    CHECK(std::string(state.getCurrentTrackName()) == "Settings");
  }
}

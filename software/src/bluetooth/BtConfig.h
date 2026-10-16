#pragma once
#include "../logging.h"
#include "BtModel.h"
#include <SdFat.h>

// The per-device settings the module can't keep (alias, preferred codec),
// in /openpod/bt.cfg on the SD card: one "MAC<TAB>codec<TAB>alias" line per
// device (BtModel::serializeConfig()). Plain text, so it can be fixed by
// hand on a PC. Without an SD card nothing is saved and renaming is off.
class BtConfig {
public:
  // Loads the file into `bt`. Returns whether the card is usable.
  bool begin(SdFat &card, BtModel &bt) {
    // Through `card` itself rather than FsFile::open(path), which uses
    // whichever SdFat instance began last (there is only one now, but the
    // tests make their own).
    FsFile root = card.open("/");
    if (!root) {
      LOG_WARN("Bluetooth settings: no SD card, aliases and codecs not saved");
      return false;
    }
    root.close();
    sd = &card;

    FsFile file = card.open(PATH, O_RDONLY);
    if (!file)
      return true; // nothing saved yet
    char line[96];
    size_t len = 0;
    int c;
    while ((c = file.read()) >= 0) {
      if (c == '\n' || c == '\r') {
        line[len] = '\0';
        if (len)
          bt.loadConfigLine(line);
        len = 0;
      } else if (len < sizeof(line) - 1) {
        line[len++] = (char)c;
      }
    }
    if (len) {
      line[len] = '\0';
      bt.loadConfigLine(line);
    }
    file.close();
    return true;
  }

  bool available() const { return sd != nullptr; }

  // Rewrites the file when the settings changed.
  void saveIfDirty(BtModel &bt) {
    if (!sd || !bt.configDirty())
      return;
    sd->mkdir("/openpod");
    FsFile file = sd->open(PATH, O_WRONLY | O_CREAT | O_TRUNC);
    if (!file) {
      LOG_WARN("Bluetooth settings: can't write %s", PATH);
      return;
    }
    // A line at a time: no buffer for the whole file (RAM is tight).
    char line[BtModel::ALIAS_LEN + 24];
    int slot = 0;
    while (bt.configLine(slot, line, sizeof(line)))
      file.write(line, strlen(line));
    file.close();
    bt.markConfigSaved();
    LOG_CAT(BLUETOOTH, "Saved %s", PATH);
  }

private:
  static constexpr const char *PATH = "/openpod/bt.cfg";
  SdFat *sd = nullptr;
};

#pragma once
#include "../bluetooth/BtModel.h"
#include <stdio.h>
#include <string.h>

// Toasts raised by the app itself (podcast playback), shown in the header
// like BtModel's: the UI shows whichever of the two changed last. Both write
// the same message (BtModel::sharedToast()). They have no icon: neutral for
// what the user did, success for what happened on its own.
class AppToast {
public:
  AppToast() : toast(BtModel::sharedToast()), count(0) {}

  // `bold` is drawn in the bold face between `before` and `after`.
  void show(uint8_t tone, const char *before, const char *bold = "",
            const char *after = "") {
    toast.tone = tone;
    snprintf(toast.text, sizeof(toast.text), "%s%s%s", before, bold, after);
    size_t start = strlen(before), len = strlen(bold);
    size_t max = sizeof(toast.text) - 1;
    if (start > max)
      start = max;
    if (start + len > max)
      len = max - start;
    toast.nameStart = (uint8_t)start;
    toast.nameLen = (uint8_t)len;
    toast.plain = true;
    count++;
  }

  uint8_t serial() const { return count; }
  const BtModel::Toast &last() const { return toast; }

private:
  BtModel::Toast &toast;
  uint8_t count;
};

extern AppToast appToast;

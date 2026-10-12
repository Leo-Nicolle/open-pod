#pragma once
#include <Arduino.h>

// Text typed with the wheel on a letter ribbon: scroll moves a cursor along
// `charset`, a click appends the character under it. Shared by the search
// screen (A-Z0-9, cursor stops at both ends) and the Bluetooth rename screen
// (a-z0-9, cursor wraps from 9 back to a). No display code.
class TextEntry {
public:
  static const int CAPACITY = 32; // max for any maxLen

  // charset must outlive the entry (a string literal).
  TextEntry(const char *charset, int maxLen, bool wrap);

  // Sets the text (cut to maxLen) and puts the cursor on the first
  // character of the charset.
  void reset(const char *initial = "");

  // Moves the cursor by `clicks` characters: clamped to the charset, or
  // around it when wrapping. Returns true if it moved.
  bool moveCursor(int clicks);

  // Edits. Each returns true if the text changed.
  bool appendChar(char c); // false when full
  bool append() { return appendChar(cursorChar()); }
  bool backspace();        // drop the last character
  bool space();            // not first, not after another space

  const char *text() const { return buf; }
  int length() const { return len; }
  int maxLength() const { return maxLen; }
  int cursor() const { return cur; }
  char cursorChar() const { return charset[cur]; }
  const char *chars() const { return charset; }
  int charCount() const { return count; }
  bool wraps() const { return wrap; }

private:
  const char *charset;
  int count;
  int maxLen;
  bool wrap;
  char buf[CAPACITY + 1];
  int len;
  int cur;
};

#include "text_entry.h"

TextEntry::TextEntry(const char *charset, int maxLen, bool wrap)
    : charset(charset), count(strlen(charset)),
      maxLen(constrain(maxLen, 0, CAPACITY)), wrap(wrap), len(0), cur(0) {
  buf[0] = '\0';
}

void TextEntry::reset(const char *initial) {
  strncpy(buf, initial ? initial : "", maxLen);
  buf[maxLen] = '\0';
  len = strlen(buf);
  cur = 0;
}

bool TextEntry::moveCursor(int clicks) {
  int next;
  if (wrap) {
    next = (cur + clicks) % count;
    if (next < 0)
      next += count;
  } else {
    next = constrain(cur + clicks, 0, count - 1);
  }
  if (next == cur)
    return false;
  cur = next;
  return true;
}

bool TextEntry::appendChar(char c) {
  if (len >= maxLen)
    return false;
  buf[len++] = c;
  buf[len] = '\0';
  return true;
}

bool TextEntry::backspace() {
  if (len == 0)
    return false;
  buf[--len] = '\0';
  return true;
}

bool TextEntry::space() {
  if (len == 0 || buf[len - 1] == ' ')
    return false;
  return appendChar(' ');
}

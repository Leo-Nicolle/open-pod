#include "fast_scroll.h"

int fastScrollLetterCount(int total) {
  if (total > 1500)
    return 3;
  if (total > 150)
    return 2;
  return 1;
}

void fastScrollLabel(const char *title, int total, char *out) {
  int want = fastScrollLetterCount(total);
  int n = 0;
  for (const char *c = title; c && *c && n < want; c++) {
    char ch = *c;
    if (ch >= 'a' && ch <= 'z')
      out[n++] = ch - 'a' + 'A';
    else if (ch >= 'A' && ch <= 'Z')
      out[n++] = ch;
  }
  if (n == 0)
    out[n++] = '#';
  out[n] = '\0';
}

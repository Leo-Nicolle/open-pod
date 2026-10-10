#pragma once

// Fast-scroll letter overlay label (see docs/ui-builder, "Track list"):
// while the wheel spins fast on a long list, the first letters of the
// highlighted title are shown over the rows.

// Letters shown for a list of `total` items: 1 up to 150, 2 up to 1500,
// 3 beyond.
int fastScrollLetterCount(int total);

// Writes the label for `title` into `out` (at least 4 bytes): ASCII letters
// only, uppercased, fastScrollLetterCount(total) of them, or "#" if the
// title has none.
void fastScrollLabel(const char *title, int total, char *out);

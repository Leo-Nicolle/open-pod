// Layout spec shown under the builder previews: per-element geometry and the
// redraw-budget rationale. Mirrors screens.ts; keep the two in sync.

export interface GeometryRow {
  el: string;
  box: string;
  note: string;
}

export const GEOMETRY: GeometryRow[] = [
  { el: 'header', box: '0,0 320×30', note: 'SURFACE + 1px LINE at y=29. Chunk 0.' },
  { el: 'play/pause', box: '8,10 9×11', note: 'Sprite, accent tint' },
  { el: 'track n/N', box: '24,9 · mono 11', note: '"3/12"' },
  { el: 'SHUF / RPT chips', box: '62,9 · 10px caps', note: 'Accent border when active, LINE when off' },
  { el: 'clock', box: 'right 34,7 · mono 12', note: 'Redraw once a minute, chunk 0 only' },
  { el: 'battery', box: '290,9 22×11', note: 'Sprite + 3 fill cells' },
  { el: 'album art', box: '8,40 160×160', note: '1px LINE border, ends y=200' },
  { el: 'title', box: '180,40 132×32', note: 'IBMPlexSans16, 2 lines, ellipsis on overflow' },
  { el: 'artist', box: '180,80 · 12px', note: 'Needs an IBMPlexSans12 bitmap; else reuse 16 and drop the album line' },
  { el: 'album', box: '180,96 · 12px', note: 'MUTED' },
  { el: 'bottom band', box: '0,210 320×30', note: 'Exactly one CHUNK_HEIGHT buffer. Holds every animated element.' },
  { el: 'elapsed / total', box: '8,212 / right 8,212', note: 'mono 12, elapsed turns ACCENT in seek mode' },
  { el: 'progress bar', box: '8,232 304×6', note: 'r=3, LINE track, ACCENT fill' },
  { el: 'seek bar', box: '8,228 304×10', note: 'r=2, ACCENT_DK fill, handle at y=226' },
  { el: 'volume band', box: '0,210 320×30', note: 'SURFACE panel replaces the band, bar at 56,221 220×8' },
  { el: 'list header', box: '0,0 320×30', note: 'Back chevron 8,10 7×11 · one 14px title (album or artist, per menu) · battery right' },
  { el: 'row i', box: '0,30+i*30 320×30', note: 'i = 0…6. One CHUNK_HEIGHT each, on a chunk boundary' },
  { el: 'row · number', box: '10,+8 18px right', note: 'mono 11, MUTED (TEXT when selected)' },
  { el: 'row · title', box: '36,+6 224×18', note: 'IBMPlexSans16, clipped with ellipsis' },
  { el: 'row · duration', box: '264,+8 40px right', note: 'mono 11, right-aligned, clears the scrollbar' },
  { el: 'row · separator', box: '0,+29 320×1', note: '#1B1917, one shade above BG' },
  { el: 'row · selected', box: 'fill 320×30', note: 'ACCENT_DK fill + 3px ACCENT left edge, title to #FFFFFF' },
  { el: 'scrollbar track', box: '313,32 5×204', note: 'r=2, #23201E' },
  { el: 'scrollbar thumb', box: '313,32+ 5×h', note: 'h = max(24, 204*7/N), y offset = (204-h)*first/(N-7)' },
  { el: 'search header', box: '0,0 320×30', note: 'Chevron · 11px magnifier at 23,9 · query 14px · 2px caret · battery right' },
  { el: 'letter ribbon', box: '0,30 320×30', note: '13 cells of 19×20 at y=35, gap 2. Cursor cell filled ACCENT, glyph BG' },
  { el: 'result row i', box: '0,60+i*30 320×30', note: '6 rows while typing; 7 rows from y=30 once the ribbon drops' },
  { el: 'row · kind chip', box: '10,+9 22×14', note: 'mono 9, TRK / ALB / ART, 1px LINE border' },
  { el: 'row · title', box: '40,+6 150×18', note: 'IBMPlexSans16, ellipsis' },
  { el: 'row · context', box: '196,+8 108px right', note: '12px MUTED — artist, year, or album count' },
];

export const REDRAW_NOTES: { i: string; t: string }[] = [
  { i: '01', t: 'Everything that moves lives in y=210…239, which is one CHUNK_HEIGHT buffer at a chunk boundary. updateNowPlaying() becomes a single renderChunk(display, 0, 210, 320) — no loop, no partial-chunk math.' },
  { i: '02', t: 'Volume and seek reuse that same band instead of overlaying the artwork, so switching modes never touches the cover or the metadata column.' },
  { i: '03', t: 'Header is chunk 0 and only redraws on clock, battery, play state or shuffle/repeat changes.' },
  { i: '04', t: 'Bars are drawn from the 26×6 nine-slice atlas: blit left cap, repeat the 1px mid column across the span, blit right cap. Two atlases (6px and 10px) cover progress, seek and volume at any width.' },
  { i: '05', t: 'Glyph sprites are 1-bit masks blitted with a runtime tint colour, so play/pause/speaker/battery cost ~30 bytes each and follow the theme for free.' },
  { i: '06', t: 'On the track list, the 30px row pitch means the highlight is a two-row redraw: clear the old row, fill the new one. Wheel scrolling past the edges shifts firstIndex and redraws rows 0…6 through the same single buffer.' },
  { i: '07', t: 'Cover at 160×160 is the largest square that leaves a readable 132px metadata column. A 176px cover would push the title under 120px, which starts wrapping most titles to three lines.' },
];

// Layout spec shown in the builder's Spec mode: per-element geometry and the
// redraw-budget rationale. Mirrors screens.ts; keep the two in sync.

export interface GeometryRow {
  el: string;
  box: string;
  note: string;
  /** slideshow group the element belongs to (see screenStates.ts) */
  group: 'nowplaying' | 'tracklist' | 'search' | 'bluetooth';
}

export const GEOMETRY: GeometryRow[] = [
  { el: 'header', box: '0,0 320×30', note: 'SURFACE + 1px LINE at y=29. Chunk 0.', group: 'nowplaying' },
  { el: 'play/pause', box: '8,10 9×11', note: 'Sprite, accent tint', group: 'nowplaying' },
  { el: 'track n/N', box: '24,9 · mono 11', note: '"3/12"', group: 'nowplaying' },
  { el: 'SHUF / RPT chips', box: '62,9 · 10px caps', note: 'Accent border when active, LINE when off', group: 'nowplaying' },
  { el: 'clock', box: 'right 34,7 · mono 12', note: 'Redraw once a minute, chunk 0 only', group: 'nowplaying' },
  { el: 'battery', box: '290,9 22×11', note: 'Sprite + 3 fill cells', group: 'nowplaying' },
  { el: 'album art', box: '8,40 160×160', note: '1px LINE border, ends y=200', group: 'nowplaying' },
  { el: 'title', box: '180,40 132×42', note: 'IBMPlexSans16 (21px line height), 2 lines, ellipsis on overflow', group: 'nowplaying' },
  { el: 'artist', box: '180,92 · 12px', note: 'Needs an IBMPlexSans12 bitmap; else reuse 16 and drop the album line', group: 'nowplaying' },
  { el: 'album', box: '180,110 · 12px', note: 'ACCENT', group: 'nowplaying' },
  { el: 'bottom band', box: '0,210 320×30', note: 'Exactly one CHUNK_HEIGHT buffer. Holds every animated element.', group: 'nowplaying' },
  { el: 'elapsed / total', box: '8,212 / right 8,212', note: 'mono 12, elapsed turns ACCENT in seek mode', group: 'nowplaying' },
  { el: 'progress bar', box: '8,232 304×6', note: 'r=3, LINE track, ACCENT fill', group: 'nowplaying' },
  { el: 'seek bar', box: '8,228 304×10', note: 'r=2, ACCENT_DK fill, handle at y=226', group: 'nowplaying' },
  { el: 'volume band', box: '0,210 320×30', note: 'SURFACE panel replaces the band, bar at 56,221 220×8', group: 'nowplaying' },
  { el: 'list header', box: '0,0 320×30', note: 'Back chevron 8,10 7×11 · one 14px title (album or artist, per menu) · battery right', group: 'tracklist' },
  { el: 'row i', box: '0,30+i*30 320×30', note: 'i = 0…6. One CHUNK_HEIGHT each, on a chunk boundary', group: 'tracklist' },
  { el: 'row · number', box: '10,+8 18px right', note: 'mono 11, MUTED (TEXT when selected)', group: 'tracklist' },
  { el: 'row · title', box: '36,+6 224×18', note: 'IBMPlexSans16, clipped with ellipsis', group: 'tracklist' },
  { el: 'row · duration', box: '264,+8 40px right', note: 'mono 11, right-aligned, clears the scrollbar', group: 'tracklist' },
  { el: 'row · separator', box: '0,+29 320×1', note: '#1B1917, one shade above BG', group: 'tracklist' },
  { el: 'row · selected', box: 'fill 320×30', note: 'ACCENT_DK fill + 3px ACCENT left edge, title to #FFFFFF', group: 'tracklist' },
  { el: 'scrollbar track', box: '313,32 5×204', note: 'r=2, #23201E', group: 'tracklist' },
  { el: 'scrollbar thumb', box: '313,32+ 5×h', note: 'h = max(24, 204*7/N), y offset = (204-h)*first/(N-7)', group: 'tracklist' },
  { el: 'fast-scroll overlay', box: '100,90 120×90', note: 'Chunks 3–5 exactly. SURFACE fill, 2px ACCENT border inset, r=6', group: 'tracklist' },
  { el: 'fast-scroll label', box: 'centred · Sans 600', note: 'Selected title, non-letters stripped, caps. 1/2/3 letters at 60/46/36px for N ≤150/≤1500/more; "#" if empty', group: 'tracklist' },
  { el: 'scrollbar thumb (fast)', box: '312,32+ 7×h', note: 'Widens from 5px while the overlay is shown', group: 'tracklist' },
  { el: 'search header', box: '0,0 320×30', note: 'Chevron · 11px magnifier at 23,9 · query 14px · 2px caret · battery right', group: 'search' },
  { el: 'letter ribbon', box: '0,30 320×30', note: '13 cells of 19×20 at y=35, gap 2. Cursor cell filled ACCENT, glyph BG', group: 'search' },
  { el: 'result row i', box: '0,60+i*30 320×30', note: '6 rows while typing; 7 rows from y=30 once the ribbon drops', group: 'search' },
  { el: 'row · kind chip', box: '10,+9 22×14', note: 'mono 9, TRK / ALB / ART, 1px LINE border', group: 'search' },
  { el: 'row · title', box: '40,+6 150×18', note: 'IBMPlexSans16, ellipsis', group: 'search' },
  { el: 'row · context', box: '196,+8 108px right', note: '12px MUTED — artist, year, or album count', group: 'search' },
  { el: 'BT icon box', box: '270,10 11×11', note: 'Every header. Rune 7×11 at x+2; connected adds 2×3 ticks at (0,4),(9,4). MUTED off, TEXT on, ACCENT connected; connecting alternates every 500ms', group: 'bluetooth' },
  { el: 'header title (BT)', box: '23 … 262', note: 'Titles end 8px before the icon box', group: 'bluetooth' },
  { el: 'menu row · label', box: '10,+6 · 14px', note: 'Ellipsis before the status ever shrinks. READONLY: MUTED, DISABLED: COLOR_DISABLED, destructive: COLOR_DESTRUCTIVE', group: 'bluetooth' },
  { el: 'menu row · status', box: 'right edge 304', note: 'Note, value, bars, chevron/switch from left to right, 8px apart. Clears the scrollbar at 313', group: 'bluetooth' },
  { el: 'signal bars', box: '11×10', note: '3px bars, 1px gap, 4/7/10 tall. ≥−60 3 bars, ≥−75 2, else 1. DIM lit / BAR_OFF unlit (TEXT / BAR_OFF_HI selected)', group: 'bluetooth' },
  { el: 'switch', box: '22×12 r=6', note: 'On: ACCENT, BG knob right. Off: 1px DIM border, DIM knob left. Knob 8×8, 2px in', group: 'bluetooth' },
  { el: 'scan row', box: 'row 2', note: '"Scanning" + 3 dots (3×3, 3px gap, step 400ms), "Stop" + countdown, 2px ACCENT progress at row bottom − 2', group: 'bluetooth' },
  { el: 'toast', box: '0,0 320×30', note: 'Takes over the header for 2.5s. Success ACCENT_DK + 2px ACCENT at y=28, neutral LINE, error COLOR_ERROR', group: 'bluetooth' },
  { el: 'forget text', box: 'chunks 1–3', note: 'Name 14/600 at 10,44, then 12px/17 DIM body. Cancel (preselected) chunk 4, Forget chunk 5', group: 'bluetooth' },
  { el: 'rename ribbon', box: '0,30 320×30', note: 'a–z0–9, wraps; cursor always centered. Other cells MUTED', group: 'bluetooth' },
  { el: 'rename text', box: '12,94 · 24/600', note: '2×24 ACCENT caret, then Mono 11 MUTED hint and n/24 at y=132', group: 'bluetooth' },
  { el: 'rename footer', box: '0,210 320×30', note: 'RIBBON band, LINE top, MENU cancel · BACK delete · NEXT space · PLAY save', group: 'bluetooth' },
];

export const REDRAW_NOTES: { i: string; t: string }[] = [
  { i: '01', t: 'Everything that moves lives in y=210…239, which is one CHUNK_HEIGHT buffer at a chunk boundary. updateNowPlaying() becomes a single renderChunk(display, 0, 210, 320) — no loop, no partial-chunk math.' },
  { i: '02', t: 'Volume and seek reuse that same band instead of overlaying the artwork, so switching modes never touches the cover or the metadata column.' },
  { i: '03', t: 'Header is chunk 0 and only redraws on clock, battery, play state or shuffle/repeat changes.' },
  { i: '04', t: 'Bars are drawn from the 26×6 nine-slice atlas: blit left cap, repeat the 1px mid column across the span, blit right cap. Two atlases (6px and 10px) cover progress, seek and volume at any width.' },
  { i: '05', t: 'Glyph sprites are 1-bit masks blitted with a runtime tint colour, so play/pause/speaker/battery cost ~30 bytes each and follow the theme for free.' },
  { i: '06', t: 'On the track list, the 30px row pitch means the highlight is a two-row redraw: clear the old row, fill the new one. Wheel scrolling past the edges shifts firstIndex and redraws rows 0…6 through the same single buffer.' },
  { i: '07', t: 'Cover at 160×160 is the largest square that leaves a readable 132px metadata column. A 176px cover would push the title under 120px, which starts wrapping most titles to three lines.' },
  { i: '08', t: 'Fast-scroll overlay sits on chunks 3–5, so it is composed into those three row buffers before they are pushed: rows never flash under it while scrolling. Hiding it re-renders rows 2…4 only.' },
  { i: '09', t: 'Bluetooth: a link change repaints only the 11×11 icon box at (270,10), and its connecting blink the same box every 500ms. A device status change repaints its row (or the rows it moved across); a scan tick repaints row 2 only; a codec change or signal refresh one row; a toast the header chunk.' },
];

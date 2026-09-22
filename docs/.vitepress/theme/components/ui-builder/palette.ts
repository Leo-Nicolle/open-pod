// Color palette model for the Now Playing UI builder.
//
// The screen (ILI9341, 320x240, RGB565) is themed by 13 colors: ten
// "structural" ones (background, surfaces, text levels, list chrome) and the
// three-color accent family. All of them are editable here.

export type PaletteKey =
  | 'bg'
  | 'surface'
  | 'line'
  | 'text'
  | 'dim'
  | 'muted'
  | 'textHi'
  | 'separator'
  | 'scrollTrack'
  | 'ribbon'
  | 'accent'
  | 'accentDark'
  | 'accentAlt';

export interface Palette {
  bg: string; // #RRGGBB
  surface: string;
  line: string;
  text: string;
  dim: string;
  muted: string;
  textHi: string;
  separator: string;
  scrollTrack: string;
  ribbon: string;
  accent: string;
  accentDark: string;
  accentAlt: string;
}

export interface PaletteMeta {
  key: PaletteKey;
  /** C macro name, e.g. COLOR_BG */
  name: string;
  /** Short human label */
  label: string;
  description: string;
}

export const PALETTE_META: PaletteMeta[] = [
  { key: 'bg', name: 'COLOR_BG', label: 'Background', description: 'Screen background' },
  { key: 'surface', name: 'COLOR_SURFACE', label: 'Surface', description: 'Header bar, volume band, art plate' },
  { key: 'line', name: 'COLOR_LINE', label: 'Line', description: '1px rules, empty track, borders' },
  { key: 'text', name: 'COLOR_TEXT', label: 'Text', description: 'Title, elapsed time, clock' },
  { key: 'dim', name: 'COLOR_DIM', label: 'Dim', description: 'Artist, secondary numbers' },
  { key: 'muted', name: 'COLOR_MUTED', label: 'Muted', description: 'Album, total time, inactive chips' },
  { key: 'textHi', name: 'COLOR_TEXT_HI', label: 'Text high', description: 'Title of the selected list row' },
  { key: 'separator', name: 'COLOR_SEPARATOR', label: 'Separator', description: 'List row separators, one shade above BG' },
  { key: 'scrollTrack', name: 'COLOR_SCROLL_TRACK', label: 'Scroll track', description: 'List scrollbar track' },
  { key: 'ribbon', name: 'COLOR_RIBBON', label: 'Ribbon', description: 'Search letter ribbon, cover placeholder' },
  { key: 'accent', name: 'COLOR_ACCENT', label: 'Accent', description: 'Progress fill, active state' },
  { key: 'accentDark', name: 'COLOR_ACCENT_DK', label: 'Accent dark', description: 'Seek track fill behind the handle' },
  { key: 'accentAlt', name: 'COLOR_ACCENT_ALT', label: 'Accent alt', description: 'Seek handle, SEEK chip' },
];

export const DEFAULT_PALETTE: Palette = {
  bg: '#12100F',
  surface: '#1E1B19',
  line: '#332E2B',
  text: '#F2EDE6',
  dim: '#9B918A',
  muted: '#6E655F',
  textHi: '#FFFFFF',
  separator: '#1B1917',
  scrollTrack: '#23201E',
  ribbon: '#171513',
  accent: '#31AAA9',
  accentDark: '#6C1A1A',
  accentAlt: '#F8E0A4',
};

/** Parse a "#RRGGBB" (or "#RGB") string into [r, g, b] 0-255. */
export function hexToRgb(hex: string): [number, number, number] {
  let h = hex.trim().replace(/^#/, '');
  if (h.length === 3) {
    h = h
      .split('')
      .map((c) => c + c)
      .join('');
  }
  const n = parseInt(h, 16);
  if (Number.isNaN(n) || h.length !== 6) return [0, 0, 0];
  return [(n >> 16) & 0xff, (n >> 8) & 0xff, n & 0xff];
}

/** Normalize any hex-ish string to "#RRGGBB". */
export function normalizeHex(hex: string): string {
  let h = hex.trim().replace(/^#/, '');
  if (h.length === 3) {
    h = h
      .split('')
      .map((c) => c + c)
      .join('');
  }
  if (!/^[0-9a-fA-F]{6}$/.test(h)) return '#000000';
  return '#' + h.toUpperCase();
}

export function rgbToHex(r: number, g: number, b: number): string {
  const to2 = (v: number) => Math.max(0, Math.min(255, Math.round(v))).toString(16).padStart(2, '0');
  return ('#' + to2(r) + to2(g) + to2(b)).toUpperCase();
}

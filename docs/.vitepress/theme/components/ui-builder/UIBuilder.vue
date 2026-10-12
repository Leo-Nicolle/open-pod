<script lang="ts" setup>
import { computed, onBeforeUnmount, onMounted, reactive, ref, shallowRef, watch } from 'vue';
import { NButton, NSelect, NSwitch } from 'naive-ui';
import { DEFAULT_PALETTE, PALETTE_META, normalizeHex, type Palette, type PaletteKey } from './palette';
import { SPRITES } from './sprites';
import {
  buildSpritesHeader,
  buildThemeHeader,
  copyToClipboard,
  exportAllPngs,
  hexTo565,
  downloadText,
} from './export';
import ScreenCanvas from './ScreenCanvas.vue';
import SpriteStage from './SpriteStage.vue';
import BootStage from './BootStage.vue';
import {
  BOOT,
  buildLogoHeader,
  LOGO_ROLES,
  logoPalette,
  rasterizeBootLogo,
  rasterizeBootSquash,
  type Wordmark,
} from './logo';
import { renderWordmark } from './wordmark';
import {
  BT_DEVICES,
  btDeviceRows,
  KID_A,
  KID_RESULTS,
  LIBRARY,
  RADIOHEAD,
  type MenuRowSpec,
  type MenuScreenOpts,
} from './screens';
import { GEOMETRY, REDRAW_NOTES } from './spec';

const palette = reactive<Palette>({ ...DEFAULT_PALETTE });
const STORAGE_KEY = 'openpod-ui-builder-palette';
const zoom = ref(1);
const grid = ref(false);
const playing = ref(true);
const status = ref('');
let statusTimer: ReturnType<typeof setTimeout> | undefined;

const zoomOptions = [
  { label: '1×', value: 1 },
  { label: '2×', value: 2 },
  { label: '3×', value: 3 },
];
const states = [
  { key: 'play' as const, label: 'playing' },
  { key: 'volume' as const, label: 'volume (wheel scrub, 1.5s timeout)' },
  { key: 'seek' as const, label: 'seek (center click → wheel scrubs)' },
];

const nowPlaying = computed(() =>
  states.map((s) => ({
    label: s.label,
    screen: {
      kind: 'nowplaying' as const,
      opts: { mode: s.key, playing: playing.value, grid: grid.value, bt: 'connected' as const, codec: 'aptX HD' },
    },
  })),
);

const trackLists = computed(() => [
  {
    label: 'top of list',
    screen: {
      kind: 'tracklist' as const,
      opts: { heading: 'Kid A', tracks: KID_A, first: 0, selected: 2, grid: grid.value },
    },
  },
  {
    label: 'artist menu, scrolled, last row selected',
    screen: {
      kind: 'tracklist' as const,
      opts: { heading: 'Radiohead', tracks: RADIOHEAD, first: 5, selected: 6, grid: grid.value },
    },
  },
  {
    label: 'fast scroll — 800 songs, 2-letter label',
    screen: {
      kind: 'tracklist' as const,
      opts: {
        heading: 'Songs',
        tracks: LIBRARY,
        total: 800,
        first: 410,
        selected: 2,
        numbered: false,
        fastScroll: true,
        grid: grid.value,
      },
    },
  },
]);

const searches = computed(() => {
  const base = { query: 'kid', letter: 'K', results: KID_RESULTS, total: 24, selected: 0, first: 0, grid: grid.value };
  return [
    { label: 'typing — ribbon + 6 results', screen: { kind: 'search' as const, opts: { ...base, mode: 'typing' as const } } },
    {
      label: 'browsing results — 7 rows',
      screen: { kind: 'search' as const, opts: { ...base, mode: 'browsing' as const, selected: 2 } },
    },
    {
      label: 'no matches',
      screen: {
        kind: 'search' as const,
        opts: { ...base, mode: 'typing' as const, query: 'kidz', letter: 'Z', results: [], total: 0 },
      },
    },
  ];
});

// Bluetooth (.agents/bluetooth-design/README.md). Rows mirror the firmware's
// BtMenu (software/src/state/bt_menu.cpp).
const settingsRows = (status: MenuRowSpec): MenuRowSpec[] => [
  { label: 'Bluetooth', trail: 'chevron', ...status },
  { label: 'Display', trail: 'chevron' },
  { label: 'Audio', trail: 'chevron' },
  { label: 'System', trail: 'chevron' },
  { label: 'Reset', kind: 'destructive' },
];
const radioRow = (on: boolean): MenuRowSpec => ({ label: 'Bluetooth', trail: on ? 'switchOn' : 'switchOff' });
const scanRow: MenuRowSpec = { label: 'Scan for devices' };
const scanningRow = (remaining: number): MenuRowSpec => ({
  label: 'Scanning',
  dots: 2,
  note: 'Stop',
  value: `${Math.ceil(remaining / 1000)}s`,
  valueTone: 'count',
  mono: true,
  progress: (320 * remaining) / 10000,
});
const deviceRows = (o: { link: 'connected' | 'idle' | 'connecting' | 'failed'; paired: boolean; alias?: string }) => {
  const rows: MenuRowSpec[] = [];
  if (o.link === 'connected') rows.push({ label: 'Disconnect' });
  else if (o.link === 'connecting') rows.push({ label: 'Connecting', dots: 1, note: 'Cancel' });
  else if (o.link === 'failed') rows.push({ label: "Couldn't connect", kind: 'failed', note: 'is it on?', noteTone: 'cream' });
  else rows.push({ label: 'Connect' });
  if (o.paired) rows.push({ label: 'Codec', value: 'Auto' });
  if (o.link === 'connected') rows.push({ label: 'In use', kind: 'readonly', note: 'aptX HD', noteTone: 'accent' });
  if (o.link === 'connected') rows.push({ label: 'Signal', kind: 'readonly', value: '−54 dBm', mono: true, bars: 3 });
  else rows.push({ label: 'Signal', kind: 'readonly', note: 'last scan', noteTone: 'dimmer', value: '−71 dBm', mono: true, bars: 2 });
  rows.push(o.alias ? { label: 'Rename…', note: o.alias, noteTone: 'dimmer' } : { label: 'Rename…' });
  if (o.paired) rows.push({ label: 'Forget device', kind: 'destructive' });
  return rows;
};

const bluetooth = computed(() => {
  const g = grid.value;
  const menu = (opts: Omit<MenuScreenOpts, 'grid' | 'icon' | 'selected'> & Partial<MenuScreenOpts>) => ({
    kind: 'menu' as const,
    opts: { grid: g, icon: 'connected', selected: 0, ...opts } as MenuScreenOpts,
  });
  return {
    icons: [{ label: 'header icon states (11×11 box at 270,10) and signal bars', screen: { kind: 'bticons' as const, opts: { grid: g } } }],
    lists: [
      {
        label: 'Settings — Bluetooth is the first entry',
        screen: menu({ title: 'Settings', rows: settingsRows({ note: 'Sony WH-1000XM4', noteTone: 'accent' }) }),
      },
      {
        label: 'radio off — other rows hidden',
        screen: menu({
          title: 'Bluetooth',
          icon: 'off',
          rows: [radioRow(false)],
          hint: { fromChunk: 2, title: 'Bluetooth is off', sub: 'paired devices are kept' },
        }),
      },
      {
        label: 'empty — cursor on Scan',
        screen: menu({
          title: 'Bluetooth',
          icon: 'on',
          rows: [radioRow(true), scanRow],
          selected: 1,
          hint: { fromChunk: 3, title: 'No devices yet', sub: 'scan to find headphones' },
        }),
      },
      {
        label: 'scanning — only row 2 repaints',
        screen: menu({
          title: 'Bluetooth',
          icon: 'on',
          rows: [radioRow(true), scanningRow(5600), ...btDeviceRows(BT_DEVICES.slice(3, 6))],
          selected: 1,
        }),
      },
      {
        label: 'populated — connected, paired, then by signal',
        screen: menu({
          title: 'Bluetooth',
          rows: [radioRow(true), scanRow, ...btDeviceRows(BT_DEVICES)],
          selected: 3,
          first: 1,
        }),
      },
    ],
    devices: [
      {
        label: 'connected — cursor on Codec',
        screen: menu({ title: 'Sony WH-1000XM4', rows: deviceRows({ link: 'connected', paired: true }), selected: 1 }),
      },
      {
        label: 'paired, not connected (alias set)',
        screen: menu({
          title: 'Kitchen',
          icon: 'on',
          rows: deviceRows({ link: 'idle', paired: true, alias: 'JBL Charge 4' }),
        }),
      },
      {
        label: 'connecting — icon blinks',
        screen: menu({
          title: 'Bose QC45',
          icon: 'connecting',
          blinkOn: true,
          rows: deviceRows({ link: 'connecting', paired: false }),
        }),
      },
      {
        label: 'connect failed — 2 s',
        screen: menu({ title: 'Bose QC45', icon: 'on', rows: deviceRows({ link: 'failed', paired: false }) }),
      },
      {
        label: 'forget confirmation — Cancel preselected',
        screen: { kind: 'forget' as const, opts: { name: 'Kitchen', selected: 0, icon: 'on' as const, grid: g } },
      },
      {
        label: 'rename — same input as search',
        screen: {
          kind: 'rename' as const,
          opts: { text: 'kitchen', letter: 'n', broadcastName: 'JBL Charge 4', icon: 'on' as const, grid: g },
        },
      },
    ],
    toasts: [
      {
        label: 'success — over a list',
        screen: menu({
          title: 'Bluetooth',
          rows: [radioRow(true), scanRow, ...btDeviceRows(BT_DEVICES.slice(0, 3))],
          selected: 2,
          toast: { tone: 'success', before: 'Connected to ', name: 'Sony WH-1000XM4' },
        }),
      },
      {
        label: 'neutral — something the user did',
        screen: menu({
          title: 'Bluetooth',
          icon: 'on',
          rows: [radioRow(true), scanRow, ...btDeviceRows(BT_DEVICES.slice(1, 8))],
          selected: 1,
          toast: { tone: 'neutral', before: 'Scan finished' },
        }),
      },
      {
        label: 'error — over Now Playing, playback paused',
        screen: {
          kind: 'nowplaying' as const,
          opts: {
            mode: 'play' as const,
            playing: false,
            grid: g,
            bt: 'on' as const,
            toast: { tone: 'error' as const, before: 'Lost ', name: 'Sony WH-1000XM4', after: ' · paused' },
          },
        },
      },
    ],
  };
});

// Title screen. The logo frames never depend on the theme; the wordmark needs
// the web font, so it is rendered once on mount (null until then).
const logoRaster = rasterizeBootLogo();
const squashRaster = rasterizeBootSquash();
const wordmark = shallowRef<Wordmark | null>(null);
const bootPlaying = ref(true);
const logoColors = computed(() => logoPalette(palette));
const bootBytes = computed(() => {
  const img = (w: number, h: number) => Math.ceil(w / 2) * h;
  const w = wordmark.value;
  return img(logoRaster.w, logoRaster.h) + img(squashRaster.w, squashRaster.h) + (w ? img(w.w, w.h) : 0);
});
const bootKeyframes = [
  { t: 480, label: '0.48s · drop' },
  { t: 740, label: '0.74s · land squash' },
  { t: 1200, label: '1.2s · wordmark types' },
  { t: 1650, label: '1.65s · hop' },
  { t: 2240, label: '2.24s · chunk wipe' },
];
const s = (ms: number) => (ms / 1000).toFixed(2) + 's';
const bootSteps = [
  { t: `0–${s(BOOT.DROP_START)}`, d: 'Blank background. Boot init runs here and in the other idle gaps.' },
  {
    t: `${s(BOOT.DROP_START)}–${s(BOOT.DROP_END)}`,
    d: `Bird drops ${BOOT.DROP_PX}px into place, ease-out. Each frame pushes the union of the old and new logo rects.`,
  },
  {
    t: `${s(BOOT.DROP_END)}–${s(BOOT.SQUASH_END)}`,
    d: `Landing squash: one pre-baked ${squashRaster.w}×${squashRaster.h} frame, bottom-centre anchored, then the normal sprite.`,
  },
  {
    t: `${s(BOOT.LETTER_START)}–${s(BOOT.LETTER_START + 6 * BOOT.LETTER_STEP)}`,
    d: `"openpod" types in, one glyph every ${BOOT.LETTER_STEP}ms: "open" in TEXT, "pod" in ACCENT.`,
  },
  { t: `${s(BOOT.HOP_START)}–${s(BOOT.HOP_END)}`, d: `A single ${BOOT.HOP_PX}px hop.` },
  {
    t: `${s(BOOT.WIPE_AT)}–${s(BOOT.WIPE_AT + 8 * BOOT.CHUNK_MS)}`,
    d: `Chunk wipe into the menu, ${BOOT.CHUNK_MS}ms per chunk. Held on the last frame until boot init is done; a button press skips ahead.`,
  },
];
const paletteName = (key: PaletteKey) => PALETTE_META.find((m) => m.key === key)?.name ?? key;

function to565(hex: string): string {
  const v = hexTo565(hex);
  return '0x' + v.toString(16).toUpperCase().padStart(4, '0');
}

/** Trailing-edge debounce; `cancel` drops a pending call. */
function debounce<A extends unknown[]>(fn: (...args: A) => void, ms: number) {
  let t: ReturnType<typeof setTimeout> | undefined;
  const run = (...args: A) => {
    if (t) clearTimeout(t);
    t = setTimeout(() => {
      t = undefined;
      fn(...args);
    }, ms);
  };
  run.cancel = () => t && clearTimeout(t);
  return run;
}

// A color-picker drag fires `input` continuously and every palette change
// repaints all screens and sprites, so only apply the value once it settles.
const colorInputs = new Map<PaletteKey, ReturnType<typeof debounce<[string]>>>();

function setColor(key: PaletteKey, e: Event) {
  const value = (e.target as HTMLInputElement).value;
  let apply = colorInputs.get(key);
  if (!apply) {
    apply = debounce((v: string) => (palette[key] = normalizeHex(v)), 40);
    colorInputs.set(key, apply);
  }
  apply(value);
}

function setHex(key: PaletteKey, e: Event) {
  palette[key] = normalizeHex((e.target as HTMLInputElement).value);
}

function reset() {
  Object.assign(palette, DEFAULT_PALETTE);
}

// Remember the last palette per browser. Storage can be unavailable (private
// mode, blocked site data), so every access is guarded. Loaded on mount rather
// than at setup so SSR and the first client render agree.
function loadPalette() {
  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? 'null') as Partial<Palette> | null;
    if (!saved) return;
    for (const { key } of PALETTE_META) {
      const v = saved[key];
      if (typeof v === 'string') palette[key] = normalizeHex(v);
    }
  } catch {
    // ignore unreadable or corrupt storage
  }
}

onMounted(() => {
  loadPalette();
  renderWordmark()
    .then((w) => (wordmark.value = w))
    .catch((e) => flash(`Wordmark failed: ${e}`));
  watch(palette, savePalette, { deep: true });
});

function persistPalette() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(palette));
  } catch {
    // storage unavailable
  }
}

const savePalette = debounce(persistPalette, 300);

onBeforeUnmount(() => {
  colorInputs.forEach((apply) => apply.cancel());
  // flush instead of dropping an edit made just before leaving the page
  savePalette.cancel();
  persistPalette();
});

function flash(msg: string) {
  status.value = msg;
  if (statusTimer) clearTimeout(statusTimer);
  statusTimer = setTimeout(() => (status.value = ''), 2200);
}

function exportSprites() {
  downloadText('sprites.h', buildSpritesHeader(palette));
  flash('Downloaded sprites.h');
}

function exportTheme() {
  downloadText('theme.h', buildThemeHeader(palette));
  flash('Downloaded theme.h');
}

function logoHeader(): string | null {
  return wordmark.value ? buildLogoHeader(palette, logoRaster, squashRaster, wordmark.value) : null;
}

function exportLogo() {
  const h = logoHeader();
  if (!h) return;
  downloadText('logo.h', h);
  flash('Downloaded logo.h');
}

async function copyLogo() {
  const h = logoHeader();
  if (!h) return;
  const ok = await copyToClipboard(h);
  flash(ok ? 'Copied logo.h to clipboard' : 'Copy failed');
}

async function exportPngs() {
  await exportAllPngs(palette);
  flash('Downloaded sprite PNGs');
}

async function copySprites() {
  const ok = await copyToClipboard(buildSpritesHeader(palette));
  flash(ok ? 'Copied sprites.h to clipboard' : 'Copy failed');
}

async function copyTheme() {
  const ok = await copyToClipboard(buildThemeHeader(palette));
  flash(ok ? 'Copied theme.h to clipboard' : 'Copy failed');
}
</script>

<template>
  <div class="ub">
    <aside class="ub-controls">
      <section>
        <h3>Palette</h3>
        <div class="palette-list">
          <div v-for="meta in PALETTE_META" :key="meta.key" class="palette-row">
            <label class="swatch" :style="{ background: palette[meta.key] }">
              <input type="color" :value="palette[meta.key]" @input="setColor(meta.key, $event)" />
            </label>
            <div class="palette-info">
              <div class="palette-line">
                <span class="palette-name">{{ meta.name }}</span>
                <input
                  class="palette-hex"
                  :value="palette[meta.key]"
                  spellcheck="false"
                  @change="setHex(meta.key, $event)"
                />
                <span class="palette-565">{{ to565(palette[meta.key]) }}</span>
              </div>
              <div class="palette-desc">{{ meta.description }}</div>
            </div>
          </div>
        </div>
        <n-button size="tiny" quaternary @click="reset">Reset to defaults</n-button>
      </section>

      <section>
        <h3>Options</h3>
        <div class="option-row">
          <span>Screen zoom</span>
          <n-select v-model:value="zoom" :options="zoomOptions" style="width: 90px" />
        </div>
        <div class="option-row">
          <span>Chunk grid</span>
          <n-switch v-model:value="grid" />
        </div>
        <div class="option-row">
          <span>Playing</span>
          <n-switch v-model:value="playing" />
        </div>
      </section>

      <section>
        <h3>Export</h3>
        <div class="export-buttons">
          <n-button type="primary" @click="exportSprites">Download sprites.h</n-button>
          <n-button @click="exportTheme">Download theme.h</n-button>
          <n-button @click="copySprites">Copy sprites.h</n-button>
          <n-button @click="copyTheme">Copy theme.h</n-button>
          <n-button :disabled="!wordmark" @click="exportLogo">Download logo.h</n-button>
          <n-button :disabled="!wordmark" @click="copyLogo">Copy logo.h</n-button>
          <n-button @click="exportPngs">Download PNG previews</n-button>
        </div>
        <div v-if="status" class="status">{{ status }}</div>
      </section>
    </aside>

    <main class="ub-preview">
      <section>
        <h3>Now playing</h3>
        <p class="lede">
          160px cover on the left, metadata column on the right, and every changing element confined
          to a single 30px chunk at the bottom of the screen, so a progress tick or a volume change
          costs one 320×30 buffer push instead of a full body redraw.
        </p>
        <div class="states">
          <figure v-for="s in nowPlaying" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
      </section>

      <section>
        <h3>Track list</h3>
        <p class="lede">
          Seven rows of exactly 30px fill y=30…240, so every row is one CHUNK_HEIGHT buffer on a chunk
          boundary. Moving the highlight redraws two rows, not the screen; scrolling redraws all seven
          but reuses one buffer seven times. While the wheel spins fast on a long list, a letter overlay
          covers exactly chunks 3–5 (1, 2 or 3 letters for ≤150, ≤1500 or more items) and the thumb widens
          to 7px; it hides ~600ms after the wheel slows down.
        </p>
        <div class="states">
          <figure v-for="s in trackLists" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
      </section>

      <section>
        <h3>Search</h3>
        <p class="lede">
          The query lives in the header, and the letter ribbon under it is the wheel's input surface:
          scrolling moves the cursor through the alphabet, a click appends. While typing, the ribbon
          occupies chunk 1 and six results fit below; clicking into the list drops the ribbon and a
          seventh row takes its place.
        </p>
        <div class="states">
          <figure v-for="s in searches" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
      </section>

      <section>
        <h3>Bluetooth</h3>
        <p class="lede">
          Settings › Bluetooth uses the track list's grid: a 30px header and seven 30px rows, so every
          Bluetooth event repaints the header, the 11×11 icon box at (270,10), one row or a run of rows,
          never the whole screen. Read-only rows (In use, Signal) are skipped by the cursor. Toasts take
          over the header chunk for 2.5s and never cover a row. On Now Playing the icon and the codec in
          use sit before the clock.
        </p>
        <div class="states">
          <figure v-for="s in [...bluetooth.icons, ...bluetooth.lists]" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
        <div class="states">
          <figure v-for="s in bluetooth.devices" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
        <div class="states">
          <figure v-for="s in bluetooth.toasts" :key="s.label" class="state">
            <ScreenCanvas :palette="palette" :screen="s.screen" :zoom="zoom" />
            <figcaption>{{ s.label }}</figcaption>
          </figure>
        </div>
      </section>

      <section>
        <h3>Title screen <span class="muted">(Drop &amp; hop)</span></h3>
        <p class="lede">
          The bird at its native 99×122 at (110, 44), the wordmark under it, then a chunk wipe into the
          root menu. The logo is rasterized from <code>public/openpod_logo.svg</code> into 4-bit palette
          indices, one per SVG colour, so its pixels never change with the theme: only the 16-colour
          palette in <code>logo.h</code> does. Each SVG colour follows one theme colour; colours sharing
          it form a ramp that keeps the SVG's lightness steps between facets. The wordmark is rendered
          here in IBM Plex Sans 600 28px and baked as 4-bit coverage with a BG→colour ramp.
        </p>
        <div class="logo">
          <figure class="state">
            <BootStage
              :palette="palette"
              :logo="logoRaster"
              :squash="squashRaster"
              :word="wordmark"
              :zoom="zoom"
              :playing="bootPlaying"
            />
            <figcaption class="boot-caption">
              <span>live loop · logo.h pixel data {{ bootBytes }} B</span>
              <span class="boot-toggle">play <n-switch v-model:value="bootPlaying" size="small" /></span>
            </figcaption>
          </figure>
          <div class="logo-map">
            <div v-for="(role, i) in LOGO_ROLES" :key="role.svg" class="logo-map-row">
              <span class="logo-chip" :style="{ background: role.svg }" :title="role.svg" />
              <span class="logo-arrow">→</span>
              <span class="logo-chip" :style="{ background: logoColors[i + 1] }" :title="logoColors[i + 1]" />
              <span class="logo-name">{{ role.name }}</span>
              <span class="geometry-box">{{ paletteName(role.key) }}</span>
              <span class="muted">{{ role.role }}</span>
            </div>
          </div>
        </div>
        <div class="boot-frames">
          <figure v-for="k in bootKeyframes" :key="k.t" class="boot-frame">
            <BootStage
              :palette="palette"
              :logo="logoRaster"
              :squash="squashRaster"
              :word="wordmark"
              :zoom="0.5"
              :t="k.t"
            />
            <figcaption>{{ k.label }}</figcaption>
          </figure>
        </div>
        <div class="geometry boot-steps">
          <div v-for="st in bootSteps" :key="st.t" class="boot-step">
            <span class="geometry-box">{{ st.t }}</span>
            <span class="muted">{{ st.d }}</span>
          </div>
        </div>
      </section>

      <section>
        <h3>Sprites <span class="muted">(1× and 6×, pixel grid)</span></h3>
        <div class="sprites">
          <figure v-for="def in SPRITES" :key="def.id" class="sprite">
            <div class="sprite-stage" :style="{ background: palette.bg }">
              <SpriteStage :def="def" :palette="palette" :scale="1" />
              <SpriteStage :def="def" :palette="palette" :scale="6" :grid="true" />
            </div>
            <figcaption>
              <strong>{{ def.id }}</strong>
              <span class="muted">{{ def.w }}×{{ def.h }}</span>
            </figcaption>
            <p class="sprite-note">{{ def.note }}</p>
          </figure>
        </div>
      </section>

      <section>
        <h3>Geometry</h3>
        <div class="geometry">
          <div v-for="g in GEOMETRY" :key="g.el + g.box" class="geometry-row">
            <span>{{ g.el }}</span>
            <span class="geometry-box">{{ g.box }}</span>
            <span class="muted">{{ g.note }}</span>
          </div>
        </div>
      </section>

      <section>
        <h3>Redraw budget</h3>
        <ol class="notes">
          <li v-for="n in REDRAW_NOTES" :key="n.i">
            <span class="note-i">{{ n.i }}</span>
            <span>{{ n.t }}</span>
          </li>
        </ol>
      </section>
    </main>
  </div>
</template>

<style>
/* Full-bleed the builder past the .vp-doc content column, centered on the
   viewport (the page sets `aside: false` so the column itself is centered). */
.ub {
  position: relative;
  left: 50%;
  transform: translateX(-50%);
  width: min(1280px, calc(100vw - 48px));
}
</style>

<style scoped>
.ub {
  display: grid;
  grid-template-columns: 320px minmax(0, 1fr);
  gap: 32px;
  align-items: start;
  font-size: 14px;
  color: var(--vp-c-text-1);
}

h3 {
  margin: 0 0 12px;
  font-size: 12px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--vp-c-text-2);
  font-weight: 600;
}

section {
  margin-bottom: 28px;
}

.ub-controls {
  position: sticky;
  top: 24px;
  display: flex;
  flex-direction: column;
  gap: 4px;
  background: var(--vp-c-bg-soft);
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  padding: 20px;
}

.palette-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.palette-row {
  display: flex;
  gap: 10px;
  align-items: flex-start;
}

.swatch {
  flex: none;
  width: 38px;
  height: 38px;
  border-radius: 7px;
  border: 1px solid var(--vp-c-divider);
  cursor: pointer;
  position: relative;
  overflow: hidden;
}

.swatch input[type='color'] {
  position: absolute;
  inset: -4px;
  width: calc(100% + 8px);
  height: calc(100% + 8px);
  opacity: 0;
  cursor: pointer;
}

.palette-info {
  flex: 1;
  min-width: 0;
}

.palette-line {
  display: flex;
  align-items: center;
  gap: 6px;
}

.palette-name {
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 12px;
  font-weight: 600;
  white-space: nowrap;
}

.palette-hex {
  width: 76px;
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 12px;
  padding: 2px 4px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 4px;
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
}

.palette-565 {
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 11px;
  color: var(--vp-c-text-3);
}

.palette-desc {
  font-size: 11px;
  color: var(--vp-c-text-3);
  line-height: 1.35;
  margin-top: 1px;
}

.option-row {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 6px 0;
}

.option-row > span {
  flex: 1;
  font-size: 13px;
}

.export-buttons {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.status {
  margin-top: 10px;
  font-size: 12px;
  color: var(--vp-c-brand-1);
}

.ub-preview {
  min-width: 0;
}

.states {
  display: flex;
  flex-wrap: wrap;
  gap: 20px;
}

.state {
  margin: 0;
}

.state figcaption {
  margin-top: 6px;
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 11px;
  color: var(--vp-c-text-2);
}

.logo {
  display: flex;
  flex-wrap: wrap;
  gap: 24px;
  align-items: flex-start;
}

.boot-caption {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
}

.boot-toggle {
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.boot-frames {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  margin-top: 18px;
}

.boot-frame {
  margin: 0;
}

.boot-frame figcaption {
  margin-top: 4px;
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 10px;
  color: var(--vp-c-text-3);
}

.boot-steps {
  margin-top: 16px;
}

.boot-step {
  display: grid;
  grid-template-columns: 96px 1fr;
  gap: 14px;
  padding: 8px 14px;
  border-bottom: 1px solid var(--vp-c-divider);
  font-size: 12px;
  line-height: 18px;
}

.boot-step:last-child {
  border-bottom: none;
}

.boot-step .geometry-box {
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 11px;
}

.logo-map {
  flex: 1;
  min-width: 300px;
  max-width: 520px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  padding: 6px 0;
}

.logo-map-row {
  display: grid;
  grid-template-columns: 16px 12px 16px 56px 130px 1fr;
  gap: 8px;
  align-items: center;
  padding: 3px 12px;
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 11px;
  line-height: 16px;
}

.logo-chip {
  width: 16px;
  height: 16px;
  border-radius: 4px;
  border: 1px solid var(--vp-c-divider);
}

.logo-arrow {
  color: var(--vp-c-text-3);
  text-align: center;
}

.sprites {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));
  gap: 16px;
}

.sprite {
  margin: 0;
  padding: 14px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg-soft);
}

.sprite-stage {
  max-width: 100%;
  box-sizing: border-box;
  overflow-x: auto;
  padding: 10px;
  border-radius: 6px;
  display: inline-flex;
  align-items: flex-end;
  gap: 16px;
}

.lede {
  margin: -4px 0 16px;
  max-width: 760px;
  font-size: 13px;
  line-height: 1.6;
  color: var(--vp-c-text-2);
}

.geometry {
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  overflow: hidden;
  max-width: 900px;
}

.geometry-row {
  display: grid;
  grid-template-columns: 1.1fr 1fr 1.3fr;
  gap: 16px;
  padding: 8px 14px;
  border-bottom: 1px solid var(--vp-c-divider);
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 11px;
  line-height: 16px;
}

.geometry-row:last-child {
  border-bottom: none;
}

.geometry-box {
  color: var(--vp-c-brand-1);
}

.notes {
  list-style: none;
  margin: 0;
  padding: 0;
  max-width: 820px;
}

.notes li {
  display: grid;
  grid-template-columns: 26px 1fr;
  gap: 12px;
  margin: 0 0 10px;
  font-size: 13px;
  line-height: 20px;
  color: var(--vp-c-text-2);
}

.note-i {
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  color: var(--vp-c-text-3);
}

.sprite figcaption {
  display: flex;
  justify-content: space-between;
  align-items: baseline;
  margin-top: 10px;
  font-family: ui-monospace, 'SF Mono', 'Cascadia Code', monospace;
  font-size: 12px;
}

.sprite-note {
  margin: 6px 0 0;
  font-size: 11px;
  line-height: 1.45;
  color: var(--vp-c-text-3);
}

.muted {
  color: var(--vp-c-text-3);
  font-weight: 400;
}

@media (max-width: 900px) {
  .ub {
    grid-template-columns: 1fr;
  }
  .ub-controls {
    position: static;
  }
}
</style>

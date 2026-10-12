// The slides shown by the UI builder: every firmware screen state, grouped by
// flow. Each slide has a short UX caption and, for Spec mode, the
// implementation detail behind it.
import {
  BT_DEVICES,
  btDeviceRows,
  KID_A,
  KID_RESULTS,
  LIBRARY,
  RADIOHEAD,
  type BtIconsOpts,
  type ForgetOpts,
  type MenuRowSpec,
  type MenuScreenOpts,
  type NowPlayingOpts,
  type PodcastListOpts,
  type PodRowSpec,
  type RenameOpts,
  type SearchOpts,
  type TrackListOpts,
} from './screens';

export type Screen =
  | { kind: 'nowplaying'; opts: NowPlayingOpts }
  | { kind: 'tracklist'; opts: TrackListOpts }
  | { kind: 'search'; opts: SearchOpts }
  | { kind: 'menu'; opts: MenuScreenOpts }
  | { kind: 'podcast'; opts: PodcastListOpts }
  | { kind: 'forget'; opts: ForgetOpts }
  | { kind: 'rename'; opts: RenameOpts }
  | { kind: 'bticons'; opts: BtIconsOpts };

export type GroupId = 'nowplaying' | 'tracklist' | 'search' | 'bluetooth' | 'podcasts' | 'title';

export interface SlideGroup {
  id: GroupId;
  title: string;
  /** What the user sees, in plain words */
  summary: string;
  /** How it is built (Spec mode) */
  lede: string;
}

export interface Slide {
  id: string;
  group: GroupId;
  label: string;
  /** implementation detail, shown in Spec mode */
  spec?: string;
  /** undefined for the title-screen animation, drawn by BootStage */
  screen?: Screen;
}

export const GROUPS: SlideGroup[] = [
  {
    id: 'nowplaying',
    title: 'Now playing',
    summary: 'Cover art, title, artist and album, with the progress bar at the bottom. Turning the wheel changes the volume; a centre click switches the wheel to seeking.',
    lede: '160px cover on the left, metadata column on the right, and every changing element confined to a single 30px chunk at the bottom of the screen, so a progress tick or a volume change costs one 320×30 buffer push instead of a full body redraw.',
  },
  {
    id: 'tracklist',
    title: 'Track list',
    summary: 'Seven rows per screen. Spinning the wheel fast on a long list shows a big letter so you know where you are.',
    lede: 'Seven rows of exactly 30px fill y=30…240, so every row is one CHUNK_HEIGHT buffer on a chunk boundary. Moving the highlight redraws two rows, not the screen; scrolling redraws all seven but reuses one buffer seven times. While the wheel spins fast on a long list, a letter overlay covers exactly chunks 3–5 (1, 2 or 3 letters for ≤150, ≤1500 or more items) and the thumb widens to 7px; it hides ~600ms after the wheel slows down.',
  },
  {
    id: 'search',
    title: 'Search',
    summary: 'Type with the wheel: scroll through the letter ribbon, click to add a letter. Results update as you type.',
    lede: "The query lives in the header, and the letter ribbon under it is the wheel's input surface: scrolling moves the cursor through the alphabet, a click appends. While typing, the ribbon occupies chunk 1 and six results fit below; clicking into the list drops the ribbon and a seventh row takes its place.",
  },
  {
    id: 'bluetooth',
    title: 'Bluetooth',
    summary: 'Turn Bluetooth on, scan for headphones, connect, rename or forget them. Short messages appear in the header.',
    lede: 'Settings › Bluetooth uses the track list\'s grid: a 30px header and seven 30px rows, so every Bluetooth event repaints the header, the 11×11 icon box at (270,10), one row or a run of rows, never the whole screen. Read-only rows (In use, Signal) are skipped by the cursor. Toasts take over the header chunk for 2.5s and never cover a row. On Now Playing the icon and the codec in use sit before the clock.',
  },
  {
    id: 'podcasts',
    title: 'Podcasts',
    summary: 'Browse publisher, show and episodes. A square marks what is new, a half square what you started, and a line under the row shows how far you got. Playback resumes where you stopped; hold Center on an episode for its actions.',
    lede: 'The track list\'s grid: a 30px header and seven 30px rows. A 6×6 mark at x=10 carries the state by shape (full, half, none), so it survives the highlight, where it turns cream; folders roll the marks up from what is inside. The 2px progress underline replaces the row rule on in-progress episodes. Series folders (every title starts with "N/M") show the number in a 28px Mono column and sort by it; other shows sort newest first. Toasts take over the header with no icon, text at x=8: grey for what you did, teal for what happened on its own.',
  },
  {
    id: 'title',
    title: 'Title screen',
    summary: 'The bird drops in, the "openpod" wordmark types itself, then the menu wipes in.',
    lede: 'The bird at its native 99×122 at (110, 44), the wordmark under it, then a chunk wipe into the root menu. The logo is rasterized from public/openpod_logo.svg into 4-bit palette indices, one per SVG colour, so its pixels never change with the theme: only the 16-colour palette in logo.h does. Each SVG colour follows one theme colour; colours sharing it form a ramp that keeps the SVG\'s lightness steps between facets. The wordmark is rendered in IBM Plex Sans 600 28px and baked as 4-bit coverage with a BG→colour ramp.',
  },
];

// Bluetooth rows mirror the firmware's BtMenu (software/src/state/bt_menu.cpp).
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

// Podcast rows, France Culture sample data from the design handoff.
const TCHEKHOV = 'Envie de dormir : troubles dans le sommeil : Anton Tchekhov, fatigué de la vie';
const WEIL = 'Simone Weil au pouvoir : défendons nos besoins essentiels !';
const VARGAS = 'Fred vargas: Debout les morts';
const continueRow: PodRowSpec = { title: TCHEKHOV, mark: 'progress', status: '−34:13', statusTone: 'accent', statusMono: true, progress: 0.41 };
const folder = (title: string, o: { new?: number; eps?: number; mark?: PodRowSpec['mark']; played?: boolean }): PodRowSpec => ({
  title,
  mark: o.mark ?? (o.new ? 'new' : 'none'),
  played: o.played,
  status: o.new ? `${o.new} new` : `${o.eps} ep`,
  statusTone: o.new ? 'accent' : 'dimmer',
  chevron: true,
});
const datedRows = (o: { tchekhovPlayed?: boolean } = {}): PodRowSpec[] => [
  o.tchekhovPlayed
    ? { title: TCHEKHOV, mark: 'none', played: true, status: '12 Oct' }
    : { title: TCHEKHOV, mark: 'progress', status: '12 Oct', progress: 0.41 },
  { title: WEIL, mark: 'new', status: '10 Oct' },
  { title: 'Hannah Arendt, penser sans rampe', mark: 'new', status: '9 Oct' },
  { title: "Lucrèce, la nature des choses", mark: 'none', played: true, status: '8 Oct' },
  { title: 'Spinoza et la joie', mark: 'none', played: true, status: '7 Oct' },
  { title: 'Montaigne, que sais-je ?', mark: 'none', played: true, status: '6 Oct' },
  { title: 'Pascal, le divertissement', mark: 'none', played: true, status: '3 Oct' },
];
const seriesRows: PodRowSpec[] = [
  { title: 'Le hêtre', number: '1/10', mark: 'none', played: true, status: '28:40', statusMono: true },
  { title: 'Marc, Mathias et Lucien', number: '2/10', mark: 'progress', status: '29:05', statusMono: true, progress: 0.62 },
];
const actions = (state: 'progress' | 'new' | 'played'): MenuRowSpec[] => [
  ...(state === 'progress'
    ? [{ label: 'Resume', value: '23:47', mono: true, valueTone: 'dimmer' as const }, { label: 'Play from start' }, { label: 'Mark as played' }]
    : state === 'new'
      ? [{ label: 'Play' }, { label: 'Mark as played' }]
      : [{ label: 'Play' }, { label: 'Mark as unplayed' }]),
  { label: 'Published', kind: 'readonly', value: 'Mon, 12 Oct 2026', valueTone: 'dimmer' },
  { label: 'Length', kind: 'readonly', value: '58:00', mono: true, valueTone: 'dimmer' },
];

export function buildSlides(o: { grid: boolean; playing: boolean }): Slide[] {
  const g = o.grid;
  const menu = (opts: Omit<MenuScreenOpts, 'grid' | 'icon' | 'selected'> & Partial<MenuScreenOpts>): Screen => ({
    kind: 'menu',
    opts: { grid: g, icon: 'connected', selected: 0, ...opts } as MenuScreenOpts,
  });
  const np = (mode: NowPlayingOpts['mode']): Screen => ({
    kind: 'nowplaying',
    opts: { mode, playing: o.playing, grid: g, bt: 'connected', codec: 'aptX HD' },
  });
  const pod = (opts: Omit<PodcastListOpts, 'grid' | 'selected'> & Partial<PodcastListOpts>): Screen => ({
    kind: 'podcast',
    opts: { grid: g, selected: 0, ...opts } as PodcastListOpts,
  });
  const podNp = (podcast: NonNullable<NowPlayingOpts['podcast']>, toast?: NowPlayingOpts['toast']): Screen => ({
    kind: 'nowplaying',
    opts: { mode: 'play', playing: o.playing, grid: g, bt: 'connected', podcast, toast },
  });
  const search = { query: 'kid', letter: 'K', results: KID_RESULTS, total: 24, selected: 0, first: 0, grid: g };

  return [
    { id: 'np-play', group: 'nowplaying', label: 'Playing', screen: np('play') },
    { id: 'np-volume', group: 'nowplaying', label: 'Volume', spec: 'wheel scrub, hides after 1.5s', screen: np('volume') },
    { id: 'np-seek', group: 'nowplaying', label: 'Seeking', spec: 'centre click, then the wheel scrubs', screen: np('seek') },

    {
      id: 'tl-top',
      group: 'tracklist',
      label: 'Album, top of the list',
      screen: { kind: 'tracklist', opts: { heading: 'Kid A', tracks: KID_A, first: 0, selected: 2, grid: g } },
    },
    {
      id: 'tl-scrolled',
      group: 'tracklist',
      label: 'Artist, scrolled down',
      spec: 'last row selected',
      screen: { kind: 'tracklist', opts: { heading: 'Radiohead', tracks: RADIOHEAD, first: 5, selected: 6, grid: g } },
    },
    {
      id: 'tl-fast',
      group: 'tracklist',
      label: 'Fast scroll through 800 songs',
      spec: '2-letter label for ≤1500 items',
      screen: {
        kind: 'tracklist',
        opts: { heading: 'Songs', tracks: LIBRARY, total: 800, first: 410, selected: 2, numbered: false, fastScroll: true, grid: g },
      },
    },

    {
      id: 'se-typing',
      group: 'search',
      label: 'Typing',
      spec: 'ribbon + 6 results',
      screen: { kind: 'search', opts: { ...search, mode: 'typing' } },
    },
    {
      id: 'se-browsing',
      group: 'search',
      label: 'Browsing the results',
      spec: 'ribbon dropped, 7 rows',
      screen: { kind: 'search', opts: { ...search, mode: 'browsing', selected: 2 } },
    },
    {
      id: 'se-empty',
      group: 'search',
      label: 'No matches',
      screen: { kind: 'search', opts: { ...search, mode: 'typing', query: 'kidz', letter: 'Z', results: [], total: 0 } },
    },

    {
      id: 'bt-settings',
      group: 'bluetooth',
      label: 'Settings',
      spec: 'Bluetooth is the first entry',
      screen: menu({ title: 'Settings', rows: settingsRows({ note: 'Sony WH-1000XM4', noteTone: 'accent' }) }),
    },
    {
      id: 'bt-off',
      group: 'bluetooth',
      label: 'Bluetooth off',
      spec: 'other rows hidden',
      screen: menu({
        title: 'Bluetooth',
        icon: 'off',
        rows: [radioRow(false)],
        hint: { fromChunk: 2, title: 'Bluetooth is off', sub: 'paired devices are kept' },
      }),
    },
    {
      id: 'bt-empty',
      group: 'bluetooth',
      label: 'No devices yet',
      spec: 'cursor on Scan',
      screen: menu({
        title: 'Bluetooth',
        icon: 'on',
        rows: [radioRow(true), scanRow],
        selected: 1,
        hint: { fromChunk: 3, title: 'No devices yet', sub: 'scan to find headphones' },
      }),
    },
    {
      id: 'bt-scanning',
      group: 'bluetooth',
      label: 'Scanning',
      spec: 'only row 2 repaints',
      screen: menu({
        title: 'Bluetooth',
        icon: 'on',
        rows: [radioRow(true), scanningRow(5600), ...btDeviceRows(BT_DEVICES.slice(3, 6))],
        selected: 1,
      }),
    },
    {
      id: 'bt-list',
      group: 'bluetooth',
      label: 'Device list',
      spec: 'connected first, then paired, then by signal',
      screen: menu({ title: 'Bluetooth', rows: [radioRow(true), scanRow, ...btDeviceRows(BT_DEVICES)], selected: 3, first: 1 }),
    },
    {
      id: 'bt-connected',
      group: 'bluetooth',
      label: 'Connected device',
      spec: 'cursor on Codec',
      screen: menu({ title: 'Sony WH-1000XM4', rows: deviceRows({ link: 'connected', paired: true }), selected: 1 }),
    },
    {
      id: 'bt-paired',
      group: 'bluetooth',
      label: 'Paired device, not connected',
      spec: 'alias set',
      screen: menu({ title: 'Kitchen', icon: 'on', rows: deviceRows({ link: 'idle', paired: true, alias: 'JBL Charge 4' }) }),
    },
    {
      id: 'bt-connecting',
      group: 'bluetooth',
      label: 'Connecting',
      spec: 'header icon blinks every 500ms',
      screen: menu({ title: 'Bose QC45', icon: 'connecting', blinkOn: true, rows: deviceRows({ link: 'connecting', paired: false }) }),
    },
    {
      id: 'bt-failed',
      group: 'bluetooth',
      label: 'Connection failed',
      spec: 'shown for 2s',
      screen: menu({ title: 'Bose QC45', icon: 'on', rows: deviceRows({ link: 'failed', paired: false }) }),
    },
    {
      id: 'bt-forget',
      group: 'bluetooth',
      label: 'Forget device?',
      spec: 'Cancel preselected',
      screen: { kind: 'forget', opts: { name: 'Kitchen', selected: 0, icon: 'on', grid: g } },
    },
    {
      id: 'bt-rename',
      group: 'bluetooth',
      label: 'Rename',
      spec: 'same wheel input as search',
      screen: { kind: 'rename', opts: { text: 'kitchen', letter: 'n', broadcastName: 'JBL Charge 4', icon: 'on', grid: g } },
    },
    {
      id: 'bt-toast-ok',
      group: 'bluetooth',
      label: 'Message: connected',
      spec: 'success toast over a list',
      screen: menu({
        title: 'Bluetooth',
        rows: [radioRow(true), scanRow, ...btDeviceRows(BT_DEVICES.slice(0, 3))],
        selected: 2,
        toast: { tone: 'success', before: 'Connected to ', name: 'Sony WH-1000XM4' },
      }),
    },
    {
      id: 'bt-toast-neutral',
      group: 'bluetooth',
      label: 'Message: scan finished',
      spec: 'neutral toast, for something the user did',
      screen: menu({
        title: 'Bluetooth',
        icon: 'on',
        rows: [radioRow(true), scanRow, ...btDeviceRows(BT_DEVICES.slice(1, 8))],
        selected: 1,
        toast: { tone: 'neutral', before: 'Scan finished' },
      }),
    },
    {
      id: 'bt-toast-error',
      group: 'bluetooth',
      label: 'Message: connection lost',
      spec: 'error toast over Now Playing, playback paused',
      screen: {
        kind: 'nowplaying',
        opts: {
          mode: 'play',
          playing: false,
          grid: g,
          bt: 'on',
          toast: { tone: 'error', before: 'Lost ', name: 'Sony WH-1000XM4', after: ' · paused' },
        },
      },
    },
    {
      id: 'bt-icons',
      group: 'bluetooth',
      label: 'Header icon and signal bars',
      spec: '11×11 icon box at (270,10)',
      screen: { kind: 'bticons', opts: { grid: g } },
    },

    {
      id: 'pc-root',
      group: 'podcasts',
      label: 'Podcasts',
      spec: 'continue row on top, cursor starts there',
      screen: pod({ title: 'Podcasts', rows: [continueRow, folder('France Culture', { new: 3 }), folder("L'Équipe", { eps: 4, mark: 'none', played: true })] }),
    },
    {
      id: 'pc-publisher',
      group: 'podcasts',
      label: 'Publisher',
      spec: 'folder marks rolled up',
      screen: pod({ title: 'France Culture', rows: [folder('Les chemins de la philosophie', { new: 1 }), folder('La serie Fiction', { new: 2 })] }),
    },
    {
      id: 'pc-dated',
      group: 'podcasts',
      label: 'Dated show',
      spec: 'newest first, cursor on the newest unfinished',
      screen: pod({ title: 'Les chemins de la philosophie', rows: datedRows() }),
    },
    {
      id: 'pc-show-series',
      group: 'podcasts',
      label: 'Show with series',
      spec: 'a series is a sub-folder',
      screen: pod({
        title: 'La serie Fiction',
        rows: [folder(VARGAS, { eps: 2, mark: 'progress' }), folder('"Jane Eyre" de Charlotte Brontë', { new: 2 }), folder('Older series', { eps: 5, played: true })],
      }),
    },
    {
      id: 'pc-series',
      group: 'podcasts',
      label: 'Series episodes',
      spec: '"N/M" moves to the Mono column',
      screen: pod({ title: VARGAS, rows: seriesRows, selected: 1 }),
    },
    {
      id: 'pc-np-series',
      group: 'podcasts',
      label: 'Now playing, series',
      spec: 'time left on the right',
      screen: podNp({ index: '2/10', title: 'Marc, Mathias et Lucien', show: VARGAS, third: 'La serie Fiction', elapsed: '18:02', left: '−11:03', frac: 0.62 }),
    },
    {
      id: 'pc-np-resumed',
      group: 'podcasts',
      label: 'Resumed',
      spec: 'neutral toast, title clamped at 5 lines',
      screen: podNp(
        { index: '1/7', title: TCHEKHOV, show: 'Les chemins de la philosophie', third: 'Mon, 12 Oct 2026', thirdMono: true, elapsed: '23:42', left: '−34:18', frac: 0.41 },
        { tone: 'neutral', icon: false, before: 'Resumed at ', name: '23:47' },
      ),
    },
    {
      id: 'pc-actions',
      group: 'podcasts',
      label: 'Episode actions, in progress',
      spec: 'hold Center on an episode row',
      screen: menu({ title: TCHEKHOV, icon: 'on', rows: actions('progress') }),
    },
    {
      id: 'pc-actions-new',
      group: 'podcasts',
      label: 'Episode actions, new',
      screen: menu({ title: WEIL, icon: 'on', rows: actions('new') }),
    },
    {
      id: 'pc-actions-played',
      group: 'podcasts',
      label: 'Episode actions, played',
      screen: menu({ title: 'Spinoza et la joie', icon: 'on', rows: actions('played') }),
    },
    {
      id: 'pc-end-series',
      group: 'podcasts',
      label: 'End of a series episode',
      spec: 'the next number starts on its own',
      screen: podNp(
        { index: '2/10', title: 'Marc, Mathias et Lucien', show: VARGAS, third: 'La serie Fiction', elapsed: '0:00', left: '−29:05', frac: 0 },
        { tone: 'success', icon: false, before: 'Finished ', name: '1/10', after: ' · playing 2/10' },
      ),
    },
    {
      id: 'pc-end-dated',
      group: 'podcasts',
      label: 'End of a dated episode',
      spec: 'stops, back to the list, cursor on the next unfinished',
      screen: pod({
        title: 'Les chemins de la philosophie',
        rows: datedRows({ tchekhovPlayed: true }),
        selected: 1,
        toast: { tone: 'success', icon: false, before: 'Finished · marked as played' },
      }),
    },

    { id: 'title', group: 'title', label: 'Title screen animation', spec: 'live loop of logo.h' },
  ];
}

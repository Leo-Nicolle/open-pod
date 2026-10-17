// App state shared by the screens, and the actions that change it.
import { computed, reactive } from 'vue';
import {
  api,
  type CardInfo,
  type CardPodcasts,
  type Cards,
  type LibraryView,
  type PlanSummary,
  type ReadProgress,
  type Settings,
  type SettingsUpdate,
  type SourceEpisode,
  type SyncProgress,
  type SyncReport,
} from './api';

export type Screen = 'setup' | 'whatsnew' | 'syncing' | 'library' | 'settings';

export const store = reactive({
  screen: 'setup' as Screen,
  cards: null as Cards | null,
  settings: null as Settings | null,
  /** Reading an unread card once. */
  read: { running: false, progress: null as ReadProgress | null, error: null as string | null },
  plan: {
    checking: false,
    summary: null as PlanSummary | null,
    /** When it was computed (ms) and how long it took (s). */
    at: 0,
    took: 0,
    error: null as string | null,
  },
  sync: {
    state: 'idle' as 'idle' | 'running' | 'stopped' | 'done' | 'error',
    stopping: false,
    progress: null as SyncProgress | null,
    report: null as SyncReport | null,
    error: null as string | null,
    /** Files the sync set out to write (tracks, then episodes), for "85 of 761". */
    total: 0,
    /** Podcast episodes among them: the counters then say "tracks & episodes". */
    episodes: 0,
  },
  library: null as LibraryView | null,
  podcasts: null as CardPodcasts | null,
  /** Podcast episodes in the podcasts folder but not on the card, from the
   * last check: kept when a keep setting changes, since they don't depend
   * on it (the show page re-ranks them). */
  podSource: null as SourceEpisode[] | null,
  report: null as SyncReport | null,
  /** "On the card": tab, select mode, and the albums ticked for removal. */
  libraryTab: 'albums' as 'albums' | 'podcasts' | 'removed',
  /** The open show (CardShow.key) on the Podcasts tab. */
  podcastShow: null as string | null,
  librarySelect: false,
  selection: new Set<string>(),
  /** Removals and bring-backs asked for during a sync, applied when it ends. */
  queued: { remove: new Set<string>(), back: new Set<string>() },
  toast: null as string | null,
});

export const card = computed<CardInfo | null>(() => store.cards?.selected ?? null);

export const canCheck = computed(() => {
  const c = card.value;
  return !!c && (c.state.kind === 'ready' || c.state.kind === 'empty') && !store.read.running && !!store.settings?.libraries.length;
});

let toastTimer: number | undefined;
export function toast(msg: string) {
  store.toast = msg;
  clearTimeout(toastTimer);
  toastTimer = window.setTimeout(() => (store.toast = null), 4000);
}

const message = (e: unknown) => (typeof e === 'string' ? e : e instanceof Error ? e.message : String(e));

export async function refreshCards() {
  try {
    store.cards = await api.cards();
  } catch (e) {
    toast(message(e));
  }
}

export async function loadSettings() {
  store.settings = await api.settings();
}

export async function saveSettings(update: SettingsUpdate) {
  try {
    store.settings = await api.saveSettings(update);
    store.plan.summary = null;
  } catch (e) {
    toast(message(e));
  }
}

export async function addLibrary() {
  const path = await api.pickFolder('Choose a music folder');
  if (!path || !store.settings) return;
  const libs = store.settings.libraries.map((l) => l.path);
  if (!libs.includes(path)) await saveSettings({ libraries: [...libs, path] });
}

export async function removeLibrary(path: string) {
  if (!store.settings) return;
  await saveSettings({ libraries: store.settings.libraries.map((l) => l.path).filter((p) => p !== path) });
}

/** The podcasts folder: mirrored to the card, so episodes removed there
 * are removed from the card too. */
export async function choosePodcasts() {
  const path = await api.pickFolder('Choose your podcasts folder', store.settings?.podcasts?.path, false, 'podcasts');
  if (path) await saveSettings({ podcasts: path });
}

export async function stopPodcasts() {
  await saveSettings({ podcasts: '' });
}

/** Excluded folders are stored relative to their music folder. */
export async function addExclude() {
  const s = store.settings;
  if (!s) return;
  const path = await api.pickFolder('Choose a folder to leave out', s.libraries[0]?.path);
  if (!path) return;
  const lib = s.libraries.find((l) => path.startsWith(l.path));
  if (!lib) {
    toast('Choose a folder inside one of your music folders.');
    return;
  }
  const rel = path.slice(lib.path.length).replace(/^[\\/]+/, '');
  if (rel && !s.exclude.includes(rel)) await saveSettings({ exclude: [...s.exclude, rel] });
}

export async function removeExclude(rel: string) {
  if (store.settings) await saveSettings({ exclude: store.settings.exclude.filter((e) => e !== rel) });
}

export async function chooseCardFolder() {
  const path = await api.pickFolder("Choose the SD card's folder", undefined, true);
  if (!path) return;
  await api.selectCard(path);
  await refreshCards();
}

export async function useCard(path: string) {
  await api.selectCard(path);
  store.plan.summary = null;
  store.podSource = null;
  store.library = null;
  store.podcasts = null;
  await refreshCards();
}

export async function readCard() {
  store.read = { running: true, progress: null, error: null };
  try {
    await api.readCard();
  } catch (e) {
    store.read = { running: false, progress: null, error: message(e) };
  }
}

export async function check() {
  store.screen = 'whatsnew';
  store.plan.checking = true;
  store.plan.error = null;
  const t0 = performance.now();
  try {
    store.plan.summary = await api.check();
    store.podSource = store.plan.summary.podcasts?.episodes ?? null;
    store.plan.at = Date.now();
    store.plan.took = (performance.now() - t0) / 1000;
  } catch (e) {
    store.plan.error = message(e);
  } finally {
    store.plan.checking = false;
  }
}

export async function startSync() {
  store.sync = {
    state: 'running',
    stopping: false,
    progress: null,
    report: null,
    error: null,
    total: store.plan.summary
      ? store.plan.summary.tracks +
        store.plan.summary.upgrades +
        store.plan.summary.fixes +
        (store.plan.summary.podcasts?.new ?? 0)
      : 0,
    episodes: store.plan.summary?.podcasts?.new ?? 0,
  };
  store.screen = 'syncing';
  try {
    await api.syncStart();
    void refreshCards();
  } catch (e) {
    store.sync.state = 'error';
    store.sync.error = message(e);
  }
}

export async function stopSync() {
  store.sync.stopping = true;
  await api.syncStop();
}

export async function loadLibrary() {
  try {
    store.library = await api.library();
  } catch (e) {
    toast(message(e));
  }
}

export async function loadPodcasts() {
  try {
    store.podcasts = await api.podcasts();
  } catch (e) {
    toast(message(e));
  }
}

/** How many latest episodes a show keeps on the card (null: all). What's
 * new has to be checked again. */
export async function setPodcastKeep(show: string, keep: number | null) {
  // Shown at once; the card is written behind it.
  const sh = store.podcasts?.shows.find((s) => s.key === show);
  if (sh) sh.keep = keep;
  try {
    store.podcasts = await api.setPodcastKeep(show, keep);
    store.plan.summary = null;
  } catch (e) {
    toast(message(e));
  }
}

/** Marks an episode played or not on the card, as the player does. */
export async function setPodcastPlayed(path: string, played: boolean) {
  try {
    store.podcasts = await api.setPodcastPlayed(path, played);
  } catch (e) {
    toast(message(e));
  }
}

export async function removeSelected() {
  const ids = [...store.selection];
  if (store.sync.state === 'running') {
    store.queued.remove = new Set([...store.queued.remove, ...ids]);
    store.selection = new Set();
    toast(`${ids.length === 1 ? 'It' : 'They'}'ll be removed when the sync finishes.`);
    return;
  }
  try {
    store.library = await api.removeAlbums(ids);
    store.selection = new Set();
    store.plan.summary = null;
    void refreshCards();
  } catch (e) {
    toast(message(e));
  }
}

export async function bringBack(id: string) {
  if (store.sync.state === 'running') {
    store.queued.back = new Set([...store.queued.back, id]);
    return;
  }
  try {
    store.library = await api.bringBack(id);
    store.plan.summary = null;
  } catch (e) {
    toast(message(e));
  }
}

/** Takes back a removal queued during the sync. */
export function unqueue(id: string) {
  const r = new Set(store.queued.remove);
  const b = new Set(store.queued.back);
  r.delete(id);
  b.delete(id);
  store.queued = { remove: r, back: b };
}

/** Applies what was queued during the sync, once it has ended. */
async function applyQueued() {
  const { remove, back } = store.queued;
  if (!remove.size && !back.size) return;
  store.queued = { remove: new Set(), back: new Set() };
  try {
    if (remove.size) store.library = await api.removeAlbums([...remove]);
    for (const id of back) store.library = await api.bringBack(id);
    store.plan.summary = null;
  } catch (e) {
    toast(message(e));
  }
  void refreshCards();
}

export async function loadReport() {
  try {
    store.report = await api.report();
  } catch {
    store.report = null;
  }
}

export async function eject() {
  try {
    await api.eject();
    toast('The card was ejected. You can unplug it.');
    await refreshCards();
  } catch (e) {
    toast(message(e));
  }
}

export async function installOnCard() {
  try {
    const name = await api.installOnCard();
    toast(`Copied ${name} to the card's opentone/tools folder.`);
  } catch (e) {
    toast(message(e));
  }
}

/** Wires backend events and starts watching for cards. */
export function start() {
  api.onReadProgress((p) => (store.read.progress = p));
  api.onReadDone((error) => {
    store.read = { running: false, progress: null, error };
    void refreshCards();
  });
  api.onSyncProgress((p) => {
    if (store.sync.state === 'idle') store.sync.state = 'running';
    store.sync.progress = p;
  });
  api.onSyncDone((d) => {
    store.sync.stopping = false;
    if (d.error) {
      store.sync.state = 'error';
      store.sync.error = d.error;
    } else {
      store.sync.report = d.report;
      store.report = d.report;
      store.sync.state = d.report?.interrupted ? 'stopped' : 'done';
    }
    store.plan.summary = null;
    store.podSource = null;
    store.library = null;
    store.podcasts = null;
    void refreshCards();
    void applyQueued();
  });
  void loadSettings();
  void refreshCards().then(async () => {
    // A window opened (or reloaded) during a sync shows it.
    const p = await api.syncStatus();
    if (p) {
      store.sync.state = 'running';
      store.sync.progress = p;
      store.sync.total = p.filesTotal;
      store.screen = 'syncing';
    }
  });
  window.setInterval(() => {
    if (!document.hidden && store.sync.state !== 'running' && !store.read.running) void refreshCards();
  }, 3000);
}

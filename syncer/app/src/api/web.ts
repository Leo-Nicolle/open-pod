// The browser backend (Chrome/Edge): folders come from the File System
// Access API, per-file work runs in a pool of workers (web-worker.ts), and
// a Session from openpod-web (Rust, WebAssembly) on this page plans syncs
// and keeps the card's manifest. See plans/sync-tool.md, phase 5.
import type {
  Access,
  Api,
  CardInfo,
  Cards,
  CardPodcasts,
  LibraryView,
  PlanSummary,
  ReadProgress,
  Settings,
  SettingsUpdate,
  SyncDone,
  SyncProgress,
  SyncReport,
} from './types';
import type { Request, Response } from './web-worker';

/** A worker request before the pool numbers it. */
type Job = Request extends infer R ? (R extends Request ? Omit<R, 'id'> : never) : never;

// ---- Folder handles (File System Access API) ------------------------------
/* eslint-disable @typescript-eslint/no-explicit-any */
type Dir = FileSystemDirectoryHandle;
type FileH = FileSystemFileHandle;

const AUDIO = new Set(['mp3', 'flac', 'fla', 'wav', 'aif', 'aiff', 'aifc', 'm4a', 'mp4', 'aac', 'ogg', 'oga', 'opus', 'wma', 'wv']);
const COVER = /^(cover|folder|album|front)\.(jpe?g|png|webp)$/i;
const ext = (name: string) => name.slice(name.lastIndexOf('.') + 1).toLowerCase();

async function* entries(dir: Dir): AsyncGenerator<[string, Dir | FileH]> {
  for await (const e of (dir as any).entries()) yield e as [string, Dir | FileH];
}

/** Every file under `dir` as (path relative to it, handle); hidden entries
 * and `skip(rel)` folders left out. */
async function walk(dir: Dir, skip: (rel: string) => boolean, prefix = ''): Promise<[string, FileH][]> {
  const out: [string, FileH][] = [];
  for await (const [name, h] of entries(dir)) {
    if (name.startsWith('.')) continue;
    const rel = prefix ? `${prefix}/${name}` : name;
    if (skip(rel)) continue;
    if (h.kind === 'directory') out.push(...(await walk(h as Dir, skip, rel)));
    else out.push([rel, h as FileH]);
  }
  return out;
}

async function dirAt(root: Dir, parts: string[], create = false): Promise<Dir> {
  let d = root;
  for (const p of parts) d = await d.getDirectoryHandle(p, { create });
  return d;
}

const split = (rel: string) => {
  const parts = rel.split('/');
  return { dirs: parts.slice(0, -1), name: parts[parts.length - 1] };
};

async function fileAt(root: Dir, rel: string): Promise<File | null> {
  try {
    const { dirs, name } = split(rel);
    return await (await (await dirAt(root, dirs)).getFileHandle(name)).getFile();
  } catch {
    return null;
  }
}

/** Writes through `createWritable()`: Chrome writes a temporary file and
 * swaps it in on close, so an interrupted write never leaves half a file. */
async function writeAt(root: Dir, rel: string, data: Blob | BufferSource | string | ReadableStream<Uint8Array>) {
  const { dirs, name } = split(rel);
  const h = await (await dirAt(root, dirs, true)).getFileHandle(name, { create: true });
  const w = await (h as any).createWritable();
  if (data instanceof ReadableStream) await data.pipeTo(w);
  else {
    await w.write(data);
    await w.close();
  }
}

async function removeAt(root: Dir, rel: string) {
  const { dirs, name } = split(rel);
  try {
    await (await dirAt(root, dirs)).removeEntry(name);
  } catch {
    /* already gone */
  }
  // Remove the folders left empty (or holding only cover images).
  for (let n = dirs.length; n > 0; n--) {
    const parent = await dirAt(root, dirs.slice(0, n - 1));
    const d = await dirAt(root, dirs.slice(0, n));
    const names: string[] = [];
    let onlyArt = true;
    for await (const [nm, h] of entries(d)) {
      names.push(nm);
      if (h.kind !== 'file' || !/\.(jpe?g|png|webp)$/i.test(nm)) onlyArt = false;
    }
    if (!onlyArt) return;
    for (const nm of names) await d.removeEntry(nm);
    await parent.removeEntry(dirs[n - 1]);
  }
}

/** Moves a card file (renamed or moved folder in the podcasts folder):
 * `move()` where the browser has it, else copy then delete. */
async function moveAt(root: Dir, from: string, to: string) {
  const src = split(from);
  const dst = split(to);
  try {
    const h = await (await dirAt(root, src.dirs)).getFileHandle(src.name);
    const dest = await dirAt(root, dst.dirs, true);
    if (typeof (h as any).move === 'function') await (h as any).move(dest, dst.name);
    else {
      await writeAt(root, to, await h.getFile());
      await (await dirAt(root, src.dirs)).removeEntry(src.name);
    }
  } catch {
    return; /* gone already */
  }
  // Clean up the folders the move emptied.
  try {
    await removeAt(root, from);
  } catch {
    /* nothing left to clean */
  }
}

/** Copies the cover images of a source folder next to its files on the card
 * (when missing or a different size). */
async function copyCovers(where: { root: Dir; dirs: string[] } | null, root: Dir, destDir: string) {
  if (!where) return;
  try {
    const src = await dirAt(where.root, where.dirs);
    for await (const [name, h] of entries(src)) {
      if (h.kind !== 'file' || !COVER.test(name)) continue;
      const existing = await fileAt(root, `${destDir}/${name}`);
      const file = await (h as FileH).getFile();
      if (existing?.size !== file.size) await writeAt(root, `${destDir}/${name}`, file);
    }
  } catch {
    /* no cover */
  }
}

// ---- Remembered folders and settings -------------------------------------

function idb<T>(op: (s: IDBObjectStore) => IDBRequest<T>, mode: IDBTransactionMode = 'readonly'): Promise<T> {
  return new Promise((resolve, reject) => {
    const open = indexedDB.open('openpod-sync', 1);
    open.onupgradeneeded = () => open.result.createObjectStore('kv');
    open.onerror = () => reject(open.error);
    open.onsuccess = () => {
      const r = op(open.result.transaction('kv', mode).objectStore('kv'));
      r.onsuccess = () => resolve(r.result);
      r.onerror = () => reject(r.error);
    };
  });
}
const kvGet = <T>(k: string) => idb<T | undefined>((s) => s.get(k) as IDBRequest<T | undefined>);
const kvSet = (k: string, v: unknown) => idb((s) => s.put(v, k), 'readwrite');

interface Prefs {
  machine: string;
  exclude: string[];
  albumKey: 'artist-album' | 'name';
}
const PREFS_KEY = 'openpod-sync-prefs';
function prefs(): Prefs {
  try {
    return { machine: 'browser', exclude: [], albumKey: 'artist-album', ...JSON.parse(localStorage.getItem(PREFS_KEY) ?? '{}') };
  } catch {
    return { machine: 'browser', exclude: [], albumKey: 'artist-album' };
  }
}
const savePrefs = (p: Prefs) => localStorage.setItem(PREFS_KEY, JSON.stringify(p));

/** Music folders by name (the name is their id in library paths). */
let libs: { name: string; handle: Dir }[] = [];
let card: Dir | null = null;
/** The podcasts folder, mirrored to the card's Podcasts/. */
let podcastsDir: Dir | null = null;
const loaded = (async () => {
  libs = (await kvGet<{ name: string; handle: Dir }[]>('libs')) ?? [];
  card = (await kvGet<Dir>('card')) ?? null;
  podcastsDir = (await kvGet<Dir>('podcasts')) ?? null;
})();

async function granted(h: Dir, write: boolean): Promise<boolean> {
  return (await (h as any).queryPermission({ mode: write ? 'readwrite' : 'read' })) === 'granted';
}

// ---- WebAssembly: the page's session and the worker pool -------------------

type Wasm = typeof import('../wasm/openpod_web.js');
let wasmP: Promise<Wasm> | null = null;
const wasm = () =>
  (wasmP ??= import('../wasm/openpod_web.js').then(async (m) => {
    await m.default();
    return m;
  }));

class Pool {
  private workers: { w: Worker; busy: boolean }[] = [];
  private queue: { req: Job; resolve: (v: unknown) => void; reject: (e: Error) => void }[] = [];
  private pending = new Map<number, { resolve: (v: unknown) => void; reject: (e: Error) => void; slot: number }>();
  private next = 1;

  constructor(size: number) {
    for (let i = 0; i < size; i++) {
      const w = new Worker(new URL('./web-worker.ts', import.meta.url), { type: 'module' });
      w.onmessage = (e: MessageEvent<Response>) => {
        const p = this.pending.get(e.data.id)!;
        this.pending.delete(e.data.id);
        this.workers[p.slot].busy = false;
        if (e.data.ok) p.resolve(e.data.value);
        else p.reject(new Error(e.data.error));
        this.pump();
      };
      this.workers.push({ w, busy: false });
    }
  }

  run<T>(req: Job): Promise<T> {
    return new Promise((resolve, reject) => {
      this.queue.push({ req, resolve: resolve as (v: unknown) => void, reject });
      this.pump();
    });
  }

  private pump() {
    for (let slot = 0; slot < this.workers.length; slot++) {
      if (this.workers[slot].busy || !this.queue.length) continue;
      const job = this.queue.shift()!;
      const id = this.next++;
      this.workers[slot].busy = true;
      this.pending.set(id, { resolve: job.resolve, reject: job.reject, slot });
      this.workers[slot].w.postMessage({ ...job.req, id } as Request);
    }
  }
}
let poolI: Pool | null = null;
const pool = () => (poolI ??= new Pool(Math.max(2, Math.min(8, navigator.hardwareConcurrency || 4))));

/** Runs `f` over `items`, all in flight at once (the pool bounds the
 * concurrency), keeping the order; `tick` after each. */
async function mapAll<T, R>(items: T[], f: (t: T) => Promise<R>, tick?: (i: number) => void): Promise<R[]> {
  let done = 0;
  return Promise.all(
    items.map(async (t) => {
      const r = await f(t);
      tick?.(++done);
      return r;
    }),
  );
}

// ---- Card and library state --------------------------------------------------

let session: InstanceType<Wasm['Session']> | null = null;
/** Library files by path (`<folder name>/<rel>`) from the last check. */
let libFiles = new Map<string, File>();
/** The podcast mirror planned by the last check, and its files (`pod/<rel>`). */
let podSession: InstanceType<Wasm['PodcastSession']> | null = null;
let podFiles = new Map<string, File>();

/** `library.json`'s (size, lastModified) when the session was loaded. */
let sessionStamp = '';

/** Loads the card's manifest into the session, unless it is already
 * loaded and unchanged on the card. Never during a sync: the session is
 * the one being written. */
async function loadSession() {
  if (syncing) return session;
  const f = card ? await fileAt(card, 'openpod/library.json') : null;
  const stamp = f ? `${f.size}:${f.lastModified}` : '';
  if (session && stamp === sessionStamp) return session;
  const w = await wasm();
  session?.free();
  session = f ? new w.Session(await f.text()) : null;
  sessionStamp = stamp;
  return session;
}

async function saveManifest() {
  if (!card || !session) return;
  await writeAt(card, 'openpod/library.json', session.manifest());
  const f = await fileAt(card, 'openpod/library.json');
  sessionStamp = f ? `${f.size}:${f.lastModified}` : '';
}

/** Audio files on the card (outside openpod/), card path → File. */
async function cardAudio(): Promise<Map<string, File>> {
  const out = new Map<string, File>();
  if (!card) return out;
  // Podcasts/ is the podcast mirror's, not music.
  const files = await walk(card, (rel) => rel === 'openpod' || rel === 'Podcasts' || rel === 'System Volume Information');
  for (const [rel, h] of files) if (AUDIO.has(ext(rel))) out.set(rel, await h.getFile());
  return out;
}

async function cardInfo(): Promise<CardInfo | null> {
  await loaded;
  if (!card) return null;
  const base = { path: card.name, name: card.name.toUpperCase(), capacity: 0, free: 0 };
  if (!(await granted(card, true))) return { ...base, state: { kind: 'locked', reason: 'The browser needs your permission again' } };
  const s = await loadSession();
  if (s) {
    const st = JSON.parse(s.stats());
    const m = JSON.parse(s.manifest());
    return { ...base, state: { kind: 'ready', tracks: st.tracks, albums: st.albums, lastSync: m.last_sync ?? null } };
  }
  const audio = await cardAudio();
  if (!audio.size) return { ...base, state: { kind: 'empty' } };
  let bytes = 0;
  for (const f of audio.values()) bytes += f.size;
  return { ...base, state: { kind: 'unread', files: audio.size, bytes } };
}

/** Library files, `<folder name>/<rel>` → File, minus excluded folders. */
async function scanLibraries(): Promise<Map<string, File>> {
  const exclude = prefs().exclude.map((e) => e.replace(/^\/+|\/+$/g, ''));
  const out = new Map<string, File>();
  for (const l of libs) {
    const skip = (rel: string) => exclude.some((e) => rel === e || rel.startsWith(`${e}/`));
    for (const [rel, h] of await walk(l.handle, skip)) if (AUDIO.has(ext(rel))) out.set(`${l.name}/${rel}`, await h.getFile());
  }
  return out;
}

/** A virtual path's File: `card/<card path>` or `<folder>/<rel>`. */
async function fileOf(path: string): Promise<File | null> {
  if (path.startsWith('card/')) return card ? fileAt(card, path.slice(5)) : null;
  if (path.startsWith('pod/')) return podFiles.get(path) ?? (podcastsDir ? fileAt(podcastsDir, path.slice(4)) : null);
  const known = libFiles.get(path);
  if (known) return known;
  const slash = path.indexOf('/');
  const lib = libs.find((l) => l.name === path.slice(0, slash));
  return lib ? fileAt(lib.handle, path.slice(slash + 1)) : null;
}

/** The folder of a virtual path, and the root it's under. */
function folderOf(path: string): { root: Dir; dirs: string[] } | null {
  const parts = path.split('/');
  if (parts[0] === 'card') return card ? { root: card, dirs: parts.slice(1, -1) } : null;
  if (parts[0] === 'pod') return podcastsDir ? { root: podcastsDir, dirs: parts.slice(1, -1) } : null;
  const lib = libs.find((l) => l.name === parts[0]);
  return lib ? { root: lib.handle, dirs: parts.slice(1, -1) } : null;
}

/** Embedded art, else the folder's cover image. */
async function artOf(path: string): Promise<Uint8Array | null> {
  const f = await fileOf(path);
  if (!f) return null;
  const embedded = await pool().run<Uint8Array | null>({ op: 'art', file: f, name: f.name }).catch(() => null);
  if (embedded) return embedded;
  const where = folderOf(path);
  if (!where) return null;
  try {
    const d = await dirAt(where.root, where.dirs);
    for await (const [name, h] of entries(d)) {
      if (h.kind === 'file' && COVER.test(name)) return new Uint8Array(await (await (h as FileH).getFile()).arrayBuffer());
    }
  } catch {
    /* no folder */
  }
  return null;
}

// ---- Events ------------------------------------------------------------------

const on = {
  read: [] as ((p: ReadProgress) => void)[],
  readDone: [] as ((e: string | null) => void)[],
  sync: [] as ((p: SyncProgress) => void)[],
  syncDone: [] as ((d: SyncDone) => void)[],
};

/** Speed and time left over the last 10 s. */
class Rate {
  private samples: [number, number][] = [];
  readonly start = performance.now();
  update(done: number, total: number): { speed: number | null; eta: number | null } {
    const now = performance.now();
    this.samples.push([now, done]);
    while (this.samples.length > 2 && now - this.samples[1][0] > 10_000) this.samples.shift();
    const [t0, d0] = this.samples[0];
    const secs = (now - t0) / 1000;
    if (secs < 2 || done <= d0) return { speed: null, eta: null };
    const speed = (done - d0) / secs;
    return { speed, eta: (total - done) / speed };
  }
  get elapsed() {
    return (performance.now() - this.start) / 1000;
  }
}

/** "Artist / Album / 01 Title.flac". */
const short = (p: string) => p.split('/').slice(-3).join(' / ');

// ---- Index ---------------------------------------------------------------------

/** Rebuilds openpod/*.bin from the session's manifest. Returns warnings. */
async function reindex() {
  const s = session!;
  const key = prefs().albumKey;
  const tracks: string[] = JSON.parse(s.art_tracks(key));
  await mapAll(tracks, async (rel) => {
    const art = await artOf(`card/${rel}`);
    if (art) s.add_art(rel, art);
  });
  const names: string[] = JSON.parse(s.build_index(key));
  for (const n of names) await writeAt(card!, `openpod/${n}`, s.index_file(n)! as BufferSource);
}

/** Plans the podcast mirror (the podcast part of "What's new"). */
async function checkPodcasts(): Promise<PlanSummary['podcasts']> {
  podSession?.free();
  podSession = null;
  if (!podcastsDir || !card) return undefined;
  const w = await wasm();
  const manifest = await fileAt(card, 'openpod/podcasts.json');
  const ps = (podSession = new w.PodcastSession(manifest ? await manifest.text() : undefined));
  podFiles = new Map();
  for (const [rel, h] of await walk(podcastsDir, () => false)) if (AUDIO.has(ext(rel))) podFiles.set(`pod/${rel}`, await h.getFile());
  const files = [...podFiles].map(([path, f]) => ({ path, rel: path.slice(4), size: f.size, modified: f.lastModified }));
  const toHash: string[] = JSON.parse(ps.triage(JSON.stringify(files), prefs().machine));
  const hashes = await mapAll(toHash, (p) => pool().run<string>({ op: 'hash', file: podFiles.get(p)! }).catch(() => null));
  const toProbe: { path: string; rel: string }[] = JSON.parse(ps.match_hashes(JSON.stringify(hashes)));
  const probes = await mapAll(toProbe, async (p) => {
    const f = podFiles.get(p.path)!;
    return JSON.parse(await pool().run<string>({ op: 'probeEpisode', file: f, name: f.name, rel: p.rel }));
  });
  return JSON.parse(ps.finish(JSON.stringify(probes)));
}

/** Rebuilds the podcast tables and remaps the listening state the player
 * wrote, then saves podcasts.json (its generation changed). */
async function reindexPodcasts(root: Dir, ps: InstanceType<Wasm['PodcastSession']>) {
  const sources: [string, string | null][] = JSON.parse(ps.art_sources());
  await mapAll(sources, async ([dir, first]) => {
    let bytes: Uint8Array | null = null;
    try {
      for await (const [name, h] of entries(await dirAt(root, dir.split('/')))) {
        if (h.kind === 'file' && COVER.test(name)) {
          bytes = new Uint8Array(await (await (h as FileH).getFile()).arrayBuffer());
          break;
        }
      }
    } catch {
      /* no folder */
    }
    if (!bytes && first) {
      const f = await fileAt(root, first);
      if (f) bytes = await pool().run<Uint8Array | null>({ op: 'art', file: f, name: f.name }).catch(() => null);
    }
    if (bytes) ps.add_art(dir, bytes);
  });
  const old = await fileAt(root, 'openpod/podcast_state.bin');
  const names: string[] = JSON.parse(ps.build_index(old ? new Uint8Array(await old.arrayBuffer()) : undefined));
  // In order: the state, then the meta file last (see podcasts::build_files).
  for (const n of names) await writeAt(root, `openpod/${n}`, ps.index_file(n)! as BufferSource);
  await writeAt(root, 'openpod/podcasts.json', ps.manifest());
}

// ---- The Api -------------------------------------------------------------------

let syncing: SyncProgress | null = null;
let stop = false;

export const webApi: Api = {
  desktop: false,

  async settings(): Promise<Settings> {
    await loaded;
    const p = prefs();
    return {
      machine: p.machine,
      hostname: 'browser',
      libraries: libs.map((l) => ({ path: l.name, exists: true, tracks: 0, albums: 0, formats: [] })),
      podcasts: podcastsDir ? { path: podcastsDir.name, exists: true, episodes: 0, shows: 0 } : null,
      exclude: p.exclude,
      albumKey: p.albumKey,
      ffmpeg: null,
      configPath: '',
    };
  },

  async saveSettings(u: SettingsUpdate) {
    const p = prefs();
    if (u.machine !== undefined) p.machine = u.machine.trim() || 'browser';
    if (u.exclude) p.exclude = u.exclude;
    if (u.albumKey) p.albumKey = u.albumKey;
    savePrefs(p);
    if (u.libraries) {
      libs = libs.filter((l) => u.libraries!.includes(l.name));
      await kvSet('libs', libs);
    }
    if (u.podcasts === '') {
      podcastsDir = null;
      await kvSet('podcasts', null);
    }
    return webApi.settings();
  },

  async cards(): Promise<Cards> {
    const selected = await cardInfo();
    return { selected, others: [], busy: syncing ? 'syncing' : null };
  },

  async selectCard() {
    /* the picker already chose it */
  },

  async readCard() {
    const rate = new Rate();
    void (async () => {
      try {
        const audio = [...(await cardAudio())];
        const w = await wasm();
        const cost = audio.map(([rel, f]) => w.probe_cost(rel, f.size, false));
        const total = cost.reduce((a, b) => a + b, 0);
        let done = 0;
        let files = 0;
        const probed = await mapAll(audio, async ([rel, f], ) => {
          const p = JSON.parse(await pool().run<string>({ op: 'probe', file: f, name: f.name, rel, lpc: false }));
          done += w.probe_cost(rel, f.size, false);
          files++;
          const r = rate.update(done, total);
          on.read.forEach((cb) => cb({ filesDone: files, files: audio.length, fraction: total ? done / total : 1, elapsed: rate.elapsed, eta: r.eta }));
          return { rel, size: f.size, probed: p };
        });
        const s = new w.Session(undefined);
        s.adopt(JSON.stringify(probed));
        session?.free();
        session = s;
        await saveManifest();
        on.readDone.forEach((cb) => cb(null));
      } catch (e) {
        on.readDone.forEach((cb) => cb(String(e instanceof Error ? e.message : e)));
      }
    })();
  },

  async eject() {
    throw new Error('The browser can’t eject the card: use your computer’s Eject or Safely Remove.');
  },

  async check(): Promise<PlanSummary> {
    if (!card) throw new Error('Choose the card first');
    const w = await wasm();
    if (!session) {
      if ((await cardAudio()).size) throw new Error('The card has to be read first');
      session = new w.Session(undefined);
    }
    const s = session;
    libFiles = await scanLibraries();
    const files = [...libFiles].map(([path, f]) => ({ path, rel: path.slice(path.indexOf('/') + 1), size: f.size, modified: f.lastModified }));
    const toHash: string[] = JSON.parse(s.triage(JSON.stringify(files), JSON.stringify({ machine: prefs().machine, includeRemoved: false })));
    const hashes = await mapAll(toHash, (p) => pool().run<string>({ op: 'hash', file: libFiles.get(p)! }).catch(() => null));
    const toProbe: { path: string; rel: string }[] = JSON.parse(s.match_hashes(JSON.stringify(hashes)));
    const probed = await mapAll(toProbe, async (p) => {
      const f = libFiles.get(p.path)!;
      return JSON.parse(await pool().run<string>({ op: 'probe', file: f, name: f.name, rel: p.rel, lpc: true }));
    });
    const cardFiles: Record<string, [number, number]> = {};
    for (const [rel, f] of await cardAudio()) cardFiles[rel] = [f.size, f.lastModified];
    // The browser can't see free space on the card: 0 means unknown.
    const summary: PlanSummary = JSON.parse(s.finish(JSON.stringify(probed), JSON.stringify(cardFiles), 0));
    const pod = await checkPodcasts();
    if (pod) {
      summary.podcasts = pod;
      summary.bytes += pod.bytes;
      summary.estSeconds = Math.round(summary.bytes / Math.max(1, summary.writeSpeed));
    }
    return summary;
  },

  async syncStart() {
    // Re-plan right before writing, like the desktop app.
    const planned = await webApi.check();
    const s = session!;
    const root = card!;
    const writes: { source: string; dest: string; copy: boolean; policy: unknown; estimatedSize: number }[] = JSON.parse(s.start());
    await saveManifest();
    stop = false;
    // One bar and one time left for the whole sync: music, then podcasts.
    const total = writes.reduce((n, w) => n + w.estimatedSize, 0) + (planned.podcasts?.bytes ?? 0);
    const p: SyncProgress = (syncing = {
      bytesDone: 0,
      bytesTotal: total,
      filesDone: 0,
      filesTotal: writes.length + (planned.podcasts?.new ?? 0),
      speed: null,
      elapsed: 0,
      eta: null,
      copying: null,
      converting: [],
      recent: [],
    });
    const rate = new Rate();
    let advanced = 0;
    let lastEmit = 0;
    const emit = (force = false) => {
      const r = rate.update(p.bytesDone, total);
      p.speed = r.speed;
      p.eta = r.eta;
      p.elapsed = rate.elapsed;
      const now = performance.now();
      if (force || now - lastEmit > 200) {
        lastEmit = now;
        on.sync.forEach((cb) => cb({ ...p, converting: [...p.converting], recent: [...p.recent] }));
      }
    };
    emit(true);

    /** The podcast mirror: deletions and moves on the card, then the new
     * episodes (one at a time, converted in the pool first), then the
     * podcast tables. Continues the music's bar. */
    const syncPodcasts = async (root: Dir, ps: InstanceType<Wasm['PodcastSession']>) => {
      const ops: {
        deletes: string[];
        renames: [string, string][];
        copies: { source: string; dest: string; copy: boolean; policy: unknown; estimatedSize: number }[];
      } = JSON.parse(ps.start());
      for (const d of ops.deletes) await removeAt(root, d);
      for (const [from, to] of ops.renames) await moveAt(root, from, to);
      await writeAt(root, 'openpod/podcasts.json', ps.manifest());
      emit(true);
      for (let i = 0; i < ops.copies.length && !stop; i++) {
        const c = ops.copies[i];
        try {
          const f = await fileOf(c.source);
          if (!f) throw new Error('the file is gone');
          const data = c.copy
            ? f
            : new Blob([(await pool().run<Uint8Array>({ op: 'convert', file: f, name: f.name, policy: JSON.stringify(c.policy) })) as BlobPart]);
          await writeAt(root, c.dest, data);
          ps.copied(i, data.size);
          const dir = c.dest.split('/').slice(0, -1).join('/');
          await copyCovers(folderOf(c.source), root, dir);
          const r = p.recent.find((x) => x.dir === dir);
          if (r) r.tracks++;
          else {
            const parts = dir.split('/');
            p.recent = [{ dir, title: parts[parts.length - 1], artist: parts.slice(1, -1).join(' / '), tracks: 1, cover: `card/${c.dest}` }, ...p.recent].slice(0, 6);
          }
        } catch (e) {
          ps.failed(i, String(e instanceof Error ? e.message : e));
        }
        advanced += c.estimatedSize;
        p.bytesDone = Math.min(total, advanced);
        p.filesDone++;
        emit();
        if (i % 25 === 24) await writeAt(root, 'openpod/podcasts.json', ps.manifest());
      }
      if (ps.changed() || !(await fileAt(root, 'openpod/podcast_meta.bin'))) await reindexPodcasts(root, ps);
      else await writeAt(root, 'openpod/podcasts.json', ps.manifest());
      return JSON.parse(ps.report(stop)) as { report: NonNullable<SyncReport['podcasts']>; text: string };
    };

    void (async () => {
      let writeSecs = 0;
      let error: string | null = null;
      try {
        // Conversions run ahead in the pool; writes go one at a time.
        const ahead = 4;
        const prepared: Promise<Blob | File | Error>[] = [];
        const prepare = (i: number) => {
          const w = writes[i];
          prepared[i] = (async () => {
            const f = await fileOf(w.source);
            if (!f) return new Error('the file is gone');
            if (w.copy) return f;
            const slot = { worker: i, path: short(w.source), what: 'converting' };
            p.converting.push(slot);
            try {
              const bytes = await pool().run<Uint8Array>({ op: 'convert', file: f, name: f.name, policy: JSON.stringify(w.policy) });
              return new Blob([bytes as BlobPart]);
            } catch (e) {
              return e instanceof Error ? e : new Error(String(e));
            } finally {
              p.converting = p.converting.filter((c) => c !== slot);
            }
          })();
        };
        for (let i = 0; i < Math.min(ahead, writes.length); i++) prepare(i);
        let sinceSave = 0;
        for (let i = 0; i < writes.length && !stop; i++) {
          if (i + ahead < writes.length) prepare(i + ahead);
          const w = writes[i];
          const data = await prepared[i];
          if (stop) break;
          if (data instanceof Error) {
            s.failed(i, data.message);
          } else {
            const t0 = performance.now();
            let done = 0;
            const counter = new TransformStream<Uint8Array, Uint8Array>({
              transform(chunk, c) {
                done += chunk.byteLength;
                p.copying = { path: short(w.dest), done, total: data.size };
                p.bytesDone = Math.min(total, advanced + Math.min(done, w.estimatedSize));
                emit();
                c.enqueue(chunk);
              },
            });
            await writeAt(root, w.dest, data.stream().pipeThrough(counter));
            writeSecs += (performance.now() - t0) / 1000;
            const old = s.written(i, data.size);
            if (old) await removeAt(root, old);
            // The album's cover image goes along.
            if (!w.source.startsWith('card/')) await copyCovers(folderOf(w.source), root, w.dest.split('/').slice(0, -1).join('/'));
            const dir = w.dest.split('/').slice(0, -1).join('/');
            const r = p.recent.find((x) => x.dir === dir);
            if (r) {
              r.tracks++;
              p.recent = [r, ...p.recent.filter((x) => x !== r)];
            } else {
              const [artist, title] = dir.split('/').slice(-2);
              p.recent = [{ dir, title: title ?? dir, artist: artist ?? '', tracks: 1, cover: `card/${w.dest}` }, ...p.recent].slice(0, 6);
            }
          }
          advanced += w.estimatedSize;
          p.bytesDone = Math.min(total, advanced);
          p.filesDone++;
          p.copying = null;
          emit();
          if (++sinceSave >= 25) {
            await saveManifest();
            sinceSave = 0;
          }
        }
      } catch (e) {
        error = String(e instanceof Error ? e.message : e);
      }
      let report: SyncReport | null = null;
      try {
        await saveManifest();
        await reindex();
        const done = JSON.parse(s.finish_sync(rate.elapsed, writeSecs, stop));
        report = done.report;
        let text: string = done.text;
        // Then the podcast mirror.
        const ps = podSession;
        if (ps && !stop && !error) {
          const pod = await syncPodcasts(root, ps);
          report!.podcasts = pod.report;
          text += pod.text;
        }
        await writeAt(root, 'openpod/last-sync.json', JSON.stringify(report, null, 2));
        await writeAt(root, 'openpod/sync-report.txt', text);
        await saveManifest();
      } catch (e) {
        error ??= String(e instanceof Error ? e.message : e);
      }
      emit(true);
      syncing = null;
      on.syncDone.forEach((cb) => cb(error ? { report: null, error } : { report, error: null }));
    })();
  },

  async syncStop() {
    stop = true;
  },

  async syncStatus() {
    return syncing;
  },

  async report() {
    if (!card) return null;
    const f = await fileAt(card, 'openpod/last-sync.json');
    return f ? (JSON.parse(await f.text()) as SyncReport) : null;
  },

  async library(): Promise<LibraryView> {
    const s = session ?? (await loadSession());
    if (!s) return { albums: [], removed: [], tracks: 0, bytes: 0, capacity: 0 };
    const st = JSON.parse(s.stats());
    const albums = JSON.parse(s.albums()).map((a: { cover: string }) => ({ ...a, cover: `card/${a.cover}` }));
    return { albums, removed: JSON.parse(s.removed()), tracks: st.tracks, bytes: st.bytes, capacity: 0 };
  },

  async removeAlbums(ids) {
    if (syncing) throw new Error('A sync is running');
    const s = session!;
    const paths: string[] = JSON.parse(s.forget(JSON.stringify(ids), prefs().machine));
    for (const p of paths) await removeAt(card!, p);
    await saveManifest();
    await reindex();
    return webApi.library();
  },

  async bringBack(id) {
    if (syncing) throw new Error('A sync is running');
    session!.unforget(id);
    await saveManifest();
    return webApi.library();
  },

  async podcasts(): Promise<CardPodcasts> {
    const w = await wasm();
    const m = card ? await fileAt(card, 'openpod/podcasts.json') : null;
    const st = card ? await fileAt(card, 'openpod/podcast_state.bin') : null;
    const v: CardPodcasts = JSON.parse(
      w.card_podcasts(m ? await m.text() : undefined, st ? new Uint8Array(await st.arrayBuffer()) : undefined, prefs().machine),
    );
    // Sources are virtual `pod/<rel>` paths: show them under the folder's name.
    const name = podcastsDir?.name ?? 'Podcasts';
    for (const s of v.shows) {
      s.cover = `card/${s.cover}`;
      if (s.source.startsWith('pod')) s.source = name + s.source.slice(3);
    }
    return v;
  },

  async setPodcastKeep(show, keep) {
    const w = await wasm();
    const m = await fileAt(card!, 'openpod/podcasts.json');
    await writeAt(card!, 'openpod/podcasts.json', w.podcast_set_keep(m ? await m.text() : undefined, show, keep ?? undefined));
    return webApi.podcasts();
  },

  async setPodcastPlayed(path, played) {
    const w = await wasm();
    const m = await fileAt(card!, 'openpod/podcasts.json');
    const st = await fileAt(card!, 'openpod/podcast_state.bin');
    if (!m || !st) throw new Error('No podcasts on this card yet.');
    const out = w.podcast_set_played(await m.text(), new Uint8Array(await st.arrayBuffer()), path, played);
    await writeAt(card!, 'openpod/podcast_state.bin', out as BufferSource);
    return webApi.podcasts();
  },

  async installOnCard() {
    throw new Error('Only the desktop app can do that.');
  },

  async pickFolder(_title, _start, write, purpose) {
    let h: Dir;
    try {
      const id = write ? 'openpod-card' : purpose === 'podcasts' ? 'openpod-podcasts' : 'openpod-music';
      h = await (window as any).showDirectoryPicker({ mode: write ? 'readwrite' : 'read', id });
    } catch {
      return null; // cancelled
    }
    if (purpose === 'podcasts') {
      podcastsDir = h;
      await kvSet('podcasts', h);
      return h.name;
    }
    if (write) {
      card = h;
      await kvSet('card', h);
      session?.free();
      session = null;
      return h.name;
    }
    let name = h.name;
    for (let n = 2; libs.some((l) => l.name === name); n++) name = `${h.name} (${n})`;
    // saveSettings() keeps only the folders it's given: add it there.
    libs.push({ name, handle: h });
    await kvSet('libs', libs);
    return name;
  },

  coverUrl(path) {
    return coverCache.get(path) ?? coverCache.set(path, artOf(path).then((b) => (b ? URL.createObjectURL(new Blob([b as BlobPart])) : null))).get(path)!;
  },

  async access(): Promise<Access> {
    await loaded;
    return {
      music: await Promise.all(
        [...libs, ...(podcastsDir ? [{ name: podcastsDir.name, handle: podcastsDir }] : [])].map(async (l) => ({
          name: l.name,
          granted: await granted(l.handle, false),
          tracks: null,
        })),
      ),
      card: card ? { name: card.name, granted: await granted(card, true) } : null,
    };
  },

  async grantAccess() {
    await loaded;
    for (const l of libs) if (!(await granted(l.handle, false))) await (l.handle as any).requestPermission({ mode: 'read' });
    if (podcastsDir && !(await granted(podcastsDir, false))) await (podcastsDir as any).requestPermission({ mode: 'read' });
    if (card && !(await granted(card, true))) await (card as any).requestPermission({ mode: 'readwrite' });
    return webApi.access();
  },

  onReadProgress: (cb) => void on.read.push(cb),
  onReadDone: (cb) => void on.readDone.push(cb),
  onSyncProgress: (cb) => void on.sync.push(cb),
  onSyncDone: (cb) => void on.syncDone.push(cb),
};

const coverCache = new Map<string, Promise<string | null>>();

/** Chrome and Edge only: Firefox and Safari can't write to a chosen folder. */
export const webSupported = 'showDirectoryPicker' in window;

/** Self-test hook (`?selftest`): use these folders instead of picked ones. */
export function useFolders(music: { name: string; handle: Dir }[], cardDir: Dir, podcasts: Dir | null = null) {
  libs = music;
  card = cardDir;
  podcastsDir = podcasts;
  session?.free();
  session = null;
  sessionStamp = '';
}

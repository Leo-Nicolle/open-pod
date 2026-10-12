// Sample backend for a plain browser (`npm run dev`): the design's sample
// data, a simulated sync, and states picked with the URL:
//   ?card=nocard|locked|empty|unread   ?plan=nospace|uptodate
import type {
  Api,
  CardAlbum,
  CardInfo,
  CardPodcasts,
  CardShow,
  LibraryView,
  PlanAlbum,
  PlanSummary,
  ReadProgress,
  Settings,
  SyncDone,
  SyncProgress,
  SyncReport,
} from './types';

const q = new URLSearchParams(location.search);
const GB = 1e9;
const MB = 1e6;
const wait = (ms = 150) => new Promise((r) => setTimeout(r, ms));

const NEW: [string, string, number, string][] = [
  ['Djangologie 6 (1937)', 'Django Reinhardt', 20, 'n'],
  ["Que c'est triste Venise", 'Charles Aznavour', 12, 'u'],
  ["J'ai de bonnes nouvelles (1977–1978)", 'Anne Sylvestre', 24, 'n'],
  ['IV', 'Cypress Hill', 21, 'nc'],
  ['Complete Decca Solo Recordings', 'Radu Lupu', 114, 'nc'],
  ["Let's Get Free", 'dead prez', 18, 'n'],
  ['Nuées ardentes', 'Birds on a Wire', 7, 'nt'],
  ['Les 50 plus belles chansons', 'Bernard Lavilliers', 50, 'n'],
  ['Face B', 'Unknown artist', 9, 'nt'],
  ['Kind of Blue', 'Miles Davis', 5, 'u'],
];
const LIB: [string, string, number, string][] = [
  ['Pastel Blues', 'Nina Simone', 9, ''],
  ['Nuées ardentes', 'Birds on a Wire', 7, 't'],
  ['Barbara chante Barbara', 'Barbara', 12, ''],
  ['La mauvaise réputation', 'Georges Brassens', 10, ''],
  ["Que c'est triste Venise", 'Charles Aznavour', 12, ''],
  ['IV', 'Cypress Hill', 21, 'c'],
  ["Let's Get Free", 'dead prez', 18, ''],
  ['Djangologie 6 (1937)', 'Django Reinhardt', 20, ''],
  ['Face B', 'Unknown artist', 9, 't'],
  ['Les 50 plus belles chansons', 'Bernard Lavilliers', 50, ''],
  ['Kind of Blue', 'Miles Davis', 5, ''],
  ['Complete Decca Solo Recordings', 'Radu Lupu', 114, 'c'],
  ["J'ai de bonnes nouvelles (1977–1978)", 'Anne Sylvestre', 24, ''],
  ['cd-2004', 'Unknown artist', 14, 't'],
];
const TRACKS = ['Minor Swing', "Viper's Dream", 'Swinging with Django', 'Paramount Stomp', 'Chicago', 'Bouncin’ Around', 'Exactly Like You', 'Charleston', "You're Driving Me Crazy", 'Tears', 'Solitude', 'Hot Lips'];

let settings: Settings = {
  machine: 'leo-pc',
  hostname: 'leo-pc',
  libraries: [
    { path: 'D:\\Musique', exists: true, tracks: 1412, albums: 71, formats: ['FLAC', 'MP3'] },
    { path: 'E:\\Rips\\Vinyl', exists: true, tracks: 188, albums: 9, formats: ['FLAC', 'WAV'] },
  ],
  podcasts: { path: 'D:\\Podcasts', exists: true, episodes: 134, shows: 6 },
  exclude: [],
  albumKey: 'artist-album',
  ffmpeg: '6.1',
  configPath: '~/.config/openpod/config.toml',
};

let cardKind = q.get('card') ?? 'ready';
let synced = false;
const card = (): CardInfo => {
  const base = { path: 'E:\\', name: 'OPENPOD', capacity: 119 * GB, free: synced ? 95 * GB : 111 * GB };
  switch (cardKind) {
    case 'nocard':
      return { ...base, state: { kind: 'missing' } };
    case 'locked':
      return { ...base, state: { kind: 'locked', reason: 'Read-only file system' } };
    case 'empty':
      return { ...base, name: 'NO NAME', free: 119 * GB, state: { kind: 'empty' } };
    case 'unread':
      return { ...base, free: 104 * GB, state: { kind: 'unread', files: 4212, bytes: 15 * GB } };
    default:
      return {
        ...base,
        free: q.get('plan') === 'nospace' ? 9.2 * GB : base.free,
        state: {
          kind: 'ready',
          tracks: synced ? 1583 : 823,
          albums: synced ? 83 : 36,
          lastSync: { machine: 'laptop', at: Date.parse('2026-10-03T18:00:00') / 1000, write_speed: 18.4 * MB },
        },
      };
  }
};

const planAlbums: PlanAlbum[] = NEW.map(([title, artist, tracks, f], i) => ({
  id: `p${i}`,
  title,
  artist,
  tracks,
  bytes: tracks * 25 * MB,
  new: f.includes('n'),
  upgrade: f.includes('u'),
  converted: f.includes('c'),
  tagsGuessed: f.includes('t'),
  cover: `tone:${i}`,
}));
// 47 albums in the design: repeat the ten samples' flags for the rest.
for (let i = 10; i < 47; i++) {
  const s = planAlbums[i % 8];
  planAlbums.push({ ...s, id: `p${i}`, title: `${s.title} ${i}`, upgrade: false, cover: `tone:${i}` });
}

const plan = (): PlanSummary => {
  const upToDate = q.get('plan') === 'uptodate' || synced;
  return {
    tracks: upToDate ? 0 : 761,
    albums: upToDate ? 0 : 47,
    upgrades: upToDate ? 0 : 3,
    fixes: 0,
    conversions: upToDate ? 0 : 140,
    bytes: upToDate ? 0 : 15.6 * GB,
    free: card().free,
    writeSpeed: 18.4 * MB,
    speedMeasured: true,
    estSeconds: upToDate ? 0 : 13 * 60,
    alreadyOnCard: upToDate ? 1583 : 823,
    unchanged: upToDate ? 1583 : 0,
    removed: 0,
    albumList: upToDate ? [] : planAlbums,
    podcasts: settings.podcasts
      ? {
          new: upToDate ? 0 : 7,
          moved: upToDate ? 0 : 2,
          deleted: upToDate ? 0 : 4,
          leaving: upToDate ? 0 : 4,
          bytes: upToDate ? 0 : 412 * MB,
          shows: upToDate
            ? []
            : [
                { key: 'France Culture/Avec philosophie', folder: 'France Culture / Avec philosophie', episodes: 1, leaving: 1, bytes: 54 * MB, cover: 'tone:3' },
                { key: 'France Culture/La Série fiction', folder: 'France Culture / La Série fiction', episodes: 2, leaving: 2, bytes: 96 * MB, cover: 'tone:4' },
                { key: 'Radio Nova/Nuits de Nova', folder: 'Radio Nova / Nuits de Nova', episodes: 3, leaving: 1, bytes: 210 * MB, cover: 'tone:6' },
                { key: 'Studio Ardoise/Le Temps long', folder: 'Studio Ardoise / Le Temps long', episodes: 1, leaving: 0, bytes: 52 * MB, cover: 'tone:7' },
              ],
          episodes: upToDate
            ? []
            : [
                { show: 'France Culture/Avec philosophie', title: "Phèdre de Platon : bien parler d'amour", date: OCT(5), duration: 3492, bytes: 54 * MB, held: false },
                { show: 'France Culture/Avec philosophie', title: "Ménon : la vertu peut-elle s'apprendre ?", date: OCT(-1), duration: 3501, bytes: 54 * MB, held: true },
                { show: 'France Culture/Avec philosophie', title: 'Apologie de Socrate : parler devant ses juges', date: OCT(-4), duration: 3468, bytes: 54 * MB, held: true },
                { show: 'France Culture/Avec philosophie', title: 'Criton : faut-il obéir aux lois injustes ?', date: OCT(-5), duration: 3510, bytes: 54 * MB, held: true },
              ],
          skipped: [],
        }
      : undefined,
    skipped: [],
    unreadable: upToDate
      ? []
      : [
          { path: 'E:\\Rips\\Vinyl\\scratch.aiff', reason: "Can't be read: the file is damaged" },
          { path: 'D:\\Musique\\Cypress Hill\\IV\\14 Riot Starter.mp3', reason: "Can't be read: the file ends early" },
        ],
  };
};

/** Days since 1970 of a day in October 2026 (0: 30 September). */
const OCT = (d: number) => 20727 + d - 1;

const ep = (title: string, date: number, duration: number, state: '' | 'played' | number = '') => ({
  title,
  date,
  duration,
  bytes: Math.round((duration / 60) * 0.93 * MB),
  path: `Podcasts/${title}.mp3`,
  position: typeof state === 'number' ? state : 0,
  played: state === 'played',
});
const show = (key: string, keep: number | null, source: string, cover: number, episodes: CardShow['episodes']): CardShow => {
  const parts = key.split('/');
  return {
    key,
    title: parts[parts.length - 1],
    publisher: parts.slice(0, -1).join(' / '),
    keep,
    source,
    machines: ['leo-pc'],
    bytes: episodes.reduce((n, e) => n + e.bytes, 0),
    cover: `tone:${cover}`,
    episodes,
  };
};
let podShows: CardShow[] = [
  show('France Culture/Avec philosophie', 3, 'D:\\Podcasts\\France Culture\\Avec philosophie', 3, [
    ep("Le Banquet : qui a le droit de parler d'amour ?", OCT(2), 3520, 2260),
    ep('Gorgias, la rhétorique contre la philosophie', OCT(1), 3475, 'played'),
    ep('Ion : le poète est-il inspiré ?', OCT(0), 3483, 'played'),
  ]),
  show('France Culture/La Série fiction', 5, 'D:\\Podcasts\\France Culture\\La Série fiction', 4, [
    ep('Debout les morts de Fred Vargas 10/10', OCT(9), 1680),
    ep('Debout les morts de Fred Vargas 9/10', OCT(8), 1702),
    ep('Debout les morts de Fred Vargas 8/10', OCT(7), 1655, 'played'),
    ep('Debout les morts de Fred Vargas 7/10', OCT(6), 1690, 'played'),
    ep('Debout les morts de Fred Vargas 6/10', OCT(5), 1671),
  ]),
  show(
    'Chemins de la philo 2017',
    null,
    'D:\\Musique\\Podcasts\\CheminsdelaPhilo2017',
    5,
    Array.from({ length: 40 }, (_, i) =>
      ep(`Les chemins de la philosophie ${40 - i}`, 17_450 - i * 7, 3300, i >= 28 ? 'played' : ''),
    ),
  ),
  show('Radio Nova/Nuits de Nova', 3, 'D:\\Podcasts\\Radio Nova\\Nuits de Nova', 6, [
    ep('Nuit du 10 octobre', OCT(10), 7200),
    ep('Nuit du 3 octobre', OCT(3), 7140),
    ep('Nuit du 26 septembre', OCT(-4), 7230),
  ]),
  show('Studio Ardoise/Le Temps long', 3, 'D:\\Podcasts\\Studio Ardoise\\Le Temps long', 7, [
    ep('Les forêts', OCT(8), 2900, 'played'),
    ep('Le sel', OCT(1), 2840, 'played'),
    ep('Les cartes', OCT(-6), 2950, 'played'),
  ]),
];
const podView = (): CardPodcasts => {
  const shows = q.get('pod') === 'empty' || !settings.podcasts ? [] : podShows;
  const all = shows.flatMap((s) => s.episodes);
  return {
    shows,
    episodes: all.length,
    bytes: all.reduce((n, e) => n + e.bytes, 0),
    played: all.filter((e) => e.played).length,
    started: all.filter((e) => !e.played && e.position > 0).length,
    stateRead: true,
  };
};

const albums = (): CardAlbum[] =>
  LIB.map(([title, artist, n, f], i) => ({
    id: `a${i}`,
    title,
    artist,
    bytes: n * 19.4 * MB,
    duration: n * 174,
    format: i === 7 ? 'FLAC 44.1/16' : 'FLAC 44.1/16',
    machines: i % 3 === 0 ? ['leo-pc', 'laptop'] : i % 3 === 1 ? ['leo-pc'] : [],
    conversions: f.includes('c') ? ['96 kHz/24 → 44.1 kHz/16'] : [],
    tagsGuessed: f.includes('t'),
    addedAt: Date.parse('2026-10-03T12:00:00') / 1000 - i * 86400,
    cover: `tone:${i}`,
    tracks: Array.from({ length: n }, (_, k) => ({
      disc: 1,
      no: k + 1,
      title: TRACKS[k % TRACKS.length],
      duration: 170 + ((k * 37) % 40),
      format: k === 10 ? 'MP3 320k' : 'FLAC 44.1/16',
      path: '',
    })),
  }));

let lib: LibraryView = {
  albums: albums(),
  removed: [
    { id: 'r0', title: 'Best of Radio Nostalgie', artist: 'Various artists', tracks: 32, removedAt: Date.parse('2026-10-02') / 1000, machine: 'laptop' },
    { id: 'r1', title: 'Mm..Food (instrumentals)', artist: 'MF DOOM', tracks: 15, removedAt: Date.parse('2026-09-28') / 1000, machine: 'leo-pc' },
  ],
  tracks: 1583,
  bytes: 23.7 * GB,
  capacity: 119 * GB,
};

const listeners = {
  read: [] as ((p: ReadProgress) => void)[],
  readDone: [] as ((e: string | null) => void)[],
  sync: [] as ((p: SyncProgress) => void)[],
  syncDone: [] as ((d: SyncDone) => void)[],
};

let timer: number | undefined;
let progress: SyncProgress | null = null;
let stopRequested = false;

const report = (interrupted: boolean, written: number): SyncReport => ({
  machine: 'leo-pc',
  at: Date.now() / 1000,
  seconds: 852,
  interrupted,
  written,
  bytesWritten: 15.6 * GB,
  upgraded: 3,
  conversions: [
    ['Hi-res FLAC → FLAC', 114],
    ['WAV → FLAC', 22],
    ['AAC/Ogg/Opus → MP3', 4],
  ],
  converted: 140,
  skipped: [],
  failed: [{ path: 'Cypress Hill / IV / 14 Riot Starter.mp3', reason: 'the file ends early and can’t be read' }],
  tagsGuessed: ['Birds on a Wire/Nuées ardentes', 'Unknown Artist/Face B', 'Unknown Artist/cd-2004'],
  warnings: ['cover art of Birds on a Wire/Nuées ardentes/01 Ramages.flac can’t be read; the album gets the placeholder'],
  cardTracks: 1583,
  cardAlbums: 83,
  podcasts: settings.podcasts
    ? { copied: 5, moved: 2, deleted: 3, bytesWritten: 280 * MB, skipped: [], failed: [], episodes: 136, shows: 6, listened: 41 }
    : undefined,
});

export const mockApi: Api = {
  desktop: q.get('web') === null,
  async settings() {
    await wait();
    return settings;
  },
  async saveSettings(u) {
    await wait();
    settings = {
      ...settings,
      ...(u.machine !== undefined ? { machine: u.machine || settings.hostname } : {}),
      ...(u.exclude ? { exclude: u.exclude } : {}),
      ...(u.albumKey ? { albumKey: u.albumKey } : {}),
      ...(u.podcasts !== undefined
        ? { podcasts: u.podcasts ? { path: u.podcasts, exists: true, episodes: 134, shows: 6 } : null }
        : {}),
      ...(u.libraries
        ? {
            libraries: u.libraries.map(
              (p) => settings.libraries.find((l) => l.path === p) ?? { path: p, exists: true, tracks: 240, albums: 18, formats: ['MP3'] },
            ),
          }
        : {}),
    };
    return settings;
  },
  async cards() {
    await wait(60);
    const c = card();
    return { selected: c.state.kind === 'missing' ? null : c, others: [], busy: timer ? 'syncing' : null };
  },
  async selectCard() {
    cardKind = 'ready';
  },
  async readCard() {
    let done = 0;
    const t = window.setInterval(() => {
      done = Math.min(4212, done + 160);
      listeners.read.forEach((cb) =>
        cb({ filesDone: done, files: 4212, fraction: done / 4212, elapsed: done / 70, eta: (4212 - done) / 70 }),
      );
      if (done >= 4212) {
        clearInterval(t);
        cardKind = 'ready';
        listeners.readDone.forEach((cb) => cb(null));
      }
    }, 200);
  },
  async eject() {
    await wait(400);
  },
  async check() {
    await wait(400);
    return plan();
  },
  async syncStart() {
    stopRequested = false;
    const total = 15.6 * GB;
    const start = Date.now();
    progress = {
      bytesDone: 0,
      bytesTotal: total,
      filesDone: 0,
      filesTotal: 761,
      speed: null,
      elapsed: 0,
      eta: null,
      copying: null,
      converting: [],
      recent: [],
    };
    timer = window.setInterval(() => {
      const p = progress!;
      p.bytesDone = Math.min(total, p.bytesDone + 0.11 * GB);
      p.filesDone = Math.round((p.bytesDone / total) * 761);
      p.elapsed = (Date.now() - start) / 1000;
      p.speed = 18.4 * MB;
      p.eta = (total - p.bytesDone) / p.speed;
      p.copying = { path: 'Bernard Lavilliers / Les 50 plus belles chansons / 2-29 Faits divers.flac', done: (p.filesDone % 10) * 4.8 * MB, total: 48 * MB };
      p.converting = [{ worker: 0, path: 'Radu Lupu / Complete Decca Solo Recordings / 3-07 Impromptu D. 899 No. 3.flac', what: '96 kHz/24 → 44.1 kHz/16' }];
      p.recent = NEW.slice(0, Math.min(6, 1 + Math.floor(p.filesDone / 40)))
        .map(([title, artist, tracks], i) => ({ dir: title, title, artist, tracks, cover: `tone:${i}` }))
        .reverse();
      listeners.sync.forEach((cb) => cb({ ...p }));
      const finished = p.bytesDone >= total;
      if (finished || stopRequested) {
        clearInterval(timer);
        timer = undefined;
        if (finished) synced = true;
        const r = report(!finished, p.filesDone);
        progress = null;
        listeners.syncDone.forEach((cb) => cb({ report: r, error: null }));
      }
    }, 200);
  },
  async syncStop() {
    stopRequested = true;
  },
  async syncStatus() {
    return progress;
  },
  async report() {
    return report(false, 760);
  },
  async library() {
    await wait();
    return lib;
  },
  async removeAlbums(ids) {
    await wait(500);
    const gone = lib.albums.filter((a) => ids.includes(a.id));
    lib = {
      ...lib,
      albums: lib.albums.filter((a) => !ids.includes(a.id)),
      removed: [
        ...gone.map((a) => ({ id: a.id, title: a.title, artist: a.artist, tracks: a.tracks.length, removedAt: Date.now() / 1000, machine: settings.machine })),
        ...lib.removed,
      ],
    };
    return lib;
  },
  async podcasts() {
    await wait();
    return podView();
  },
  async setPodcastKeep(key, keep) {
    await wait(60);
    podShows = podShows.map((s) => (s.key === key ? { ...s, keep } : s));
    return podView();
  },
  async setPodcastPlayed(path, played) {
    await wait(60);
    podShows = podShows.map((s) => ({
      ...s,
      episodes: s.episodes.map((e) => (e.path === path ? { ...e, played, position: played ? e.position : 0 } : e)),
    }));
    return podView();
  },
  async bringBack(id) {
    await wait();
    lib = { ...lib, removed: lib.removed.filter((r) => r.id !== id) };
    return lib;
  },
  async installOnCard() {
    await wait(600);
    return 'openpod-sync-windows-x86_64.exe';
  },
  async pickFolder() {
    return window.prompt('Folder path') || null;
  },
  coverUrl: (path) => path,
  access: async () => null,
  grantAccess: async () => null,
  onReadProgress: (cb) => void listeners.read.push(cb),
  onReadDone: (cb) => void listeners.readDone.push(cb),
  onSyncProgress: (cb) => void listeners.sync.push(cb),
  onSyncDone: (cb) => void listeners.syncDone.push(cb),
};

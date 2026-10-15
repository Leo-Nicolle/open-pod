// Shapes returned by the backend (src-tauri/src/lib.rs and
// openpod-core's card.rs / view.rs). Keep in sync with the Rust structs.

export interface LastSync {
  machine: string;
  /** Unix seconds. */
  at: number;
  /** Bytes/s, 0 when not measured. */
  write_speed: number;
}

export type CardState =
  | { kind: 'missing' }
  | { kind: 'locked'; reason: string }
  | { kind: 'empty' }
  | { kind: 'unread'; files: number; bytes: number }
  | { kind: 'ready'; tracks: number; albums: number; lastSync: LastSync | null };

export interface CardInfo {
  path: string;
  name: string;
  capacity: number;
  free: number;
  state: CardState;
}

export interface Cards {
  selected: CardInfo | null;
  others: CardInfo[];
  busy: 'reading' | 'syncing' | null;
}

export interface LibraryInfo {
  path: string;
  exists: boolean;
  tracks: number;
  albums: number;
  formats: string[];
}

export type AlbumKey = 'artist-album' | 'name';

/** The podcasts folder, mirrored to the card's Podcasts/ (adds, moves and deletes). */
export interface PodcastsInfo {
  path: string;
  exists: boolean;
  episodes: number;
  /** Folders holding episodes. */
  shows: number;
}

export interface Settings {
  machine: string;
  hostname: string;
  libraries: LibraryInfo[];
  podcasts: PodcastsInfo | null;
  exclude: string[];
  albumKey: AlbumKey;
  ffmpeg: string | null;
  configPath: string;
}

export interface SettingsUpdate {
  machine?: string;
  libraries?: string[];
  exclude?: string[];
  albumKey?: AlbumKey;
  /** '' stops mirroring podcasts. */
  podcasts?: string;
}

export interface ReadProgress {
  filesDone: number;
  files: number;
  fraction: number;
  elapsed: number;
  eta: number | null;
}

export interface Skipped {
  path: string;
  reason: string;
}

export interface PlanAlbum {
  id: string;
  title: string;
  artist: string;
  tracks: number;
  bytes: number;
  new: boolean;
  upgrade: boolean;
  converted: boolean;
  tagsGuessed: boolean;
  /** A file of the album, for its cover. */
  cover: string;
}

export interface PlanSummary {
  tracks: number;
  albums: number;
  upgrades: number;
  fixes: number;
  conversions: number;
  bytes: number;
  free: number;
  writeSpeed: number;
  speedMeasured: boolean;
  estSeconds: number;
  alreadyOnCard: number;
  unchanged: number;
  removed: number;
  albumList: PlanAlbum[];
  skipped: Skipped[];
  unreadable: Skipped[];
  /** The podcast mirror, when a podcasts folder is set. */
  podcasts?: PodcastPlan;
}

export interface PodcastPlan {
  new: number;
  moved: number;
  deleted: number;
  /** Deletions past a show's keep setting (the rest are gone from the folder). */
  leaving: number;
  bytes: number;
  /** Shows getting new episodes or losing some ("France Culture / Les chemins…"). */
  shows: { key: string; folder: string; episodes: number; leaving: number; bytes: number; cover: string }[];
  /** Source episodes not on the card: new, or past their show's keep setting (`held`). */
  episodes: SourceEpisode[];
  skipped: Skipped[];
}

export interface SourceEpisode {
  /** CardShow.key */
  show: string;
  title: string;
  /** Days since 1970-01-01. */
  date: number;
  duration: number;
  bytes: number;
  held: boolean;
}

export interface CardEpisode {
  title: string;
  /** Days since 1970-01-01, 0 when unknown. */
  date: number;
  duration: number;
  bytes: number;
  path: string;
  /** Where the player stopped, in seconds. */
  position: number;
  played: boolean;
}

/** A show: the folder holding its episodes. */
export interface CardShow {
  /** Folder names from the podcasts folder, '/'-joined. */
  key: string;
  title: string;
  /** The folders above it ("France Culture"). */
  publisher: string;
  /** Latest episodes kept on the card; null: all. */
  keep: number | null;
  /** Its folder on this computer; '' when mirrored from another one. */
  source: string;
  machines: string[];
  bytes: number;
  cover: string;
  /** Newest first. */
  episodes: CardEpisode[];
}

export interface CardPodcasts {
  shows: CardShow[];
  episodes: number;
  bytes: number;
  played: number;
  started: number;
  /** podcast_state.bin was read. */
  stateRead: boolean;
}

export interface PodcastReport {
  copied: number;
  moved: number;
  deleted: number;
  bytesWritten: number;
  skipped: Skipped[];
  failed: Skipped[];
  episodes: number;
  shows: number;
  /** Episodes started or played. */
  listened: number;
}

export interface SyncProgress {
  bytesDone: number;
  bytesTotal: number;
  filesDone: number;
  filesTotal: number;
  speed: number | null;
  elapsed: number;
  eta: number | null;
  copying: { path: string; done: number; total: number } | null;
  /** The next files to write, ready (first few). */
  queued: string[];
  converting: { worker: number; path: string; what: string }[];
  recent: { dir: string; title: string; artist: string; tracks: number; cover: string }[];
}

export interface SyncReport {
  machine: string;
  at: number;
  seconds: number;
  interrupted: boolean;
  written: number;
  bytesWritten: number;
  upgraded: number;
  conversions: [string, number][];
  converted: number;
  skipped: Skipped[];
  failed: Skipped[];
  tagsGuessed: string[];
  warnings: string[];
  cardTracks: number;
  cardAlbums: number;
  podcasts?: PodcastReport;
}

export interface SyncDone {
  report: SyncReport | null;
  error: string | null;
}

export interface CardTrack {
  disc: number;
  no: number;
  title: string;
  duration: number;
  format: string;
  path: string;
}

export interface CardAlbum {
  id: string;
  title: string;
  artist: string;
  bytes: number;
  duration: number;
  format: string;
  machines: string[];
  conversions: string[];
  tagsGuessed: boolean;
  addedAt: number;
  cover: string;
  tracks: CardTrack[];
}

export interface RemovedAlbum {
  id: string;
  title: string;
  artist: string;
  tracks: number;
  removedAt: number;
  machine: string;
}

export interface LibraryView {
  albums: CardAlbum[];
  removed: RemovedAlbum[];
  tracks: number;
  bytes: number;
  capacity: number;
}

export interface Access {
  music: { name: string; granted: boolean; tracks: number | null }[];
  card: { name: string; granted: boolean } | null;
}

/** What the screens need from a backend: Tauri commands on the desktop,
 * sample data in a plain browser (`npm run dev`), WASM later on the web. */
export interface Api {
  readonly desktop: boolean;
  settings(): Promise<Settings>;
  saveSettings(update: SettingsUpdate): Promise<Settings>;
  cards(): Promise<Cards>;
  selectCard(path: string): Promise<void>;
  readCard(): Promise<void>;
  eject(): Promise<void>;
  check(): Promise<PlanSummary>;
  syncStart(): Promise<void>;
  syncStop(): Promise<void>;
  syncStatus(): Promise<SyncProgress | null>;
  report(): Promise<SyncReport | null>;
  library(): Promise<LibraryView>;
  removeAlbums(ids: string[]): Promise<LibraryView>;
  bringBack(id: string): Promise<LibraryView>;
  podcasts(): Promise<CardPodcasts>;
  /** How many latest episodes a show keeps on the card (null: all); the next sync applies it. */
  setPodcastKeep(show: string, keep: number | null): Promise<CardPodcasts>;
  /** Marks an episode (card path) played or not, as the player does. */
  setPodcastPlayed(path: string, played: boolean): Promise<CardPodcasts>;
  installOnCard(): Promise<string>;
  /** Folder picker; null when cancelled. `write`: the folder will be
   * written to (the card). `purpose` 'podcasts': the podcasts folder (the
   * browser keeps its handle apart from the music folders). */
  pickFolder(title: string, start?: string, write?: boolean, purpose?: 'podcasts'): Promise<string | null>;
  /** URL of an album cover thumbnail for one of its files: a URL right
   * away, or a promise of one (null: no cover). */
  coverUrl(path: string): string | Promise<string | null>;
  /** Web only: which chosen folders the browser lets the page use (null on
   * the desktop, where there is nothing to grant). */
  access(): Promise<Access | null>;
  /** Web only: asks the browser again for the folders chosen before (needs
   * a click). */
  grantAccess(): Promise<Access | null>;
  onReadProgress(cb: (p: ReadProgress) => void): void;
  onReadDone(cb: (error: string | null) => void): void;
  onSyncProgress(cb: (p: SyncProgress) => void): void;
  onSyncDone(cb: (d: SyncDone) => void): void;
}

import { convertFileSrc, invoke } from '@tauri-apps/api/core';
import { listen } from '@tauri-apps/api/event';
import { open } from '@tauri-apps/plugin-dialog';
import type { Api } from './types';

/** The desktop backend: commands in src-tauri/src/lib.rs. */
export const tauriApi: Api = {
  desktop: true,
  settings: () => invoke('settings'),
  saveSettings: (update) => invoke('save_settings', { update }),
  cards: () => invoke('cards'),
  selectCard: (path) => invoke('select_card', { path }),
  readCard: () => invoke('read_card'),
  eject: () => invoke('eject'),
  check: () => invoke('check'),
  syncStart: () => invoke('sync_start'),
  syncStop: () => invoke('sync_stop'),
  syncStatus: () => invoke('sync_status'),
  report: () => invoke('report'),
  library: () => invoke('library_albums'),
  removeAlbums: (ids) => invoke('remove_albums', { ids }),
  bringBack: (id) => invoke('bring_back', { id }),
  installOnCard: () => invoke('install_on_card'),
  async pickFolder(title, start) {
    const r = await open({ directory: true, multiple: false, title, defaultPath: start });
    return typeof r === 'string' ? r : null;
  },
  coverUrl: (path) => convertFileSrc(path, 'cover'),
  access: async () => null,
  grantAccess: async () => null,
  onReadProgress: (cb) => void listen('card-read', (e) => cb(e.payload as never)),
  onReadDone: (cb) => void listen('card-read-done', (e) => cb(e.payload as never)),
  onSyncProgress: (cb) => void listen('sync-progress', (e) => cb(e.payload as never)),
  onSyncDone: (cb) => void listen('sync-done', (e) => cb(e.payload as never)),
};

import { mockApi } from './mock';
import { tauriApi } from './tauri';
import type { Api } from './types';
import { webApi, webSupported } from './web';

export * from './types';

/** Tauri in the desktop app; in a browser, the real web backend, or the
 * sample data with `?mock` (and wherever the web backend can't run). */
const q = new URLSearchParams(location.search);
export const api: Api =
  '__TAURI_INTERNALS__' in window ? tauriApi : q.has('mock') || !webSupported ? mockApi : webApi;

/** A browser that can't write to a chosen folder (Firefox, Safari). */
export const unsupportedBrowser = !('__TAURI_INTERNALS__' in window) && !webSupported && !q.has('mock');

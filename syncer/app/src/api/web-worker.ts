// A Web Worker running openpod-web: reads library and card files
// synchronously (FileReaderSync) for the per-file work. The page's pool
// (web.ts) sends one request at a time.
import init, { art, convert, hash, probe, probe_episode } from '../wasm/openpod_web.js';

export type Request =
  | { id: number; op: 'probe'; file: File; name: string; rel: string; lpc: boolean }
  | { id: number; op: 'probeEpisode'; file: File; name: string; rel: string }
  | { id: number; op: 'hash'; file: File }
  | { id: number; op: 'convert'; file: File; name: string; policy: string }
  | { id: number; op: 'art'; file: File; name: string };

export type Response = { id: number; ok: true; value: unknown } | { id: number; ok: false; error: string };

const ready = init();

self.onmessage = async (e: MessageEvent<Request>) => {
  const r = e.data;
  await ready;
  try {
    let value: unknown;
    let transfer: Transferable[] = [];
    switch (r.op) {
      case 'probe':
        value = probe(r.file, r.name, r.rel, r.lpc);
        break;
      case 'probeEpisode':
        value = probe_episode(r.file, r.name, r.rel);
        break;
      case 'hash':
        value = hash(r.file);
        break;
      case 'convert': {
        const bytes = convert(r.file, r.name, r.policy);
        value = bytes;
        transfer = [bytes.buffer];
        break;
      }
      case 'art': {
        const bytes = art(r.file, r.name);
        value = bytes ?? null;
        if (bytes) transfer = [bytes.buffer];
        break;
      }
    }
    (self as unknown as Worker).postMessage({ id: r.id, ok: true, value } satisfies Response, transfer);
  } catch (err) {
    (self as unknown as Worker).postMessage({ id: r.id, ok: false, error: String(err instanceof Error ? err.message : err) } satisfies Response);
  }
};

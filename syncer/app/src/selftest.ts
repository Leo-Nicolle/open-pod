// `?selftest`: runs the web backend end to end on the browser's private
// storage (OPFS, same folder API as a picked folder): a small library of
// generated WAVs, an empty "card", then check → sync → remove → bring back.
// Results go to <pre id="selftest">.
import { useFolders, webApi } from './api/web';
import type { SyncDone } from './api/types';

function wav(freq: number, secs = 0.5, rate = 44100): Blob {
  const n = Math.floor(rate * secs);
  const buf = new ArrayBuffer(44 + n * 4);
  const v = new DataView(buf);
  const str = (o: number, s: string) => [...s].forEach((c, i) => v.setUint8(o + i, c.charCodeAt(0)));
  str(0, 'RIFF');
  v.setUint32(4, 36 + n * 4, true);
  str(8, 'WAVEfmt ');
  v.setUint32(16, 16, true);
  v.setUint16(20, 1, true);
  v.setUint16(22, 2, true);
  v.setUint32(24, rate, true);
  v.setUint32(28, rate * 4, true);
  v.setUint16(32, 4, true);
  v.setUint16(34, 16, true);
  str(36, 'data');
  v.setUint32(40, n * 4, true);
  for (let i = 0; i < n; i++) {
    const s = Math.round(Math.sin((i * freq * 2 * Math.PI) / rate) * 8000);
    v.setInt16(44 + i * 4, s, true);
    v.setInt16(46 + i * 4, s, true);
  }
  return new Blob([buf]);
}

async function put(root: FileSystemDirectoryHandle, path: string, data: Blob) {
  const parts = path.split('/');
  let d = root;
  for (const p of parts.slice(0, -1)) d = await d.getDirectoryHandle(p, { create: true });
  const w = await (await d.getFileHandle(parts[parts.length - 1], { create: true })).createWritable();
  await w.write(data);
  await w.close();
}

async function ls(d: FileSystemDirectoryHandle, prefix = ''): Promise<string[]> {
  const out: string[] = [];
  for await (const [name, h] of (d as any).entries()) {
    const rel = prefix + name;
    if (h.kind === 'directory') out.push(...(await ls(h, rel + '/')));
    else out.push(`${rel} ${(await h.getFile()).size}`);
  }
  return out.sort();
}

export async function selftest() {
  const log: string[] = [];
  const out = document.createElement('pre');
  out.id = 'selftest';
  document.body.append(out);
  const say = (s: string) => {
    log.push(s);
    out.textContent = log.join('\n');
  };
  try {
    const root = await navigator.storage.getDirectory();
    for (const n of ['music', 'card']) await root.removeEntry(n, { recursive: true }).catch(() => {});
    const music = await root.getDirectoryHandle('music', { create: true });
    const card = await root.getDirectoryHandle('card', { create: true });
    await put(music, 'Band/First Album/01 Opening.wav', wav(440));
    await put(music, 'Band/First Album/02 Closing.wav', wav(550));
    await put(music, 'Other/Second Album/01 Hello.wav', wav(660));
    useFolders([{ name: 'music', handle: music }], card);

    const s1 = await webApi.check();
    say(`check1: tracks=${s1.tracks} albums=${s1.albums} conversions=${s1.conversions}`);
    const done = new Promise<SyncDone>((r) => webApi.onSyncDone(r));
    await webApi.syncStart();
    const d = await done;
    say(`sync: error=${d.error} written=${d.report?.written} converted=${d.report?.converted}`);
    say('card:\n  ' + (await ls(card)).join('\n  '));
    const s2 = await webApi.check();
    say(`check2 (expect 0): tracks=${s2.tracks} unchanged=${s2.unchanged}`);
    const lib = await webApi.library();
    say(`library: ${lib.albums.map((a) => `${a.artist}/${a.title}:${a.tracks.length}`).join(', ')}`);
    const first = lib.albums.find((a) => a.title === 'First Album')!;
    const after = await webApi.removeAlbums([first.id]);
    say(`removed: albums=${after.albums.length} removedList=${after.removed.length}`);
    const s3 = await webApi.check();
    say(`check3 (expect 0, 2 removed): tracks=${s3.tracks} removed=${s3.removed}`);
    await webApi.bringBack(after.removed[0].id);
    const s4 = await webApi.check();
    say(`check4 (expect 2): tracks=${s4.tracks}`);
    say('SELFTEST DONE');
  } catch (e) {
    say(`SELFTEST FAILED: ${e instanceof Error ? e.stack : e}`);
  }
}

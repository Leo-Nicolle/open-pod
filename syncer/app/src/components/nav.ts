import { computed } from 'vue';
import { gb, num } from '../format';
import { card, store, type Screen } from '../store';

/** The card block in the sidebar and the chip in the top bar. */
export const cardSummary = computed(() => {
  const c = card.value;
  if (!c) return { ok: false, status: 'Not found', name: 'No card', pct: null, line: 'Plug in the SD card', chip: 'No card' };
  const known = c.capacity > 0; // a browser can't see the card's size
  const used = known ? Math.round(((c.capacity - c.free) / c.capacity) * 100) : null;
  const base = { name: c.name, pct: used };
  const size = (s: string) => (known ? s : '');
  if (store.sync.state === 'running' || store.cards?.busy === 'syncing')
    return { ...base, ok: false, status: 'Writing', line: `${gb(c.capacity)} · ${gb(c.free)} free`, chip: `${c.name} · writing` };
  switch (c.state.kind) {
    case 'missing':
      return { ...base, ok: false, status: 'Not found', name: 'No card', pct: null, line: 'Plug in the SD card', chip: 'No card' };
    case 'locked':
      return { ...base, ok: false, status: 'Locked', pct: null, line: 'Read-only', chip: 'Card locked' };
    case 'empty':
      return { ...base, ok: true, status: 'New', line: known ? `${gb(c.capacity)} · empty` : 'Empty', chip: 'New card' };
    case 'unread':
      return {
        ...base,
        ok: false,
        status: 'Not read yet',
        line: known ? `${gb(c.capacity)} · ${gb(c.capacity - c.free)} used` : 'Has music, not read yet',
        chip: `${c.name} · ${store.read.running ? 'reading' : 'not read yet'}`,
      };
    case 'ready':
      return {
        ...base,
        ok: true,
        status: 'Ready',
        line: `${known ? `${gb(c.capacity)} · ${gb(c.free)} free · ` : ''}${num(c.state.tracks)} tracks · ${num(c.state.albums)} albums`,
        chip: known ? `${c.name} · ${gb(c.free)} free` : `${c.name} · ${num(c.state.albums)} albums`,
      };
  }
});

export const nav = computed(() => {
  const c = card.value;
  const albums = c?.state.kind === 'ready' ? num(c.state.albums) : '';
  const tracks = store.plan.summary ? store.plan.summary.tracks + store.plan.summary.upgrades : null;
  const items: [Screen, string, string][] = [
    ['setup', 'Card & music', ''],
    ['whatsnew', "What's new", tracks ? num(tracks) : ''],
    ['syncing', 'Sync', ''],
    ['library', 'On the card', albums],
    ['settings', 'Report & settings', ''],
  ];
  return items.map(([id, label, count]) => ({ id, label, count }));
});

// What the Podcasts tab shows for a show's episodes: the card's, plus the
// ones in the podcasts folder from the last check, ranked against the
// show's keep setting like the sync (podcasts/mirror.rs `past_keep`; ties
// there go by file name, here by title).
import type { CardEpisode, CardShow, SourceEpisode } from '../api';

export type EpisodeStatus = 'next' | 'progress' | 'played' | 'on' | 'leave' | 'pc';

export interface EpisodeRow {
  title: string;
  date: number;
  duration: number;
  status: EpisodeStatus;
  /** Seconds left and fraction listened, for 'progress'. */
  left: number;
  done: number;
  /** Card path of an episode on the card (null: only on this computer). */
  path: string | null;
  played: boolean;
}

export const KEEP_MAX = 10;

export const keepLabel = (keep: number | null) => (keep ? `Latest ${keep}` : 'All');

/** One step of the keep stepper: 1…10, then All. */
export function keepStep(keep: number | null, dir: 1 | -1): number | null {
  if (keep === null) return dir < 0 ? KEEP_MAX : null;
  const n = keep + dir;
  return n > KEEP_MAX ? null : Math.max(1, n);
}

/** Newest first, with what the next sync does to each. */
export function episodeRows(show: CardShow, source: SourceEpisode[] | null, keep = show.keep): EpisodeRow[] {
  type Cand = { e: CardEpisode | SourceEpisode; card: boolean };
  const cands: Cand[] = [
    ...show.episodes.map((e) => ({ e, card: true })),
    ...(source ?? []).filter((e) => e.show === show.key).map((e) => ({ e, card: false })),
  ];
  cands.sort((a, b) => b.e.date - a.e.date || b.e.title.localeCompare(a.e.title));
  return cands.map(({ e, card }, i) => {
    const kept = keep === null || i < keep;
    const c = card ? (e as CardEpisode) : null;
    let status: EpisodeStatus;
    if (!kept) status = card ? 'leave' : 'pc';
    else if (!c) status = 'next';
    else if (c.played) status = 'played';
    else if (c.position > 0) status = 'progress';
    else status = 'on';
    const left = c ? Math.max(0, e.duration - c.position) : 0;
    const done = c && e.duration > 0 ? Math.min(1, c.position / e.duration) : 0;
    return { title: e.title, date: e.date, duration: e.duration, status, left, done, path: c?.path ?? null, played: !!c?.played };
  });
}

/** "2 unplayed", "all played", "none played" style counts for a show. */
export function playedCounts(show: CardShow) {
  const played = show.episodes.filter((e) => e.played).length;
  return { played, unplayed: show.episodes.length - played };
}

// Numbers as the design writes them: "1,583", "15.6 GB", "3:02",
// "about 11 min", "3 October".

const nf = new Intl.NumberFormat('en-US');

export const num = (n: number) => nf.format(n);

/** [value, unit]: "15.6", "GB". Decimal units, like the OS shows them. */
export function bytesParts(b: number): [string, string] {
  if (b >= 1e9) return [(b / 1e9).toFixed(b >= 100e9 ? 0 : 1), 'GB'];
  if (b >= 1e6) return [(b / 1e6).toFixed(b >= 100e6 ? 0 : 1), 'MB'];
  if (b >= 1e3) return [(b / 1e3).toFixed(0), 'KB'];
  return [String(b), 'B'];
}

export const bytes = (b: number) => bytesParts(b).join(' ');

/** Whole gigabytes for capacities: "119 GB". */
export const gb = (b: number) => (b >= 10e9 ? `${Math.round(b / 1e9)} GB` : bytes(b));

export const speed = (bps: number) => `${(bps / 1e6).toFixed(1)} MB/s`;

/** "0:42", "3:02", "1:02:09". */
export function clock(secs: number): string {
  const s = Math.max(0, Math.floor(secs));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const ss = String(s % 60).padStart(2, '0');
  return h ? `${h}:${String(m).padStart(2, '0')}:${ss}` : `${m}:${ss}`;
}

/** "about 40 s", "about 11 min", "about 1 h 20 min". */
export function about(secs: number): string {
  if (secs < 60) return `about ${Math.max(5, Math.round(secs / 5) * 5)} s`;
  const min = Math.round(secs / 60);
  if (min < 60) return `about ${min} min`;
  return `about ${Math.floor(min / 60)} h ${min % 60} min`;
}

/** Track length "3:13"; album length "58 min" / "2 h 05 min". */
export const trackTime = (secs: number) => clock(secs);
export function albumTime(secs: number): string {
  const min = Math.round(secs / 60);
  return min < 60 ? `${min} min` : `${Math.floor(min / 60)} h ${String(min % 60).padStart(2, '0')} min`;
}

const MONTHS = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
const SHORT = MONTHS.map((m) => m.slice(0, 3));

/** "3 October", or "3 October 2025" outside the current year. */
export function day(unix: number, withYear = false): string {
  const d = new Date(unix * 1000);
  const y = withYear || d.getFullYear() !== new Date().getFullYear() ? ` ${d.getFullYear()}` : '';
  return `${d.getDate()} ${MONTHS[d.getMonth()]}${y}`;
}

/** "2 Oct". */
export const shortDay = (unix: number) => {
  const d = new Date(unix * 1000);
  return `${d.getDate()} ${SHORT[d.getMonth()]}`;
};

/** "today 14:32", "yesterday 09:10", "3 October 14:32". */
export function when(unix: number): string {
  const d = new Date(unix * 1000);
  const hm = `${String(d.getHours()).padStart(2, '0')}:${String(d.getMinutes()).padStart(2, '0')}`;
  const today = new Date();
  const y = new Date(today);
  y.setDate(today.getDate() - 1);
  if (d.toDateString() === today.toDateString()) return `today ${hm}`;
  if (d.toDateString() === y.toDateString()) return `yesterday ${hm}`;
  return `${day(unix)} ${hm}`;
}

export const plural = (n: number, one: string, many = `${one}s`) => `${num(n)} ${n === 1 ? one : many}`;

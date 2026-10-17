<template>
  <section class="bl-panel st">
    <div class="st-head">
      <h2>Fast scroll</h2>
      <span class="bl-badge" :class="TIERS[tier].cls">{{ TIERS[tier].label }}</span>
      <span class="bl-spacer" />
      <span v-if="sending" class="bl-small bl-muted">sending…</span>
      <span v-else-if="synced" class="bl-badge green">on the board</span>
    </div>

    <p class="bl-small bl-muted">
      Turn the wheel (the real one, the remote's, or the mouse wheel over it) on a long list: Tracks, Albums or
      Artists. Only lists longer than 3 screens jump by letters. Speed shifts up one gear at a time (3 letters, 2,
      then first letters): the average over a speed window of travel has to reach the next gear's threshold, and
      each gear starts the window over. In a gear, each click is one jump at any speed, both ways. Holding still
      shifts one gear down, lifting the finger goes back to rows. The board forgets these values on reboot: copy them
      into <code>fast_scroll.cpp</code> once they feel right.
    </p>

    <!-- Speed: now, and the last seconds against the thresholds -->
    <div class="st-gauge" aria-label="Wheel speed">
      <div class="st-bar">
        <div class="st-fill" :class="TIERS[tier].cls" :style="{ width: `${pct(shownSpeed)}%` }" />
        <template v-for="m in marks" :key="m.key">
          <span class="st-mark" :style="{ left: `${pct(m.v)}%` }" :title="`${m.label}: ${m.v} c/s`" />
          <span class="st-mark-label bl-xs" :style="{ left: `${pct(m.v)}%` }">{{ m.short }}</span>
        </template>
      </div>
      <div class="st-speed">
        <span class="bl-mono st-big">{{ shownSpeed }}</span>
        <span class="bl-small bl-muted">clicks/s</span>
      </div>
    </div>

    <div class="st-hist" aria-label="Speed over the last 10 seconds">
      <template v-for="m in marks" :key="m.key">
        <span class="th" :style="{ bottom: inset(pct(m.v)) }" />
      </template>
      <span
        v-for="(p, i) in histPoints"
        :key="i"
        class="dot"
        :class="TIERS[p.tier].cls"
        :style="{ left: inset(p.x), bottom: inset(pct(p.v)) }"
      />
    </div>
    <div class="st-hist-axis bl-xs bl-muted"><span>−10 s</span><span>now</span></div>

    <!-- Tuning -->
    <form class="st-fields" :class="{ locked: busy || !device }" :inert="busy || !device" @submit.prevent>
      <div v-for="f in FIELDS" :key="f.key" class="st-field">
        <label :for="`st-${f.key}`">
          <b>{{ f.label }}</b>
          <span class="bl-xs bl-muted">{{ f.hint }}</span>
        </label>
        <input
          type="range"
          :min="f.min"
          :max="f.max"
          :step="f.step"
          :value="form[f.key]"
          :aria-label="f.label"
          @input="set(f.key, ($event.target as HTMLInputElement).valueAsNumber)"
        />
        <span class="st-num">
          <input
            :id="`st-${f.key}`"
            type="number"
            class="bl-mono"
            :min="f.min"
            :max="f.max"
            :value="form[f.key]"
            @change="set(f.key, ($event.target as HTMLInputElement).valueAsNumber)"
          />
          <span class="bl-xs bl-muted">{{ f.unit }}</span>
        </span>
      </div>
    </form>

    <div class="st-actions">
      <button class="bl-ghost md" :disabled="!device || busy" @click="apply(DEFAULTS)">Firmware defaults</button>
      <button v-if="saved && !same(saved, form)" class="bl-ghost md" :disabled="!device || busy" @click="apply(saved)">
        Re-apply last session
      </button>
      <span class="bl-spacer" />
      <button class="bl-ghost md" @click="copy">{{ copied ? "Copied" : "Copy as C++" }}</button>
    </div>
    <code class="st-cpp bl-mono">{{ cpp }}</code>

    <!-- What the wheel did -->
    <div class="st-events">
      <div class="bl-label">Wheel events</div>
      <p v-if="!recent.length" class="bl-small bl-muted">
        Nothing yet. Turning the physical wheel should print <code>touch</code>, <code>scroll</code> and
        <code>fast</code> here: if only the remote does, the board isn't seeing the real finger.
      </p>
      <ol v-else class="bl-mono bl-xs">
        <li v-for="(e, i) in recent" :key="i" :class="e.type">
          <span class="bl-muted">{{ e.dt }}</span> {{ e.text }}
        </li>
      </ol>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, reactive, ref, watch } from "vue";
import type { Fields } from "@bench/device.ts";
import { useBenchLive } from "./state.ts";

const { device, busy, wheelEvents, now, fail } = useBenchLive();

type Key = "s1" | "s2" | "s3" | "win" | "dwell";
type Tuning = Record<Key, number>;

// ScrollTuning in software/src/state/fast_scroll.h / .cpp.
const DEFAULTS: Tuning = { s1: 16, s2: 28, s3: 40, win: 12, dwell: 520 };
const FIELDS: { key: Key; label: string; hint: string; unit: string; min: number; max: number; step: number }[] = [
  { key: "s1", label: "1st gear", hint: "rows → 3 letters (ABA → ABB)", unit: "c/s", min: 1, max: 120, step: 1 },
  { key: "s2", label: "2nd gear", hint: "→ 2 letters (AB → AC)", unit: "c/s", min: 1, max: 120, step: 1 },
  { key: "s3", label: "3rd gear", hint: "→ first letters (A → B)", unit: "c/s", min: 1, max: 120, step: 1 },
  { key: "win", label: "Speed window", hint: "travel each shift averages over (12 = half a turn)", unit: "clicks", min: 2, max: 24, step: 1 },
  { key: "dwell", label: "Hold to shift down", hint: "still this long: one gear down", unit: "ms", min: 100, max: 1500, step: 10 },
];
const KEYS = FIELDS.map((f) => f.key);
const TIERS = [
  { label: "rows", cls: "l0" },
  // Indexed by the letters a click jumps by: 1 = first-letter jumps, the fastest.
  { label: "1 letter", cls: "l1" },
  { label: "2 letters", cls: "l2" },
  { label: "3 letters", cls: "l3" },
];

const form = reactive<Tuning>({ ...DEFAULTS });
const board = ref<Tuning | null>(null);
const sending = ref(false);
const synced = computed(() => board.value !== null && same(board.value, form));
const same = (a: Tuning, b: Tuning) => KEYS.every((k) => a[k] === b[k]);

function fromFields(f: Fields): Tuning | null {
  const t = {} as Tuning;
  for (const k of KEYS) {
    if (f[k] === undefined) return null;
    t[k] = +f[k];
  }
  return t;
}

// --- Remembered across sessions (the board resets to its defaults on boot) ---
const STORE = "opentone.scrollcfg.v3"; // v3: gears (s1-s3, dwell)
function load(): Tuning | null {
  try {
    const t = JSON.parse(localStorage.getItem(STORE) ?? "null");
    return t && KEYS.every((k) => typeof t[k] === "number") ? t : null;
  } catch {
    return null;
  }
}
const saved = ref<Tuning | null>(load());
function save(t: Tuning) {
  saved.value = { ...t };
  try {
    localStorage.setItem(STORE, JSON.stringify(t));
  } catch {
    // Private window or blocked storage: only this session keeps them.
  }
}

// --- Talking to the board -----------------------------------------------------
const dirty = new Set<Key>();
let sendTimer: ReturnType<typeof setTimeout> | null = null;

function set(k: Key, v: number) {
  if (!Number.isFinite(v)) return;
  const f = FIELDS.find((x) => x.key === k)!;
  form[k] = Math.max(f.min, Math.min(f.max, Math.round(v)));
  dirty.add(k);
  if (sendTimer) clearTimeout(sendTimer);
  sendTimer = setTimeout(flush, 150);
}
function apply(t: Tuning) {
  for (const k of KEYS) if (form[k] !== t[k]) set(k, t[k]);
}

async function flush() {
  sendTimer = null;
  const d = device.value;
  if (!d || !dirty.size) return;
  const args: Partial<Tuning> = {};
  for (const k of dirty) args[k] = form[k];
  dirty.clear();
  sending.value = true;
  try {
    const t = fromFields(await d.scrollConfig(args));
    if (t) {
      board.value = t;
      // Keys changed again while this was on its way keep the newer value.
      for (const k of KEYS) if (!dirty.has(k)) form[k] = t[k];
      save(t);
    }
  } catch (e) {
    fail("#scrollcfg failed", e instanceof Error ? e.message : String(e));
  } finally {
    sending.value = false;
  }
}

// On connect: read what the board runs now.
watch(
  device,
  async (d) => {
    board.value = null;
    if (!d) return;
    try {
      const t = fromFields(await d.scrollConfig());
      if (t) {
        board.value = t;
        Object.assign(form, t);
      }
    } catch (e) {
      fail("Couldn't read the scroll settings", "Is the bench firmware up to date? `pio run -e bench -t upload`", String(e));
    }
  },
  { immediate: true },
);
onBeforeUnmount(() => {
  if (sendTimer) clearTimeout(sendTimer);
});

// --- Speed --------------------------------------------------------------------
const HIST_MS = 10000;
const fastEvents = computed(() => wheelEvents.value.filter((e) => e.type === "fast"));
const last = computed(() => fastEvents.value.at(-1));
// The speed starts over after a pause of `dwell`. The gear holds until the board says
// otherwise (it reports each shift down and the lift-off as a "fast" event with v=0).
const stale = computed(() => !last.value || now.value - last.value.t > Math.max(form.dwell, 600));
const shownSpeed = computed(() => (stale.value ? 0 : (last.value?.value ?? 0)));
const tier = computed(() => last.value?.letters ?? 0);

const scaleMax = computed(() => Math.max(60, Math.ceil((Math.max(form.s1, form.s2, form.s3) * 1.5) / 10) * 10));
const pct = (v: number) => Math.min(100, (100 * v) / scaleMax.value);
// A % of the history box, 6 px in from its edges so dots there stay whole.
const inset = (p: number) => `calc(6px + (100% - 12px) * ${p / 100})`;
const marks = computed(() => [
  { key: "s1", v: form.s1, label: "1st gear (3 letters)", short: "1st" },
  { key: "s2", v: form.s2, label: "2nd gear (2 letters)", short: "2nd" },
  { key: "s3", v: form.s3, label: "3rd gear (1 letter)", short: "3rd" },
]);
const histPoints = computed(() => {
  const end = Math.max(now.value, last.value?.t ?? 0);
  return fastEvents.value
    .filter((e) => end - e.t < HIST_MS)
    .map((e) => ({ x: 100 - (100 * (end - e.t)) / HIST_MS, v: e.value ?? 0, tier: e.letters ?? 0 }));
});

// --- The wheel's own events ------------------------------------------------------
const recent = computed(() => {
  const evs = wheelEvents.value.filter((e) => ["touch", "untouch", "scroll", "fast"].includes(e.type)).slice(-24);
  return evs
    .map((e, i) => ({
      type: e.type,
      dt: i ? `+${String(e.boardMs - evs[i - 1].boardMs).padStart(4)}` : "     ",
      text:
        e.type === "scroll"
          ? `scroll ${e.value! > 0 ? "+" : ""}${e.value}`
          : e.type === "fast"
            ? e.value
              ? `  ${e.value} c/s → ${TIERS[e.letters ?? 0].label}`
              : `  shift down → ${TIERS[e.letters ?? 0].label}`
            : e.type,
    }))
    .reverse();
});

// --- Copy -------------------------------------------------------------------------
const cpp = computed(
  () => `ScrollTuning scrollTuning = {${form.s1}, ${form.s2}, ${form.s3}, ${form.win}, ${form.dwell}};`,
);
const copied = ref(false);
async function copy() {
  try {
    await navigator.clipboard.writeText(cpp.value);
    copied.value = true;
    setTimeout(() => (copied.value = false), 1200);
  } catch {
    // No clipboard access: the line is shown below to copy by hand.
  }
}
</script>

<style scoped>
.st {
  display: flex;
  flex-direction: column;
  gap: 16px;
  padding: 22px;
  min-width: 0;
}
.st-head {
  display: flex;
  align-items: center;
  gap: 10px;
}
.st-head h2 {
  margin: 0;
}
.bl-badge.l0 {
  background: var(--bl-surface2);
  color: var(--bl-muted);
}
.bl-badge.l1,
.bl-badge.l2,
.bl-badge.l3 {
  background: var(--bl-brandsoft);
  color: var(--bl-brand);
}

/* Gauge */
.st-gauge {
  display: flex;
  align-items: center;
  gap: 16px;
}
.st-bar {
  position: relative;
  flex: 1;
  height: 18px;
  margin-bottom: 14px;
  border-radius: 9px;
  background: var(--bl-surface2);
}
.st-fill {
  height: 100%;
  border-radius: 9px;
  background: var(--bl-muted);
  transition: width 120ms ease-out;
}
.st-fill.l3 {
  background: var(--bl-brand2);
}
.st-fill.l2 {
  background: var(--bl-brandhover);
}
.st-fill.l1 {
  background: var(--bl-brand);
}
.st-mark {
  position: absolute;
  top: -3px;
  bottom: -3px;
  width: 2px;
  margin-left: -1px;
  background: var(--bl-text);
}
.st-mark-label {
  position: absolute;
  top: 22px;
  transform: translateX(-50%);
  color: var(--bl-muted);
}
.st-speed {
  display: flex;
  align-items: baseline;
  gap: 6px;
  min-width: 110px;
}
.st-big {
  font-size: 28px;
  font-weight: 600;
  font-variant-numeric: tabular-nums;
}

/* History */
.st-hist {
  position: relative;
  height: 120px;
  border: 1px solid var(--bl-line);
  border-radius: 8px;
  background: var(--bl-bg);
}
.st-hist .th,
.st-hist .dot {
  position: absolute;
}
.st-hist .th {
  left: 0;
  right: 0;
  border-top: 1px solid var(--bl-line2);
}
.st-hist .dot {
  width: 6px;
  height: 6px;
  margin: 0 0 -3px -3px;
  border-radius: 50%;
  background: var(--bl-muted);
}
.st-hist .dot.l3 {
  background: var(--bl-brand2);
}
.st-hist .dot.l2 {
  background: var(--bl-brandhover);
}
.st-hist .dot.l1 {
  background: var(--bl-brand);
}
.st-hist-axis {
  display: flex;
  justify-content: space-between;
  margin-top: -10px;
}

/* Fields */
.st-fields {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
.st-fields.locked {
  opacity: 0.4;
}
.st-field {
  display: grid;
  grid-template-columns: minmax(150px, 220px) minmax(0, 1fr) auto;
  align-items: center;
  gap: 14px;
}
.st-field label {
  display: flex;
  flex-direction: column;
  font-size: 13px;
}
.st-field input[type="range"] {
  width: 100%;
  accent-color: var(--bl-brand);
}
.st-num {
  display: flex;
  align-items: center;
  gap: 6px;
}
.st-num input {
  width: 64px;
  padding: 4px 6px;
  border: 1px solid var(--bl-line2);
  border-radius: 6px;
  background: var(--bl-bg);
  color: var(--bl-text);
  text-align: right;
}
.st-num span {
  width: 36px;
}
.st-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}
.st-cpp {
  display: block;
  padding: 8px 10px;
  overflow-x: auto;
  white-space: nowrap;
  font-size: 12px;
}

/* Events */
.st-events ol {
  max-height: 260px;
  margin: 8px 0 0;
  padding: 0;
  overflow-y: auto;
  list-style: none;
  white-space: pre;
}
.st-events li.fast {
  color: var(--bl-muted);
}
.st-events li.touch,
.st-events li.untouch {
  color: var(--bl-brand);
}

@container (max-width: 640px) {
  .st-field {
    grid-template-columns: minmax(0, 1fr) auto;
  }
  .st-field input[type="range"] {
    grid-column: 1 / -1;
    grid-row: 2;
  }
}
</style>

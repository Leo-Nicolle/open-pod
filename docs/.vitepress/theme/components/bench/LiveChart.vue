<template>
  <figure class="lc">
    <header class="lc-head">
      <div class="lc-titles">
        <div class="lc-title">{{ title }}</div>
        <div class="lc-sub">{{ sub }}</div>
      </div>
      <div class="lc-readout">{{ readout }}<span>{{ readoutUnit }}</span></div>
    </header>
    <div class="lc-legend">
      <div v-for="l in legend" :key="l.name" class="lc-key">
        <i :class="{ dot: l.dot }" :style="{ background: l.color }" />
        <span>{{ l.name }}</span>
        <b>{{ l.value }}</b>
      </div>
      <span class="lc-spacer" />
      <slot name="units" />
    </div>
    <div
      ref="plot"
      class="lc-plot"
      :style="{ height: `${height}px` }"
      role="img"
      :aria-label="`${title}, last ${windowS} seconds`"
      @mousemove="onMove"
      @mouseleave="hoverT = null"
    >
      <template v-for="t in yTicks" :key="t.top">
        <div class="lc-grid" :style="{ top: t.top }" />
        <div class="lc-tick" :style="{ top: t.top }">{{ t.label }}</div>
      </template>
      <svg :viewBox="`0 0 ${W} ${H}`" preserveAspectRatio="none">
        <rect v-for="(b, i) in bandRects" :key="'b' + i" :x="b.x" y="0" :width="b.w" :height="H" class="lc-band" />
        <template v-for="s in series" :key="s.name">
          <path v-if="s.area && mode === 'line'" :d="areaFor(s)" class="lc-area" />
        </template>
        <path v-if="threshold" :d="`M0,${y(threshold.v).toFixed(1)} H${W}`" class="lc-thresh" />
        <template v-for="s in [...series].reverse()" :key="s.name">
          <path
            v-if="mode === 'line'"
            :d="pathFor(s)"
            class="lc-line"
            :style="{ stroke: s.color, strokeWidth: s.width ?? 2, strokeDasharray: s.dash ?? 'none' }"
          />
          <template v-else>
            <path :d="dotsFor(s, false)" class="lc-dots" :style="{ stroke: s.color }" />
            <path :d="dotsFor(s, true)" class="lc-dots bad" />
          </template>
        </template>
        <line v-if="hover" :x1="hover.x" :x2="hover.x" y1="0" :y2="H" class="lc-cross" />
      </svg>
      <div v-if="threshold" class="lc-thresh-label" :style="{ top: `${(100 * y(threshold.v)) / H}%` }">
        {{ threshold.label }}
      </div>
      <div v-if="hover" class="lc-over">
        <span
          v-for="h in hover.items"
          :key="h.name"
          class="lc-hover-dot"
          :style="{ left: `${(100 * hover.x) / W}%`, top: `${(100 * y(h.v)) / H}%`, background: h.color }"
        />
        <div class="lc-tip" :style="{ left: `${Math.min(Math.max((100 * hover.x) / W, 18), 82)}%` }">
          <div class="lc-tip-t">{{ hover.ago }}</div>
          <div v-for="h in hover.items" :key="h.name" class="lc-tip-row">
            <i :style="{ background: h.color }" />
            <span>{{ h.label ?? h.name }}</span>
            <b>{{ format(h.v) }}</b>
          </div>
        </div>
      </div>
      <p v-if="!hasData" class="lc-empty">{{ empty }}</p>
    </div>
    <div class="lc-x"><span>{{ windowLabel }}</span><span>now</span></div>
  </figure>
</template>

<script setup lang="ts">
import { computed, ref } from "vue";

export type Point = { t: number; v: number; label?: string };
export type Series = {
  name: string;
  color: string;
  points: Point[];
  width?: number;
  dash?: string;
  /** Fills under the line with the brand's soft colour. */
  area?: boolean;
};
export type LegendItem = { name: string; color: string; value: string; dot?: boolean };

const props = withDefaults(
  defineProps<{
    title: string;
    sub?: string;
    readout?: string;
    readoutUnit?: string;
    legend?: LegendItem[];
    series: Series[];
    /** Device time of the right edge, ms. */
    now: number;
    windowS?: number;
    mode?: "line" | "dots";
    /** Values are multiplied by this before drawing (unit switches). */
    scale?: number;
    /** The y axis reaches at least this (scaled) value. */
    yFloor?: number;
    /** A dashed amber line at a scaled value, e.g. the frame budget. */
    threshold?: { v: number; label: string } | null;
    /** Dots above this scaled value are drawn red. */
    badAbove?: number;
    /** Shaded spans [from, to] in device ms, e.g. seconds with underruns. */
    bands?: [number, number][];
    height?: number;
    /** Formats a scaled value, for the axis and the tooltip. */
    format?: (v: number) => string;
    tick?: (v: number) => string;
    empty?: string;
  }>(),
  {
    sub: "",
    readout: "—",
    readoutUnit: "",
    legend: () => [],
    windowS: 60,
    mode: "line",
    scale: 1,
    yFloor: 0,
    threshold: null,
    badAbove: Infinity,
    bands: () => [],
    height: 230,
    format: (v: number) => String(Math.round(v * 10) / 10),
    tick: undefined,
    empty: "Waiting for data…",
  },
);

const W = 600;
const H = 200;
/** A gap longer than this breaks the line (disconnected, or the board rebooted). */
const GAP_MS = 3500;

const from = computed(() => props.now - props.windowS * 1000);
const visible = (s: Series) =>
  s.points.filter((p) => p.t >= from.value && p.t <= props.now).map((p) => ({ ...p, v: p.v * props.scale }));
const hasData = computed(() => props.series.some((s) => visible(s).length));

/** 1, 2, 2.5 or 5 times a power of ten, with a little headroom. */
function niceMax(v: number) {
  v = Math.max(v * 1.12, 1e-6);
  const p = Math.pow(10, Math.floor(Math.log10(v)));
  for (const m of [1, 2, 2.5, 5, 10]) if (m * p >= v) return m * p;
  return 10 * p;
}

const top = computed(() => {
  const max = Math.max(props.yFloor, props.threshold?.v ?? 0, ...props.series.flatMap((s) => visible(s).map((p) => p.v)));
  return niceMax(max || 1);
});

const tickLabel = (v: number) => (props.tick ? props.tick(v) : String(+v.toFixed(2)));
const yTicks = computed(() => [
  { label: tickLabel(top.value), top: "0%" },
  { label: tickLabel(top.value / 2), top: "50%" },
  { label: "0", top: "100%" },
]);

const windowLabel = computed(() =>
  props.windowS >= 60 && props.windowS % 60 === 0 ? `−${props.windowS / 60} min` : `−${props.windowS} s`,
);

const x = (t: number) => ((t - from.value) / (props.windowS * 1000)) * W;
const y = (v: number) => H - (Math.min(v, top.value) / top.value) * H;

const bandRects = computed(() =>
  props.bands
    .filter(([a, b]) => b >= from.value && a <= props.now)
    .map(([a, b]) => {
      const x0 = x(Math.max(a, from.value));
      return { x: x0, w: Math.max(2, x(Math.min(b, props.now)) - x0) };
    }),
);

function pathFor(s: Series) {
  let d = "";
  let prev: Point | null = null;
  for (const p of visible(s)) {
    d += `${!prev || p.t - prev.t > GAP_MS ? "M" : "L"}${x(p.t).toFixed(1)},${y(p.v).toFixed(1)}`;
    prev = p;
  }
  return d;
}
function areaFor(s: Series) {
  const v = visible(s);
  if (!v.length) return "";
  return `${pathFor(s)}L${x(v[v.length - 1].t).toFixed(1)},${H}L${x(v[0].t).toFixed(1)},${H}Z`;
}
// Zero-length segments with round caps: dots that stay round in a stretched SVG.
function dotsFor(s: Series, bad: boolean) {
  return visible(s)
    .filter((p) => p.v > props.badAbove === bad)
    .map((p) => `M${x(p.t).toFixed(1)},${y(p.v).toFixed(1)}l0,0`)
    .join("");
}

// --- Hover: crosshair + nearest point of each series ---------------------
const plot = ref<HTMLDivElement | null>(null);
const hoverT = ref<number | null>(null);

function onMove(e: MouseEvent) {
  const rect = plot.value!.getBoundingClientRect();
  const left = rect.left + 52; // the y-axis column
  const fx = (e.clientX - left) / (rect.width - 52);
  hoverT.value = fx < 0 || fx > 1 ? null : from.value + fx * props.windowS * 1000;
}

const hover = computed(() => {
  if (hoverT.value === null) return null;
  const t = hoverT.value;
  const reach = props.mode === "dots" ? 2500 : 1500;
  const items = props.series
    .map((s) => {
      let best: Point | null = null;
      for (const p of visible(s)) if (!best || Math.abs(p.t - t) < Math.abs(best.t - t)) best = p;
      return best && Math.abs(best.t - t) <= reach ? { name: s.name, color: s.color, ...best } : null;
    })
    .filter((h): h is NonNullable<typeof h> => h !== null);
  if (!items.length) return null;
  const at = items[0].t;
  const ago = (props.now - at) / 1000;
  return { x: x(at), items, ago: ago < 1 ? "now" : `${ago.toFixed(0)} s ago` };
});
</script>

<style scoped>
.lc {
  display: flex;
  flex-direction: column;
  gap: 12px;
  margin: 0;
  padding: 18px 20px 14px;
  border: 1px solid var(--bl-line);
  border-radius: 12px;
  background: var(--bl-surface);
  min-width: 0;
}
.lc-head {
  display: flex;
  align-items: flex-start;
  gap: 12px;
}
.lc-titles {
  flex: 1;
  min-width: 0;
}
.lc-title {
  font-size: 15px;
  font-weight: 600;
}
.lc-sub {
  margin-top: 2px;
  font-size: 12px;
  color: var(--bl-muted);
}
.lc-readout {
  font-family: var(--bl-mono);
  font-size: 22px;
  font-weight: 500;
  line-height: 1.2;
  white-space: nowrap;
}
.lc-readout span {
  margin-left: 4px;
  font-size: 12px;
  color: var(--bl-muted);
}
.lc-legend {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px 14px;
  min-height: 28px;
}
.lc-key {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
}
.lc-key i {
  width: 12px;
  height: 3px;
  border-radius: 2px;
}
.lc-key i.dot {
  width: 7px;
  height: 7px;
  border-radius: 50%;
}
.lc-key span {
  color: var(--bl-muted);
}
.lc-key b {
  font-family: var(--bl-mono);
  font-weight: 400;
}
.lc-spacer {
  flex: 1;
}
.lc-plot {
  position: relative;
  margin: 6px 0 2px;
  touch-action: pan-y;
}
.lc-grid {
  position: absolute;
  left: 52px;
  right: 0;
  border-top: 1px solid var(--bl-line);
}
.lc-tick {
  position: absolute;
  left: 0;
  width: 44px;
  transform: translateY(-50%);
  text-align: right;
  font-family: var(--bl-mono);
  font-size: 11px;
  color: var(--bl-muted);
}
svg {
  position: absolute;
  left: 52px;
  top: 0;
  width: calc(100% - 52px);
  height: 100%;
  overflow: visible;
}
svg path,
svg line {
  vector-effect: non-scaling-stroke;
}
.lc-band {
  fill: var(--bl-band);
}
.lc-area {
  fill: var(--bl-brandsoft);
}
.lc-thresh {
  fill: none;
  stroke: var(--bl-amber);
  stroke-width: 1;
  stroke-dasharray: 4 4;
}
.lc-thresh-label {
  position: absolute;
  right: 0;
  transform: translateY(-130%);
  padding: 0 4px;
  background: var(--bl-surface);
  font-family: var(--bl-mono);
  font-size: 11px;
  color: var(--bl-amber);
}
.lc-line {
  fill: none;
  stroke-linejoin: round;
}
.lc-dots {
  fill: none;
  stroke-width: 7;
  stroke-linecap: round;
}
.lc-dots.bad {
  stroke: var(--bl-red);
}
.lc-cross {
  stroke: var(--bl-muted);
  stroke-width: 1;
  stroke-dasharray: 3 3;
}
/* The svg's box, for overlays placed in its coordinates. */
.lc-over {
  position: absolute;
  inset: 0 0 0 52px;
  pointer-events: none;
}
.lc-hover-dot {
  position: absolute;
  width: 9px;
  height: 9px;
  margin: -4.5px 0 0 -4.5px;
  border-radius: 50%;
  box-shadow: 0 0 0 2px var(--bl-surface);
}
.lc-tip {
  position: absolute;
  top: 8px;
  transform: translateX(-50%);
  padding: 6px 8px;
  border: 1px solid var(--bl-line2);
  border-radius: 8px;
  background: var(--bl-bg);
  font-size: 12px;
  white-space: nowrap;
  z-index: 2;
}
.lc-tip-t {
  color: var(--bl-muted);
  margin-bottom: 2px;
}
.lc-tip-row {
  display: flex;
  align-items: center;
  gap: 6px;
  color: var(--bl-muted);
}
.lc-tip-row i {
  width: 10px;
  height: 3px;
  border-radius: 2px;
}
.lc-tip-row b {
  margin-left: auto;
  padding-left: 12px;
  font-family: var(--bl-mono);
  font-weight: 400;
  color: var(--bl-text);
}
.lc-empty {
  position: absolute;
  inset: 0 0 0 52px;
  display: grid;
  place-items: center;
  margin: 0;
  font-size: 13px;
  color: var(--bl-muted);
  pointer-events: none;
}
.lc-x {
  display: flex;
  justify-content: space-between;
  padding-left: 52px;
  font-family: var(--bl-mono);
  font-size: 11px;
  color: var(--bl-muted);
}
</style>

<template>
  <figure class="et">
    <figcaption>
      <span class="et-title">Wheel events</span>
      <span class="et-unit">what the UI handled, from the wheel or the bench</span>
      <span class="et-legend">
        <span><i class="k-scroll" />1 click</span>
        <span><i class="k-multi" />several clicks in one event (a page jump)</span>
        <span><i class="k-touch" />finger on the wheel</span>
        <span><i class="k-press" />button held</span>
        <span><i class="k-long" />long press</span>
      </span>
      <span class="et-spacer" />
      <label class="et-window">
        <select v-model.number="windowS" aria-label="Time window">
          <option :value="5">5 s</option>
          <option :value="10">10 s</option>
          <option :value="30">30 s</option>
        </select>
      </label>
    </figcaption>
    <svg
      :viewBox="`0 0 ${W} ${H}`"
      role="img"
      :aria-label="`Wheel events, last ${windowS} seconds: ${visible.length} events`"
      @mousemove="onMove"
      @mouseleave="hover = null"
    >
      <!-- Lanes -->
      <text :x="L - 6" :y="scrollMid + 3.5" class="et-lane" text-anchor="end">scroll</text>
      <rect
        v-for="(b, i) in touchBands"
        :key="'t' + i"
        :x="b.x"
        :y="T"
        :width="b.w"
        :height="SCROLL_H"
        class="et-touch"
      />
      <line :x1="L" :x2="W - R" :y1="scrollMid" :y2="scrollMid" class="et-axis" />
      <text :x="W - R + 4" :y="T + 8" class="et-tick">up</text>
      <text :x="W - R + 4" :y="T + SCROLL_H - 2" class="et-tick">down</text>
      <rect
        v-for="(s, i) in scrollBars"
        :key="'s' + i"
        :x="s.x - 1.5"
        :y="s.y"
        width="3"
        :height="s.h"
        rx="1"
        class="et-scroll"
        :class="{ multi: s.multi }"
      />
      <template v-for="(name, row) in BUTTONS" :key="name">
        <text :x="L - 6" :y="rowY(row) + 3.5" class="et-lane" text-anchor="end">{{ name }}</text>
        <line :x1="L" :x2="W - R" :y1="rowY(row)" :y2="rowY(row)" class="et-grid" />
      </template>
      <rect
        v-for="(p, i) in presses"
        :key="'p' + i"
        :x="p.x"
        :y="rowY(p.row) - 4"
        :width="p.w"
        height="8"
        rx="4"
        class="et-press"
      />
      <g v-for="(m, i) in marks" :key="'m' + i">
        <circle :cx="m.x" :cy="rowY(m.row)" :r="m.kind === 'click' ? 3 : 4.5" class="et-mark" :class="m.kind" />
        <text v-if="m.kind !== 'click'" :x="m.x + 6" :y="rowY(m.row) - 6" class="et-label">{{ m.kind === "long" ? "long" : "very long" }}</text>
      </g>
      <text v-for="t in xTicks" :key="'x' + t.label" :x="t.x" :y="H - 2" class="et-tick" text-anchor="middle">{{ t.label }}</text>
      <line v-if="hover" :x1="hover.x" :x2="hover.x" :y1="T" :y2="H - B" class="et-cross" />
    </svg>
    <div v-if="hover" class="et-tip" :style="{ left: `${Math.min(Math.max((hover.x / W) * 100, 18), 82)}%` }">
      <div class="et-tip-t">{{ hover.ago }}</div>
      <div v-for="(l, i) in hover.lines" :key="i">{{ l }}</div>
    </div>
    <p v-if="!visible.length" class="et-empty">Turn or press the wheel, here or on the board.</p>
    <details class="et-list">
      <summary>Event list</summary>
      <table>
        <thead>
          <tr><th class="num">Board time</th><th class="num">Gap</th><th>Event</th></tr>
        </thead>
        <tbody>
          <tr v-for="(e, i) in listed" :key="i">
            <td class="num">{{ (e.boardMs / 1000).toFixed(3) }} s</td>
            <td class="num">{{ e.gap === null ? "" : `+${e.gap} ms` }}</td>
            <td>{{ describe(e) }}</td>
          </tr>
        </tbody>
      </table>
    </details>
  </figure>
</template>

<script setup lang="ts">
import { computed, ref } from "vue";
import type { WheelEvent } from "@bench/device.ts";

const props = defineProps<{ events: WheelEvent[]; now: number }>();
const windowS = ref(10);
const hover = ref<{ x: number; ago: string; lines: string[] } | null>(null);

const BUTTONS = ["select", "menu", "next", "play", "prev"] as const;
const W = 720;
const H = 196;
const L = 52;
const R = 36;
const T = 8;
const B = 16;
const SCROLL_H = 70;
const ROW_H = 18;
const scrollMid = T + SCROLL_H / 2;
const rowY = (row: number) => T + SCROLL_H + 14 + row * ROW_H;

const from = computed(() => props.now - windowS.value * 1000);
const x = (t: number) => L + ((t - from.value) / (windowS.value * 1000)) * (W - L - R);
const visible = computed(() => props.events.filter((e) => e.t >= from.value && e.t <= props.now));

const scrollBars = computed(() => {
  const unit = (SCROLL_H / 2 - 4) / 4; // 4 clicks reach the lane's edge
  return visible.value
    .filter((e) => e.type === "scroll")
    .map((e) => {
      const v = e.value ?? 0;
      const h = Math.max(3, Math.min(Math.abs(v), 4) * unit);
      return { x: x(e.t), y: v > 0 ? scrollMid - h : scrollMid, h, multi: Math.abs(v) > 1 };
    });
});

/** Spans between two events (touch..untouch, down..up); an open span runs to now. */
function spans(startType: string, endType: string, key: (e: WheelEvent) => string) {
  const out: { key: string; a: number; b: number }[] = [];
  const open = new Map<string, number>();
  // From all events: a span may have started before the window.
  for (const e of props.events) {
    if (e.type === startType) open.set(key(e), e.t);
    else if (e.type === endType && open.has(key(e))) {
      out.push({ key: key(e), a: open.get(key(e))!, b: e.t });
      open.delete(key(e));
    }
  }
  for (const [k, a] of open) out.push({ key: k, a, b: props.now });
  return out.filter((s) => s.b >= from.value && s.a <= props.now);
}
const clip = (s: { a: number; b: number }) => {
  const x0 = x(Math.max(s.a, from.value));
  return { x: x0, w: Math.max(2, x(Math.min(s.b, props.now)) - x0) };
};

const touchBands = computed(() => spans("touch", "untouch", () => "wheel").map(clip));
const presses = computed(() =>
  spans("down", "up", (e) => e.button ?? "").map((s) => ({ ...clip(s), row: BUTTONS.indexOf(s.key as any) })),
);
const marks = computed(() =>
  visible.value
    .filter((e) => e.type === "click" || e.type === "long" || e.type === "verylong")
    .map((e) => ({ x: x(e.t), row: BUTTONS.indexOf(e.button as any), kind: e.type })),
);

const xTicks = computed(() => {
  const n = 5;
  return Array.from({ length: n + 1 }, (_, i) => {
    const ago = windowS.value - (i * windowS.value) / n;
    return { x: x(props.now - ago * 1000), label: ago === 0 ? "now" : `−${+ago.toFixed(1)} s` };
  });
});

function describe(e: WheelEvent) {
  switch (e.type) {
    case "scroll":
      return `scroll ${e.value! > 0 ? "+" : ""}${e.value} (${e.value! > 0 ? "up" : "down"})`;
    case "touch":
      return "finger down";
    case "untouch":
      return "finger up";
    case "verylong":
      return `very long press ${e.button}`;
    case "long":
      return `long press ${e.button}`;
    default:
      return `${e.type} ${e.button ?? ""}`;
  }
}

const listed = computed(() => {
  const last = props.events.slice(-40);
  return last
    .map((e, i) => ({ ...e, gap: i ? e.boardMs - last[i - 1].boardMs : null }))
    .reverse();
});

function onMove(ev: MouseEvent) {
  const svg = ev.currentTarget as SVGSVGElement;
  const r = svg.getBoundingClientRect();
  const px = ((ev.clientX - r.left) / r.width) * W;
  if (px < L || px > W - R) return void (hover.value = null);
  // Events within ~6 px of the cursor.
  const near = visible.value.filter((e) => Math.abs(x(e.t) - px) <= 6);
  if (!near.length) return void (hover.value = null);
  const t = near[0].t;
  const ago = (props.now - t) / 1000;
  hover.value = {
    x: x(t),
    ago: ago < 0.1 ? "now" : `${ago.toFixed(2)} s ago`,
    lines: near.slice(0, 6).map(describe),
  };
}
</script>

<style scoped>
.et {
  position: relative;
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
figcaption {
  display: flex;
  flex-wrap: wrap;
  align-items: baseline;
  gap: 2px 12px;
}
.et-title {
  font-size: 15px;
  font-weight: 600;
}
.et-unit {
  font-size: 12px;
  color: var(--bl-muted);
}
.et-spacer {
  flex: 1;
}
.et-legend {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 14px;
  width: 100%;
  order: 3;
  margin-top: 10px;
  font-size: 12px;
  color: var(--bl-muted);
}
.et-legend i {
  display: inline-block;
  width: 10px;
  height: 10px;
  margin-right: 6px;
  border-radius: 3px;
  vertical-align: -1px;
}
.k-scroll {
  background: var(--bl-brand);
}
.k-multi {
  background: var(--bl-brand2);
}
.k-touch {
  background: var(--bl-brandsoft);
}
.k-press {
  background: color-mix(in srgb, var(--bl-sgreen) 45%, transparent);
}
.k-long {
  background: var(--bl-text);
  border-radius: 50% !important;
}
.et-window select {
  height: 26px;
  padding: 0 6px;
  border: 1px solid var(--bl-line2);
  border-radius: 6px;
  background: transparent;
  color: var(--bl-text);
  font-family: var(--bl-mono);
  font-size: 12px;
}
.et-window option {
  background: var(--bl-bg);
}
svg {
  display: block;
  width: 100%;
  height: auto;
  overflow: visible;
}
.et-lane,
.et-tick,
.et-label {
  fill: var(--bl-muted);
  font-family: var(--bl-mono);
  font-size: 10px;
}
.et-axis {
  stroke: var(--bl-line2);
  stroke-width: 1;
}
.et-grid {
  stroke: var(--bl-line);
  stroke-width: 1;
}
.et-touch {
  fill: var(--bl-brandsoft);
}
.et-scroll {
  fill: var(--bl-brand);
}
.et-scroll.multi {
  fill: var(--bl-brand2);
}
.et-press {
  fill: var(--bl-sgreen);
  opacity: 0.45;
}
.et-mark {
  fill: var(--bl-sgreen);
  stroke: var(--bl-surface);
  stroke-width: 2;
}
.et-mark.long,
.et-mark.verylong {
  fill: var(--bl-text);
}
.et-cross {
  stroke: var(--bl-muted);
  stroke-width: 1;
  stroke-dasharray: 3 3;
}
.et-tip {
  position: absolute;
  top: 60px;
  transform: translateX(-50%);
  pointer-events: none;
  padding: 6px 8px;
  border: 1px solid var(--bl-line2);
  border-radius: 8px;
  background: var(--bl-bg);
  font-size: 12px;
  white-space: nowrap;
  z-index: 2;
}
.et-tip-t {
  color: var(--bl-muted);
  margin-bottom: 2px;
}
.et-empty {
  position: absolute;
  inset: 110px 0 auto;
  margin: 0;
  text-align: center;
  font-size: 13px;
  color: var(--bl-muted);
  pointer-events: none;
}
.et-list summary {
  cursor: pointer;
  font-size: 12px;
  color: var(--bl-muted);
}
.et-list summary:hover {
  color: var(--bl-text);
}
.et-list table {
  display: table;
  width: 100%;
  margin: 8px 0 0;
  border-collapse: collapse;
  font-size: 12px;
}
.et-list tr {
  border: 0;
  background: none;
}
.et-list th,
.et-list td {
  border: 0;
  background: none;
}
.et-list th {
  padding: 0 8px 6px;
  border-bottom: 1px solid var(--bl-line);
  font-weight: 500;
  text-align: left;
  color: var(--bl-muted);
}
.et-list td {
  padding: 4px 8px;
  border-bottom: 1px solid var(--bl-line);
}
.num {
  text-align: right !important;
  font-family: var(--bl-mono);
}
</style>

<template>
  <section class="bl-panel bl-remote" @wheel="onWheel">
    <span v-if="busy" class="bl-badge amber bl-locked">locked · run in progress</span>
    <div class="bl-remote-body" :class="{ locked: busy }" :inert="busy">
      <div class="bl-wheel-wrap">
        <div
          ref="wheelEl"
          class="bl-wheel"
          :class="{ dragging: dragAngle !== null }"
          role="group"
          aria-label="Click wheel: press a button, or drag around the ring to turn it"
        >
          <button
            v-for="w in WEDGES"
            :key="w.name"
            class="bl-wedge"
            :class="[w.cls, { lit: lit(w.name) }]"
            :aria-label="w.label"
            @pointerdown="gestureDown(w.name, $event)"
            @pointermove="gestureMove"
            @pointerup="gestureUp"
            @pointercancel="gestureUp"
            @keydown.enter.prevent="click(w.name)"
            @keydown.space.prevent="click(w.name)"
          >
            {{ w.text }}
          </button>
          <button
            class="bl-center"
            :class="{ lit: lit('select') }"
            aria-label="Select (right-click for a long press)"
            @pointerdown="gestureDown('select', $event)"
            @pointerup="gestureUp"
            @pointercancel="gestureUp"
            @contextmenu.prevent="longPress"
            @keydown.enter.prevent="click('select')"
            @keydown.space.prevent="click('select')"
          >
            SELECT
          </button>
          <svg v-if="trailSegments.length || thumb" class="bl-trail" viewBox="0 0 280 280" aria-hidden="true">
            <line
              v-for="(l, i) in trailSegments"
              :key="i"
              :x1="l.x1"
              :y1="l.y1"
              :x2="l.x2"
              :y2="l.y2"
              :stroke-opacity="l.o"
            />
            <circle v-if="thumb" :cx="thumb.x" :cy="thumb.y" r="13" />
          </svg>
          <span
            v-if="fingerAngle >= 0 && dragAngle === null"
            class="bl-finger"
            :style="{
              left: `${140 + 97 * Math.cos((fingerAngle * Math.PI) / 180)}px`,
              top: `${140 - 97 * Math.sin((fingerAngle * Math.PI) / 180)}px`,
            }"
            aria-hidden="true"
          />
        </div>
      </div>
      <div class="bl-scroll">
        <button class="bl-ghost mono" @click="press('scroll', '24')">▲24</button>
        <button class="bl-ghost mono" @click="press('scroll', '1')">▲1</button>
        <button class="bl-ghost mono" @click="press('scroll', '-1')">▼1</button>
        <button class="bl-ghost mono" @click="press('scroll', '-24')">▼24</button>
      </div>
      <form class="bl-field" @submit.prevent="typeQuery">
        <label class="bl-small bl-muted" for="bl-search">Search</label>
        <div class="bl-field-row">
          <input id="bl-search" v-model="query" placeholder="Artist, album, song" />
          <button class="bl-ghost md" :disabled="!query">Type</button>
          <button class="bl-text-btn md" type="button" @click="clearSearch">Clear</button>
        </div>
      </form>
      <form class="bl-field" @submit.prevent="press('seek', String(seekTo))">
        <label class="bl-small bl-muted" for="bl-seek">Seek to</label>
        <div class="bl-field-row">
          <input id="bl-seek" v-model.number="seekTo" type="number" min="0" class="bl-mono short" />
          <span class="bl-muted">s</span>
          <button class="bl-ghost md">Seek</button>
        </div>
      </form>
    </div>
  </section>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, ref, shallowRef } from "vue";
import type { ButtonName } from "@bench/device.ts";
import { useBenchLive } from "./state.ts";

const { device, busy, fingerAngle, query, seekTo, press, lit, click, longPress, fail } = useBenchLive();

async function typeQuery() {
  await press("type", query.value);
  query.value = "";
}
function clearSearch() {
  query.value = "";
  void press("clear", "");
}

onBeforeUnmount(() => {
  cancelAnimationFrame(trailFrame);
  if (wheelTimer) clearTimeout(wheelTimer);
});

// --- The wheel: buttons, the virtual finger, the mouse wheel -------------------------

const WEDGES: { name: ButtonName; cls: string; label: string; text: string }[] = [
  { name: "menu", cls: "w-top", label: "Menu", text: "MENU" },
  { name: "next", cls: "w-right", label: "Next", text: "⏭︎" },
  { name: "play", cls: "w-bottom", label: "Play or pause", text: "⏯︎" },
  { name: "prev", cls: "w-left", label: "Previous", text: "⏮︎" },
];
const wheelEl = ref<HTMLDivElement | null>(null);

/** Held this long without moving, a press becomes a hold (the wheel times it, 800 ms is a long press). */
const HOLD_AFTER_MS = 150;
/** Moved this far first, it becomes a finger dragged around the ring. */
const DRAG_PX = 6;

type Gesture = { name: ButtonName; x: number; y: number; mode: "pending" | "hold" | "drag"; timer: ReturnType<typeof setTimeout> };
let gesture: Gesture | null = null;

/** The pointer's angle around the wheel's center: 0 = right, counter-clockwise, like the firmware. */
function pointerAngle(e: PointerEvent): number | null {
  const r = wheelEl.value!.getBoundingClientRect();
  const dx = e.clientX - (r.left + r.width / 2);
  const dy = e.clientY - (r.top + r.height / 2);
  const dist = Math.hypot(dx, dy) / (r.width / 2);
  if (dist < 0.3 || dist > 1.1) return null; // the center button, or off the wheel
  return (Math.round((Math.atan2(-dy, dx) * 180) / Math.PI) + 360) % 360;
}

let lastSentAt = 0;
let pendingAngle: number | null = null;
let sendTimer: ReturnType<typeof setTimeout> | null = null;
// At most ~60 updates a second: the board samples the wheel once a loop pass.
function sendFinger(angle: number | null) {
  pendingAngle = angle;
  if (sendTimer) return;
  const wait = Math.max(0, 16 - (Date.now() - lastSentAt));
  sendTimer = setTimeout(() => {
    sendTimer = null;
    lastSentAt = Date.now();
    void device.value?.finger(pendingAngle).catch(() => {});
  }, wait);
}

function gestureDown(name: ButtonName, e: PointerEvent) {
  if (!device.value || busy.value || e.button !== 0) return;
  try {
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
  } catch {
    // Not an active pointer (a synthetic event): the press still counts.
  }
  const g: Gesture = {
    name,
    x: e.clientX,
    y: e.clientY,
    mode: "pending",
    timer: setTimeout(() => {
      if (gesture !== g || g.mode !== "pending") return;
      g.mode = "hold";
      void device.value?.button(name, true).catch((err) => fail(`#btn ${name} failed`, String(err)));
    }, HOLD_AFTER_MS),
  };
  gesture = g;
}
function gestureMove(e: PointerEvent) {
  const g = gesture;
  if (!g) return;
  if (g.mode === "pending" && g.name !== "select" && Math.hypot(e.clientX - g.x, e.clientY - g.y) > DRAG_PX) {
    clearTimeout(g.timer);
    g.mode = "drag";
  }
  if (g.mode === "drag") {
    const a = pointerAngle(e);
    if (a === null) return;
    dragAngle.value = a;
    addTrail(a);
    sendFinger(a);
  }
}
function gestureUp() {
  const g = gesture;
  gesture = null;
  if (!g) return;
  clearTimeout(g.timer);
  const refresh = (ms: number) => setTimeout(() => void device.value?.send("state", "", 2000).catch(() => {}), ms);
  if (g.mode === "pending") click(g.name);
  else if (g.mode === "hold")
    void device.value
      ?.button(g.name, false)
      .then(() => refresh(100))
      .catch(() => {});
  else {
    dragAngle.value = null;
    sendFinger(null);
    refresh(150);
  }
}

// --- Drag feedback: the thumb on the ring, and a trail fading behind it ------

const RING_R = 97; // the middle of the ring, in the wheel's 280 px
const TRAIL_MS = 600;
/** Where the page's own finger is while dragging, null otherwise. */
const dragAngle = ref<number | null>(null);
const trail = shallowRef<{ a: number; t: number }[]>([]);
const trailNow = ref(0);
let trailFrame = 0;

function ringPoint(a: number) {
  const r = (a * Math.PI) / 180;
  return { x: 140 + RING_R * Math.cos(r), y: 140 - RING_R * Math.sin(r) };
}
function addTrail(a: number) {
  const t = performance.now();
  trail.value = [...trail.value.filter((p) => t - p.t < TRAIL_MS), { a, t }];
  if (!trailFrame) trailFrame = requestAnimationFrame(fadeTrail);
}
// Runs while the trail has points, so it fades out after the finger stops or lifts.
function fadeTrail() {
  trailNow.value = performance.now();
  trail.value = trail.value.filter((p) => trailNow.value - p.t < TRAIL_MS);
  trailFrame = trail.value.length ? requestAnimationFrame(fadeTrail) : 0;
}
const trailSegments = computed(() => {
  const pts = trail.value;
  const out = [];
  for (let i = 1; i < pts.length; i++) {
    const a = ringPoint(pts[i - 1].a);
    const b = ringPoint(pts[i].a);
    out.push({ x1: a.x, y1: a.y, x2: b.x, y2: b.y, o: 0.45 * Math.max(0, 1 - (trailNow.value - pts[i].t) / TRAIL_MS) });
  }
  return out;
});
const thumb = computed(() => (dragAngle.value === null ? null : ringPoint(dragAngle.value)));

// --- The mouse wheel: a finger gliding round the ring ------------------------
// Over the remote panel, mouse-wheel / trackpad scrolling drags the virtual
// finger round the ring, so it goes through the wheel's own scroll path
// (acceleration, fast scroll, liftoff) like a real thumb. Scrolling down
// turns clockwise: down the list, as on an iPod.

/** Ring degrees per scrolled pixel: a mouse notch (~100 px) is one 15° click. */
const WHEEL_DEG_PER_PX = 0.15;
/** Per 16 ms step. The board reads the turn the short way round once a loop pass (~50 ms, more while a screen slides), so a step stays far below 180°. */
const WHEEL_MAX_STEP = 20;
/** Lift the finger this long after the last scroll event (more than the wheel's 80 ms liftoff hold). */
const WHEEL_LIFT_MS = 250;

let wheelTarget = 0; // unwrapped degrees the scrolling asks for
let wheelAt: number | null = null; // where the finger is, null = not touching
let wheelLastEventAt = 0;
let wheelTimer: ReturnType<typeof setTimeout> | null = null;

/** The pointer's angle around the wheel's center, wherever it is in the panel. */
function panelAngle(e: MouseEvent): number {
  const r = wheelEl.value!.getBoundingClientRect();
  const dx = e.clientX - (r.left + r.width / 2);
  const dy = e.clientY - (r.top + r.height / 2);
  if (Math.hypot(dx, dy) < 1) return 90;
  return (Math.atan2(-dy, dx) * 180) / Math.PI;
}
const wrap = (a: number) => ((Math.round(a) % 360) + 360) % 360;

function onWheel(e: globalThis.WheelEvent) {
  if (!device.value || busy.value || gesture || !wheelEl.value) return;
  const px = e.deltaY * (e.deltaMode === 1 ? 40 : e.deltaMode === 2 ? 800 : 1);
  if (!px) return;
  e.preventDefault();
  if (wheelAt === null) {
    // Touch down under the cursor first: the wheel only counts motion from there.
    wheelAt = wheelTarget = panelAngle(e);
    lastSentAt = Date.now();
    void device.value.finger(wrap(wheelAt)).catch(() => {});
  }
  wheelTarget -= px * WHEEL_DEG_PER_PX;
  wheelLastEventAt = Date.now();
  if (!wheelTimer) wheelTimer = setTimeout(wheelStep, 16);
}
function wheelStep() {
  wheelTimer = null;
  if (wheelAt === null) return;
  const left = wheelTarget - wheelAt;
  if (Math.abs(left) >= 0.5) {
    wheelAt += Math.max(-WHEEL_MAX_STEP, Math.min(WHEEL_MAX_STEP, left));
    const a = wrap(wheelAt);
    dragAngle.value = a;
    addTrail(a);
    sendFinger(a);
  } else if (Date.now() - wheelLastEventAt >= WHEEL_LIFT_MS) {
    wheelAt = null;
    dragAngle.value = null;
    sendFinger(null);
    setTimeout(() => void device.value?.send("state", "", 2000).catch(() => {}), 150);
    return;
  }
  wheelTimer = setTimeout(wheelStep, 16);
}
</script>

<style scoped>
/* Remote */
.bl-remote {
  /* Stays in view under the nav and the status bar while the charts scroll. */
  --bl-stick: calc(var(--vp-nav-height, 64px) + var(--bl-status-h) + 16px);
  position: sticky;
  top: var(--bl-stick);
  display: flex;
  flex-direction: column;
  gap: 18px;
  padding: 22px;
}
.bl-locked {
  align-self: center;
}
.bl-remote-body {
  display: flex;
  flex-direction: column;
  gap: 18px;
}
.bl-remote-body.locked {
  opacity: 0.4;
  pointer-events: none;
}
.bl-wheel-wrap {
  display: flex;
  justify-content: center;
}
.bl-wheel {
  position: relative;
  width: 280px;
  height: 280px;
  flex: none;
  border-radius: 50%;
  background: var(--bl-surface2);
  overflow: hidden;
  user-select: none;
  touch-action: none;
}
.bl-wedge {
  position: absolute;
  inset: 0;
  display: flex;
  border: 0;
  background: transparent;
  color: var(--bl-text);
  font-size: 18px;
  cursor: pointer;
}
.bl-wedge:hover {
  background: var(--bl-line2);
}
/* Dragging around the ring: no button is being pressed, so none lights up. */
.bl-wheel.dragging {
  cursor: grabbing;
}
.bl-wheel.dragging .bl-wedge,
.bl-wheel.dragging .bl-wedge:hover {
  background: transparent;
  cursor: grabbing;
}
.bl-trail {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  pointer-events: none;
}
.bl-trail line {
  stroke: var(--bl-brand);
  stroke-width: 18;
  stroke-linecap: round;
}
.bl-trail circle {
  fill: var(--bl-brand);
  stroke: var(--bl-surface);
  stroke-width: 3;
}
.bl-wedge.lit,
.bl-center.lit {
  color: var(--bl-brand);
}
.w-top {
  clip-path: polygon(0 0, 100% 0, 50% 50%);
  justify-content: center;
  align-items: flex-start;
  padding-top: 26px;
  font-size: 13px;
  font-weight: 700;
  letter-spacing: 0.1em;
}
.w-right {
  clip-path: polygon(100% 0, 100% 100%, 50% 50%);
  justify-content: flex-end;
  align-items: center;
  padding-right: 28px;
}
.w-bottom {
  clip-path: polygon(0 100%, 100% 100%, 50% 50%);
  justify-content: center;
  align-items: flex-end;
  padding-bottom: 26px;
}
.w-left {
  clip-path: polygon(0 0, 0 100%, 50% 50%);
  justify-content: flex-start;
  align-items: center;
  padding-left: 28px;
}
.bl-center {
  position: absolute;
  left: 86px;
  top: 86px;
  width: 108px;
  height: 108px;
  border: 1px solid var(--bl-line2);
  border-radius: 50%;
  background: var(--bl-surface);
  color: var(--bl-text);
  font-size: 12px;
  font-weight: 600;
  letter-spacing: 0.06em;
  cursor: pointer;
}
.bl-center:hover {
  background: var(--bl-bg);
}
.bl-finger {
  position: absolute;
  width: 22px;
  height: 22px;
  margin: -11px 0 0 -11px;
  border-radius: 50%;
  background: var(--bl-brand);
  opacity: 0.55;
  pointer-events: none;
}
.bl-scroll {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 6px;
}
.bl-field {
  display: flex;
  flex-direction: column;
  gap: 8px;
}
.bl-field-row {
  display: flex;
  align-items: center;
  gap: 6px;
}
.bl-field-row input:not(.short) {
  flex: 1;
  min-width: 0;
}
.bl input.short {
  width: 80px;
}
</style>

<script setup lang="ts">
import { computed, ref } from "vue";
import type { PropType } from "vue";
import { PINS, findPin, type PinDef, morphoLabel } from "./pins";
import { CONNECTORS, type Connector, type ConnectorPin } from "./board";
import { parseUsage } from "./usage";
import { type Assignment, parseMapping } from "./mapping";

const props = defineProps({
  usage: { type: String, default: "" },
  mapping: { type: String, default: "" },
  defaultMode: { type: String as PropType<"chip" | "board">, default: "chip" },
});

const mode = ref<"chip" | "board">(props.defaultMode);

/* ------------------------------------------------------------------ usage */
// The "which pins are used" model is a list of peripherals, each with pins.
// It comes from the `mapping` prop (firmware mapping) or, as a fallback, from a
// flat `usage` spec (JSON / CSV / markdown), which is wrapped as a single group.
const assignments = computed<Assignment[]>(() => {
  if (props.mapping) {
    const m = parseMapping(props.mapping);
    if (m.length) return m;
  }
  const usage = parseUsage(props.usage);
  if (usage.length) {
    return [{ peripheral: "Used pins", pins: usage.map((u) => ({ pin: u.pin, role: u.label })) }];
  }
  return [];
});

interface UseRef {
  peripheral: string;
  role: string;
}

const usageByPin = computed(() => {
  const m = new Map<string, UseRef[]>();
  for (const a of assignments.value) {
    for (const p of a.pins) {
      const pin = findPin(p.pin);
      if (!pin) continue;
      const key = pin.name.toUpperCase();
      const arr = m.get(key) ?? [];
      arr.push({ peripheral: a.peripheral, role: p.role });
      m.set(key, arr);
    }
  }
  return m;
});

function usageFor(pin: PinDef): UseRef[] {
  return usageByPin.value.get(pin.name.toUpperCase()) ?? [];
}

const peripheralNames = computed(() => assignments.value.map((a) => a.peripheral));

/* ------------------------------------------------------- function families */
const FAMILIES = [
  "SPI1", "SPI2", "SPI3", "SPI4",
  "I2S1", "I2S2", "I2S3",
  "I2C1", "I2C2", "I2C3", "FMPI2C1",
  "USART1", "USART2", "USART3", "USART6", "UART4", "UART5",
  "CAN1", "CAN2",
  "TIM1", "TIM2", "TIM3", "TIM4", "TIM5", "TIM8", "TIM9", "TIM10", "TIM11", "TIM12", "TIM13", "TIM14",
  "ADC", "DAC", "SAI1", "SAI2",
  "QUADSPI", "SDIO", "DCMI", "FMC",
  "USB", "RTC", "HDMI_CEC", "MCO", "TRACED", "SWD",
];

function isDigit(c?: string): boolean {
  return !!c && c >= "0" && c <= "9";
}

function segmentMatchesFamily(seg: string, fam: string): boolean {
  const s = seg.toUpperCase();
  if (fam === "SWD") {
    return /SWD|JTMS|JTCK|JTDI|JTDO|NJTRST/.test(s);
  }
  if (fam === "USB") {
    return s.startsWith("USB") || s.startsWith("OTG");
  }
  if (!s.startsWith(fam)) return false;
  if (/\d$/.test(fam)) return !isDigit(s[fam.length]);
  return true;
}

function pinSegments(pin: PinDef): string[] {
  const segs: string[] = [];
  const add = (s: string) => {
    const t = s.trim();
    if (t && t !== "GPIO" && t !== "—") segs.push(t);
  };
  add(pin.signal);
  for (const a of pin.alt) for (const part of a.split("/")) add(part);
  for (const e of pin.extra) for (const part of e.split("/")) add(part);
  return segs;
}

const functionFamilies = computed(() => {
  const out: { family: string; count: number }[] = [];
  for (const f of FAMILIES) {
    let n = 0;
    for (const p of PINS) {
      if (pinSegments(p).some((s) => segmentMatchesFamily(s, f))) n++;
    }
    if (n) out.push({ family: f, count: n });
  }
  return out;
});

/* ------------------------------------------------------------ highlighting */
const selPeripheral = ref<string>("");
const selFunction = ref<string>("");
const hasFilter = computed(() => !!selPeripheral.value || !!selFunction.value);

function onPeripheralChange(e: Event) {
  selPeripheral.value = (e.target as HTMLSelectElement).value;
  selFunction.value = "";
}
function onFunctionChange(e: Event) {
  selFunction.value = (e.target as HTMLSelectElement).value;
  selPeripheral.value = "";
}
function clearFilter() {
  selPeripheral.value = "";
  selFunction.value = "";
}

function pinMatchesFilter(pin: PinDef): boolean {
  if (selPeripheral.value) {
    return usageFor(pin).some((u) => u.peripheral === selPeripheral.value);
  }
  if (selFunction.value) {
    return pinSegments(pin).some((s) => segmentMatchesFamily(s, selFunction.value));
  }
  return true;
}

function filterClass(pin: PinDef): string {
  if (!hasFilter.value) return "";
  return pinMatchesFilter(pin) ? "filter-hit" : "filter-dim";
}

function cpinFilterClass(cpin: ConnectorPin): string {
  if (!hasFilter.value) return "";
  const p = cpin.chip ? findPin(cpin.chip) : undefined;
  if (!p) return "filter-dim";
  return pinMatchesFilter(p) ? "filter-hit" : "filter-dim";
}

/* ------------------------------------------------------------- categories */
const DEBUG_PINS = new Set(["PA13", "PA14", "PA15", "PB3", "PB4"]);

function category(pin: PinDef): string {
  if (pin.type === "Power") return "power";
  if (pin.type === "Reset") return "reset";
  if (pin.type === "Boot") return "boot";
  if (usageFor(pin).length) return "used";
  if (DEBUG_PINS.has(pin.name)) return "debug";
  return "io";
}

const IO_STRUCTURE: Record<string, string> = {
  FT: "5 V tolerant I/O",
  FTf: "5 V tolerant I/O, I2C FM+ option",
  TTa: "3.3 V tolerant I/O, directly connected to ADC",
  B: "Dedicated BOOT0 pin",
  RST: "Bidirectional reset pin with weak pull-up",
  S: "Supply pin",
};

/* --------------------------------------------------------------- hover/… */
const hoverPin = ref<PinDef | null>(null);
const hoverCtx = ref<{ connId: string; cpin: ConnectorPin } | null>(null);
const pinnedPin = ref<PinDef | null>(null);
const pinnedCtx = ref<{ connId: string; cpin: ConnectorPin } | null>(null);

const activePin = computed<PinDef | null>(() => pinnedPin.value ?? hoverPin.value);
const activeCtx = computed(() => pinnedCtx.value ?? hoverCtx.value);
const isPinned = computed(() => pinnedPin.value !== null || pinnedCtx.value !== null);

function setHover(pin: PinDef | null, ctx: { connId: string; cpin: ConnectorPin } | null = null) {
  hoverPin.value = pin;
  hoverCtx.value = ctx;
}

function togglePin(
  pin: PinDef | null,
  ctx: { connId: string; cpin: ConnectorPin } | null = null
) {
  if (!pin) return;
  if (pinnedPin.value === pin) {
    pinnedPin.value = null;
    pinnedCtx.value = null;
  } else {
    pinnedPin.value = pin;
    pinnedCtx.value = ctx;
  }
}

function togglePeripheral(name: string) {
  selPeripheral.value = selPeripheral.value === name ? "" : name;
  selFunction.value = "";
}

/* ------------------------------------------------------------- chip layout */
const CHIP = 320;
const PAD = 26;
const M = 70;
const pitch = CHIP / 16;

interface Geo {
  pin: PinDef;
  pad: { x: number; y: number; w: number; h: number };
  label: { x: number; y: number; anchor: string };
  num: { x: number; y: number; anchor: string };
}

function layout(pin: PinDef): Geo {
  const n = pin.num;
  const gap = 1;
  if (n <= 16) {
    const i = n - 1;
    const y = M + i * pitch;
    return {
      pin,
      pad: { x: M - PAD, y, w: PAD, h: pitch - gap },
      label: { x: M + 8, y: y + pitch / 2 + 3.3, anchor: "start" },
      num: { x: M - PAD + 3, y: y + pitch / 2 + 3.3, anchor: "start" },
    };
  }
  if (n <= 32) {
    const i = n - 17;
    const x = M + i * pitch;
    return {
      pin,
      pad: { x, y: M + CHIP, w: pitch - gap, h: PAD },
      label: { x: x + pitch / 2, y: M + CHIP - 12, anchor: "middle" },
      num: { x: x + pitch / 2, y: M + CHIP + PAD - 4, anchor: "middle" },
    };
  }
  if (n <= 48) {
    const i = n - 33;
    const y = M + CHIP - (i + 1) * pitch;
    return {
      pin,
      pad: { x: M + CHIP, y, w: PAD, h: pitch - gap },
      label: { x: M + CHIP - 8, y: y + pitch / 2 + 3.3, anchor: "end" },
      num: { x: M + CHIP + PAD - 3, y: y + pitch / 2 + 3.3, anchor: "end" },
    };
  }
  const i = n - 49;
  const x = M + CHIP - (i + 1) * pitch;
  return {
    pin,
    pad: { x, y: M - PAD, w: pitch - gap, h: PAD },
    label: { x: x + pitch / 2, y: M + 16, anchor: "middle" },
    num: { x: x + pitch / 2, y: M - PAD + 11, anchor: "middle" },
  };
}

const geos = PINS.map(layout);
const svgSize = M * 2 + CHIP + PAD * 2;
const chipLabel = (p: PinDef) => p.fullName ?? p.name;

/* ------------------------------------------------------------- board lookups */
function boardCategory(cpin: ConnectorPin): string {
  if (cpin.chip) {
    const p = findPin(cpin.chip);
    if (p) return category(p);
  }
  if (cpin.kind === "power") return "power";
  if (cpin.kind === "nc") return "nc";
  if (cpin.kind === "ref") return "ref";
  return "io";
}

function boardUsed(cpin: ConnectorPin): boolean {
  if (cpin.chip) {
    const p = findPin(cpin.chip);
    if (p && usageFor(p).length) return true;
  }
  return false;
}

const getConn = (id: string): Connector => CONNECTORS.find((c) => c.id === id)!;
const boardGroups: { id: string; connectors: Connector[] }[] = [
  { id: "morpho-left", connectors: [getConn("CN7")] },
  { id: "arduino-left", connectors: [getConn("CN6"), getConn("CN8")] },
  { id: "center", connectors: [] },
  { id: "arduino-right", connectors: [getConn("CN5"), getConn("CN9")] },
  { id: "morpho-right", connectors: [getConn("CN10")] },
];
</script>

<template>
  <div class="pinout">
    <div class="controls">
      <div class="modes" role="tablist" aria-label="Pinout view">
        <button :class="{ active: mode === 'chip' }" role="tab" @click="mode = 'chip'">
          IC (chip)
        </button>
        <button :class="{ active: mode === 'board' }" role="tab" @click="mode = 'board'">
          Nucleo board
        </button>
      </div>

      <div class="filters">
        <label class="filter">
          <span>Peripheral</span>
          <select :value="selPeripheral" @change="onPeripheralChange">
            <option value="">All</option>
            <option v-for="p in peripheralNames" :key="p" :value="p">{{ p }}</option>
          </select>
        </label>
        <label class="filter">
          <span>Function</span>
          <select :value="selFunction" @change="onFunctionChange">
            <option value="">All</option>
            <option v-for="f in functionFamilies" :key="f.family" :value="f.family">
              {{ f.family }} ({{ f.count }})
            </option>
          </select>
        </label>
        <button v-if="hasFilter" class="clear" @click="clearFilter">Clear</button>
      </div>

      <div class="legend">
        <span class="sw sw-power"></span> Power
        <span class="sw sw-reset"></span> Reset
        <span class="sw sw-boot"></span> Boot
        <span class="sw sw-debug"></span> Debug
        <span class="sw sw-io"></span> Free I/O
        <span class="sw sw-used"></span> Used
      </div>
    </div>

    <div class="body">
      <div class="diagram">
        <!-- ============================== IC / CHIP ============================ -->
        <svg
          v-if="mode === 'chip'"
          class="chip-svg"
          :viewBox="`0 0 ${svgSize} ${svgSize}`"
          @mouseleave="setHover(null)"
        >
          <rect
            class="chip-body"
            :x="M"
            :y="M"
            :width="CHIP"
            :height="CHIP"
            rx="8"
          />
          <text class="chip-title" :x="M + CHIP / 2" :y="M + CHIP / 2 - 6" text-anchor="middle">
            STM32F446RE
          </text>
          <text class="chip-sub" :x="M + CHIP / 2" :y="M + CHIP / 2 + 14" text-anchor="middle">
            LQFP64
          </text>
          <circle class="pin1-dot" :cx="M" :cy="M" r="5" />

          <g
            v-for="g in geos"
            :key="g.pin.num"
            class="pin"
            :class="[`cat-${category(g.pin)}`, filterClass(g.pin)]"
            @mouseenter="setHover(g.pin)"
            @click="togglePin(g.pin)"
          >
            <rect
              class="pad"
              :x="g.pad.x"
              :y="g.pad.y"
              :width="g.pad.w"
              :height="g.pad.h"
              rx="1.5"
            />
            <text
              class="name"
              :class="{ active: activePin === g.pin }"
              :x="g.label.x"
              :y="g.label.y"
              :text-anchor="g.label.anchor"
            >
              {{ chipLabel(g.pin) }}
            </text>
            <text
              class="num"
              :x="g.num.x"
              :y="g.num.y"
              :text-anchor="g.num.anchor"
            >
              {{ g.pin.num }}
            </text>
          </g>
        </svg>

        <!-- ============================== NUCLEO BOARD ========================= -->
        <div v-else class="board">
          <template v-for="group in boardGroups" :key="group.id">
            <div v-if="group.id === 'center'" class="board-center">
              <div class="mini-chip">
                <span class="mini-chip-title">STM32F446RE</span>
                <span class="mini-chip-sub">LQFP64</span>
                <span class="mini-chip-sub">pin 1 ⦿</span>
              </div>
            </div>
            <div v-else class="board-group" :class="group.id">
              <div v-for="conn in group.connectors" :key="conn.id" class="conn">
                <div class="conn-title">{{ conn.title }}</div>
                <div class="conn-cols">
                  <div v-for="(col, ci) in conn.columns" :key="ci" class="conn-col">
                    <div
                      v-for="cpin in col"
                      :key="cpin.pos"
                      class="cpin"
                      :class="[`cat-${boardCategory(cpin)}`, { used: boardUsed(cpin) }, cpinFilterClass(cpin)]"
                      @mouseenter="setHover(cpin.chip ? findPin(cpin.chip) ?? null : null, { connId: conn.id, cpin })"
                      @mouseleave="setHover(null)"
                      @click="togglePin(cpin.chip ? findPin(cpin.chip) ?? null : null, { connId: conn.id, cpin })"
                    >
                      <span class="cpin-label">{{ cpin.label }}</span>
                      <span v-if="cpin.chip" class="cpin-chip">{{ cpin.chip }}</span>
                      <span v-else-if="cpin.func" class="cpin-func">{{ cpin.func }}</span>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </template>
        </div>
      </div>

      <!-- ============================== DETAIL PANEL =========================== -->
      <aside class="detail" :class="{ pinned: isPinned }">
        <template v-if="activePin">
          <header class="detail-head">
            <span class="pin-num">Pin {{ activePin.num }}</span>
            <h3>{{ activePin.name }}</h3>
            <span class="badge" :class="`cat-${category(activePin)}`">{{ activePin.type }}</span>
          </header>

          <dl>
            <div v-if="activeCtx" class="row">
              <dt>Board</dt>
              <dd>
                {{ activeCtx.connId }} pin {{ activeCtx.cpin.pos }} ·
                <strong>{{ activeCtx.cpin.label }}</strong>
              </dd>
            </div>
            <div class="row">
              <dt>Default</dt>
              <dd>{{ activePin.signal }}</dd>
            </div>
            <div class="row">
              <dt>I/O structure</dt>
              <dd>{{ activePin.io }} — {{ IO_STRUCTURE[activePin.io] ?? "" }}</dd>
            </div>
            <div v-if="activePin.alt.length" class="row">
              <dt>Alternate</dt>
              <dd class="chips">
                <span v-for="a in activePin.alt" :key="a" class="chip">{{ a }}</span>
              </dd>
            </div>
            <div v-if="activePin.extra.length" class="row">
              <dt>Additional</dt>
              <dd class="chips">
                <span v-for="a in activePin.extra" :key="a" class="chip extra">{{ a }}</span>
              </dd>
            </div>
            <div v-if="activePin.arduino.length" class="row">
              <dt>Arduino</dt>
              <dd class="chips">
                <span v-for="a in activePin.arduino" :key="a.pin" class="chip arduino">
                  {{ a.pin }}<template v-if="a.func"> · {{ a.func }}</template>
                </span>
              </dd>
            </div>
            <div v-if="activePin.morpho.length" class="row">
              <dt>Morpho</dt>
              <dd>{{ morphoLabel(activePin) }}</dd>
            </div>
            <div v-if="usageFor(activePin).length" class="row">
              <dt>Used as</dt>
              <dd class="used-list">
                <span v-for="u in usageFor(activePin)" :key="u.peripheral + u.role" class="used-tag">
                  {{ u.peripheral }} · {{ u.role }}
                </span>
              </dd>
            </div>
            <div v-if="activePin.note" class="row">
              <dt>Note</dt>
              <dd>{{ activePin.note }}</dd>
            </div>
          </dl>
        </template>

        <template v-else-if="activeCtx">
          <header class="detail-head">
            <h3>{{ activeCtx.cpin.label }}</h3>
            <span class="badge cat-power">Net</span>
          </header>
          <dl>
            <div class="row">
              <dt>Board</dt>
              <dd>{{ activeCtx.connId }} pin {{ activeCtx.cpin.pos }}</dd>
            </div>
            <div v-if="activeCtx.cpin.func" class="row">
              <dt>Function</dt>
              <dd>{{ activeCtx.cpin.func }}</dd>
            </div>
          </dl>
        </template>

        <!-- Empty state: show the pin assignment overview -->
        <div v-else class="detail-assign">
          <header class="detail-head">
            <h3>Pin assignment</h3>
          </header>
          <p class="hint">Hover a pin for details, click to keep it. Click a peripheral to highlight it.</p>

          <div v-if="assignments.length">
            <div v-for="a in assignments" :key="a.peripheral" class="assign-group">
              <button
                class="assign-head"
                :class="{ on: selPeripheral === a.peripheral }"
                @click="togglePeripheral(a.peripheral)"
              >
                <span class="assign-name">{{ a.peripheral }}</span>
                <span v-if="a.bus" class="assign-bus">{{ a.bus }}</span>
              </button>
              <ul class="assign-pins">
                <li v-for="p in a.pins" :key="p.pin + p.role">
                  <button class="assign-pin" @click="togglePin(findPin(p.pin) ?? null)">
                    <code>{{ p.pin }}</code> {{ p.role }}
                  </button>
                </li>
              </ul>
            </div>
          </div>
          <p v-else class="hint">No pin mapping supplied.</p>
        </div>
      </aside>
    </div>
  </div>
</template>

<style scoped>
.pinout {
  --power: #64748b;
  --reset: #ef4444;
  --boot: #a855f7;
  --debug: #f59e0b;
  --io: #94a3b8;
  --used: #10b981;
  --used-bg: rgba(16, 185, 129, 0.12);
  --filter: #38bdf8;
  border: 1px solid var(--vp-c-divider);
  border-radius: 10px;
  padding: 14px;
  background: var(--vp-c-bg-soft);
}

.controls {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  align-items: center;
  justify-content: space-between;
  margin-bottom: 12px;
}
.modes {
  display: inline-flex;
  gap: 4px;
  background: var(--vp-c-bg);
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  padding: 3px;
}
.modes button {
  border: 0;
  background: transparent;
  padding: 5px 12px;
  border-radius: 6px;
  cursor: pointer;
  color: var(--vp-c-text-2);
  font-size: 13px;
}
.modes button.active {
  background: var(--vp-c-brand);
  color: #fff;
}

.filters {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  align-items: center;
}
.filter {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
  color: var(--vp-c-text-2);
}
.filter select {
  font-size: 12px;
  padding: 3px 6px;
  border-radius: 6px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  max-width: 170px;
}
.clear {
  font-size: 12px;
  padding: 3px 10px;
  border-radius: 6px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg);
  color: var(--vp-c-text-2);
  cursor: pointer;
}
.clear:hover {
  color: var(--vp-c-brand);
  border-color: var(--vp-c-brand);
}

.legend {
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
  align-items: center;
  font-size: 12px;
  color: var(--vp-c-text-2);
}
.sw {
  width: 10px;
  height: 10px;
  border-radius: 2px;
  display: inline-block;
  margin-right: 3px;
  vertical-align: -1px;
}
.sw-power { background: var(--power); }
.sw-reset { background: var(--reset); }
.sw-boot { background: var(--boot); }
.sw-debug { background: var(--debug); }
.sw-io { background: var(--io); }
.sw-used { background: var(--used); }

.body {
  display: flex;
  gap: 14px;
  align-items: flex-start;
}
.diagram {
  flex: 1 1 auto;
  min-width: 0;
  overflow-x: auto;
}
.detail {
  flex: 0 0 300px;
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg);
  padding: 12px;
  position: sticky;
  top: 12px;
  max-height: 80vh;
  overflow-y: auto;
}
.detail.pinned {
  border-color: var(--used);
  box-shadow: 0 0 0 1px var(--used);
}
.detail-head {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
  margin-bottom: 8px;
}
.detail-head h3 {
  margin: 0;
  font-size: 18px;
}
.pin-num {
  font-size: 12px;
  color: var(--vp-c-text-2);
}
.badge {
  font-size: 11px;
  padding: 2px 7px;
  border-radius: 999px;
  color: #fff;
  background: var(--io);
}
.badge.cat-power { background: var(--power); }
.badge.cat-reset { background: var(--reset); }
.badge.cat-boot { background: var(--boot); }
.badge.cat-debug { background: var(--debug); }
.badge.cat-io { background: var(--io); }
.badge.cat-used { background: var(--used); }
dl { margin: 0; }
.row {
  display: grid;
  grid-template-columns: 96px 1fr;
  gap: 6px;
  padding: 6px 0;
  border-top: 1px solid var(--vp-c-divider);
  font-size: 13px;
}
.row dt {
  color: var(--vp-c-text-2);
  font-weight: 500;
}
.row dd { margin: 0; }
.chips { display: flex; flex-wrap: wrap; gap: 4px; }
.chip {
  font-size: 11px;
  padding: 1px 6px;
  border-radius: 4px;
  background: var(--vp-c-bg-soft);
  border: 1px solid var(--vp-c-divider);
  font-family: var(--vp-font-family-mono);
}
.chip.extra { border-color: var(--debug); color: var(--debug); }
.chip.arduino { border-color: var(--power); color: var(--power); }
.used-list { display: flex; flex-wrap: wrap; gap: 4px; }
.used-tag {
  font-size: 12px;
  padding: 2px 8px;
  border-radius: 5px;
  background: var(--used);
  color: #fff;
  font-weight: 600;
}

/* --------------------------------------------- assignment overview (empty) */
.detail-assign .hint { color: var(--vp-c-text-3); font-size: 12px; margin: 0 0 8px; }
.assign-group { border-top: 1px solid var(--vp-c-divider); }
.assign-head {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 8px;
  width: 100%;
  border: 0;
  background: transparent;
  padding: 7px 2px;
  cursor: pointer;
  text-align: left;
}
.assign-head:hover .assign-name { color: var(--vp-c-brand); }
.assign-head.on .assign-name { color: var(--filter); }
.assign-name { font-size: 13px; font-weight: 600; color: var(--vp-c-text-1); }
.assign-bus {
  font-size: 11px;
  font-family: var(--vp-font-family-mono);
  color: var(--vp-c-text-3);
}
.assign-pins {
  list-style: none;
  margin: 0 0 4px;
  padding: 0 0 4px 4px;
  display: flex;
  flex-direction: column;
  gap: 1px;
}
.assign-pin {
  display: flex;
  gap: 8px;
  align-items: baseline;
  border: 0;
  background: transparent;
  padding: 2px 4px;
  border-radius: 4px;
  cursor: pointer;
  text-align: left;
  font-size: 12px;
  color: var(--vp-c-text-2);
}
.assign-pin:hover {
  background: var(--vp-c-bg-soft);
  color: var(--vp-c-text-1);
}
.assign-pin code {
  font-family: var(--vp-font-family-mono);
  color: var(--vp-c-brand);
}

/* ------------------------------- chip svg ------------------------------- */
.chip-svg {
  width: 100%;
  max-width: 620px;
  display: block;
}
.chip-body {
  fill: #0f172a;
  stroke: #475569;
  stroke-width: 1.5;
}
.chip-title {
  fill: #e2e8f0;
  font-size: 22px;
  font-weight: 600;
  font-family: var(--vp-font-family-mono);
}
.chip-sub {
  fill: #94a3b8;
  font-size: 13px;
  font-family: var(--vp-font-family-mono);
}
.pin1-dot { fill: #e2e8f0; }
.pin .pad { cursor: pointer; stroke: #0f172a; stroke-width: 0.5; }
.pin .name {
  font-size: 9.5px;
  font-family: var(--vp-font-family-mono);
  fill: #cbd5e1;
  pointer-events: none;
}
.pin .num {
  font-size: 6.5px;
  font-family: var(--vp-font-family-mono);
  fill: #64748b;
  pointer-events: none;
}
.pin:hover .pad { filter: brightness(1.25); }
.pin .name.active { font-weight: 700; fill: #fff; }

.cat-power .pad { fill: var(--power); }
.cat-reset .pad { fill: var(--reset); }
.cat-boot .pad { fill: var(--boot); }
.cat-debug .pad { fill: var(--debug); }
.cat-io .pad { fill: var(--io); }
.cat-used .pad { fill: var(--used); }
.cat-used .name { fill: #052e1f; font-weight: 700; }

.pin.filter-dim { opacity: 0.16; }
.pin.filter-hit .pad {
  stroke: var(--filter);
  stroke-width: 2.5;
  filter: brightness(1.25);
}
.pin.filter-hit .name {
  fill: var(--filter);
  font-weight: 700;
}

/* ------------------------------- board ------------------------------- */
.board {
  display: flex;
  gap: 14px;
  align-items: stretch;
  justify-content: flex-start;
}
.board-group {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
.board-center {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 0 6px;
}
.mini-chip {
  border: 2px solid #475569;
  background: #0f172a;
  color: #e2e8f0;
  border-radius: 8px;
  width: 130px;
  height: 130px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 4px;
}
.mini-chip-title {
  font-family: var(--vp-font-family-mono);
  font-weight: 700;
  font-size: 13px;
}
.mini-chip-sub {
  font-family: var(--vp-font-family-mono);
  font-size: 10px;
  color: #94a3b8;
}
.conn {
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  background: var(--vp-c-bg);
  padding: 8px;
}
.conn-title {
  font-size: 12px;
  font-weight: 600;
  color: var(--vp-c-text-2);
  margin-bottom: 6px;
  white-space: nowrap;
}
.conn-cols {
  display: flex;
  gap: 8px;
}
.conn-col {
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.cpin {
  display: flex;
  gap: 6px;
  align-items: baseline;
  justify-content: space-between;
  padding: 2px 7px;
  border-radius: 4px;
  font-size: 12px;
  cursor: default;
  min-width: 84px;
  background: var(--vp-c-bg-soft);
  border: 1px solid transparent;
}
.cpin:hover { border-color: var(--vp-c-brand); }
.cpin-label {
  font-family: var(--vp-font-family-mono);
  font-weight: 600;
}
.cpin-chip, .cpin-func {
  font-family: var(--vp-font-family-mono);
  font-size: 10px;
  color: var(--vp-c-text-2);
}
.cpin.cat-power .cpin-label { color: var(--power); }
.cpin.cat-reset .cpin-label { color: var(--reset); }
.cpin.cat-boot .cpin-label { color: var(--boot); }
.cpin.cat-debug .cpin-label { color: var(--debug); }
.cpin.cat-nc .cpin-label { color: var(--vp-c-text-3); }
.cpin.cat-ref .cpin-label { color: var(--debug); }
.cpin.used {
  background: var(--used-bg);
  border-color: var(--used);
}
.cpin.used .cpin-chip { color: var(--used); font-weight: 700; }
.cpin.filter-dim { opacity: 0.2; }
.cpin.filter-hit {
  border-color: var(--filter);
  box-shadow: 0 0 0 1px var(--filter);
}
.cpin.filter-hit .cpin-chip { color: var(--filter); font-weight: 700; }

@media (max-width: 900px) {
  .body { flex-direction: column; }
  .detail { flex: 1 1 auto; width: 100%; position: static; max-height: none; }
}
</style>

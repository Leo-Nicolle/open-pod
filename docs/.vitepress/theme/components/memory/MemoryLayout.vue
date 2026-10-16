<template>
  <p v-if="!memory" class="mem-hint">{{ error ?? "Loading memory map…" }}</p>
  <div v-else class="mem viz-root">
    <div class="mem-bar-row">
      <div class="mem-tabs" role="tablist">
        <button
          v-for="m in memories"
          :key="m.key"
          role="tab"
          :aria-selected="m.key === memKey"
          :class="['mem-tab', { active: m.key === memKey }]"
          @click="switchMemory(m.key)"
        >
          {{ m.label }}
        </button>
      </div>
      <label class="mem-check">
        <input type="checkbox" v-model="exact" /> Exact bytes
      </label>
    </div>

    <p class="mem-sub">{{ memory.subtitle }}</p>

    <div class="mem-tiles">
      <div v-for="t in memory.tiles" :key="t.label" class="mem-tile">
        <span class="mem-tile-label">{{ t.label }}</span>
        <span class="mem-tile-value">{{ fmt(t.value) }}</span>
        <span class="mem-tile-note">{{ t.note }}</span>
      </div>
    </div>

    <!-- Breadcrumb: the memory, then each drilled-into item. -->
    <nav class="mem-crumbs" aria-label="Drill-down">
      <button class="mem-crumb" :disabled="!path.length" @click="path = []">{{ memory.label }}</button>
      <template v-for="(p, i) in path" :key="p.id">
        <span class="mem-crumb-sep">›</span>
        <button class="mem-crumb" :disabled="i === path.length - 1" @click="path = path.slice(0, i + 1)">
          {{ p.nickname }}
        </button>
      </template>
    </nav>

    <!-- The map: the whole memory, then one bar per drilled level. -->
    <div class="mem-map">
      <div v-for="(level, li) in levels" :key="level.id" class="mem-level">
        <div class="mem-level-head">
          <span>{{ level.title }}</span>
          <span class="mem-muted">{{ fmt(level.total) }}</span>
        </div>
        <div class="mem-strip" :class="{ top: li === 0 }">
          <button
            v-for="seg in level.segments"
            :key="seg.id"
            :class="[
              'mem-seg',
              'g-' + seg.group,
              {
                hot: hoverId === seg.id,
                picked: isOnPath(seg),
                dim: hoverId && hoverId !== seg.id && !isOnPath(seg),
              },
            ]"
            :style="{ flexGrow: seg.size }"
            :aria-label="`${seg.nickname}, ${fmt(seg.size)}`"
            @mouseenter="hover(seg, $event)"
            @mousemove="moveTip($event)"
            @mouseleave="unhover"
            @focus="hover(seg)"
            @blur="unhover"
            @click="pick(seg, li)"
          >
            <span v-if="seg.size / level.total > 0.07" class="mem-seg-label">
              {{ seg.nickname }}<small>{{ fmt(seg.size) }}</small>
            </span>
          </button>
        </div>
        <div v-if="li === 0" class="mem-axis">
          <span
            v-for="t in memory.ticks"
            :key="t.at"
            class="mem-tick"
            :style="{ left: (t.at / memory.total) * 100 + '%', transform: tickShift(t.at, memory.total) }"
          >
            {{ t.label }}
          </span>
        </div>
      </div>
    </div>

    <ul class="mem-legend">
      <li v-for="g in legendGroups" :key="g.key">
        <span :class="['mem-swatch', 'g-' + g.key]"></span>{{ g.label }}
        <span class="mem-muted">{{ fmt(g.size) }}</span>
      </li>
    </ul>

    <!-- Detail of the selected item. -->
    <section v-if="selected" class="mem-detail">
      <header>
        <span :class="['mem-swatch', 'g-' + selected.group]"></span>
        <h3>{{ selected.nickname }}</h3>
        <span class="mem-detail-size">{{ fmt(selected.size) }}</span>
        <span class="mem-muted">{{ pct(selected.size, memory.total) }} of {{ memory.label }}</span>
        <button class="mem-close" aria-label="Close" @click="path = path.slice(0, -1)">×</button>
      </header>
      <p>{{ selected.description }}</p>
      <dl>
        <template v-if="selected.address">
          <dt>Address</dt>
          <dd><code>{{ selected.address }}</code></dd>
        </template>
        <dt>Variable</dt>
        <dd><code>{{ selected.variable }}</code></dd>
        <dt>File</dt>
        <dd>
          <a v-if="link(selected.file)" :href="link(selected.file)!" target="_blank" rel="noopener"><code>{{ selected.file }}</code></a>
          <code v-else>{{ selected.file }}</code>
        </dd>
        <dt>Usage</dt>
        <dd>{{ selected.usage }}</dd>
        <template v-if="selected.children">
          <dt>Breakdown</dt>
          <dd>{{ selected.children.length }} parts, shown in the bar above.</dd>
        </template>
      </dl>
    </section>
    <p v-else class="mem-hint">Hover a block for its size, click it for details and its breakdown.</p>

    <aside v-if="memory.notes.length" class="mem-notes">
      <strong>Findings</strong>
      <ul>
        <li v-for="n in memory.notes" :key="n">{{ n }}</li>
      </ul>
    </aside>

    <!-- Every allocation, sortable and filterable. -->
    <div class="mem-table-bar">
      <h3>All allocations</h3>
      <input v-model="query" class="mem-filter" type="search" placeholder="Filter by name, variable, file…" />
    </div>
    <div class="mem-table-wrap">
      <table class="mem-table">
        <thead>
          <tr>
            <th v-for="c in columns" :key="c.key" :class="{ num: c.num }">
              <button class="mem-sort" @click="sortBy(c.key)">
                {{ c.label }}<span class="mem-sort-mark">{{ sortKey === c.key ? (sortDesc ? "▼" : "▲") : "" }}</span>
              </button>
            </th>
          </tr>
        </thead>
        <tbody>
          <template v-for="row in rows" :key="row.item.id">
            <tr
              :class="['mem-row', { child: row.depth > 0, hot: hoverId === row.item.id || hoverId === row.barId, picked: selected?.id === row.item.id }]"
              @mouseenter="hoverId = row.barId"
              @mouseleave="hoverId = null"
              @click="selectRow(row)"
            >
              <td>
                <span class="mem-name" :style="{ paddingLeft: row.depth * 20 + 'px' }">
                  <button
                    v-if="row.item.children && !query"
                    class="mem-expand"
                    :aria-expanded="open.has(row.item.id)"
                    @click.stop="toggle(row.item.id)"
                  >{{ open.has(row.item.id) ? "▾" : "▸" }}</button>
                  <span v-else class="mem-expand-pad"></span>
                  <span :class="['mem-swatch', 'g-' + row.item.group]"></span>
                  {{ row.item.nickname }}
                </span>
              </td>
              <td class="num">{{ fmt(row.item.size) }}</td>
              <td class="num">
                <span class="mem-pct">
                  <span class="mem-pct-bar" :style="{ width: Math.max(1, (row.item.size / memory.total) * 100) + '%' }"></span>
                  {{ pct(row.item.size, memory.total) }}
                </span>
              </td>
              <td><code>{{ row.item.variable }}</code></td>
              <td class="mem-file">
                <a v-if="link(row.item.file)" :href="link(row.item.file)!" target="_blank" rel="noopener" @click.stop>{{ shortFile(row.item.file) }}</a>
                <span v-else>{{ shortFile(row.item.file) }}</span>
              </td>
              <td class="mem-usage">{{ row.item.usage }}</td>
            </tr>
          </template>
          <tr v-if="!rows.length">
            <td colspan="6" class="mem-muted">Nothing matches “{{ query }}”.</td>
          </tr>
        </tbody>
      </table>
    </div>

    <p class="mem-source">
      Measured {{ data.generated }} on <code>{{ data.target }}</code>. Source:
      <a :href="withBase('/RAM.json')" target="_blank" rel="noopener"><code>RAM.json</code></a>.
      {{ data.method }}
    </p>

    <div v-if="tip" class="mem-tip" :style="{ left: tip.x + 'px', top: tip.y + 'px' }" role="tooltip">
      <div class="mem-tip-head">
        <span :class="['mem-swatch', 'g-' + tip.seg.group]"></span>
        <strong>{{ tip.seg.nickname }}</strong>
      </div>
      <div class="mem-tip-size">{{ fmt(tip.seg.size) }} <span class="mem-muted">· {{ pct(tip.seg.size, memory.total) }}</span></div>
      <div v-if="tip.seg.address" class="mem-muted"><code>{{ tip.seg.address }}</code></div>
      <div class="mem-muted">{{ tip.seg.children ? "Click to see its parts" : "Click for details" }}</div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, onMounted, reactive, ref, shallowRef } from "vue";
import { withBase } from "vitepress";

type Raw = {
  nickname: string;
  variable: string;
  file: string;
  size: number;
  usage: string;
  description: string;
  address?: string;
  children?: Raw[];
};

type Item = Raw & { id: string; group: GroupKey; children?: Item[] };

type GroupKey = "audio" | "display" | "state" | "library" | "io" | "system" | "podcast" | "spare";

const GROUPS: Record<GroupKey, string> = {
  audio: "Audio",
  display: "Display & UI",
  state: "App state & search",
  library: "Library index",
  io: "Bluetooth & I/O",
  system: "Runtime & debug",
  podcast: "Podcasts",
  spare: "Free / unused",
};

// Which group each top-level allocation belongs to, by variable or nickname.
const GROUP_OF: Record<string, GroupKey> = {
  player: "audio",
  "Feeder timer": "audio",
  g_buffers: "display",
  fontRenderer: "display",
  ui: "display",
  albumArt: "display",
  "List row cache": "display",
  "Animation slots": "display",
  state: "state",
  "Static menus": "state",
  scrollSpeed: "state",
  "BtModel::toast": "state",
  podcastMenu: "podcast",
  musicIndex: "library",
  pageStore: "library",
  musicLookup: "library",
  bluetooth: "io",
  btModel: "io",
  btMenu: "io",
  Serial2: "io",
  wheel: "io",
  Wire: "io",
  Wire3: "io",
  SPI: "io",
  "PSRAM SPI": "io",
  line: "io",
  "Audio ring": "audio",
  "Next-track prefetch": "audio",
  "Cover scratch": "spare",
  "Podcast listening state": "podcast",
  "Index page cache": "library",
};

const repo = "https://github.com/Leo-Nicolle/open-pod/blob/master/";

function build(raw: Raw[], prefix: string, group?: GroupKey): Item[] {
  return raw.map((r, i) => {
    const id = `${prefix}/${i}`;
    const g = group ?? GROUP_OF[r.nickname] ?? GROUP_OF[r.variable] ?? "system";
    return { ...r, id, group: g, children: r.children ? build(r.children, id, g) : undefined };
  });
}

// RAM.json lives in docs/public/, so it is fetched at runtime rather than imported.
const data = shallowRef<any>(null);
const error = ref<string | null>(null);

onMounted(async () => {
  try {
    const res = await fetch(withBase("/RAM.json"));
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    data.value = await res.json();
  } catch (e) {
    error.value = `Could not load RAM.json (${(e as Error).message}).`;
  }
});

// The RAM bar groups allocations by kind (largest first within a group);
// the PSRAM bar keeps address order.
const ORDER: GroupKey[] = ["audio", "display", "state", "podcast", "library", "io", "system", "spare"];
const KB = 1024;

function buildMemories(data: any) {
const ram = data.ram;
const ramItems: Item[] = [
  ...build(ram.static as Raw[], "ram/static"),
  ...build(ram.heap as Raw[], "ram/heap"),
  {
    id: "ram/free",
    nickname: "Stack + free",
    variable: "-",
    file: "linker script (_estack)",
    size: ram.sections.left_for_stack_estimated,
    usage: "main stack, grows down from the top of RAM",
    description:
      "What's left once static data and the heap are allocated. The stack lives here; its high-water mark is reported by the firmware (app/stack_probe.cpp: [prof] line, bench stats).",
    group: "spare",
  },
];
const psramItems: Item[] = build(data.psram.regions as Raw[], "psram");

const ramOrdered = [...ramItems].sort(
  (a, b) => ORDER.indexOf(a.group) - ORDER.indexOf(b.group) || b.size - a.size
);

return [
  {
    key: "ram",
    label: "RAM",
    subtitle: "STM32F446RE internal SRAM: static data, heap and the stack share 128 KB.",
    total: ram.total,
    items: ramOrdered,
    notes: ram.notes,
    tiles: [
      { label: "Total", value: ram.total, note: "internal SRAM" },
      { label: "Static", value: ram.sections.static_total, note: `.data + .bss · ${pctN(ram.sections.static_total, ram.total)}` },
      { label: "Heap", value: ram.sections.heap_estimated, note: "allocated once at boot" },
      { label: "Left for the stack", value: ram.sections.left_for_stack_estimated, note: "high-water mark: [prof] / bench stats" },
    ],
    ticks: [0, 32, 64, 96, 128].map((k) => ({ at: k * KB, label: `${k} KB` })),
  },
  {
    key: "psram",
    label: "PSRAM",
    subtitle: `${data.psram.chip}, on SPI2. Fixed regions, addresses from PSRAM_controller.hpp.`,
    total: data.psram.total,
    items: psramItems,
    notes: [
      "The 1 MB cover region is only used by the PSRAM self-test and the benchmarks: AlbumArt reads covers straight from the SD card, so it can be reclaimed.",
      "The index page cache is full: 1,966,080 B / 4 KB = 480 slots of PageStore's 512.",
    ],
    tiles: [
      { label: "Total", value: data.psram.total, note: "8 MB chip" },
      { label: "Audio", value: sumGroup(psramItems, "audio"), note: "ring + next-track prefetch" },
      { label: "Index + podcasts", value: sumGroup(psramItems, "library") + sumGroup(psramItems, "podcast"), note: "page cache + listening state" },
      { label: "Unused in production", value: sumGroup(psramItems, "spare"), note: "cover scratch" },
    ],
    ticks: spaced([...psramItems.map((r) => parseInt(r.address ?? "0", 16)), data.psram.total], data.psram.total).map((at) => ({
      at,
      label: "0x" + at.toString(16).toUpperCase().padStart(6, "0"),
    })),
  },
];
}

const memories = computed(() => (data.value ? buildMemories(data.value) : []));

// Region starts closer than 8% of the bar would overlap: keep the first.
function spaced(ats: number[], total: number) {
  const out: number[] = [];
  for (const a of ats) if (!out.length || (a - out[out.length - 1]) / total > 0.08) out.push(a);
  return out;
}

function tickShift(at: number, total: number) {
  return at === 0 ? "none" : at >= total ? "translateX(-100%)" : "translateX(-50%)";
}

function sumGroup(items: Item[], g: GroupKey) {
  return items.filter((i) => i.group === g).reduce((s, i) => s + i.size, 0);
}

const memKey = ref<"ram" | "psram">("ram");
const memory = computed(() => memories.value.find((m) => m.key === memKey.value));
const exact = ref(false);
const path = ref<Item[]>([]);
const hoverId = ref<string | null>(null);
const tip = ref<{ seg: Item; x: number; y: number } | null>(null);
const query = ref("");
const open = reactive(new Set<string>());
const sortKey = ref<string>("size");
const sortDesc = ref(true);

function switchMemory(k: "ram" | "psram") {
  memKey.value = k;
  path.value = [];
  query.value = "";
  open.clear();
}

// Blocks narrower than this share of their bar are folded into one
// "smaller items" block, which drills down into them.
const MIN_SHARE = 0.012;

function segmentsOf(items: Item[], total: number, prefix: string): Item[] {
  const out: Item[] = [];
  let small: Item[] = [];
  const flush = () => {
    if (!small.length) return;
    if (small.length === 1) out.push(small[0]);
    else {
      const size = small.reduce((s, i) => s + i.size, 0);
      out.push({
        id: `${prefix}/small-${out.length}`,
        nickname: `${small.length} smaller items`,
        variable: small.map((s) => s.variable).join(", "),
        file: "various",
        size,
        usage: "-",
        description: `${small.length} allocations too small to draw at this scale: ${small.map((s) => s.nickname).join(", ")}.`,
        group: small[0].group,
        children: small,
      });
    }
    small = [];
  };
  for (const it of items) {
    if (it.size / total < MIN_SHARE && (!small.length || small[0].group === it.group)) small.push(it);
    else {
      flush();
      if (it.size / total < MIN_SHARE) small.push(it);
      else out.push(it);
    }
  }
  flush();
  return out;
}

const levels = computed(() => {
  const m = memory.value!;
  const out = [{ id: m.key, title: `${m.label} map`, total: m.total, segments: segmentsOf(m.items as Item[], m.total, m.key) }];
  for (const p of path.value) {
    if (!p.children) break;
    out.push({ id: p.id + "/lvl", title: `Inside ${p.nickname}`, total: p.size, segments: segmentsOf(p.children, p.size, p.id) });
  }
  return out;
});

const selected = computed(() => path.value[path.value.length - 1] ?? null);

const legendGroups = computed(() =>
  ORDER.map((g) => ({ key: g, label: GROUPS[g], size: sumGroup(memory.value!.items as Item[], g) })).filter((g) => g.size > 0)
);

function isOnPath(seg: Item) {
  return path.value.some((p) => p.id === seg.id);
}

function pick(seg: Item, levelIndex: number) {
  const base = path.value.slice(0, levelIndex);
  path.value = path.value[levelIndex]?.id === seg.id ? base : [...base, seg];
}

function hover(seg: Item, e?: MouseEvent) {
  hoverId.value = seg.id;
  if (e) tip.value = { seg, ...tipPos(e) };
}
function moveTip(e: MouseEvent) {
  if (tip.value) tip.value = { ...tip.value, ...tipPos(e) };
}
function unhover() {
  hoverId.value = null;
  tip.value = null;
}
function tipPos(e: MouseEvent) {
  const w = 240;
  const x = e.clientX + 14 + w > window.innerWidth ? e.clientX - w - 14 : e.clientX + 14;
  return { x, y: e.clientY + 16 };
}

// Table
const columns = [
  { key: "nickname", label: "Allocation" },
  { key: "size", label: "Size", num: true },
  { key: "share", label: "Share", num: true },
  { key: "variable", label: "Variable" },
  { key: "file", label: "File" },
  { key: "usage", label: "Usage" },
];

function sortBy(k: string) {
  if (sortKey.value === k) sortDesc.value = !sortDesc.value;
  else {
    sortKey.value = k;
    sortDesc.value = k === "size" || k === "share";
  }
}

function compare(a: Item, b: Item) {
  const k = sortKey.value === "share" ? "size" : sortKey.value;
  const va = (a as any)[k];
  const vb = (b as any)[k];
  const c = typeof va === "number" ? va - vb : String(va).localeCompare(String(vb));
  return sortDesc.value ? -c : c;
}

type Row = { item: Item; depth: number; barId: string; parent?: Item };

const rows = computed<Row[]>(() => {
  const items = memory.value!.items as Item[];
  const q = query.value.trim().toLowerCase();
  const out: Row[] = [];
  if (q) {
    const hit = (i: Item) =>
      [i.nickname, i.variable, i.file, i.description, i.usage].some((s) => s.toLowerCase().includes(q));
    const all: Row[] = [];
    for (const it of items) {
      if (hit(it)) all.push({ item: it, depth: 0, barId: it.id });
      for (const c of it.children ?? []) if (hit(c)) all.push({ item: c, depth: 0, barId: it.id, parent: it });
    }
    return all.sort((a, b) => compare(a.item, b.item));
  }
  for (const it of [...items].sort(compare)) {
    out.push({ item: it, depth: 0, barId: it.id });
    if (it.children && open.has(it.id))
      for (const c of [...it.children].sort(compare)) out.push({ item: c, depth: 1, barId: c.id, parent: it });
  }
  return out;
});

function toggle(id: string) {
  if (open.has(id)) open.delete(id);
  else open.add(id);
}

function selectRow(row: Row) {
  path.value = row.parent ? [row.parent, row.item] : [row.item];
  if (row.parent) open.add(row.parent.id);
}

// Formatting
function fmt(n: number) {
  if (exact.value) return `${n.toLocaleString("en-US")} B`;
  if (n >= 1024 * 1024) return `${+(n / 1024 / 1024).toFixed(2)} MB`;
  if (n >= 1024) return `${+(n / 1024).toFixed(1)} KB`;
  return `${n} B`;
}
function pctN(n: number, total: number) {
  const p = (n / total) * 100;
  return `${p >= 10 ? p.toFixed(0) : p >= 1 ? p.toFixed(1) : p.toFixed(2)}%`;
}
const pct = pctN;

function link(file: string) {
  const m = /^(software\/[^\s,:]+)(?::(\d+))?$/.exec(file);
  if (!m) return null;
  return repo + m[1] + (m[2] ? `#L${m[2]}` : "");
}
function shortFile(file: string) {
  return file.replace(/^software\/src\//, "").replace(/^~\/\.platformio\/packages\/framework-arduinoststm32\//, "framework/");
}
</script>

<style scoped>
.mem {
  --series-1: #2a78d6;
  --series-2: #eb6834;
  --series-3: #1baf7a;
  --series-4: #eda100;
  --series-5: #e87ba4;
  --series-6: #008300;
  --series-7: #6250d6;
  --spare: #b9b8b2;
  --spare-line: #8f8e88;
  --gap: var(--vp-c-bg);
  position: relative;
  margin-top: 24px;
  font-variant-numeric: tabular-nums;
}
.dark .mem {
  --series-1: #3987e5;
  --series-2: #d95926;
  --series-3: #199e70;
  --series-4: #c98500;
  --series-5: #d55181;
  --series-6: #008300;
  --series-7: #9085e9;
  --spare: #4a4a47;
  --spare-line: #6d6c68;
}

.g-audio { --c: var(--series-1); }
.g-display { --c: var(--series-2); }
.g-state { --c: var(--series-3); }
.g-library { --c: var(--series-4); }
.g-io { --c: var(--series-5); }
.g-system { --c: var(--series-6); }
.g-podcast { --c: var(--series-7); }
.g-spare {
  --c: var(--spare);
  background-image: repeating-linear-gradient(45deg, transparent 0 5px, var(--spare-line) 5px 6px) !important;
}

.mem-muted { color: var(--vp-c-text-3); }
.mem code { font-size: 0.85em; }

/* Top bar */
.mem-bar-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
}
.mem-tabs {
  display: inline-flex;
  padding: 3px;
  border-radius: 10px;
  background: var(--vp-c-bg-soft);
  border: 1px solid var(--vp-c-divider);
}
.mem-tab {
  padding: 6px 18px;
  border-radius: 7px;
  font-weight: 600;
  color: var(--vp-c-text-2);
  transition: background 0.15s, color 0.15s;
}
.mem-tab.active {
  background: var(--vp-c-bg);
  color: var(--vp-c-text-1);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.12);
}
.mem-check {
  display: inline-flex;
  gap: 6px;
  align-items: center;
  font-size: 14px;
  color: var(--vp-c-text-2);
  cursor: pointer;
}
.mem-sub {
  margin: 10px 0 16px;
  color: var(--vp-c-text-2);
  font-size: 14px;
}

/* Tiles */
.mem-tiles {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  gap: 10px;
  margin-bottom: 20px;
}
.mem-tile {
  display: flex;
  flex-direction: column;
  gap: 2px;
  padding: 12px 14px;
  border-radius: 10px;
  background: var(--vp-c-bg-soft);
}
.mem-tile-label { font-size: 13px; color: var(--vp-c-text-2); }
.mem-tile-value { font-size: 24px; font-weight: 600; color: var(--vp-c-text-1); line-height: 1.3; }
.mem-tile-note { font-size: 12px; color: var(--vp-c-text-3); }

/* Breadcrumb */
.mem-crumbs {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 4px;
  font-size: 14px;
  margin-bottom: 8px;
}
.mem-crumb {
  padding: 2px 6px;
  border-radius: 6px;
  color: var(--vp-c-brand-1);
  font-weight: 500;
}
.mem-crumb:hover:not(:disabled) { background: var(--vp-c-bg-soft); }
.mem-crumb:disabled { color: var(--vp-c-text-1); cursor: default; }
.mem-crumb-sep { color: var(--vp-c-text-3); }

/* Map */
.mem-level { margin-bottom: 14px; }
.mem-level-head {
  display: flex;
  justify-content: space-between;
  font-size: 13px;
  color: var(--vp-c-text-2);
  margin-bottom: 4px;
}
.mem-strip {
  display: flex;
  gap: 2px;
  height: 56px;
  animation: mem-in 0.25s ease-out;
}
.mem-strip.top { height: 72px; }
@keyframes mem-in {
  from { opacity: 0; transform: translateY(-4px); }
}
.mem-seg {
  flex-basis: 0;
  min-width: 3px;
  position: relative;
  overflow: hidden;
  background-color: var(--c);
  border-radius: 2px;
  transition: opacity 0.15s, filter 0.15s, transform 0.15s;
  text-align: left;
  cursor: pointer;
}
.mem-seg:first-child { border-radius: 4px 2px 2px 4px; }
.mem-seg:last-child { border-radius: 2px 4px 4px 2px; }
.mem-seg.dim { opacity: 0.45; }
.mem-seg.hot { filter: brightness(1.08); transform: translateY(-2px); }
.mem-seg.picked { box-shadow: inset 0 0 0 2px var(--vp-c-text-1); }
.mem-seg:focus-visible { outline: 2px solid var(--vp-c-brand-1); outline-offset: 2px; }
.mem-seg-label {
  position: absolute;
  left: 8px;
  right: 6px;
  top: 7px;
  font-size: 12px;
  font-weight: 600;
  line-height: 1.25;
  color: #fff;
  text-shadow: 0 1px 2px rgba(0, 0, 0, 0.35);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.mem-seg-label small { display: block; font-weight: 500; opacity: 0.9; }
.g-spare .mem-seg-label { color: var(--vp-c-text-1); text-shadow: none; }

.mem-axis {
  position: relative;
  height: 18px;
  margin-top: 4px;
  border-top: 1px solid var(--vp-c-divider);
}
.mem-tick {
  position: absolute;
  top: 2px;
  font-size: 11px;
  font-family: "IBM Plex Mono", monospace;
  color: var(--vp-c-text-3);
  white-space: nowrap;
}

/* Legend */
.mem-legend {
  display: flex;
  flex-wrap: wrap;
  gap: 6px 18px;
  list-style: none;
  padding: 0 !important;
  margin: 8px 0 18px !important;
  font-size: 13px;
  color: var(--vp-c-text-2);
}
.mem-legend li { display: inline-flex; align-items: center; gap: 6px; margin: 0 !important; }
.mem-swatch {
  display: inline-block;
  flex: none;
  width: 12px;
  height: 12px;
  border-radius: 3px;
  background-color: var(--c);
}

/* Detail */
.mem-detail {
  padding: 16px 18px;
  border-radius: 12px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg-soft);
  animation: mem-in 0.2s ease-out;
}
.mem-detail header {
  display: flex;
  align-items: baseline;
  gap: 10px;
  flex-wrap: wrap;
}
.mem-detail header .mem-swatch { align-self: center; }
.mem-detail h3 { margin: 0 !important; padding: 0; border: 0; font-size: 18px; }
.mem-detail-size { font-weight: 600; font-size: 16px; }
.mem-close {
  margin-left: auto;
  font-size: 20px;
  line-height: 1;
  color: var(--vp-c-text-3);
}
.mem-close:hover { color: var(--vp-c-text-1); }
.mem-detail p { margin: 8px 0 10px; }
.mem-detail dl {
  display: grid;
  grid-template-columns: max-content 1fr;
  gap: 4px 14px;
  margin: 0;
  font-size: 14px;
}
.mem-detail dt { color: var(--vp-c-text-2); }
.mem-detail dd { margin: 0; overflow-wrap: anywhere; }
.mem-hint {
  padding: 14px 18px;
  border-radius: 12px;
  border: 1px dashed var(--vp-c-divider);
  color: var(--vp-c-text-2);
  font-size: 14px;
}

.mem-notes {
  margin-top: 16px;
  padding: 12px 16px;
  border-left: 3px solid var(--vp-c-brand-1);
  background: var(--vp-c-bg-soft);
  border-radius: 0 10px 10px 0;
  font-size: 14px;
}
.mem-notes ul { margin: 6px 0 0 !important; }

/* Table */
.mem-table-bar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 12px;
  flex-wrap: wrap;
  margin-top: 28px;
}
.mem-table-bar h3 { margin: 0 !important; }
.mem-filter {
  flex: 1 1 220px;
  max-width: 320px;
  padding: 6px 10px;
  border-radius: 8px;
  border: 1px solid var(--vp-c-divider);
  background: var(--vp-c-bg);
  font-size: 14px;
}
.mem-filter:focus { border-color: var(--vp-c-brand-1); outline: none; }
.mem-table-wrap { overflow-x: auto; margin-top: 10px; }
.mem-table {
  display: table;
  width: 100%;
  margin: 0 !important;
  border-collapse: collapse;
  font-size: 13px;
}
.mem-table th,
.mem-table td {
  border: 0;
  border-bottom: 1px solid var(--vp-c-divider);
  padding: 7px 10px;
  vertical-align: top;
}
.mem-table tr { background: transparent !important; }
.mem-table th { white-space: nowrap; }
.mem-table .num { text-align: right; white-space: nowrap; }
.mem-sort {
  font-weight: 600;
  color: var(--vp-c-text-2);
}
.mem-sort-mark { display: inline-block; width: 14px; font-size: 10px; }
.mem-row { cursor: pointer; transition: background 0.1s; }
.mem-row.hot td { background: var(--vp-c-bg-soft); }
.mem-row.picked td { background: var(--vp-c-default-soft); }
.mem-row.child td { color: var(--vp-c-text-2); }
.mem-name {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  white-space: nowrap;
  font-weight: 500;
}
.mem-expand,
.mem-expand-pad {
  width: 16px;
  flex: none;
  color: var(--vp-c-text-3);
}
.mem-pct {
  display: inline-flex;
  align-items: center;
  justify-content: flex-end;
  gap: 6px;
  min-width: 90px;
}
.mem-pct-bar {
  display: inline-block;
  max-width: 44px;
  height: 6px;
  border-radius: 3px;
  background: var(--vp-c-text-3);
  opacity: 0.5;
}
.mem-file { white-space: nowrap; font-family: "IBM Plex Mono", monospace; font-size: 12px; }
.mem-usage { min-width: 180px; color: var(--vp-c-text-2); }
.mem-source { margin-top: 18px; font-size: 12px; color: var(--vp-c-text-3); }

/* Tooltip */
.mem-tip {
  position: fixed;
  z-index: 50;
  width: 240px;
  padding: 10px 12px;
  border-radius: 10px;
  background: var(--vp-c-bg-elv);
  border: 1px solid var(--vp-c-divider);
  box-shadow: 0 6px 24px rgba(0, 0, 0, 0.18);
  font-size: 13px;
  pointer-events: none;
}
.mem-tip-head { display: flex; align-items: center; gap: 6px; }
.mem-tip-size { font-size: 16px; font-weight: 600; margin: 2px 0; }

@media (max-width: 640px) {
  .mem-strip.top { height: 56px; }
  .mem-seg-label { font-size: 11px; }
  .mem-tile-value { font-size: 20px; }
}
</style>

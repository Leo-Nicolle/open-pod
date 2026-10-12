<script setup lang="ts">
import { computed } from "vue";
import { PACKAGES } from "./pins";
import { usePinout, type ColorMode, type Naming, type View } from "./usePinout";

const s = usePinout();

const viewTabs = computed(() =>
  (
    [
      ["board", "Nucleo board"],
      ["chip", "Chip"],
      ["table", "Table"],
    ] as [View, string][]
  ).filter(([id]) => id !== "board" || !!s.pkgInfo.value.board)
);
const nameTabs: [Naming, string][] = [
  ["signal", "Signal"],
  ["mcu", "MCU"],
  ["board", "Board"],
];
const colorTabs: [ColorMode, string][] = [
  ["peripheral", "Peripheral"],
  ["category", "Type"],
  ["conflict", "Checks"],
];
</script>

<template>
  <div class="toolbar">
    <div class="seg" role="tablist" aria-label="Package">
      <button
        v-for="p in PACKAGES"
        :key="p.id"
        type="button"
        :class="{ on: s.pkg.value === p.id }"
        @click="s.pkg.value = p.id"
      >
        {{ p.label }}
      </button>
    </div>
    <div class="seg" role="tablist" aria-label="View">
      <button
        v-for="[id, label] in viewTabs"
        :key="id"
        type="button"
        :class="{ on: s.view.value === id }"
        @click="s.view.value = id"
      >
        {{ label }}
      </button>
    </div>
    <label class="group">
      Label
      <span class="seg">
        <button
          v-for="[id, label] in nameTabs"
          :key="id"
          type="button"
          :class="{ on: s.naming.value === id }"
          @click="s.naming.value = id"
        >
          {{ label }}
        </button>
      </span>
    </label>
    <label class="group">
      Colour
      <span class="seg">
        <button
          v-for="[id, label] in colorTabs"
          :key="id"
          type="button"
          :class="{ on: s.color.value === id }"
          @click="s.color.value = id"
        >
          {{ label }}
        </button>
      </span>
    </label>
    <input v-model="s.q.value" class="search" type="search" placeholder="Search pin, signal, macro…" />
    <label class="group">
      Function
      <select v-model="s.fn.value">
        <option value="">All</option>
        <option v-for="o in s.fnOptions.value" :key="o.id" :value="o.id">{{ o.label }}</option>
      </select>
    </label>
    <button
      type="button"
      class="btn"
      :class="{ flip: s.flip.value }"
      title="Mirror the layout, as seen from the solder side"
      @click="s.flip.value = !s.flip.value"
    >
      {{ s.flip.value ? "Bottom view" : "Top view" }}
    </button>
    <button type="button" class="btn" :class="{ active: !!s.matchSetVal.value }" @click="s.onClear()">Clear</button>
  </div>

  <div class="filters">
    <span class="filters-title">Peripherals</span>
    <button
      v-for="c in s.periphChips.value"
      :key="c.name"
      type="button"
      class="chip"
      :class="{ on: c.active }"
      :style="{ '--c': c.color }"
      @click="s.onPeriphToggle(c.name)"
      @mouseenter="s.hoverPeriph.value = c.name"
      @mouseleave="s.hoverPeriph.value = null"
    >
      <span class="swatch"></span>{{ c.name }}
      <span class="count">{{ c.count }}</span>
    </button>
    <span class="sep"></span>
    <button
      v-for="l in s.legendItems.value"
      :key="l.id"
      type="button"
      class="chip"
      :class="{ on: l.active }"
      :style="{ '--c': l.color }"
      @mouseenter="s.onLegendEnter(l.id)"
      @mouseleave="s.onLegendLeave()"
      @click="s.onLegendClick(l.id)"
    >
      <span class="swatch"></span>{{ l.label }}
      <span class="count">{{ l.count }}</span>
    </button>
  </div>
</template>

<style scoped>
.toolbar {
  display: flex;
  flex-wrap: wrap;
  gap: 10px 14px;
  align-items: center;
  padding: 10px 12px;
  background: var(--pv-panel-2);
  border: 1px solid var(--pv-border);
  border-radius: 10px;
}
.group {
  display: flex;
  align-items: center;
  gap: 7px;
  font-size: 11px;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--pv-muted);
}
.seg {
  display: flex;
  flex-wrap: wrap;
  gap: 2px;
  padding: 2px;
  background: var(--pv-bg);
  border-radius: 7px;
}
.seg button {
  padding: 6px 11px;
  border-radius: 5px;
  font-size: 12px;
  color: var(--pv-text-2);
  white-space: nowrap;
  text-transform: none;
  letter-spacing: 0;
}
.seg button.on {
  background: var(--pv-accent);
  color: var(--pv-on-accent);
  font-weight: 600;
}
.search {
  flex: 1 1 180px;
  min-width: 0;
  padding: 7px 10px;
  background: var(--pv-bg);
  border: 1px solid var(--pv-border);
  border-radius: 7px;
  color: var(--pv-text);
  font-family: var(--pv-mono);
  font-size: 12px;
}
select {
  padding: 7px 8px;
  background: var(--pv-bg);
  border: 1px solid var(--pv-border);
  border-radius: 7px;
  color: var(--pv-text);
  font-family: var(--pv-mono);
  font-size: 12px;
  text-transform: none;
  letter-spacing: 0;
}
.btn {
  padding: 7px 12px;
  border-radius: 7px;
  font-size: 12px;
  background: var(--pv-panel-2);
  border: 1px solid var(--pv-border);
  color: var(--pv-text-2);
}
.btn.flip {
  background: color-mix(in oklab, oklch(0.72 0.13 310) 26%, var(--pv-bg));
  border-color: color-mix(in oklab, oklch(0.72 0.13 310) 70%, var(--pv-bg));
  color: var(--pv-text);
}
.btn.active {
  background: var(--pv-accent);
  border-color: transparent;
  color: var(--pv-on-accent);
  font-weight: 600;
}

.filters {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
  align-items: center;
}
.filters-title {
  margin-right: 4px;
  font-family: var(--pv-mono);
  font-size: 10px;
  letter-spacing: 0.12em;
  text-transform: uppercase;
  color: var(--pv-muted-2);
}
.chip {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  padding: 4px 9px;
  border-radius: 20px;
  font-family: var(--pv-mono);
  font-size: 11.5px;
  background: var(--pv-panel-2);
  border: 1px solid var(--pv-border);
  color: var(--pv-text-2);
  transition:
    background 0.12s,
    border-color 0.12s;
}
.chip.on {
  background: color-mix(in oklab, var(--c) 26%, var(--pv-bg));
  border-color: color-mix(in oklab, var(--c) 75%, var(--pv-bg));
  color: var(--pv-text);
}
.swatch {
  width: 8px;
  height: 8px;
  border-radius: 2px;
  background: var(--c);
}
.count {
  opacity: 0.55;
  font-size: 10px;
}
.sep {
  width: 1px;
  height: 20px;
  margin: 0 6px;
  background: var(--pv-border);
}
</style>

<script setup lang="ts">
import { computed } from "vue";
import { usePinout, type CellVM } from "./usePinout";
import PinCell from "./PinCell.vue";

type Dir = "row" | "row-reverse" | "column" | "column-reverse";

const s = usePinout();

/** pins per side of the quad package: 16 for LQFP64, 25 for LQFP100 */
const perSide = computed(() => s.pins.value.length / 4);

const cells = computed<{ cell: CellVM; dir: Dir }[]>(() =>
  s.pins.value.map((p) => {
    const label = s.naming.value === "board" ? p.ard || p.morpho || p.name : p.name;
    const cell = s.buildCell({ base: p.name, label, num: p.num, key: `ic-${p.num}` });
    const n = p.num;
    const k = perSide.value;
    const last = k + 2; // grid line count: a corner cell each side of the k pins
    let col: number, row: number, dir: Dir;
    if (n <= k) {
      [col, row, dir] = [1, n + 1, "row"];
    } else if (n <= 2 * k) {
      [col, row, dir] = [n - k + 1, last, "column"];
    } else if (n <= 3 * k) {
      [col, row, dir] = [last, last - (n - 2 * k), "row-reverse"];
    } else {
      [col, row, dir] = [last - (n - 3 * k), 1, "column-reverse"];
    }
    cell.style.gridColumn = s.flip.value ? last + 1 - col : col;
    cell.style.gridRow = row;
    return { cell, dir };
  })
);

const gridStyle = computed(() => ({
  gridTemplateColumns: `minmax(96px, 140px) repeat(${perSide.value}, minmax(22px, 34px)) minmax(96px, 140px)`,
  gridTemplateRows: `120px repeat(${perSide.value}, 26px) 120px`,
}));
const bodyStyle = computed(() => ({
  gridColumn: `2 / span ${perSide.value}`,
  gridRow: `2 / span ${perSide.value}`,
}));
const caption = computed(
  () => `${s.pkg.value} · pin 1 ${s.flip.value ? "top-right · clockwise" : "top-left · counter-clockwise"}`
);
</script>

<template>
  <!-- The scroller is a block (not a centring flex box) so an oversized chip
       stays scrollable from its left edge. -->
  <div class="chip-scroll">
    <div class="chip" :style="gridStyle">
      <div class="chip-body" :style="bodyStyle">
        <span class="chip-dot" :class="{ flip: s.flip.value }"></span>
        <span class="chip-name">{{ s.pkgInfo.value.chip }}</span>
        <span class="chip-caption">{{ caption }}</span>
      </div>
      <PinCell v-for="c in cells" :key="c.cell.key" :cell="c.cell" :dir="c.dir" />
    </div>
  </div>
</template>

<style scoped>
.chip-scroll {
  overflow-x: auto;
}
.chip {
  display: grid;
  gap: 2px;
  width: 100%;
  max-width: max-content;
  margin-inline: auto;
}
.chip-body {
  position: relative;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 6px;
  border: 1px solid var(--pv-border-strong);
  background: var(--pv-panel-2);
  border-radius: 6px;
  text-align: center;
}
.chip-dot {
  position: absolute;
  top: 10px;
  left: 10px;
  width: 9px;
  height: 9px;
  border-radius: 50%;
  background: var(--pv-muted-2);
}
.chip-dot.flip {
  left: auto;
  right: 10px;
}
.chip-name {
  font-family: var(--pv-mono);
  font-size: 15px;
  font-weight: 600;
  letter-spacing: 0.02em;
}
.chip-caption {
  font-family: var(--pv-mono);
  font-size: 11px;
  color: var(--pv-muted);
}
</style>

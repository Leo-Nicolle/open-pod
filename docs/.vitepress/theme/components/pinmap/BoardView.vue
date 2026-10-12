<script setup lang="ts">
import { computed } from "vue";
import { CONNECTORS, type ConnectorPin } from "./board";
import { usePinout, type CellVM } from "./usePinout";
import PinCell from "./PinCell.vue";

const LEFT_IDS = ["CN7", "CN6", "CN8"];
const RIGHT_IDS = ["CN5", "CN9", "CN10"];
const connById = new Map(CONNECTORS.map((c) => [c.id, c]));

const s = usePinout();

function connectorVM(id: string) {
  const cn = connById.get(id)!;
  const cols = cn.columns.length;
  const cells: CellVM[] = [];
  cn.columns.forEach((col, ci) => {
    col.forEach((p: ConnectorPin, ri: number) => {
      const base = p.chip && s.byBase.value.has(p.chip) ? p.chip : "";
      const cell = s.buildCell({ base, label: p.label, num: p.pos, key: `${cn.id}-${ci}-${ri}`, catOverride: p.kind });
      if (cols === 2) {
        cell.style.gridColumn = s.flip.value ? 2 - ci : ci + 1;
        cell.style.gridRow = ri + 1;
      }
      cells.push(cell);
    });
  });
  return { id: cn.id, title: cn.title, cells, cols };
}

const left = computed(() => (s.flip.value ? [...RIGHT_IDS].reverse() : LEFT_IDS).map(connectorVM));
const right = computed(() => (s.flip.value ? [...LEFT_IDS].reverse() : RIGHT_IDS).map(connectorVM));
</script>

<template>
  <div class="board">
    <!-- Slots are positional (index within left/right), not tied to a
         connector id: left/right already reorder on flip. -->
    <div v-for="(cn, i) in left" :key="cn.id" class="conn" :class="`slot-left-${i}`">
      <span class="conn-title">{{ cn.title }}</span>
      <div class="conn-grid" :class="{ two: cn.cols === 2 }">
        <PinCell v-for="c in cn.cells" :key="c.key" :cell="c" />
      </div>
    </div>
    <div v-for="(cn, i) in right" :key="cn.id" class="conn" :class="`slot-right-${i}`">
      <span class="conn-title">{{ cn.title }}</span>
      <div class="conn-grid" :class="{ two: cn.cols === 2 }">
        <PinCell v-for="c in cn.cells" :key="c.key" :cell="c" />
      </div>
    </div>
  </div>
</template>

<style scoped>
.board {
  display: grid;
  grid-template-columns: repeat(4, minmax(0, 1fr));
  grid-template-rows: auto auto;
  align-items: start;
  gap: 12px;
}
.conn {
  display: flex;
  flex-direction: column;
  gap: 6px;
  min-width: 0;
}
.conn-title {
  font-family: var(--pv-mono);
  font-size: 10px;
  letter-spacing: 0.1em;
  text-transform: uppercase;
  color: var(--pv-muted-2);
}
.conn-grid {
  display: grid;
  grid-template-columns: minmax(0, 1fr);
  gap: 3px;
  align-content: start;
}
.conn-grid.two {
  grid-template-columns: repeat(2, minmax(0, 1fr));
}
.slot-left-0 {
  grid-area: 1 / 1 / 3 / 2;
}
.slot-left-1 {
  grid-area: 1 / 2 / 2 / 3;
}
.slot-left-2 {
  grid-area: 2 / 2 / 3 / 3;
}
.slot-right-0 {
  grid-area: 1 / 3 / 2 / 4;
}
.slot-right-1 {
  grid-area: 2 / 3 / 3 / 4;
}
.slot-right-2 {
  grid-area: 1 / 4 / 3 / 5;
}

/* Narrow screens: one connector per row, in reading order. */
@media (max-width: 720px) {
  .board {
    grid-template-columns: minmax(0, 1fr);
    grid-template-rows: none;
  }
  .board > .conn {
    grid-area: auto;
  }
}
</style>

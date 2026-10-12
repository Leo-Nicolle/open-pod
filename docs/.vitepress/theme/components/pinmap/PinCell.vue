<script setup lang="ts">
import { usePinout, type CellVM } from "./usePinout";

defineProps<{
  cell: CellVM;
  /** how the cell is laid out around the chip; boards use "row" */
  dir?: "row" | "row-reverse" | "column" | "column-reverse";
}>();

const s = usePinout();
</script>

<template>
  <button
    type="button"
    class="pv-cell"
    :class="dir ? `pv-cell--${dir}` : 'pv-cell--row'"
    :title="cell.title"
    :style="cell.style"
    @mouseenter="s.onPinEnter(cell.base)"
    @mouseleave="s.onPinLeave()"
    @click="s.onPinClick(cell.base)"
  >
    <span class="pv-cell-num">{{ cell.num }}</span>
    <span class="pv-cell-label" :style="cell.labelStyle">{{ cell.label }}</span>
  </button>
</template>

<style scoped>
.pv-cell {
  display: flex;
  align-items: center;
  gap: 6px;
  width: 100%;
  min-width: 0;
  text-align: left;
  padding: 5px 8px;
  border-radius: 5px;
  font-family: var(--pv-mono);
  font-size: 12px;
  line-height: 1.25;
  color: var(--pv-text);
  overflow: hidden;
  transition:
    opacity 0.12s,
    box-shadow 0.12s,
    background 0.12s;
}
.pv-cell-num {
  flex: 0 0 auto;
  min-width: 15px;
  font-size: 9px;
  color: var(--pv-muted-2);
}
.pv-cell-label {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.pv-cell--row-reverse {
  flex-direction: row-reverse;
  text-align: right;
}
.pv-cell--column,
.pv-cell--column-reverse {
  justify-content: flex-start;
  padding: 5px 2px;
}
.pv-cell--column {
  flex-direction: column;
}
.pv-cell--column-reverse {
  flex-direction: column-reverse;
}
.pv-cell--column .pv-cell-num,
.pv-cell--column-reverse .pv-cell-num {
  min-width: 0;
}
.pv-cell--column .pv-cell-label,
.pv-cell--column-reverse .pv-cell-label {
  writing-mode: vertical-rl;
  max-height: 104px;
}
</style>

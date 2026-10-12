<script setup lang="ts">
import { computed } from "vue";
import { usePinout } from "./usePinout";

const s = usePinout();

const rows = computed(() => {
  const ms = s.matchSetVal.value;
  return s.pins.value
    .filter((p) => !ms || ms.has(p.name))
    .map((p) => {
      const rs = s.used.value.get(p.name) ?? [];
      return {
        n: p.num,
        base: p.name,
        name: p.fullName,
        type: p.cat === "io" ? `I/O · ${p.type}` : p.cat.charAt(0).toUpperCase() + p.cat.slice(1),
        board: [p.ard, p.morpho].filter(Boolean).join(" · ") || "—",
        used: rs.map((u) => `${u.p} · ${u.role}`).join(" | ") || "—",
        isUsed: rs.length > 0,
        af: p.af.concat(p.add).join(", ") || "—",
      };
    });
});
</script>

<template>
  <div class="table-scroll">
    <table>
      <thead>
        <tr>
          <th>#</th>
          <th>Pin</th>
          <th>Type</th>
          <th v-if="s.pkgInfo.value.board">Board</th>
          <th>Used as</th>
          <th>Alternate functions</th>
        </tr>
      </thead>
      <tbody>
        <tr
          v-for="r in rows"
          :key="r.base"
          :class="{ sel: s.sel.value === r.base, hover: s.hover.value === r.base }"
          @mouseenter="s.onPinEnter(r.base)"
          @mouseleave="s.onPinLeave()"
          @click="s.onPinClick(r.base)"
        >
          <td class="mono dim">{{ r.n }}</td>
          <td class="mono strong">{{ r.name }}</td>
          <td class="soft">{{ r.type }}</td>
          <td v-if="s.pkgInfo.value.board" class="mono soft">{{ r.board }}</td>
          <td :class="r.isUsed ? 'used' : 'dim'">{{ r.used }}</td>
          <td class="mono af">{{ r.af }}</td>
        </tr>
      </tbody>
    </table>
  </div>
</template>

<style scoped>
/* Selectors are prefixed with .table-scroll to outrank VitePress' .vp-doc
   table styles, which the page layout applies to all of its content. */
.table-scroll {
  overflow: auto;
  max-height: 70vh;
}
.table-scroll table {
  display: table;
  width: 100%;
  margin: 0;
  border-collapse: collapse;
  font-size: 12px;
  line-height: 1.4;
}
.table-scroll th,
.table-scroll td {
  border: 0;
  padding: 5px 8px;
  font-size: 12px;
  line-height: 1.4;
  vertical-align: top;
  text-align: left;
}
.table-scroll th {
  position: sticky;
  top: 0;
  z-index: 1;
  padding: 6px 8px;
  background: var(--pv-panel);
  border-bottom: 1px solid var(--pv-border);
  color: var(--pv-muted);
  font-family: var(--pv-mono);
  font-size: 10px;
  font-weight: 500;
  letter-spacing: 0.08em;
  text-transform: uppercase;
}
.table-scroll tr {
  cursor: pointer;
  background: transparent;
  border: 0;
  border-bottom: 1px solid var(--pv-border);
  transition: none;
}
.table-scroll tr.hover {
  background: var(--pv-hover);
}
.table-scroll tr.sel {
  background: var(--pv-sel);
}
.mono {
  font-family: var(--pv-mono);
}
.strong {
  font-weight: 600;
  white-space: nowrap;
}
.soft {
  color: var(--pv-muted);
}
.dim {
  color: var(--pv-muted-2);
}
.used {
  color: var(--pv-accent);
}
.af {
  min-width: 220px;
  font-size: 11px;
  line-height: 1.5;
  color: var(--pv-muted);
}
</style>

<script lang="ts" setup>
import { computed, ref } from 'vue';
import { GEOMETRY, REDRAW_NOTES } from './spec';
import type { GroupId } from './screenStates';

const props = defineProps<{ group: GroupId }>();
const showAll = ref(false);

const rows = computed(() => (showAll.value ? GEOMETRY : GEOMETRY.filter((g) => g.group === props.group)));
</script>

<template>
  <div class="spec-notes">
    <section>
      <div class="head">
        <h3>Geometry</h3>
        <label class="toggle"><input v-model="showAll" type="checkbox" /> all screens</label>
      </div>
      <div v-if="rows.length" class="geometry">
        <div v-for="g in rows" :key="g.el + g.box" class="row">
          <span class="el">{{ g.el }}</span>
          <code>{{ g.box }}</code>
          <span class="note">{{ g.note }}</span>
        </div>
      </div>
      <p v-else class="empty">No geometry notes for this screen; tick "all screens" to see every element.</p>
    </section>

    <section>
      <h3>Redraw budget</h3>
      <ol class="notes">
        <li v-for="n in REDRAW_NOTES" :key="n.i">
          <span class="i">{{ n.i }}</span>
          <span>{{ n.t }}</span>
        </li>
      </ol>
    </section>
  </div>
</template>

<style scoped>
.spec-notes {
  display: flex;
  flex-direction: column;
  gap: 28px;
}
.head {
  display: flex;
  align-items: baseline;
  justify-content: space-between;
  gap: 12px;
}
h3 {
  margin: 0 0 10px;
  font-size: 15px;
  font-weight: 600;
}
.toggle {
  display: inline-flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
  color: var(--vp-c-text-2);
  cursor: pointer;
}
.geometry {
  border: 1px solid var(--vp-c-divider);
  border-radius: 8px;
  overflow: hidden;
}
.row {
  display: grid;
  grid-template-columns: minmax(0, 1fr) minmax(0, 1fr) minmax(0, 1.6fr);
  gap: 14px;
  padding: 7px 12px;
  border-bottom: 1px solid var(--vp-c-divider);
  font-size: 12px;
  line-height: 1.45;
}
.row:last-child {
  border-bottom: none;
}
.el {
  font-weight: 500;
}
code {
  padding: 0;
  background: none;
  font-size: 11.5px;
  color: var(--vp-c-brand-1);
  overflow-wrap: anywhere;
}
.note {
  color: var(--vp-c-text-2);
}
.empty {
  margin: 0;
  font-size: 13px;
  color: var(--vp-c-text-3);
}
.notes {
  margin: 0;
  padding: 0;
  list-style: none;
}
.notes li {
  display: grid;
  grid-template-columns: 26px minmax(0, 1fr);
  gap: 10px;
  margin: 0 0 10px;
  font-size: 13px;
  line-height: 1.55;
  color: var(--vp-c-text-2);
}
.i {
  font-family: var(--vp-font-family-mono);
  color: var(--vp-c-text-3);
}
@media (max-width: 640px) {
  .row {
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
  }
  .note {
    grid-column: 1 / -1;
  }
}
</style>

<script lang="ts" setup>
import { useTemplateRef, ref, computed, nextTick } from 'vue'
import clickwheelRender from './clickwheel-render.vue';
import clickwheelForm from './clickwheel-form.vue';
import areaInterp from './area-interp.vue';
import {
  NDivider,

} from 'naive-ui';
const svg = useTemplateRef('svg');
const area = useTemplateRef('area-interp');
const canvas = useTemplateRef('canvas');
// radius of the circle
const outR = ref(19);
const W2 = ref(15);
const inR = ref(9.5);
const roundness = ref(0);
const numPads = ref(4);
const numCurves = ref(8);
const gap = ref(-Math.PI / 24); // gap between the pads
const offset = ref(0);
const strokeWidth = ref(0.2);
const margin = ref(1);
const width = computed(() => 2 * (Math.max(outR.value, W2.value) + margin.value));
const showCopper = ref(true);
const showCuts = ref(true);
const showGizmo = ref(true);
function onMouseEnter() {
  const save = {
    copper: showCopper.value,
    cuts: showCuts.value,
    gizmo: showGizmo.value,
  };
  showCopper.value = true;
  showCuts.value = false;
  showGizmo.value = false;
  nextTick(() => {
    area.value.refreshCanvas();
    showCopper.value = save.copper;
    showCuts.value = save.cuts;
    showGizmo.value = save.gizmo;
  })
}
</script>
<template>
  <clickwheel-form v-model:inR="inR" v-model:outR="outR" v-model:numPads="numPads" v-model:numCurves="numCurves"
    v-model:gap="gap" v-model:offset="offset" v-model:strokeWidth="strokeWidth" v-model:roundness="roundness"
    v-model:W2="W2" v-model:margin="margin" v-model:showCopper="showCopper" v-model:showCuts="showCuts"
    v-model:showGizmo="showGizmo" :svg="svg?.svg" />

  <n-divider></n-divider>

  <clickwheel-render :outR="outR" :inR="inR" :numPads="numPads" :numCurves="numCurves" :gap="gap" :offset="offset"
    :strokeWidth="strokeWidth" :roundness="roundness" :W2="W2" :width="width" :margin="margin" :showCopper="showCopper"
    :showCuts="showCuts" :showGizmo="showGizmo" ref="svg">

  </clickwheel-render>
  <area-interp @mouseenter="onMouseEnter" :num-pads="numPads" :svg="svg?.svg" :width="width" ref="area-interp" />

</template>

<style scoped>
.render {
  display: grid;
  grid-template-columns: 100%;
  grid-template-rows: 100%;
  width: 100%;
  height: 100%;
}
</style>
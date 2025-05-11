<script lang="ts" setup>
import { useTemplateRef, ref, computed } from 'vue'
import clickwheelRender from './clickwheel-render.vue';
import clickwheelForm from './clickwheel-form.vue';
import areaInterp from './area-interp.vue';
import {
  NDivider,

} from 'naive-ui';
const svg = useTemplateRef('svg');
const canvas = useTemplateRef('canvas');
// radius of the circle
const R = ref(19);
const W2 = ref(15);
const r = ref(R.value / 2);
const roundness = ref(0);
const numPads = ref(4);
const numCurves = ref(8);
const gap = ref(-Math.PI / 24); // gap between the pads
const offset = ref(Math.PI / 16);
const strokeWidth = ref(0.2);
const margin = ref(1);
const width = computed(() => 2 * (Math.max(R.value, W2.value) + margin.value));

const showCopper = ref(true);
const showCuts = ref(true);
const showGizmo = ref(true);
const overlaps = ref<number[]>([])
function onRatio(e) {
  console.log(e)
  overlaps.value = e
}
</script>
<template>
  <!-- <n-form label-width="120px" label-align="left"> -->
  <clickwheel-form v-model:-r="r" v-model:-R="R" v-model:numPads="numPads" v-model:numCurves="numCurves"
    v-model:gap="gap" v-model:offset="offset" v-model:strokeWidth="strokeWidth" v-model:roundness="roundness"
    v-model:W2="W2" v-model:margin="margin" v-model:showCopper="showCopper" v-model:showCuts="showCuts"
    v-model:showGizmo="showGizmo" :svg="svg?.svg" />
  <n-divider></n-divider>

  <clickwheel-render :R="R" :r="r" :numPads="numPads" :numCurves="numCurves" :gap="gap" :offset="offset"
    :strokeWidth="strokeWidth" :roundness="roundness" :W2="W2" :width="width" :margin="margin" :showCopper="showCopper"
    :showCuts="showCuts" :showGizmo="showGizmo" ref="svg">
    <area-interp :r="4" :svg="svg?.svg" @ratio="onRatio" :canvas="canvas" :width="width" />

  </clickwheel-render>
  <div>
    Overlaps
    <p v-for="overlap in overlaps">
      coucou {{ overlap }}
    </p>
  </div>
  <canvas ref="canvas"></canvas>
</template>

<style scoped>
canvas {
  width: 512px;
  height: 512px;
  border: 1px solid black;
}

.render {
  display: grid;
  grid-template-columns: 100%;
  grid-template-rows: 100%;
  width: 100%;
  height: 100%;
}
</style>
<script lang="ts" setup>
import { nextTick } from 'vue'
import {
  NFormItem, NInputNumber, NButton, NDivider,
  NCheckbox
} from 'naive-ui';

const props = defineProps<{
  svg: SVGElement | null | undefined
}>();

const R = defineModel<number>('R', {
  type: Number,
  default: 19,
});
const W2 = defineModel<number>('W2', {
  type: Number,
  default: 15,
});
const r = defineModel<number>('r', {
  type: Number,
  default: 9.5,
});
const roundness = defineModel<number>('roundness', {
  type: Number,
  default: 0,
});
const numPads = defineModel<number>('numPads', {
  type: Number,
  default: 4,
});
const numCurves = defineModel<number>('numCurves', {
  type: Number,
  default: 8,
});
const gap = defineModel<number>('gap', {
  type: Number,
  default: -Math.PI / 24,
}); // gap between the pads
const offset = defineModel<number>('offset', {
  type: Number,
  default: Math.PI / 16,
});
const strokeWidth = defineModel<number>('strokeWidth', {
  type: Number,
  default: 0.2,
});
const margin = defineModel<number>('margin', {
  type: Number,
  default: 1,
});
const showCopper = defineModel<boolean>('showCopper', {
  type: Boolean,
  default: true,
});
const showCuts = defineModel<boolean>('showCuts', {
  type: Boolean,
  default: true,
});
const showGizmo = defineModel<boolean>('showGizmo', {
  type: Boolean,
  default: true,
});
function exportSVG() {
  const save = showGizmo.value;
  showGizmo.value = false;
  nextTick(() => {
    const svgElement = props.svg;
    if (!svgElement) return;
    const serializer = new XMLSerializer();
    const svgString = serializer.serializeToString(svgElement);
    const blob = new Blob([svgString], { type: 'image/svg+xml' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = `${url}`;
    a.download = 'clickwheel.svg';
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    showGizmo.value = save;
  });
}

function exportPNG() {
  const save = showGizmo.value;
  showGizmo.value = false;
  nextTick(async () => {
    const svgElement = props.svg;
    if (!svgElement) return;
    const w = 1024;
    const h = 1024;
    const canvas = document.createElement('canvas');
    const ctx = canvas.getContext('2d');
    if (!ctx) return;
    canvas.width = w;
    canvas.height = h;
    const svgString = new XMLSerializer().serializeToString(svgElement);
    const img = new Image();
    const svgBlob = new Blob([svgString], { type: 'image/svg+xml;charset=utf-8' });
    const url = URL.createObjectURL(svgBlob);
    const promise = new Promise((resolve) => {
      img.onload = () => {
        ctx.drawImage(img, 0, 0, w, h);
        resolve();
      };
    });
    img.src = url;
    await promise;
    const pngBlob = canvas.toBlob((blob) => {
      if (!blob) return;
      const a = document.createElement('a');
      a.href = URL.createObjectURL(blob);
      a.download = 'clickwheel.png';
      document.body.appendChild(a);
      a.click();
      document.body.removeChild(a);
      URL.revokeObjectURL(a.href);
    }, 'image/png');
    URL.revokeObjectURL(url);
    showGizmo.value = save;
  });
}
function download() {
  const data = {
    R: R.value,
    r: r.value,
    numPads: numPads.value,
    numCurves: numCurves.value,
    gap: gap.value,
    offset: offset.value,
    strokeWidth: strokeWidth.value,
    roundness: roundness.value,
    W2: W2.value,
  };
  const json = JSON.stringify(data, null, 2);
  const blob = new Blob([json], { type: 'application/json' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = `${url}`;
  a.download = 'clickwheel.json';
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}
function upload() {
  const input = document.createElement('input');
  input.type = 'file';
  input.accept = '.json';
  input.onchange = (event: Event) => {
    const file = (event.target as HTMLInputElement).files?.[0];
    if (!file) return;
    const reader = new FileReader();
    reader.onload = (e) => {
      const data = JSON.parse(e.target?.result as string);
      R.value = data.R;
      r.value = data.r;
      numPads.value = data.numPads;
      numCurves.value = data.numCurves;
      gap.value = data.gap;
      offset.value = data.offset;
      strokeWidth.value = data.strokeWidth;
      roundness.value = data.roundness;
      W2.value = data.W2;
    };
    reader.readAsText(file);
  };
  input.click();
  input.remove();
}


</script>
<template>
  <!-- <n-form label-width="120px" label-align="left"> -->
  <div class="form-container">
    <n-form-item label="Radius (mm)">
      <n-input-number v-model:value="R" placeholder="Enter radius" :min="r" />
    </n-form-item>

    <n-form-item label="Inner Radius (mm)">
      <n-input-number v-model:value="r" placeholder="Enter inner radius" :min="1" :max="R" />
    </n-form-item>

    <n-form-item label="Number of Pads">
      <n-input-number v-model:value="numPads" min="3" placeholder="Enter number of pads" :min="1" />
    </n-form-item>

    <n-form-item label="Number of Curves">
      <n-input-number v-model:value="numCurves" placeholder="Enter number of curves" :min="1" />
    </n-form-item>

    <n-form-item label="Gap Between Pads">
      <n-input-number v-model:value="gap" placeholder="Enter gap (radians)" :step="0.01" />
    </n-form-item>
    <n-form-item label="Pads Roundness">
      <n-input-number v-model:value="roundness" placeholder="Enter roundness" :step="0.1" />
    </n-form-item>
    <n-form-item label="Stroke width(mm)">
      <n-input-number v-model:value="strokeWidth" placeholder="Enter stroke width" :step="0.01" />
    </n-form-item>

    <n-form-item label="Start Angle(rad)">
      <n-input-number v-model:value="offset" placeholder="Enter offset (radians)" :step="0.01" />
    </n-form-item>

    <n-form-item label="Square half width(mm)">
      <n-input-number v-model:value="W2" placeholder="Square Width" :min="r" />
    </n-form-item>
  </div>
  <n-form-item label="Show/hide">
    <n-checkbox v-model:checked="showCopper">
      Copper Layer
    </n-checkbox>
    <n-checkbox v-model:checked="showCuts">
      Cuts Layer
    </n-checkbox>
    <n-checkbox v-model:checked="showGizmo">
      Gizmo Layer
    </n-checkbox>
  </n-form-item>
  <div class="buttons">
    <n-button @click="download">Download JSON</n-button>
    <n-button @click="upload">Upload JSON</n-button>
    <n-button @click="exportSVG">Download SVG</n-button>
    <n-button @click="exportPNG">Download PNG</n-button>
  </div>
</template>

<style scoped>
.form-container {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
  gap: 16px;
  margin: 20px;
}

.n-form-item {
  margin-bottom: 16px;
}

@media (max-width: 800px) {
  .form-container {
    grid-template-columns: repeat(auto-fit, minmax(150px, 1fr));
  }
}

@media (max-width: 500px) {
  .form-container {
    grid-template-columns: 1fr;
  }
}

.buttons {
  display: flex;
  justify-content: flex-start;
  gap: 1em;
}
</style>
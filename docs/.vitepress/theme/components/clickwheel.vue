<script lang="ts" setup>
import { computed, ref, nextTick } from 'vue'
import {
  NCard, NForm, NFormItem, NInputNumber, NButton, NDivider, NDescriptions, NDescriptionsItem,
  NCheckbox
} from 'naive-ui';

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
const margin = 1;

const showCopper = ref(true);
const showCuts = ref(true);
const showGizmo = ref(true);
const svg = ref<SVGSVGElement | null>(null);
function exportSVG() {
  const save = showGizmo.value;
  showGizmo.value = false;
  nextTick(() => {
    const svgElement = svg.value;
    if (!svgElement) return;
    const serializer = new XMLSerializer();
    const svgString = serializer.serializeToString(svgElement);
    // .replaceAll(/data-v-[^\s]+/g, '')
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
  showGizmo.value = save;
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

function interp(a: number, b: number, t: number) {
  return a + (b - a) * t;
}

const width = computed(() => 2 * (Math.max(R.value, W2.value) + margin));
const cutPath = computed(() => {
  const w2 = W2.value;
  const radius = R.value;
  const center = {
    x: radius + margin,
    y: radius + margin,
  };
  // radius of the circle
  const yTopRight = Math.sin(Math.acos(w2 / radius)) * radius;
  const xTopRight = Math.cos(Math.asin(w2 / radius)) * radius;
  const squarePoints = [
    // top right
    { x: center.x + w2, y: center.y - w2 },
    { x: center.x - w2, y: center.y - w2 },
    { x: center.x - w2, y: center.y + w2 },
    { x: center.x + w2, y: center.y + w2 },
  ];

  const arcPoints = [
    // top right
    { x: center.x + w2, y: center.y - yTopRight },
    { x: center.x + xTopRight, y: center.y - w2 },
    // top left
    { x: center.x - xTopRight, y: center.y - w2 },
    { x: center.x - w2, y: center.y - yTopRight },
    // bottom left
    { x: center.x - w2, y: center.y + yTopRight },
    { x: center.x - xTopRight, y: center.y + w2 },
    // bottom right
    { x: center.x + xTopRight, y: center.y + w2 },
    { x: center.x + w2, y: center.y + yTopRight },
  ];

  let path = `M ${arcPoints[0].x} ${arcPoints[0].y}`;

  for (let i = 0; i < 4; i++) {
    const c1 = arcPoints[i * 2 + 1];
    const c2 = arcPoints[(i * 2 + 2) % arcPoints.length];
    const s = squarePoints[i];
    path += `\nL ${s.x} ${s.y} L ${c1.x} ${c1.y} A ${radius} ${radius} 0 0 0 ${c2.x} ${c2.y}\n`;
  }
  console.log(path)
  return path;

});
const copperPath = computed(() => {
  let path = '';
  for (let i = 0; i < numPads.value; i++) {
    const startAngle = (i * 2 * Math.PI) / numPads.value + gap.value + offset.value;
    const endAngle = ((i + 1) * 2 * Math.PI) / numPads.value - gap.value + offset.value;
    const center = {
      x: R.value + margin,
      y: R.value + margin,
    };
    const start = {
      x: Math.cos(startAngle),
      y: Math.sin(startAngle),
    };
    const end = {
      x: Math.cos(endAngle),
      y: Math.sin(endAngle),
    };
    let x1 = center.x + r.value * start.x;
    let y1 = center.y + r.value * start.y;
    for (let j = 0; j < numCurves.value; j++) {
      const radius = interp(r.value, R.value, (j + 1) / numCurves.value);
      const point = j % 2 === 1 ? start : end;
      const largeArc = j % 2 === 0 ? 1 : 0;
      const x2 = center.x + radius * point.x;
      const y2 = center.y + radius * point.y;
      path += `M ${x1} ${y1} `;
      path += `\nA ${radius - roundness.value} ${radius - roundness.value} 0 0 ${largeArc} ${x2} ${y2}`;
      x1 = x2;
      y1 = y2;
    }
  }
  return path;
});


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
  </div>
  <n-divider></n-divider>

  <svg ref="svg" :width="`${width}mm`" :height="`${width}mm`" :viewBox="`0 0 ${width} ${width}`"
    xmlns:dc="http://purl.org/dc/elements/1.1/" xmlns:cc="http://creativecommons.org/ns#"
    xmlns:rdf="http://www.w3.org/1999/02/22-rdf-syntax-ns#" xmlns:svg="http://www.w3.org/2000/svg"
    xmlns="http://www.w3.org/2000/svg" xmlns:sodipodi="http://sodipodi.sourceforge.net/DTD/sodipodi-0.dtd"
    xmlns:inkscape="http://www.inkscape.org/namespaces/inkscape" sodipodi:docname="clickwheel.svg"
    inkscape:version="0.92.5 (2060ec1f9f, 2020-04-08)" id="svg8" version="1.1">

    <defs>
      <mask id="coppermask">
        <rect width="100%" height="100%" fill="white" />
        <circle stroke="none" fill="black" :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${r}`" />
        <path stroke="black" :stroke-width="strokeWidth" fill="none" :d="copperPath" />
      </mask>

      <mask id="innercutmask">
        <rect width="100%" height="100%" fill="white" />
        <circle stroke="none" fill="black" :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${r}`" />
      </mask>
    </defs>



    <g v-if="showGizmo">
      <circle :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${R}`" stroke="blue" stroke-width="0.02" fill="none" />
      <circle :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${r}`" stroke="blue" stroke-width="0.02" fill="none" />
      <rect :x="`${width / 2 - W2}`" :y="`${width / 2 - W2}`" :width="`${2 * W2}`" :height="`${2 * W2}`" stroke="blue"
        stroke-width="0.02" fill="none" />
      <path stroke="blue" stroke-width="0.02" fill="none" :d="`M 0 0 L ${width} ${width} M ${width} 0 L 0 ${width}`" />
    </g>
    <g v-if="showCuts" inkscape:groupmode="layer" id="layer5" inkscape:label="Edge.Cuts" style="display:inline">
      <path stroke="none" stroke-width="0.2" fill="#0c0" :d="cutPath" mask="url(#innercutmask)" />
    </g>
    <g v-if="showCopper" i inkscape:groupmode="layer" id="layer4" inkscape:label="F.Cu" style="display:inline">
      <circle fill="#d3bc5f" :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${R}`" mask="url(#coppermask)" />
    </g>
  </svg>

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

svg {
  width: 100%;
  height: auto;
  margin-top: 20px;
}

.buttons {
  display: flex;
  justify-content: flex-start;
  gap: 1em;
}
</style>
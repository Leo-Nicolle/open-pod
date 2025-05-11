<script lang="ts" setup>
import { computed, useTemplateRef } from 'vue'


const props = defineProps<{
  outR: number
  inR: number
  width: number
  numPads: number
  numCurves: number
  gap: number
  offset: number
  strokeWidth: number
  roundness: number
  W2: number
  margin: number
  showCopper: boolean
  showCuts: boolean
  showGizmo: boolean
}>();
// const svg = ref<SVGSVGElement | null>(null);

const svg = useTemplateRef('clickwheel-svg');

defineExpose({
  svg,
});

function interp(a: number, b: number, t: number) {
  return a + (b - a) * t;
}

const cutPath = computed(() => {
  const { W2, outR, margin } = props
  const center = {
    x: outR + margin,
    y: outR + margin,
  };
  // radius of the circle
  const yTopRight = Math.sin(Math.acos(W2 / outR)) * outR;
  const xTopRight = Math.cos(Math.asin(W2 / outR)) * outR;
  const squarePoints = [
    // top right
    { x: center.x + W2, y: center.y - W2 },
    { x: center.x - W2, y: center.y - W2 },
    { x: center.x - W2, y: center.y + W2 },
    { x: center.x + W2, y: center.y + W2 },
  ];

  const arcPoints = [
    // top right
    { x: center.x + W2, y: center.y - yTopRight },
    { x: center.x + xTopRight, y: center.y - W2 },
    // top left
    { x: center.x - xTopRight, y: center.y - W2 },
    { x: center.x - W2, y: center.y - yTopRight },
    // bottom left
    { x: center.x - W2, y: center.y + yTopRight },
    { x: center.x - xTopRight, y: center.y + W2 },
    // bottom right
    { x: center.x + xTopRight, y: center.y + W2 },
    { x: center.x + W2, y: center.y + yTopRight },
  ];

  let path = `M ${arcPoints[0].x} ${arcPoints[0].y}`;

  for (let i = 0; i < 4; i++) {
    const c1 = arcPoints[i * 2 + 1];
    const c2 = arcPoints[(i * 2 + 2) % arcPoints.length];
    const s = squarePoints[i];
    path += `\nL ${s.x} ${s.y} L ${c1.x} ${c1.y} A ${outR} ${outR} 0 0 0 ${c2.x} ${c2.y}\n`;
  }
  return path;
});
const copperPath = computed(() => {
  let path = '';
  const { outR, inR, numPads, numCurves, gap, offset, roundness, margin } = props;
  for (let i = 0; i < numPads; i++) {
    const startAngle = (i * 2 * Math.PI) / numPads + gap + offset;
    const endAngle = ((i + 1) * 2 * Math.PI) / numPads - gap + offset;
    const center = {
      x: outR + margin,
      y: outR + margin,
    };
    const start = {
      x: Math.cos(startAngle),
      y: Math.sin(startAngle),
    };
    const end = {
      x: Math.cos(endAngle),
      y: Math.sin(endAngle),
    };
    let x1 = center.x + inR * start.x;
    let y1 = center.y + inR * start.y;
    for (let j = 0; j < numCurves; j++) {
      const radius = interp(inR, outR, (j + 1) / numCurves);
      const point = j % 2 === 1 ? start : end;
      const largeArc = j % 2 === 0 ? 1 : 0;
      const x2 = center.x + radius * point.x;
      const y2 = center.y + radius * point.y;
      path += `M ${x1} ${y1} `;
      path += `\nA ${radius - roundness} ${radius - roundness} 0 0 ${largeArc} ${x2} ${y2}`;
      x1 = x2;
      y1 = y2;
    }
  }
  return path;
});

</script>
<template>
  <svg ref="clickwheel-svg" :width="`${width}mm`" :height="`${width}mm`" :viewBox="`0 0 ${width} ${width}`"
    xmlns:svg="http://www.w3.org/2000/svg" xmlns="http://www.w3.org/2000/svg" version="1.1">

    <defs>
      <mask id="coppermask">
        <rect width="100%" height="100%" fill="white" />
        <circle stroke="none" fill="black" :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${inR}`" />
        <path stroke="black" :stroke-width="strokeWidth" fill="none" :d="copperPath" />
      </mask>

      <mask id="innercutmask">
        <rect width="100%" height="100%" fill="white" />
        <circle stroke="none" fill="black" :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${inR}`" />
      </mask>
    </defs>
    <g v-if="showGizmo">
      <circle :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${outR}`" stroke="blue" stroke-width="0.02" fill="none" />
      <circle :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${inR}`" stroke="blue" stroke-width="0.02" fill="none" />
      <rect :x="`${width / 2 - W2}`" :y="`${width / 2 - W2}`" :width="`${2 * W2}`" :height="`${2 * W2}`" stroke="blue"
        stroke-width="0.02" fill="none" />
      <path stroke="blue" stroke-width="0.02" fill="none" :d="`M 0 0 L ${width} ${width} M ${width} 0 L 0 ${width}`" />
    </g>
    <g v-if="showCuts" id="Edge.Cuts" style="display:inline">
      <path stroke="none" stroke-width="0.2" fill="#0c0" :d="cutPath" mask="url(#innercutmask)" />
    </g>
    <g v-if="showCopper" id="F.Cu" style="display:inline">
      <circle :cx="`${width / 2}`" :cy="`${width / 2}`" :r="`${outR}`" fill="#cd4c33" mask="url(#coppermask)" />
    </g>
    <g>
      <slot></slot>
    </g>
  </svg>

</template>

<style scoped>
svg {
  width: 100%;
  height: auto;
  margin-top: 20px;
}
</style>
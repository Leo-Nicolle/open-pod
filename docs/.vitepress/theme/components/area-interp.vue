<script lang="ts" setup>
import { ref, onMounted, onBeforeUnmount, defineProps, watch } from 'vue';
import {
  NFormItem, NInputNumber
} from 'naive-ui';

const props = defineProps<{
  svg: SVGSVGElement | null | undefined;
  numPads: number;
  width: number;
}>();
const radius = ref(64);
const position = ref({ x: 0, y: 0 });
const canvas = ref<HTMLCanvasElement>();
const overlaps = ref<number[][]>([]);
let needsUpdate = false;
let imageData: ImageData;
let animationFrameId: number;

const updatePosition = (event: MouseEvent) => {
  const rect = event.target.getBoundingClientRect();
  position.value = {
    x: 2 * (event.clientX - rect.left),
    y: 2 * (event.clientY - rect.top)
  };
  needsUpdate = true;
};

function colorise() {
  const canvasElement = canvas.value;
  if (!canvasElement) return;
  const num = props.numPads;
  const ctx = canvasElement.getContext('2d')!;
  imageData = ctx.getImageData(0, 0, canvasElement.width, canvasElement.height);
  const { width, height, data } = imageData;
  const visited = new Uint32Array(imageData.width * imageData.height);

  const getComponentColor = (i: number) => {
    const c = Math.floor(i / (num + 1) * 255);
    return [c, c, c];
  };
  const floodFill = (i, componentIndex) => {
    const x = i % width;
    const y = Math.floor(i / width);
    const stack = [[x, y]];
    const color = getComponentColor(componentIndex);
    let count = 0;
    while (stack.length > 0) {
      const [x, y] = stack.pop()!;
      const index = (y * width + x) * 4;
      if (
        x < 0 || x >= width ||
        y < 0 || y >= height ||
        visited[index / 4] ||
        data[index + 3] === 0
      ) continue;
      visited[index / 4] = componentIndex;
      data[index] = color[0];
      data[index + 1] = color[1];
      data[index + 2] = color[2];
      count++;
      stack.push(
        [x + 1, y],
        [x - 1, y],
        [x, y + 1],
        [x, y - 1]
      );
    }
    if (count < 10) return false;
    console.log(componentIndex, count);
    return true;
  };
  let currentIndex = 1;
  for (let i = 0; i < data.length; i += 4) {
    if (
      visited[i / 4] ||
      data[i + 3] === 0
    ) continue;
    if (!floodFill(i / 4, currentIndex)) continue;
    currentIndex++;
  }
  ctx.putImageData(imageData, 0, 0);
}

function refreshCanvas(
) {
  const image = document.createElement('img');
  const diameter = 2 * radius.value;
  const canvasElement = canvas.value;
  if (!diameter || !canvasElement || !props.svg) return;
  const cW = 1024;
  const W = 2048;
  canvasElement.width = cW;
  canvasElement.height = cW;
  const ctx = canvasElement.getContext('2d')!;
  ctx.clearRect(0, 0, cW, cW);
  const svgClone = props.svg.cloneNode(true) as SVGSVGElement;
  svgClone.setAttribute('width', `${W}`);
  svgClone.setAttribute('height', `${W}`);
  const svgData = new XMLSerializer().serializeToString(svgClone);
  const svgUrl = `data:image/svg+xml;base64,${btoa(svgData)}`;
  image.onload = () => {
    ctx.drawImage(image, 0, 0, W, W, 0, 0, cW, cW);
    colorise();
  };
  image.src = svgUrl;
}

defineExpose({
  refreshCanvas,
});

const computeOverlap = () => {
  if (!imageData) return;
  const cx = Math.floor(position.value.x);
  const cy = Math.floor(position.value.y);
  const { data, width, height } = imageData;
  const r = radius.value;
  const canvasElement = canvas.value;
  const ctx = canvasElement?.getContext('2d');
  ctx?.putImageData(imageData, 0, 0);
  const colorToPad = (c: number) => Math.floor(c / 255 * (props.numPads + 1));
  const padCount = new Map<number, number>();
  for (let i = 0; i < props.numPads; i++) {
    padCount.set(i + 1, 0);
  }
  for (let x = cx - r; x < cx + r; x++) {
    for (let y = cy - r; y < cy + r; y++) {
      const dx = x - cx;
      const dy = y - cy;
      if (
        x < 0 || x >= width ||
        y < 0 || y >= height ||
        dx * dx + dy * dy > r * r) continue;
      const index = (y * width + x) * 4;
      if (data[index + 3] === 0) continue;
      const pad = colorToPad(data[index]);
      if (!padCount.has(pad)) {
        continue;
      }
      padCount.set(pad, (padCount.get(pad) || 0) + 1);
    }
  }

  const total = Math.PI * r * r;
  const res = [...padCount.entries()].sort((a, b) => a[0] - b[0])
    .map(([pad, count]) => {
      const ratio = Math.floor(100 * count / total);
      return [pad, ratio];
    });
  overlaps.value = res;
  ctx?.beginPath();
  ctx?.arc(cx, cy, radius.value, 0, Math.PI * 2);
  ctx?.closePath();
  ctx?.stroke();
};

const animate = () => {
  if (needsUpdate) {
    computeOverlap();
    needsUpdate = false;
  }
  animationFrameId = requestAnimationFrame(animate);
};

onMounted(() => {
  animationFrameId = requestAnimationFrame(animate);
});

onBeforeUnmount(() => {
  cancelAnimationFrame(animationFrameId);
});

// Watch for changes in the radius and update canvas size
watch(() => radius.value, (newR) => {
  needsUpdate = true; // Trigger an update
});
</script>

<template>
  <div>

    <h2>Touch test</h2>
    <p>
      Hover this element to refresh it (if you change params in the clickwheel, just re-hover to refresh). Move the
      mouse around to see the ratios of the surfaces touched.
      Seems like the more curves, the most stable the ratios are across the width of the whee, but this tool is here so
      you can make your own idea.
    </p>
    <n-form-item label="Finger size">
      <n-input-number v-model:value="radius" placeholder="Finger radius" :min="8" />
    </n-form-item>
    <canvas ref="canvas" @mousemove="updatePosition">

    </canvas>

    <h3>Touch test</h3>
    <ul>
      <li v-for="[pad, ratio] in overlaps">
        <span>{{ pad }}</span>
        <span class="ratio" :style="{ width: `${ratio}%` }"></span>
        <span>{{ ratio }}%</span>
      </li>
    </ul>
  </div>

</template>

<style scoped>
canvas {
  width: 512px;
  height: 512px;
  border: 1px solid black;
}

ul {
  list-style: none;
  padding: 0;
  display: grid;
  grid-auto-flow: row;
  grid-template-columns: 3em auto 3em;
}

li {
  display: contents;
}

.ratio {
  background: #0c0;
  height: 1em;
  border-radius: 5px;
  margin-left: 1em;
}
</style>

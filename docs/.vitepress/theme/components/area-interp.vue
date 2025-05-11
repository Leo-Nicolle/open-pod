<script lang="ts" setup>
import { ref, onMounted, onBeforeUnmount, defineProps, watch } from 'vue';

const props = defineProps<{
  r: number;
  svg: SVGSVGElement;
  canvas?: HTMLCanvasElement;
  width: number;
}>();
const emit = defineEmits<{
  (e: 'ratio', values: number[]): void
}>();

const position = ref({ x: 0, y: 0 });
const circle = ref<SVGCircleElement>();
let animationFrameId: number;

// Throttled computation using requestAnimationFrame
let needsUpdate = false;


const updatePosition = (event: MouseEvent) => {
  if (!props.svg) return;

  const point = props.svg.createSVGPoint();
  point.x = event.clientX;
  point.y = event.clientY;

  const svgPoint = point.matrixTransform(props.svg.getScreenCTM()?.inverse());
  position.value = { x: svgPoint.x, y: svgPoint.y };
  needsUpdate = true;
};
const getDiametter = () => {
  const c = circle.value;
  if (!c) return 0;
  const bbox = c.getBoundingClientRect();
  return 5 * Math.floor(Math.min(bbox.width, bbox.height));
}
const refreshOverlap = () => {
  const image = document.createElement('img');
  const diameter = getDiametter();
  if (!diameter || !props.canvas) return;
  const cW = 128;
  const W = 2048;
  const w = 2 * props.r * W / cW;
  props.canvas.width = cW;
  props.canvas.height = cW;
  const ctx = props.canvas.getContext('2d')!;
  ctx.clearRect(0, 0, cW, cW);

  // Move the SVG to the center of the canvas
  const offsetX = position.value.x - props.r;
  const offsetY = position.value.y - props.r;

  // Update SVG viewBox to focus on the area around the circle
  const svgClone = props.svg.cloneNode(true) as SVGSVGElement;
  svgClone.setAttribute('width', `${W}`);
  svgClone.setAttribute('height', `${W}`);
  svgClone.setAttribute('viewBox', `${offsetX} ${offsetY} ${w} ${w}`);
  const svgData = new XMLSerializer().serializeToString(svgClone);
  const svgUrl = `data:image/svg+xml;base64,${btoa(svgData)}`;
  image.onload = () => {

    ctx.beginPath();
    ctx.arc(cW / 2, cW / 2, cW / 2, 0, Math.PI * 2, false);
    ctx.closePath();
    ctx.clip();
    ctx.drawImage(image, 0, 0);
    computeOverlap(ctx.getImageData(0, 0, cW, cW))
  };
  image.src = svgUrl;
};

const computeOverlap = (imgData: ImageData) => {
  const { width, height, data } = imgData;
  const visited = new Uint32Array(width * height);
  const areas: number[] = [];
  const colorToIndex = new Map<number, number>();
  // Helper function to calculate pixel index
  const pixelIndex = (x: number, y: number) => (y * width + x) * 4;
  const colorNumber = (r: number, g: number, b: number) => (
    r * 255 * 255 * 255 + g * 255 * 255 + b
  );

  let componentIndex = 1;
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const idx = pixelIndex(x, y);
      const n = colorNumber(
        data[idx],
        data[idx + 1],
        data[idx + 2],
      )
      if (data[idx + 3] > 0 && !visited[idx / 4]) {
        // Found a new component, perform flood fill
        const color = 

        const area = floodFill(x, y, componentIndex);
        areas.push(area);
        componentIndex++;
      }
    }
  }

  // Calculate ratios based on circle area
  const circleArea = Math.PI * props.r * props.r;
  const ratios = areas.map(area => area / circleArea);
  emit('ratio', ratios)
};

const animate = () => {
  if (needsUpdate) {
    refreshOverlap();
    needsUpdate = false;
  }
  animationFrameId = requestAnimationFrame(animate);
};

onMounted(() => {
  window.addEventListener('mousemove', updatePosition);
  animationFrameId = requestAnimationFrame(animate);
});

onBeforeUnmount(() => {
  window.removeEventListener('mousemove', updatePosition);
  cancelAnimationFrame(animationFrameId);
});

// Watch for changes in the radius and update canvas size
watch(() => props.r, (newR) => {
  needsUpdate = true; // Trigger an update
});
watch(() => props.canvas, (newR) => {
  needsUpdate = true; // Trigger an update
});
</script>

<template>
  <circle ref="circle" :cx="position.x" :cy="position.y" :r="props.r" stroke="blue" stroke-width="0.02" fill="none" />
  <text x="0" y="0">
    {{ }}
  </text>
</template>

<style scoped>
div {
  margin-top: 10px;
}

svg {
  border: 1px solid #ddd;
}
</style>

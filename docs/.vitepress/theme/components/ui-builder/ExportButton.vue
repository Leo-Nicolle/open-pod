<script lang="ts" setup>
import { ref } from 'vue';
import type { Palette } from './palette';
import { buildExportZip, downloadBlob } from './export';
import { buildLogoHeader, type LogoRaster, type Wordmark } from './logo';

const props = defineProps<{
  palette: Palette;
  logo: LogoRaster;
  squash: LogoRaster;
  /** null until the web font has loaded; logo.h needs it */
  wordmark: Wordmark | null;
}>();

const busy = ref(false);
const status = ref('');

async function onExport() {
  if (!props.wordmark || busy.value) return;
  busy.value = true;
  try {
    const logoH = buildLogoHeader(props.palette, props.logo, props.squash, props.wordmark);
    downloadBlob('opentone-ui.zip', await buildExportZip(props.palette, logoH));
    status.value = 'Downloaded opentone-ui.zip';
  } catch (e) {
    status.value = `Export failed: ${e}`;
  } finally {
    busy.value = false;
    setTimeout(() => (status.value = ''), 2500);
  }
}
</script>

<template>
  <div class="export">
    <button type="button" class="btn-export" :disabled="!wordmark || busy" @click="onExport">
      {{ busy ? 'Exporting…' : 'Export' }}
    </button>
    <p class="hint">{{ status || 'sprites.h · theme.h · logo.h · palette.json · PNGs, in one zip' }}</p>
  </div>
</template>

<style scoped>
.btn-export {
  width: 100%;
  padding: 8px 14px;
  border-radius: 8px;
  background: var(--vp-c-brand-1);
  color: var(--vp-c-white);
  font-weight: 600;
  font-size: 14px;
  transition: background 0.2s;
}
.btn-export:hover:not(:disabled) {
  background: var(--vp-c-brand-2);
}
.btn-export:disabled {
  opacity: 0.6;
  cursor: wait;
}
.hint {
  margin: 6px 0 0;
  font-size: 11px;
  line-height: 1.4;
  color: var(--vp-c-text-3);
}
</style>

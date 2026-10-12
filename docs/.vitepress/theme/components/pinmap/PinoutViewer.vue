<script setup lang="ts">
import { providePinout } from "./usePinout";
import PinoutToolbar from "./PinoutToolbar.vue";
import BoardView from "./BoardView.vue";
import ChipView from "./ChipView.vue";
import PinTable from "./PinTable.vue";
import PinInspector from "./PinInspector.vue";

const s = providePinout();
</script>

<template>
  <div class="pinout-viewer">
    <header class="pv-header">
      <div class="pv-title">
        <span class="pv-kicker">Pinout</span>
        <h2>
          {{ s.pkgInfo.value.chip }}<span v-if="s.pkgInfo.value.board" class="pv-board"> / {{ s.pkgInfo.value.board }}</span>
        </h2>
      </div>
      <p class="pv-blurb">
        {{ s.pkgInfo.value.blurb }} Hover a pin, a peripheral or a legend swatch to trace it; click a pin to inspect it.
      </p>
      <div class="pv-stats">
        <div>
          <span>Assigned</span>
          <strong>{{ s.usedCount.value }}</strong>
        </div>
        <div>
          <span>Free I/O</span>
          <strong class="free">{{ s.statFree.value }}</strong>
        </div>
        <div>
          <span>Flags</span>
          <strong class="flags">{{ s.statIssues.value }}</strong>
        </div>
      </div>
    </header>

    <PinoutToolbar />

    <div class="pv-main">
      <div class="pv-panel">
        <BoardView v-if="s.view.value === 'board'" />
        <ChipView v-else-if="s.view.value === 'chip'" />
        <PinTable v-else />
      </div>
      <PinInspector />
    </div>
  </div>
</template>

<style>
/* Theme tokens, unscoped so they can follow VitePress' html.dark toggle. */
.pinout-viewer {
  --pv-bg: oklch(0.985 0.003 255);
  --pv-panel: oklch(0.97 0.004 255);
  --pv-panel-2: oklch(0.955 0.005 255);
  --pv-border: oklch(0.89 0.008 255);
  --pv-border-strong: oklch(0.78 0.01 255);
  --pv-hover: oklch(0.935 0.008 255);
  --pv-sel: oklch(0.91 0.05 195);
  --pv-text: oklch(0.24 0.015 255);
  --pv-text-2: oklch(0.38 0.012 255);
  --pv-muted: oklch(0.5 0.012 255);
  --pv-muted-2: oklch(0.6 0.01 255);
  --pv-accent: oklch(0.58 0.12 195);
  --pv-on-accent: oklch(0.99 0 0);
  --pv-ok: oklch(0.55 0.13 155);
  --pv-warn: oklch(0.6 0.14 60);
  --pv-error: oklch(0.58 0.17 25);
  --pv-info: oklch(0.55 0.13 250);
  --pv-note: oklch(0.5 0.09 60);
  --pv-mono: "IBM Plex Mono", ui-monospace, monospace;
}
html.dark .pinout-viewer {
  --pv-bg: oklch(0.19 0.012 255);
  --pv-panel: oklch(0.225 0.013 255);
  --pv-panel-2: oklch(0.235 0.014 255);
  --pv-border: oklch(0.3 0.014 255);
  --pv-border-strong: oklch(0.4 0.014 255);
  --pv-hover: oklch(0.255 0.013 255);
  --pv-sel: oklch(0.27 0.02 195);
  --pv-text: oklch(0.94 0.006 255);
  --pv-text-2: oklch(0.8 0.01 255);
  --pv-muted: oklch(0.66 0.012 255);
  --pv-muted-2: oklch(0.58 0.012 255);
  --pv-accent: oklch(0.8 0.15 195);
  --pv-on-accent: oklch(0.19 0.03 195);
  --pv-ok: oklch(0.72 0.13 155);
  --pv-warn: oklch(0.8 0.13 60);
  --pv-error: oklch(0.78 0.13 25);
  --pv-info: oklch(0.72 0.13 250);
  --pv-note: oklch(0.82 0.05 60);
}
</style>

<style scoped>
.pinout-viewer {
  display: flex;
  flex-direction: column;
  gap: 14px;
  padding: 20px 22px 28px;
  border-radius: 14px;
  background: var(--pv-bg);
  color: var(--pv-text);
  font-family: "Space Grotesk", Helvetica, Arial, sans-serif;
  -webkit-font-smoothing: antialiased;
}
.pinout-viewer :deep(button),
.pinout-viewer :deep(input),
.pinout-viewer :deep(select) {
  font-family: inherit;
  box-sizing: border-box;
}
.pinout-viewer :deep(button) {
  cursor: pointer;
}
.pinout-viewer :deep(*)::selection {
  background: color-mix(in oklab, var(--pv-accent) 28%, transparent);
}
.pinout-viewer :deep(*) {
  scrollbar-width: thin;
  scrollbar-color: var(--pv-border-strong) transparent;
}

.pv-header {
  display: flex;
  flex-wrap: wrap;
  align-items: flex-end;
  gap: 16px 24px;
  padding-bottom: 14px;
  border-bottom: 1px solid var(--pv-border);
}
.pv-title {
  display: flex;
  flex-direction: column;
  gap: 4px;
}
.pv-kicker {
  font-family: var(--pv-mono);
  font-size: 11px;
  letter-spacing: 0.14em;
  text-transform: uppercase;
  color: var(--pv-muted);
}
.pv-title h2 {
  margin: 0;
  padding: 0;
  border: 0;
  font-size: 26px;
  font-weight: 600;
  letter-spacing: -0.01em;
  line-height: 1.2;
}
.pv-board {
  font-weight: 400;
  color: var(--pv-muted);
}
.pv-blurb {
  flex: 1 1 280px;
  max-width: 60ch;
  margin: 0;
  font-size: 13px;
  line-height: 1.5;
  color: var(--pv-text-2);
}
.pv-stats {
  display: flex;
  gap: 18px;
  margin-left: auto;
  font-family: var(--pv-mono);
}
.pv-stats div {
  display: flex;
  flex-direction: column;
  gap: 2px;
}
.pv-stats span {
  font-size: 10px;
  letter-spacing: 0.1em;
  text-transform: uppercase;
  color: var(--pv-muted);
}
.pv-stats strong {
  font-size: 20px;
  font-weight: 600;
}
.pv-stats .free {
  color: var(--pv-ok);
}
.pv-stats .flags {
  color: var(--pv-warn);
}

.pv-main {
  display: grid;
  grid-template-columns: minmax(0, 1fr);
  gap: 14px;
  align-items: start;
}
@media (min-width: 1100px) {
  .pv-main {
    grid-template-columns: minmax(0, 1fr) 330px;
  }
  .pv-main > :deep(.inspector) {
    position: sticky;
    top: calc(var(--vp-nav-height, 64px) + 16px);
    max-height: calc(100vh - var(--vp-nav-height, 64px) - 32px);
    overflow-y: auto;
  }
}
.pv-panel {
  min-width: 0;
  padding: 16px;
  background: var(--pv-panel);
  border: 1px solid var(--pv-border);
  border-radius: 12px;
}

@media (max-width: 640px) {
  .pinout-viewer {
    padding: 14px 12px 20px;
    border-radius: 10px;
  }
  .pv-stats {
    margin-left: 0;
  }
}
</style>

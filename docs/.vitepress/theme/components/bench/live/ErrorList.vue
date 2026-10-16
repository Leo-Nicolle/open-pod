<template>
  <div v-for="e in errors" :key="e.id" class="bl-error" role="alert">
    <span class="bl-error-dot" />
    <div class="bl-error-text">
      <div class="bl-error-title">{{ e.title }}</div>
      <div v-if="e.body" class="bl-error-body">
        <template v-for="(part, i) in e.body.split('`')" :key="i">
          <code v-if="i % 2" class="bl-chip-code">{{ part }}</code>
          <template v-else>{{ part }}</template>
        </template>
      </div>
      <div v-if="e.details" class="bl-error-details">{{ e.details }}</div>
    </div>
    <div class="bl-error-actions">
      <button v-if="supported" class="bl-ghost sm" :disabled="connecting" @click="reconnect">Reconnect</button>
      <button class="bl-text-btn" @click="dismiss(e.id)">Dismiss</button>
    </div>
  </div>
</template>

<script setup lang="ts">
import { useBenchLive } from "./state.ts";

const { errors, supported, connecting, reconnect, dismiss } = useBenchLive();
</script>

<style scoped>
/* Errors */
.bl-error {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto;
  gap: 14px;
  align-items: start;
  padding: 14px 16px;
  border-radius: 10px;
  background: var(--bl-redsoft);
}
.bl-error-dot {
  width: 8px;
  height: 8px;
  margin-top: 6px;
  border-radius: 50%;
  background: var(--bl-red);
}
.bl-error-text {
  display: flex;
  flex-direction: column;
  gap: 4px;
}
.bl-error-title {
  font-weight: 600;
  color: var(--bl-red);
}
.bl-error-body {
  max-width: 820px;
  text-wrap: pretty;
}
.bl .bl-chip-code {
  padding: 1px 5px;
  border-radius: 4px;
  background: var(--bl-surface2);
  font-family: var(--bl-mono);
  font-size: 12px;
}
.bl-error-details {
  font-family: var(--bl-mono);
  font-size: 12px;
  color: var(--bl-muted);
  overflow-wrap: anywhere;
}
.bl-error-actions {
  display: flex;
  gap: 8px;
}
.bl-error-actions .bl-text-btn {
  height: 32px;
}
</style>

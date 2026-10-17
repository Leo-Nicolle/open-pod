<script setup lang="ts">
import logo from '../assets/logo.svg';
import { store } from '../store';
import { cardSummary, nav } from './nav';

defineProps<{ brand: boolean }>();
</script>

<template>
  <header>
    <div v-if="brand" class="brand">
      <img :src="logo" alt="" style="height: 22px; width: auto; flex: none" />
      <span>opentone Sync</span>
    </div>
    <nav>
      <button v-for="it in nav" :key="it.id" :class="{ on: store.screen === it.id }" @click="store.screen = it.id">
        {{ it.label }}
      </button>
    </nav>
    <div class="chip mono">
      <span class="dot" :style="{ background: cardSummary.ok ? 'var(--grn)' : 'var(--amb)' }"></span>{{ cardSummary.chip }}
    </div>
  </header>
</template>

<style scoped>
header {
  display: flex;
  align-items: center;
  gap: 18px;
  padding: 0 20px;
  height: 52px;
  border-bottom: 1px solid var(--ln);
  background: var(--side);
  flex: none;
}
.brand {
  display: flex;
  align-items: center;
  gap: 8px;
  flex: none;
  font-weight: 700;
  font-size: 14px;
}
nav {
  display: flex;
  gap: 2px;
  flex: 1;
  min-width: 0;
  overflow-x: auto;
  scrollbar-width: none;
}
nav button {
  padding: 0 12px;
  height: 52px;
  display: flex;
  align-items: center;
  cursor: pointer;
  font-size: 14px;
  white-space: nowrap;
  font-weight: 500;
  color: var(--mu);
  background: none;
  border: none;
}
nav button.on {
  font-weight: 600;
  color: var(--tx);
  box-shadow: inset 0 -2px 0 var(--pri);
}
.chip {
  display: flex;
  align-items: center;
  gap: 7px;
  font-size: 12px;
  color: var(--mu);
  white-space: nowrap;
  flex: none;
}
@media (max-width: 720px) {
  .chip {
    display: none;
  }
}
</style>

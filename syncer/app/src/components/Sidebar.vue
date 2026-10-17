<script setup lang="ts">
import logo from '../assets/logo.svg';
import { store } from '../store';
import { cardSummary, nav } from './nav';

const emit = defineEmits<{ rename: [] }>();
</script>

<template>
  <aside>
    <div class="brand">
      <img :src="logo" alt="" style="height: 28px; width: auto; flex: none" />
      <div class="name">opentone <span>Sync</span></div>
    </div>
    <div class="card">
      <div class="row">
        <span class="label" style="font-size: 11px">Card</span>
        <span class="status" :style="{ color: cardSummary.ok ? 'var(--grn)' : 'var(--amb)' }">
          <span class="dot" :style="{ background: cardSummary.ok ? 'var(--grn)' : 'var(--amb)' }"></span>{{ cardSummary.status }}
        </span>
      </div>
      <div class="cname ellipsis">{{ cardSummary.name }}</div>
      <div v-if="cardSummary.pct !== null" class="bar usage"><div :style="{ width: cardSummary.pct + '%' }"></div></div>
      <div class="mono line">{{ cardSummary.line }}</div>
    </div>
    <nav>
      <button v-for="it in nav" :key="it.id" :class="{ on: store.screen === it.id }" @click="store.screen = it.id">
        <span>{{ it.label }}</span><span class="mono count">{{ it.count }}</span>
      </button>
    </nav>
    <div style="flex: 1"></div>
    <div class="foot">
      <span class="ellipsis">This computer: <span class="mono" style="color: var(--tx)">{{ store.settings?.machine ?? '' }}</span></span>
      <button class="link" style="font-size: 12px" @click="emit('rename')">Rename</button>
    </div>
  </aside>
</template>

<style scoped>
aside {
  width: 232px;
  flex: none;
  display: flex;
  flex-direction: column;
  background: var(--side);
  border-right: 1px solid var(--ln);
  padding: 20px 14px 16px;
}
.brand {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 0 8px 22px;
}
.name {
  font-weight: 700;
  font-size: 16px;
  letter-spacing: -0.01em;
}
.name span {
  font-weight: 400;
  color: var(--mu);
}
.card {
  background: var(--sf);
  border: 1px solid var(--ln);
  border-radius: 10px;
  padding: 14px;
  margin-bottom: 18px;
  display: flex;
  flex-direction: column;
  gap: 9px;
}
.row {
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.status {
  display: flex;
  align-items: center;
  gap: 6px;
  font-size: 12px;
}
.cname {
  font-weight: 600;
  font-size: 15px;
}
.line {
  font-size: 12px;
  color: var(--mu);
  line-height: 1.5;
}
nav {
  display: flex;
  flex-direction: column;
  gap: 2px;
}
nav button {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 9px 10px;
  border-radius: 8px;
  cursor: pointer;
  font-size: 14px;
  font-weight: 500;
  background: transparent;
  color: var(--mu);
  border: none;
  text-align: left;
}
nav button:hover {
  color: var(--tx);
}
nav button.on {
  font-weight: 600;
  background: var(--sf2);
  color: var(--tx);
}
.count {
  font-size: 12px;
  color: var(--mu);
}
.foot {
  white-space: nowrap;
  padding: 12px 8px 0;
  border-top: 1px solid var(--ln);
  font-size: 12px;
  color: var(--mu);
  display: flex;
  justify-content: space-between;
  align-items: center;
  gap: 4px;
}
</style>

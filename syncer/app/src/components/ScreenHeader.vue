<script setup lang="ts">
defineProps<{ title: string; sub?: string }>();
</script>

<template>
  <div class="head">
    <div class="txt">
      <h1>{{ title }}</h1>
      <p v-if="sub">{{ sub }}</p>
    </div>
    <div class="actions"><slot /></div>
  </div>
</template>

<style scoped>
.head {
  display: flex;
  align-items: flex-end;
  justify-content: space-between;
  gap: 20px;
  padding: 24px 32px 20px;
  border-bottom: 1px solid var(--ln);
  flex: none;
}
.txt {
  min-width: 0;
}
h1 {
  margin: 0;
  font-size: 26px;
  font-weight: 600;
  letter-spacing: -0.02em;
}
p {
  margin: 6px 0 0;
  color: var(--mu);
  font-size: 14px;
  line-height: 1.45;
  text-wrap: pretty;
}
.actions {
  display: flex;
  gap: 10px;
  flex: none;
}
</style>

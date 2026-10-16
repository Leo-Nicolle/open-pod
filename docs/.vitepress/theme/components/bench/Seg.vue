<template>
  <div class="seg" :class="size" role="radiogroup" :aria-label="label">
    <button
      v-for="o in options"
      :key="String(o.value)"
      type="button"
      role="radio"
      :aria-checked="o.value === modelValue"
      :class="{ on: o.value === modelValue }"
      @click="emit('update:modelValue', o.value)"
    >
      {{ o.label }}
    </button>
  </div>
</template>

<script setup lang="ts" generic="T extends string | number">
defineProps<{ modelValue: T; options: { value: T; label: string }[]; label?: string; size?: "md" | "sm" | "xs" }>();
const emit = defineEmits<{ "update:modelValue": [value: T] }>();
</script>

<style scoped>
.seg {
  display: flex;
  gap: 2px;
  padding: 2px;
  border: 1px solid var(--bl-line2);
  border-radius: 8px;
  flex: none;
}
button {
  height: 28px;
  padding: 0 10px;
  border: 0;
  border-radius: 6px;
  background: transparent;
  color: var(--bl-muted);
  font: inherit;
  font-size: 12px;
  white-space: nowrap;
  cursor: pointer;
}
.md button {
  font-family: var(--bl-mono);
}
.sm button {
  height: 24px;
  padding: 0 8px;
}
.xs button {
  height: 22px;
  padding: 0 8px;
}
button:hover {
  color: var(--bl-text);
}
button.on {
  background: var(--bl-surface2);
  color: var(--bl-text);
}
</style>

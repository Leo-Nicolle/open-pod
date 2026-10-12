<script setup lang="ts">
import Cover from './Cover.vue';

defineProps<{
  title: string;
  artist: string;
  count: string;
  cover: string;
  isNew?: boolean;
  upgrade?: boolean;
  converted?: boolean;
  tagsGuessed?: boolean;
  /** Select mode: show the check circle. */
  selectable?: boolean;
  selected?: boolean;
}>();
defineEmits<{ open: []; artist: [] }>();
</script>

<template>
  <div
    class="album"
    :class="{ dim: selectable && !selected, clickable: true }"
    role="button"
    tabindex="0"
    @click="$emit('open')"
    @keydown.space.prevent="$emit('open')"
  >
    <Cover :path="cover">
      <span v-if="selectable" class="check" :class="{ on: selected }">{{ selected ? '✓' : '' }}</span>
    </Cover>
    <div class="t ellipsis">{{ title }}</div>
    <div
      class="a ellipsis"
      :class="{ pick: selectable }"
      :title="selectable ? `Select every album by ${artist}` : undefined"
      @click="selectable && ($event.stopPropagation(), $emit('artist'))"
    >
      {{ artist }}
    </div>
    <div class="meta">
      <span class="mono n">{{ count }}</span>
      <span v-if="isNew" class="badge new">new</span>
      <span v-if="upgrade" class="badge upgrade">upgrade</span>
      <span v-if="converted" class="badge converted">converted</span>
      <span v-if="tagsGuessed" class="badge tags">tags guessed</span>
    </div>
  </div>
</template>

<style scoped>
.album {
  display: flex;
  flex-direction: column;
  gap: 6px;
  min-width: 0;
  outline: none;
}
.album.clickable {
  cursor: pointer;
}
.album:focus-visible :deep(.cover) {
  outline: 2px solid var(--pri);
  outline-offset: 2px;
}
.album.dim {
  opacity: 0.55;
}
.t {
  font-size: 14px;
  font-weight: 600;
  margin-top: 4px;
}
.a {
  font-size: 13px;
  color: var(--mu);
  margin-top: -4px;
}
.a.pick:hover {
  color: var(--tx);
  text-decoration: underline;
}
.meta {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
  align-items: center;
}
.n {
  font-size: 12px;
  color: var(--mu);
  margin-right: 4px;
}
.check {
  position: absolute;
  top: 8px;
  right: 8px;
  width: 22px;
  height: 22px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 13px;
  font-weight: 700;
  color: #fff;
  background: rgba(0, 0, 0, 0.25);
  border: 1.5px solid #fff;
}
.check.on {
  background: var(--pri);
  border: none;
}
</style>

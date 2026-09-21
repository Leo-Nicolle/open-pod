<script setup lang="ts">
import { computed } from "vue";
import { useData, withBase } from "vitepress";

interface HeroAction {
  text: string;
  link: string;
  theme?: "brand" | "alt";
}

const { frontmatter } = useData();

const hero = computed(() => (frontmatter.value.hero ?? {}) as Record<string, unknown>);
const name = computed(() => String(hero.value.name ?? frontmatter.value.title ?? ""));
const text = computed(() => String(hero.value.text ?? ""));
const tagline = computed(() => String(hero.value.tagline ?? ""));
const actions = computed(() => (hero.value.actions ?? []) as HeroAction[]);
</script>

<template>
  <div class="PinoutLayout">
    <header class="hero">
      <div class="hero-inner">
        <h1 v-if="name" class="hero-name">{{ name }}</h1>
        <p v-if="text" class="hero-text">{{ text }}</p>
        <p v-if="tagline" class="hero-tagline">{{ tagline }}</p>
        <div v-if="actions.length" class="hero-actions">
          <a
            v-for="a in actions"
            :key="a.link"
            class="hero-action"
            :class="a.theme ?? 'brand'"
            :href="withBase(a.link)"
          >
            {{ a.text }}
          </a>
        </div>
      </div>
    </header>

    <main class="content">
      <Content />
    </main>
  </div>
</template>

<style scoped>
.PinoutLayout {
  min-height: 100vh;
}

/* ------------------------------------------------------------------ hero */
.hero {
  border-bottom: 1px solid var(--vp-c-divider);
  background:
    radial-gradient(ellipse 80% 60% at 50% -10%, var(--vp-c-brand-dimm, transparent), transparent 70%),
    var(--vp-c-bg);
}
.hero-inner {
  margin: 0 auto;
  max-width: 1200px;
  padding: 72px 24px 56px;
}
.hero-name {
  margin: 0;
  font-size: clamp(34px, 6vw, 58px);
  font-weight: 700;
  letter-spacing: -0.02em;
  line-height: 1.1;
  background: linear-gradient(120deg, var(--vp-c-brand) 20%, var(--vp-c-brand-light, #42d392) 80%);
  -webkit-background-clip: text;
  background-clip: text;
  -webkit-text-fill-color: transparent;
}
.hero-text {
  margin: 16px 0 0;
  font-size: clamp(18px, 3vw, 26px);
  font-weight: 600;
  color: var(--vp-c-text-1);
}
.hero-tagline {
  margin: 12px 0 0;
  max-width: 720px;
  font-size: 16px;
  line-height: 1.6;
  color: var(--vp-c-text-2);
}
.hero-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 12px;
  margin-top: 24px;
}
.hero-action {
  display: inline-block;
  padding: 9px 18px;
  border-radius: 20px;
  font-size: 14px;
  font-weight: 500;
  text-decoration: none;
  transition: opacity 0.2s;
}
.hero-action:hover {
  opacity: 0.85;
}
.hero-action.brand {
  background: var(--vp-c-brand);
  color: #fff;
}
.hero-action.alt {
  border: 1px solid var(--vp-c-brand);
  color: var(--vp-c-brand);
}

/* --------------------------------------------------------------- content */
.content {
  margin: 0 auto;
  max-width: 1480px;
  padding: 32px 24px 96px;
}

@media (min-width: 768px) {
  .content {
    padding: 40px 40px 128px;
  }
}
</style>

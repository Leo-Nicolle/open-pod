// Palette state for the UI builder. Edits stay in memory; only save() writes to
// localStorage, and load() reads the saved palette back on mount.
import { computed, onMounted, reactive, ref } from 'vue';
import { DEFAULT_PALETTE, PALETTE_META, normalizeHex, type Palette, type PaletteKey } from './palette';

const STORAGE_KEY = 'openpod-ui-builder-palette';

function readSaved(): Palette | null {
  // Storage can be unavailable (private mode, blocked site data).
  try {
    const raw = JSON.parse(localStorage.getItem(STORAGE_KEY) ?? 'null') as Partial<Palette> | null;
    if (!raw) return null;
    const out = { ...DEFAULT_PALETTE };
    for (const { key } of PALETTE_META) {
      const v = raw[key];
      if (typeof v === 'string') out[key] = normalizeHex(v);
    }
    return out;
  } catch {
    return null;
  }
}

export function usePalette() {
  const palette = reactive<Palette>({ ...DEFAULT_PALETTE });
  /** What a reload would show: the saved palette, or the defaults. */
  const saved = ref<Palette>({ ...DEFAULT_PALETTE });
  const hasSaved = ref(false);

  const dirty = computed(() => PALETTE_META.some(({ key }) => palette[key] !== saved.value[key]));
  const isDefault = computed(() => PALETTE_META.every(({ key }) => palette[key] === DEFAULT_PALETTE[key]));

  // Loaded on mount rather than at setup so SSR and the first client render agree.
  onMounted(() => {
    const s = readSaved();
    if (!s) return;
    saved.value = s;
    hasSaved.value = true;
    Object.assign(palette, s);
  });

  function set(key: PaletteKey, hex: string) {
    palette[key] = normalizeHex(hex);
  }

  function save(): boolean {
    try {
      localStorage.setItem(STORAGE_KEY, JSON.stringify(palette));
    } catch {
      return false;
    }
    saved.value = { ...palette };
    hasSaved.value = true;
    return true;
  }

  function revert() {
    Object.assign(palette, saved.value);
  }

  function reset() {
    Object.assign(palette, DEFAULT_PALETTE);
  }

  return { palette, dirty, isDefault, hasSaved, set, save, revert, reset };
}

export type PaletteStore = ReturnType<typeof usePalette>;

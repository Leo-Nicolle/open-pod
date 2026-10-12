/**
 * Shared reactive state of the pinout viewer. PinoutViewer.vue creates it and
 * provides it; the toolbar, views and inspector inject it. Read-only: the
 * assignments always come from public/pinout/<package>/mapping.json.
 */
import { computed, inject, provide, ref, watch, type InjectionKey } from "vue";
import { PACKAGES, type PackageId } from "./pins";
import {
  buildPins,
  indexByBase,
  firmwareRows,
  usedIndex,
  peripheralList,
  FAMS,
  famRe,
  issueList,
  issuesByBase,
  matchSet,
  CAT,
  mix,
  periphColor,
  type Cat,
} from "./studio";

export interface CellVM {
  key: string;
  base: string;
  label: string;
  num: string;
  title: string;
  style: Record<string, string | number>;
  numStyle: Record<string, string | number>;
  labelStyle: Record<string, string | number>;
}

export type View = "board" | "chip" | "table";
export type Naming = "board" | "mcu" | "signal";
export type ColorMode = "category" | "peripheral" | "conflict";

export function createPinout() {
  const pkg = ref<PackageId>(PACKAGES[0].id);
  const pkgInfo = computed(() => PACKAGES.find((p) => p.id === pkg.value)!);
  const pins = computed(() => buildPins(pkg.value));
  const byBase = computed(() => indexByBase(pins.value));
  const rows = computed(() => firmwareRows(pkg.value));

  const view = ref<View>(pkgInfo.value.board ? "board" : "chip");
  const naming = ref<Naming>("signal");
  const color = ref<ColorMode>("peripheral");
  const flip = ref(false);
  const sel = ref("");
  const hover = ref<string | null>(null);
  const legend = ref<string | null>(null);
  const legendLocked = ref(false);
  const periph = ref<string[]>([]);
  const hoverPeriph = ref<string | null>(null);
  const fn = ref("");
  const hoverFn = ref<string | null>(null);
  const q = ref("");

  const used = computed(() => usedIndex(rows.value));
  const periphNames = computed(() => peripheralList(rows.value));
  const issues = computed(() => issueList(rows.value, byBase.value));
  const issuesByBaseMap = computed(() => issuesByBase(issues.value));
  const matchSetVal = computed(() =>
    matchSet(pins.value, used.value, {
      fn: fn.value,
      hoverFn: hoverFn.value,
      periph: periph.value,
      hoverPeriph: hoverPeriph.value,
      legend: legend.value,
      q: q.value,
    })
  );

  const selPin = computed(() => byBase.value.get(sel.value) ?? null);
  const selUsedRows = computed(() => (selPin.value ? (used.value.get(selPin.value.name) ?? []) : []));

  function defaultSel() {
    return rows.value[0]?.pin ?? pins.value.find((p) => p.cat === "io")?.name ?? "";
  }
  sel.value = defaultSel();

  watch(pkg, () => {
    periph.value = [];
    if (!byBase.value.has(sel.value)) sel.value = defaultSel();
    if (view.value === "board" && !pkgInfo.value.board) view.value = "chip";
  });

  /* ------------------------------------------------------------------ cells */
  function buildCell(opts: { base: string; label: string; num: number | null; key: string; catOverride?: Cat }): CellVM {
    const p = opts.base ? (byBase.value.get(opts.base) ?? null) : null;
    const usedRows = p ? (used.value.get(p.name) ?? []) : [];
    const modeV = color.value;

    const cat: Cat = p ? p.cat : (opts.catOverride ?? "nc");
    let colorStr: string = CAT[cat];
    if (usedRows.length) {
      if (modeV === "peripheral") colorStr = periphColor(usedRows[0].p, periphNames.value);
      else if (modeV === "category") colorStr = CAT.used;
    }
    if (modeV === "conflict") {
      const iss = p ? (issuesByBaseMap.value.get(p.name) ?? []) : [];
      const worst = iss.some((i) => i.sev === "error") ? "error" : iss.some((i) => i.sev === "warn") ? "warn" : null;
      colorStr = worst === "error" ? CAT.power : worst === "warn" ? CAT.reset : usedRows.length ? CAT.io : CAT.nc;
    }

    let text = opts.label;
    if (p) {
      if (naming.value === "mcu") text = p.name;
      else if (naming.value === "signal")
        text = usedRows.length ? usedRows[0].macro || usedRows[0].role.replace(/\s*\(.*\)/, "") : p.name;
    }

    const ms = matchSetVal.value;
    const dim = ms && p ? !ms.has(p.name) : !!ms;
    const isSel = !!p && sel.value === p.name;
    const isHover = !!p && hover.value === p.name;

    const style: Record<string, string | number> = {
      cursor: p ? "pointer" : "default",
      background: usedRows.length ? mix(colorStr, 22) : mix(colorStr, 9),
      border: `1px solid ${mix(colorStr, usedRows.length ? 62 : 30)}`,
      opacity: dim ? 0.22 : 1,
      boxShadow: isSel
        ? "0 0 0 2px var(--pv-accent)"
        : isHover
          ? "0 0 0 1px var(--pv-accent)"
          : ms && !dim
            ? `0 0 0 1px ${mix(colorStr, 80)}`
            : "none",
    };

    const titleBits: string[] = [];
    if (p) titleBits.push(`Pin ${p.num} · ${p.fullName}`);
    if (p && p.ard) titleBits.push(`Arduino ${p.ard}`);
    if (p && p.morpho) titleBits.push(p.morpho);
    if (usedRows.length) titleBits.push(usedRows.map((u) => `${u.p}: ${u.role}`).join(" | "));

    return {
      key: opts.key,
      base: p ? p.name : "",
      label: text || "—",
      num: opts.num == null ? "" : String(opts.num),
      title: titleBits.join(" · ") || opts.label,
      style,
      numStyle: {},
      labelStyle: { fontWeight: usedRows.length ? 600 : 400 },
    };
  }

  /* ------------------------------------------------------------------ stats */
  const usedCount = computed(() => new Set(rows.value.map((r) => r.pin)).size);
  const catCounts = computed(() => {
    const m: Record<string, number> = {};
    for (const p of pins.value) m[p.cat] = (m[p.cat] || 0) + 1;
    return m;
  });
  const statFree = computed(() => Math.max(0, (catCounts.value.io || 0) - usedCount.value));
  const statIssues = computed(() => issues.value.filter((i) => i.sev === "error" || i.sev === "warn").length);

  const legendItems = computed(() => {
    const defs =
      color.value === "conflict"
        ? [{ id: "used", label: "Assigned", color: CAT.io, count: usedCount.value }]
        : [
            { id: "power", label: "Power", color: CAT.power, count: catCounts.value.power || 0 },
            { id: "reset", label: "Reset", color: CAT.reset, count: catCounts.value.reset || 0 },
            { id: "boot", label: "Boot", color: CAT.boot, count: catCounts.value.boot || 0 },
            { id: "debug", label: "Debug", color: CAT.debug, count: catCounts.value.debug || 0 },
            { id: "io", label: "Free I/O", color: CAT.io, count: statFree.value },
            { id: "used", label: "Assigned", color: CAT.used, count: usedCount.value },
          ];
    return defs.map((l) => ({ ...l, active: legend.value === l.id }));
  });

  const periphChips = computed(() =>
    periphNames.value.map((n) => ({
      name: n,
      color: periphColor(n, periphNames.value),
      count: rows.value.filter((r) => r.p === n).length,
      active: periph.value.includes(n) || hoverPeriph.value === n,
    }))
  );

  const fnOptions = computed(() =>
    FAMS.map((f) => ({ id: f, n: pins.value.filter((p) => famRe(f).test(p.blob)).length }))
      .filter((o) => o.n > 0)
      .map((o) => ({ id: o.id, label: `${o.id} (${o.n})` }))
  );

  /* ----------------------------------------------------------------- events */
  function onPinEnter(base: string) {
    hover.value = base || null;
  }
  function onPinLeave() {
    hover.value = null;
  }
  function onPinClick(base: string) {
    if (base) sel.value = base;
  }
  function onLegendEnter(id: string) {
    legend.value = legend.value || id;
  }
  function onLegendLeave() {
    if (!legendLocked.value) legend.value = null;
  }
  function onLegendClick(id: string) {
    legendLocked.value = !(legendLocked.value && legend.value === id);
    legend.value = legendLocked.value ? id : null;
  }
  function onPeriphToggle(name: string) {
    periph.value = periph.value.includes(name) ? periph.value.filter((n) => n !== name) : [...periph.value, name];
  }
  function onClear() {
    q.value = "";
    fn.value = "";
    periph.value = [];
    legend.value = null;
    legendLocked.value = false;
    hoverFn.value = null;
    hoverPeriph.value = null;
  }
  function onAfClick(famId: string) {
    if (!famId) return;
    fn.value = fn.value === famId ? "" : famId;
    hoverFn.value = null;
  }

  return {
    pkg,
    pkgInfo,
    pins,
    byBase,
    rows,
    view,
    naming,
    color,
    flip,
    sel,
    hover,
    legend,
    periph,
    hoverPeriph,
    fn,
    hoverFn,
    q,
    used,
    periphNames,
    issues,
    matchSetVal,
    selPin,
    selUsedRows,
    buildCell,
    usedCount,
    statFree,
    statIssues,
    legendItems,
    periphChips,
    fnOptions,
    onPinEnter,
    onPinLeave,
    onPinClick,
    onLegendEnter,
    onLegendLeave,
    onLegendClick,
    onPeriphToggle,
    onClear,
    onAfClick,
  };
}

export type PinoutState = ReturnType<typeof createPinout>;

const KEY: InjectionKey<PinoutState> = Symbol("pinout");

export function providePinout(): PinoutState {
  const s = createPinout();
  provide(KEY, s);
  return s;
}

export function usePinout(): PinoutState {
  const s = inject(KEY);
  if (!s) throw new Error("usePinout() called outside <PinoutViewer>");
  return s;
}

<script setup lang="ts">
import { computed, onMounted, ref } from "vue";
import { CONNECTORS, type ConnectorPin } from "./board";
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
  SEV_COLOR,
  matchSet,
  exportCpp,
  exportMdRows,
  exportJsonRows,
  parseJsonRows,
  CAT,
  mix,
  periphColor,
  TAB_STYLE,
  CHIP_STYLE,
  type Cat,
  type MappingRow,
} from "./studio";

/* ---------------------------------------------------------------- static data */
const pins = buildPins();
const byBase = indexByBase(pins);
const STORAGE_KEY = "nucleo-pinmap-v1";

const LEFT_IDS = ["CN7", "CN6", "CN8"];
const RIGHT_IDS = ["CN5", "CN9", "CN10"];
const connById = new Map(CONNECTORS.map((c) => [c.id, c]));

/* --------------------------------------------------------------------- state */
const rows = ref<MappingRow[] | null>(null);
const view = ref<"board" | "chip" | "table">("board");
const naming = ref<"board" | "mcu" | "signal">("board");
const color = ref<"category" | "peripheral" | "conflict">("category");
const flip = ref(false);
const sel = ref("PB3");
const hover = ref<string | null>(null);
const legend = ref<string | null>(null);
const legendLocked = ref(false);
const periph = ref<string[]>([]);
const hoverPeriph = ref<string | null>(null);
const fn = ref("");
const hoverFn = ref<string | null>(null);
const q = ref("");
const tab = ref<"map" | "json" | "cpp" | "md">("map");
const jsonBuffer = ref("");
const jsonErr = ref("");
const aPeriphSel = ref("");
const aNewName = ref("");
const aRole = ref("");
const aMacro = ref("");
const copied = ref(false);
let copyTimer: ReturnType<typeof setTimeout> | undefined;

onMounted(() => {
  let saved: MappingRow[] | null = null;
  try {
    const s = localStorage.getItem(STORAGE_KEY);
    if (s) saved = JSON.parse(s);
  } catch {
    /* ignore */
  }
  rows.value = saved && saved.length ? saved : firmwareRows();
});

const effRows = computed(() => rows.value ?? firmwareRows());

function save(next: MappingRow[]) {
  rows.value = next;
  jsonBuffer.value = "";
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(next));
  } catch {
    /* ignore */
  }
}

/* ------------------------------------------------------------------- derived */
const used = computed(() => usedIndex(effRows.value));
const periphNames = computed(() => peripheralList(effRows.value));
const issues = computed(() => issueList(effRows.value, byBase));
const issuesByBaseMap = computed(() => issuesByBase(issues.value));
const matchSetVal = computed(() =>
  matchSet(pins, used.value, {
    fn: fn.value,
    hoverFn: hoverFn.value,
    periph: periph.value,
    hoverPeriph: hoverPeriph.value,
    legend: legend.value,
    q: q.value,
  })
);

const selPin = computed(() => byBase.get(sel.value) ?? null);
const selUsedRows = computed(() => (selPin.value ? (used.value.get(selPin.value.name) ?? []) : []));

/* --------------------------------------------------------------------- cells */
interface CellVM {
  key: string;
  base: string;
  label: string;
  num: string;
  title: string;
  style: Record<string, string | number>;
  numStyle: Record<string, string | number>;
  labelStyle: Record<string, string | number>;
}

function buildCell(opts: { base: string; label: string; num: number | null; key: string; catOverride?: Cat }): CellVM {
  const p = opts.base ? (byBase.get(opts.base) ?? null) : null;
  const usedRows = p ? (used.value.get(p.name) ?? []) : [];
  const modeV = color.value;
  const namingV = naming.value;

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
    if (namingV === "mcu") text = p.name;
    else if (namingV === "signal")
      text = usedRows.length ? usedRows[0].macro || usedRows[0].role.replace(/\s*\(.*\)/, "") : p.name;
  }

  const ms = matchSetVal.value;
  const dim = ms && p ? !ms.has(p.name) : !!ms;
  const isSel = !!p && sel.value === p.name;
  const isHover = !!p && hover.value === p.name;

  const style: Record<string, string | number> = {
    display: "flex",
    alignItems: "center",
    gap: "6px",
    width: "100%",
    textAlign: "left",
    cursor: p ? "pointer" : "default",
    padding: "5px 8px",
    borderRadius: "5px",
    fontFamily: "'IBM Plex Mono', monospace",
    fontSize: "12px",
    lineHeight: 1.25,
    background: usedRows.length ? mix(colorStr, 22) : mix(colorStr, 9),
    border: `1px solid ${mix(colorStr, usedRows.length ? 62 : 30)}`,
    color: "oklch(0.94 0.006 255)",
    opacity: dim ? 0.22 : 1,
    transition: "opacity .12s, box-shadow .12s, background .12s",
    boxShadow: isSel
      ? "0 0 0 2px oklch(0.80 0.15 195)"
      : isHover
        ? "0 0 0 1px oklch(0.80 0.15 195)"
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
    numStyle: { fontSize: "9px", color: "oklch(0.58 0.012 255)", minWidth: "15px", flex: "0 0 auto" },
    labelStyle: {
      fontWeight: usedRows.length ? 600 : 400,
      whiteSpace: "nowrap",
      overflow: "hidden",
      textOverflow: "ellipsis",
    },
  };
}

/* --------------------------------------------------------------- board view */
function connectorVM(id: string) {
  const cn = connById.get(id)!;
  const cols = cn.columns.length;
  const cells: CellVM[] = [];
  cn.columns.forEach((col, ci) => {
    col.forEach((p: ConnectorPin, ri: number) => {
      const base = p.chip && byBase.has(p.chip) ? p.chip : "";
      const cell = buildCell({ base, label: p.label, num: p.pos, key: `${cn.id}-${ci}-${ri}`, catOverride: p.kind });
      if (cols === 2) {
        cell.style.gridColumn = flip.value ? 2 - ci : ci + 1;
        cell.style.gridRow = ri + 1;
      }
      cells.push(cell);
    });
  });
  return {
    id: cn.id,
    title: cn.title,
    cells,
    grid: {
      display: "grid",
      gridTemplateColumns: cols === 2 ? "repeat(2, 104px)" : "112px",
      gap: "3px",
      alignContent: "start",
    },
  };
}

const boardLeft = computed(() => (flip.value ? [...RIGHT_IDS].reverse() : LEFT_IDS).map(connectorVM));
const boardRight = computed(() => (flip.value ? [...LEFT_IDS].reverse() : RIGHT_IDS).map(connectorVM));

/* ---------------------------------------------------------------- chip view */
const chipCells = computed<CellVM[]>(() =>
  pins.map((p) => {
    const label = naming.value === "board" ? p.ard || p.morpho || p.name : p.name;
    const cell = buildCell({ base: p.name, label, num: p.num, key: `ic-${p.num}` });
    const n = p.num;
    const setVertical = (dir: string) => {
      cell.style.flexDirection = dir;
      cell.style.alignItems = "center";
      cell.style.justifyContent = "flex-start";
      cell.style.padding = "5px 2px";
      cell.numStyle = { ...cell.numStyle, minWidth: 0, flex: "0 0 auto" };
      cell.labelStyle = {
        ...cell.labelStyle,
        writingMode: "vertical-rl",
        maxHeight: "104px",
        whiteSpace: "nowrap",
        overflow: "hidden",
        textOverflow: "ellipsis",
      };
    };
    let col: number, row: number;
    if (n <= 16) {
      col = 1;
      row = n + 1;
      cell.style.flexDirection = "row";
    } else if (n <= 32) {
      col = n - 15;
      row = 18;
      setVertical("column");
    } else if (n <= 48) {
      col = 18;
      row = 18 - (n - 32);
      cell.style.flexDirection = "row-reverse";
      cell.style.textAlign = "right";
    } else {
      col = 18 - (n - 48);
      row = 1;
      setVertical("column-reverse");
    }
    cell.style.gridColumn = flip.value ? 19 - col : col;
    cell.style.gridRow = row;
    cell.style.overflow = "hidden";
    return cell;
  })
);
const chipGrid = {
  display: "grid",
  gridTemplateColumns: "140px repeat(16, 34px) 140px",
  gridTemplateRows: "140px repeat(16, 26px) 140px",
  gap: "2px",
};
const chipCaption = computed(
  () => `LQFP64 · pin 1 ${flip.value ? "top-right · clockwise" : "top-left · counter-clockwise"}`
);
const chipDotStyle = computed(() => ({
  position: "absolute",
  top: "10px",
  left: flip.value ? "auto" : "10px",
  right: flip.value ? "10px" : "auto",
  width: "9px",
  height: "9px",
  borderRadius: "50%",
  background: "oklch(0.60 0.012 255)",
}));

/* --------------------------------------------------------------- table view */
const tableRows = computed(() => {
  const ms = matchSetVal.value;
  return pins
    .filter((p) => !ms || ms.has(p.name))
    .map((p) => {
      const rs = used.value.get(p.name) ?? [];
      return {
        n: p.num,
        base: p.name,
        name: p.fullName,
        type: p.cat === "io" ? `I/O · ${p.type}` : p.cat.charAt(0).toUpperCase() + p.cat.slice(1),
        board: [p.ard, p.morpho].filter(Boolean).join(" · ") || "—",
        used: rs.map((u) => `${u.p} · ${u.role}`).join(" | ") || "—",
        usedColor: rs.length ? "oklch(0.86 0.10 195)" : "oklch(0.60 0.012 255)",
        af: p.af.concat(p.add).join(", ") || "—",
        style: {
          background:
            sel.value === p.name
              ? "oklch(0.27 0.02 195)"
              : hover.value === p.name
                ? "oklch(0.255 0.013 255)"
                : "transparent",
          cursor: "pointer",
          borderBottom: "1px solid oklch(0.26 0.013 255)",
        },
      };
    });
});

/* -------------------------------------------------------------------- stats */
const usedCount = computed(() => new Set(effRows.value.map((r) => r.pin)).size);
const catCounts = computed(() => {
  const m: Record<string, number> = {};
  for (const p of pins) m[p.cat] = (m[p.cat] || 0) + 1;
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
          { id: "io", label: "Free I/O", color: CAT.io, count: (catCounts.value.io || 0) - usedCount.value },
          { id: "used", label: "Assigned", color: CAT.used, count: usedCount.value },
        ];
  return defs.map((l) => ({ ...l, style: CHIP_STYLE(legend.value === l.id, l.color) }));
});

const periphChips = computed(() =>
  periphNames.value.map((n) => ({
    name: n,
    color: periphColor(n, periphNames.value),
    count: effRows.value.filter((r) => r.p === n).length,
    style: CHIP_STYLE(periph.value.includes(n) || hoverPeriph.value === n, periphColor(n, periphNames.value)),
  }))
);

const fnOptions = computed(() =>
  FAMS.map((f) => ({ id: f, label: `${f} (${pins.filter((p) => famRe(f).test(p.blob)).length})` })).filter(
    (o) => !o.label.includes("(0)")
  )
);

/* ---------------------------------------------------------------------- tabs */
const viewTabs = computed(() =>
  (
    [
      ["board", "Nucleo board"],
      ["chip", "LQFP64 chip"],
      ["table", "Pin table"],
    ] as const
  ).map(([id, label]) => ({ id, label, style: TAB_STYLE(view.value === id) }))
);
const nameTabs = computed(() =>
  (
    [
      ["board", "Board"],
      ["mcu", "MCU"],
      ["signal", "Signal"],
    ] as const
  ).map(([id, label]) => ({ id, label, style: TAB_STYLE(naming.value === id) }))
);
const colorTabs = computed(() =>
  (
    [
      ["category", "Type"],
      ["peripheral", "Peripheral"],
      ["conflict", "Checks"],
    ] as const
  ).map(([id, label]) => ({ id, label, style: TAB_STYLE(color.value === id) }))
);
const workTabs = computed(() =>
  (
    [
      ["map", "Mapping"],
      ["json", "JSON"],
      ["cpp", "C++"],
      ["md", "Markdown"],
    ] as const
  ).map(([id, label]) => ({ id, label, style: TAB_STYLE(tab.value === id) }))
);

const clearStyle = computed(() => ({
  padding: "7px 12px",
  borderRadius: "7px",
  cursor: "pointer",
  fontSize: "12px",
  background: matchSetVal.value ? "oklch(0.80 0.15 195)" : "oklch(0.235 0.014 255)",
  border: `1px solid ${matchSetVal.value ? "transparent" : "oklch(0.32 0.014 255)"}`,
  color: matchSetVal.value ? "oklch(0.19 0.03 195)" : "oklch(0.70 0.012 255)",
  fontWeight: matchSetVal.value ? 600 : 400,
}));
const flipStyle = computed(() => ({
  padding: "7px 12px",
  borderRadius: "7px",
  cursor: "pointer",
  fontSize: "12px",
  background: flip.value ? mix("oklch(0.72 0.13 310)", 26) : "oklch(0.235 0.014 255)",
  border: `1px solid ${flip.value ? mix("oklch(0.72 0.13 310)", 70) : "oklch(0.32 0.014 255)"}`,
  color: flip.value ? "oklch(0.94 0.006 255)" : "oklch(0.70 0.012 255)",
}));

/* ----------------------------------------------------------------- inspector */
const selIoText = computed(() => {
  if (!selPin.value) return "";
  const t = selPin.value.type;
  return t === "FT"
    ? "FT — 5 V tolerant"
    : t === "TTa"
      ? "TTa — 3.3 V only, analog capable"
      : t === "FTf"
        ? "FTf — 5 V tolerant, FM+ I2C"
        : "supply / control";
});
const selBadgeStyle = computed(() => {
  if (!selPin.value) return {};
  const c = CAT[selPin.value.cat];
  return {
    fontFamily: "'IBM Plex Mono', monospace",
    fontSize: "10px",
    padding: "3px 7px",
    borderRadius: "4px",
    background: mix(c, 24),
    border: `1px solid ${mix(c, 60)}`,
    color: "oklch(0.94 0.006 255)",
  };
});
const selAf = computed(() => {
  if (!selPin.value) return [];
  return selPin.value.af.map((t) => {
    const fam = FAMS.find((f) => famRe(f).test(t)) || "";
    const on = (hoverFn.value || fn.value) === fam && !!fam;
    return {
      t,
      fam,
      style: {
        padding: "3px 7px",
        borderRadius: "5px",
        cursor: fam ? "pointer" : "default",
        fontFamily: "'IBM Plex Mono', monospace",
        fontSize: "10.5px",
        background: on ? "oklch(0.80 0.15 195)" : "oklch(0.28 0.013 255)",
        border: `1px solid ${on ? "transparent" : "oklch(0.34 0.014 255)"}`,
        color: on ? "oklch(0.19 0.03 195)" : "oklch(0.84 0.010 255)",
      },
    };
  });
});
const selUsed = computed(() =>
  selUsedRows.value.map((u) => {
    const c = periphColor(u.p, periphNames.value);
    return {
      ...u,
      peripheral: u.p,
      color: c,
      style: {
        display: "flex",
        alignItems: "center",
        gap: "8px",
        padding: "6px 8px",
        borderRadius: "6px",
        background: mix(c, 16),
        border: `1px solid ${mix(c, 40)}`,
      },
    };
  })
);
const selSuggest = computed(() =>
  selPin.value
    ? selPin.value.af
        .filter((t) => /_/.test(t) && t !== "EVENTOUT")
        .slice(0, 6)
        .map((t) => ({ t }))
    : []
);
const aPeriph = computed(() => aPeriphSel.value || periphNames.value[0] || "");
const aIsNew = computed(() => aPeriph.value === "__new");

/* ---------------------------------------------------------------- workbench */
const editRows = computed(() =>
  effRows.value.map((r, i) => {
    const ri = issues.value.filter((x) => x.base === r.pin);
    const bad = ri.find((x) => x.sev === "error") || ri.find((x) => x.sev === "warn");
    return {
      ...r,
      i,
      pinBorder: byBase.has(r.pin) ? "oklch(0.30 0.014 255)" : "oklch(0.60 0.13 25)",
      check: bad ? bad.msg : "",
      checkColor: bad ? SEV_COLOR[bad.sev] : "oklch(0.60 0.012 255)",
      style: { background: sel.value === r.pin ? "oklch(0.27 0.02 195)" : "oklch(0.205 0.012 255)" },
    };
  })
);
const pinNames = pins.map((p) => p.name);
const mapStatus = computed(() => `${periphNames.value.length} peripherals · ${effRows.value.length} assignments`);
const jsonText = computed(() => jsonBuffer.value || exportJsonRows(effRows.value));
const jsonErrColor = computed(() => (jsonErr.value ? "oklch(0.78 0.13 25)" : "oklch(0.60 0.012 255)"));
const cppOut = computed(() => exportCpp(effRows.value));
const mdOut = computed(() => exportMdRows(effRows.value));
const codeOut = computed(() => (tab.value === "cpp" ? cppOut.value : mdOut.value));
const showCopy = computed(() => tab.value !== "map");
const copyLabel = computed(() => (copied.value ? "Copied" : "Copy"));

/* ------------------------------------------------------------------ issues UI */
const issueRows = computed(() =>
  issues.value.map((s) => ({
    ...s,
    color: SEV_COLOR[s.sev],
    style: {
      display: "flex",
      gap: "8px",
      alignItems: "flex-start",
      padding: "7px 8px",
      borderRadius: "7px",
      cursor: "pointer",
      width: "100%",
      background: sel.value === s.base ? "oklch(0.27 0.02 195)" : "oklch(0.205 0.012 255)",
      border: `1px solid ${
        s.sev === "error"
          ? mix("oklch(0.72 0.13 25)", 45)
          : s.sev === "warn"
            ? mix("oklch(0.72 0.13 60)", 40)
            : "oklch(0.29 0.013 255)"
      }`,
    },
  }))
);
const issueCountLabel = computed(() => (issues.value.length ? `${issues.value.length} notes` : ""));

/* ------------------------------------------------------------------- events */
function onPinEnter(base: string) {
  hover.value = base || null;
}
function onPinLeave() {
  hover.value = null;
}
function onPinClick(base: string) {
  if (!base) return;
  sel.value = base;
  aRole.value = "";
  aMacro.value = "";
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
  const i = periph.value.indexOf(name);
  if (i < 0) periph.value = [...periph.value, name];
  else periph.value = periph.value.filter((n) => n !== name);
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
function onSuggest(t: string) {
  aRole.value = t;
}
function onAssign() {
  if (!selPin.value) return;
  const name = aIsNew.value ? aNewName.value || "New peripheral" : aPeriph.value;
  const bus = effRows.value.find((r) => r.p === name)?.bus || "";
  save([
    ...effRows.value,
    { p: name, bus, pin: selPin.value.name, role: aRole.value || "GPIO", macro: aMacro.value },
  ]);
  aRole.value = "";
  aMacro.value = "";
  aNewName.value = "";
}
function onCell(i: number, field: keyof MappingRow, value: string) {
  save(effRows.value.map((r, k) => (k === i ? { ...r, [field]: value } : r)));
}
function onDelRow(i: number) {
  save(effRows.value.filter((_, k) => k !== i));
}
function onAddRow() {
  save([
    ...effRows.value,
    { p: periphNames.value[0] || "New peripheral", bus: "", pin: "PA0", role: "GPIO", macro: "" },
  ]);
}
function onResetFw() {
  save(firmwareRows());
  jsonErr.value = "";
}
function onJsonInput(e: Event) {
  jsonBuffer.value = (e.target as HTMLTextAreaElement).value;
  jsonErr.value = "";
}
function onApplyJson() {
  try {
    const next = parseJsonRows(jsonBuffer.value || jsonText.value);
    save(next);
    jsonErr.value = `Applied — ${next.length} assignments.`;
  } catch (err) {
    jsonErr.value = `Invalid JSON: ${(err as Error).message}`;
  }
}
function onCopy() {
  const text = tab.value === "json" ? jsonText.value : tab.value === "cpp" ? cppOut.value : mdOut.value;
  try {
    navigator.clipboard.writeText(text);
  } catch {
    /* ignore */
  }
  copied.value = true;
  clearTimeout(copyTimer);
  copyTimer = setTimeout(() => (copied.value = false), 1400);
}
</script>

<template>
  <div
    class="pinout-studio"
    style="
      background: oklch(0.19 0.012 255);
      color: oklch(0.94 0.006 255);
      font-family: 'Space Grotesk', Helvetica, Arial, sans-serif;
      -webkit-font-smoothing: antialiased;
      border-radius: 14px;
      padding: 20px 22px 28px;
      display: flex;
      flex-direction: column;
      gap: 14px;
    "
  >
    <!-- Header -->
    <header
      style="
        display: flex;
        flex-wrap: wrap;
        align-items: flex-end;
        gap: 16px 24px;
        border-bottom: 1px solid oklch(0.3 0.014 255);
        padding-bottom: 14px;
      "
    >
      <div style="display: flex; flex-direction: column; gap: 4px">
        <span
          style="
            font-family: 'IBM Plex Mono', monospace;
            font-size: 11px;
            letter-spacing: 0.14em;
            text-transform: uppercase;
            color: oklch(0.66 0.012 255);
          "
          >Pinout studio</span
        >
        <h1 style="margin: 0; font-size: 26px; font-weight: 600; letter-spacing: -0.01em">
          STM32F446RE <span style="color: oklch(0.66 0.012 255); font-weight: 400">/ Nucleo-F446RE</span>
        </h1>
      </div>
      <p style="margin: 0; max-width: 52ch; font-size: 13px; line-height: 1.5; color: oklch(0.72 0.012 255)">
        LQFP64 as wired on the MP3 project board. Hover any pin or legend swatch to trace it; click to pin it open.
        Edits are kept locally and export to C++, Markdown or JSON.
      </p>
      <div style="margin-left: auto; display: flex; gap: 18px; font-family: 'IBM Plex Mono', monospace; font-size: 12px">
        <div style="display: flex; flex-direction: column; gap: 2px">
          <span style="color: oklch(0.66 0.012 255); font-size: 10px; letter-spacing: 0.1em; text-transform: uppercase"
            >Assigned</span
          >
          <span style="font-size: 20px; font-weight: 600">{{ usedCount }}</span>
        </div>
        <div style="display: flex; flex-direction: column; gap: 2px">
          <span style="color: oklch(0.66 0.012 255); font-size: 10px; letter-spacing: 0.1em; text-transform: uppercase"
            >Free I/O</span
          >
          <span style="font-size: 20px; font-weight: 600; color: oklch(0.72 0.13 155)">{{ statFree }}</span>
        </div>
        <div style="display: flex; flex-direction: column; gap: 2px">
          <span style="color: oklch(0.66 0.012 255); font-size: 10px; letter-spacing: 0.1em; text-transform: uppercase"
            >Flags</span
          >
          <span style="font-size: 20px; font-weight: 600; color: oklch(0.72 0.13 60)">{{ statIssues }}</span>
        </div>
      </div>
    </header>

    <!-- Toolbar -->
    <div
      style="
        display: flex;
        flex-wrap: wrap;
        gap: 10px 14px;
        align-items: center;
        padding: 10px 12px;
        background: oklch(0.235 0.014 255);
        border: 1px solid oklch(0.3 0.014 255);
        border-radius: 10px;
      "
    >
      <div style="display: flex; gap: 2px; padding: 2px; background: oklch(0.19 0.012 255); border-radius: 7px">
        <button
          v-for="t in viewTabs"
          :key="t.id"
          type="button"
          :style="t.style"
          @click="view = t.id as typeof view"
        >
          {{ t.label }}
        </button>
      </div>
      <label
        style="
          display: flex;
          align-items: center;
          gap: 7px;
          font-size: 11px;
          letter-spacing: 0.08em;
          text-transform: uppercase;
          color: oklch(0.66 0.012 255);
        "
      >
        Label
        <div style="display: flex; gap: 2px; padding: 2px; background: oklch(0.19 0.012 255); border-radius: 7px">
          <button
            v-for="t in nameTabs"
            :key="t.id"
            type="button"
            :style="t.style"
            @click="naming = t.id as typeof naming"
          >
            {{ t.label }}
          </button>
        </div>
      </label>
      <label
        style="
          display: flex;
          align-items: center;
          gap: 7px;
          font-size: 11px;
          letter-spacing: 0.08em;
          text-transform: uppercase;
          color: oklch(0.66 0.012 255);
        "
      >
        Colour
        <div style="display: flex; gap: 2px; padding: 2px; background: oklch(0.19 0.012 255); border-radius: 7px">
          <button
            v-for="t in colorTabs"
            :key="t.id"
            type="button"
            :style="t.style"
            @click="color = t.id as typeof color"
          >
            {{ t.label }}
          </button>
        </div>
      </label>
      <input
        v-model="q"
        type="search"
        placeholder="Search pin, signal, macro…"
        style="
          flex: 1 1 200px;
          min-width: 160px;
          padding: 7px 10px;
          background: oklch(0.19 0.012 255);
          border: 1px solid oklch(0.32 0.014 255);
          border-radius: 7px;
          color: oklch(0.94 0.006 255);
          font-family: 'IBM Plex Mono', monospace;
          font-size: 12px;
        "
      />
      <label
        style="
          display: flex;
          align-items: center;
          gap: 7px;
          font-size: 11px;
          letter-spacing: 0.08em;
          text-transform: uppercase;
          color: oklch(0.66 0.012 255);
        "
      >
        Function
        <select
          v-model="fn"
          style="
            padding: 7px 8px;
            background: oklch(0.19 0.012 255);
            border: 1px solid oklch(0.32 0.014 255);
            border-radius: 7px;
            color: oklch(0.94 0.006 255);
            font-family: 'IBM Plex Mono', monospace;
            font-size: 12px;
            text-transform: none;
            letter-spacing: 0;
          "
        >
          <option value="">All</option>
          <option v-for="o in fnOptions" :key="o.id" :value="o.id">{{ o.label }}</option>
        </select>
      </label>
      <button
        type="button"
        title="Mirror the layout — as seen from the solder side"
        :style="flipStyle"
        @click="flip = !flip"
      >
        {{ flip ? "Bottom view" : "Top view" }}
      </button>
      <button type="button" :style="clearStyle" @click="onClear">Clear</button>
    </div>

    <!-- Filter bar -->
    <div style="display: flex; flex-wrap: wrap; gap: 6px; align-items: center">
      <span
        style="
          font-family: 'IBM Plex Mono', monospace;
          font-size: 10px;
          letter-spacing: 0.12em;
          text-transform: uppercase;
          color: oklch(0.6 0.012 255);
          margin-right: 4px;
        "
        >Peripherals</span
      >
      <button
        v-for="c in periphChips"
        :key="c.name"
        type="button"
        :style="c.style"
        @click="onPeriphToggle(c.name)"
        @mouseenter="hoverPeriph = c.name"
        @mouseleave="hoverPeriph = null"
      >
        <span style="width: 8px; height: 8px; border-radius: 2px" :style="{ background: c.color }"></span>{{ c.name }}
        <span style="opacity: 0.55; font-size: 10px">{{ c.count }}</span>
      </button>
      <span style="width: 1px; height: 20px; background: oklch(0.3 0.014 255); margin: 0 6px"></span>
      <button
        v-for="l in legendItems"
        :key="l.id"
        type="button"
        :style="l.style"
        @mouseenter="onLegendEnter(l.id)"
        @mouseleave="onLegendLeave"
        @click="onLegendClick(l.id)"
      >
        <span style="width: 8px; height: 8px; border-radius: 2px" :style="{ background: l.color }"></span>{{ l.label }}
        <span style="opacity: 0.55; font-size: 10px">{{ l.count }}</span>
      </button>
    </div>

    <!-- Main -->
    <div style="display: flex; flex-wrap: wrap; gap: 14px; align-items: flex-start">
      <div
        style="
          flex: 1 1 640px;
          min-width: 0;
          background: oklch(0.225 0.013 255);
          border: 1px solid oklch(0.3 0.014 255);
          border-radius: 12px;
          padding: 16px;
          overflow: auto;
        "
      >
        <!-- Board view -->
        <div v-if="view === 'board'" style="gap: 12px; align-items: flex-start; justify-content: center; flex-wrap: wrap"
        
        class="board-container"
        >
          <div v-for="cn in boardLeft" :key="cn.id" 
          :class="`${cn.id}`"
          style="display: flex; flex-direction: column; gap: 6px">
            <span
              style="
                font-family: 'IBM Plex Mono', monospace;
                font-size: 10px;
                letter-spacing: 0.1em;
                text-transform: uppercase;
                color: oklch(0.62 0.012 255);
              "
              >{{ cn.title }}</span
            >
            <div :style="cn.grid">
              <button
                v-for="c in cn.cells"
                :key="c.key"
                type="button"
                :title="c.title"
                :style="c.style"
                @mouseenter="onPinEnter(c.base)"
                @mouseleave="onPinLeave"
                @click="onPinClick(c.base)"
              >
                <span :style="c.numStyle">{{ c.num }}</span>
                <span :style="c.labelStyle">{{ c.label }}</span>
              </button>
            </div>
          </div>
          <div v-for="cn in boardRight"
          :class="`${cn.id}`"
          :key="cn.id" style="display: flex; flex-direction: column; gap: 6px">
            <span
              style="
                font-family: 'IBM Plex Mono', monospace;
                font-size: 10px;
                letter-spacing: 0.1em;
                text-transform: uppercase;
                color: oklch(0.62 0.012 255);
              "
              >{{ cn.title }}</span
            >
            <div :style="cn.grid">
              <button
                v-for="c in cn.cells"
                :key="c.key"
                type="button"
                :title="c.title"
                :style="c.style"
                @mouseenter="onPinEnter(c.base)"
                @mouseleave="onPinLeave"
                @click="onPinClick(c.base)"
              >
                <span :style="c.numStyle">{{ c.num }}</span>
                <span :style="c.labelStyle">{{ c.label }}</span>
              </button>
            </div>
          </div>
        </div>

        <!-- Chip view -->
        <div v-else-if="view === 'chip'" style="display: flex; justify-content: center; overflow: auto">
          <div :style="chipGrid">
            <div
              style="
                grid-column: 2 / span 16;
                grid-row: 2 / span 16;
                border: 1px solid oklch(0.4 0.014 255);
                background: oklch(0.26 0.012 255);
                border-radius: 6px;
                display: flex;
                flex-direction: column;
                align-items: center;
                justify-content: center;
                gap: 6px;
                position: relative;
              "
            >
              <span :style="chipDotStyle"></span>
              <span style="font-family: 'IBM Plex Mono', monospace; font-size: 15px; font-weight: 600; letter-spacing: 0.02em"
                >STM32F446RET6</span
              >
              <span style="font-family: 'IBM Plex Mono', monospace; font-size: 11px; color: oklch(0.66 0.012 255)">{{
                chipCaption
              }}</span>
            </div>
            <button
              v-for="c in chipCells"
              :key="c.key"
              type="button"
              :title="c.title"
              :style="c.style"
              @mouseenter="onPinEnter(c.base)"
              @mouseleave="onPinLeave"
              @click="onPinClick(c.base)"
            >
              <span :style="c.numStyle">{{ c.num }}</span>
              <span :style="c.labelStyle">{{ c.label }}</span>
            </button>
          </div>
        </div>

        <!-- Table view -->
        <div v-else style="overflow: auto; max-height: 70vh">
          <table style="width: 100%; border-collapse: collapse; font-size: 12px">
            <thead>
              <tr
                style="
                  text-align: left;
                  color: oklch(0.66 0.012 255);
                  font-family: 'IBM Plex Mono', monospace;
                  font-size: 10px;
                  letter-spacing: 0.08em;
                  text-transform: uppercase;
                "
              >
                <th style="padding: 6px 8px; position: sticky; top: 0; background: oklch(0.225 0.013 255); border-bottom: 1px solid oklch(0.32 0.014 255)">
                  #
                </th>
                <th style="padding: 6px 8px; position: sticky; top: 0; background: oklch(0.225 0.013 255); border-bottom: 1px solid oklch(0.32 0.014 255)">
                  Pin
                </th>
                <th style="padding: 6px 8px; position: sticky; top: 0; background: oklch(0.225 0.013 255); border-bottom: 1px solid oklch(0.32 0.014 255)">
                  Type
                </th>
                <th style="padding: 6px 8px; position: sticky; top: 0; background: oklch(0.225 0.013 255); border-bottom: 1px solid oklch(0.32 0.014 255)">
                  Board
                </th>
                <th style="padding: 6px 8px; position: sticky; top: 0; background: oklch(0.225 0.013 255); border-bottom: 1px solid oklch(0.32 0.014 255)">
                  Used as
                </th>
                <th style="padding: 6px 8px; position: sticky; top: 0; background: oklch(0.225 0.013 255); border-bottom: 1px solid oklch(0.32 0.014 255)">
                  Alternate functions
                </th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="r in tableRows"
                :key="r.base"
                :style="r.style"
                @mouseenter="onPinEnter(r.base)"
                @mouseleave="onPinLeave"
                @click="onPinClick(r.base)"
              >
                <td style="padding: 5px 8px; font-family: 'IBM Plex Mono', monospace; color: oklch(0.62 0.012 255)">{{ r.n }}</td>
                <td style="padding: 5px 8px; font-family: 'IBM Plex Mono', monospace; font-weight: 600">{{ r.name }}</td>
                <td style="padding: 5px 8px; color: oklch(0.72 0.012 255)">{{ r.type }}</td>
                <td style="padding: 5px 8px; font-family: 'IBM Plex Mono', monospace; color: oklch(0.72 0.012 255)">{{ r.board }}</td>
                <td style="padding: 5px 8px" :style="{ color: r.usedColor }">{{ r.used }}</td>
                <td style="padding: 5px 8px; font-family: 'IBM Plex Mono', monospace; font-size: 11px; color: oklch(0.66 0.012 255); line-height: 1.5">
                  {{ r.af }}
                </td>
              </tr>
            </tbody>
          </table>
        </div>
      </div>

      <!-- Aside -->
      <aside style="flex: 0 0 330px; max-width: 100%; display: flex; flex-direction: column; gap: 12px; position: sticky; top: 16px">
        <div style="background: oklch(0.235 0.014 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 12px; padding: 14px">
          <div v-if="selPin">
            <div style="display: flex; align-items: center; gap: 8px; flex-wrap: wrap; margin-bottom: 12px">
              <span
                style="
                  font-family: 'IBM Plex Mono', monospace;
                  font-size: 10px;
                  padding: 2px 6px;
                  border-radius: 4px;
                  background: oklch(0.3 0.014 255);
                  color: oklch(0.76 0.01 255);
                "
                >Pin {{ selPin.num }}</span
              >
              <h2 style="margin: 0; font-family: 'IBM Plex Mono', monospace; font-size: 19px; font-weight: 600; letter-spacing: -0.01em">
                {{ selPin.fullName }}
              </h2>
              <span :style="selBadgeStyle">{{
                selPin.cat === "io" ? `I/O · ${selPin.type}` : selPin.cat.toUpperCase()
              }}</span>
            </div>
            <div style="display: flex; flex-direction: column; gap: 7px; font-size: 12px">
              <div style="display: flex; gap: 10px">
                <span style="flex: 0 0 78px; color: oklch(0.64 0.012 255); font-size: 11px; letter-spacing: 0.06em; text-transform: uppercase; padding-top: 1px"
                  >Board</span
                >
                <span style="font-family: 'IBM Plex Mono', monospace">{{
                  [selPin.ard, selPin.morpho].filter(Boolean).join(" · ") || "not on a header"
                }}</span>
              </div>
              <div style="display: flex; gap: 10px">
                <span style="flex: 0 0 78px; color: oklch(0.64 0.012 255); font-size: 11px; letter-spacing: 0.06em; text-transform: uppercase; padding-top: 1px"
                  >Default</span
                >
                <span>{{ selPin.dflt }}</span>
              </div>
              <div style="display: flex; gap: 10px">
                <span style="flex: 0 0 78px; color: oklch(0.64 0.012 255); font-size: 11px; letter-spacing: 0.06em; text-transform: uppercase; padding-top: 1px"
                  >I/O</span
                >
                <span>{{ selIoText }}</span>
              </div>
              <div style="display: flex; gap: 10px">
                <span style="flex: 0 0 78px; color: oklch(0.64 0.012 255); font-size: 11px; letter-spacing: 0.06em; text-transform: uppercase; padding-top: 3px"
                  >Alternate</span
                >
                <div style="display: flex; flex-wrap: wrap; gap: 4px; flex: 1">
                  <button
                    v-for="a in selAf"
                    :key="a.t"
                    type="button"
                    :style="a.style"
                    @mouseenter="hoverFn = a.fam || null"
                    @mouseleave="hoverFn = null"
                    @click="onAfClick(a.fam)"
                  >
                    {{ a.t }}
                  </button>
                </div>
              </div>
              <div v-if="selPin.note" style="display: flex; gap: 10px">
                <span style="flex: 0 0 78px; color: oklch(0.64 0.012 255); font-size: 11px; letter-spacing: 0.06em; text-transform: uppercase; padding-top: 1px"
                  >Board</span
                >
                <span style="font-size: 11.5px; line-height: 1.45; color: oklch(0.82 0.05 60)">{{ selPin.note }}</span>
              </div>
              <div v-if="selPin.add.length" style="display: flex; gap: 10px">
                <span style="flex: 0 0 78px; color: oklch(0.64 0.012 255); font-size: 11px; letter-spacing: 0.06em; text-transform: uppercase; padding-top: 1px"
                  >Extra</span
                >
                <span style="font-family: 'IBM Plex Mono', monospace; font-size: 11px; color: oklch(0.8 0.01 255)">{{
                  selPin.add.join(", ")
                }}</span>
              </div>
            </div>

            <div style="margin-top: 14px; padding-top: 12px; border-top: 1px solid oklch(0.3 0.014 255)">
              <span style="font-size: 11px; letter-spacing: 0.08em; text-transform: uppercase; color: oklch(0.64 0.012 255)">Assignments</span>
              <div style="display: flex; flex-direction: column; gap: 5px; margin-top: 8px">
                <div v-for="u in selUsed" :key="u.i" :style="u.style">
                  <span style="width: 8px; height: 8px; border-radius: 2px; flex: 0 0 auto" :style="{ background: u.color }"></span>
                  <span style="font-size: 12px; font-weight: 500">{{ u.peripheral }}</span>
                  <span style="font-family: 'IBM Plex Mono', monospace; font-size: 11px; color: oklch(0.78 0.01 255); flex: 1; text-align: left">{{
                    u.role
                  }}</span>
                  <button
                    type="button"
                    title="Unassign"
                    style="border: none; background: transparent; color: oklch(0.66 0.012 255); cursor: pointer; font-size: 14px; line-height: 1; padding: 2px 4px"
                    @click.stop="onDelRow(u.i)"
                  >
                    ×
                  </button>
                </div>
                <span v-if="selUsed.length === 0" style="font-size: 12px; color: oklch(0.64 0.012 255)">Unassigned — free for use.</span>
              </div>
              <div style="display: flex; flex-wrap: wrap; gap: 6px; margin-top: 10px; align-items: center">
                <select
                  :value="aPeriph"
                  style="flex: 1 1 120px; padding: 6px 7px; background: oklch(0.19 0.012 255); border: 1px solid oklch(0.32 0.014 255); border-radius: 6px; color: oklch(0.94 0.006 255); font-size: 12px"
                  @change="aPeriphSel = ($event.target as HTMLSelectElement).value"
                >
                  <option v-for="p in periphNames" :key="p" :value="p">{{ p }}</option>
                  <option value="__new">+ New peripheral…</option>
                </select>
                <input
                  v-if="aIsNew"
                  v-model="aNewName"
                  type="text"
                  placeholder="Name"
                  style="flex: 1 1 100px; padding: 6px 7px; background: oklch(0.19 0.012 255); border: 1px solid oklch(0.32 0.014 255); border-radius: 6px; color: oklch(0.94 0.006 255); font-size: 12px"
                />
                <input
                  v-model="aRole"
                  type="text"
                  placeholder="Role e.g. SPI1_SCK"
                  style="flex: 1 1 130px; padding: 6px 7px; background: oklch(0.19 0.012 255); border: 1px solid oklch(0.32 0.014 255); border-radius: 6px; color: oklch(0.94 0.006 255); font-family: 'IBM Plex Mono', monospace; font-size: 12px"
                />
                <input
                  v-model="aMacro"
                  type="text"
                  placeholder="MACRO"
                  style="flex: 1 1 90px; padding: 6px 7px; background: oklch(0.19 0.012 255); border: 1px solid oklch(0.32 0.014 255); border-radius: 6px; color: oklch(0.94 0.006 255); font-family: 'IBM Plex Mono', monospace; font-size: 12px"
                />
                <button
                  type="button"
                  style="padding: 6px 12px; background: oklch(0.8 0.15 195); border: none; border-radius: 6px; color: oklch(0.19 0.03 195); font-weight: 600; font-size: 12px; cursor: pointer"
                  @click="onAssign"
                >
                  Assign
                </button>
              </div>
              <div style="display: flex; flex-wrap: wrap; gap: 4px; margin-top: 8px">
                <button
                  v-for="s in selSuggest"
                  :key="s.t"
                  type="button"
                  style="padding: 3px 7px; background: oklch(0.27 0.013 255); border: 1px dashed oklch(0.4 0.014 255); border-radius: 5px; color: oklch(0.8 0.01 255); font-family: 'IBM Plex Mono', monospace; font-size: 10px; cursor: pointer"
                  @click="onSuggest(s.t)"
                >
                  {{ s.t }}
                </button>
              </div>
            </div>
          </div>
          <p v-else style="margin: 0; font-size: 13px; color: oklch(0.66 0.012 255); line-height: 1.5">
            Select a pin to inspect its functions and assign it.
          </p>
        </div>

        <div style="background: oklch(0.235 0.014 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 12px; padding: 14px">
          <div style="display: flex; align-items: baseline; gap: 8px; margin-bottom: 10px">
            <span style="font-size: 11px; letter-spacing: 0.08em; text-transform: uppercase; color: oklch(0.64 0.012 255)">Design checks</span>
            <span style="font-family: 'IBM Plex Mono', monospace; font-size: 11px; color: oklch(0.62 0.012 255); margin-left: auto">{{
              issueCountLabel
            }}</span>
          </div>
          <div style="display: flex; flex-direction: column; gap: 6px">
            <button
              v-for="s in issueRows"
              :key="`${s.base}-${s.msg}`"
              type="button"
              :style="s.style"
              @mouseenter="onPinEnter(s.base)"
              @mouseleave="onPinLeave"
              @click="onPinClick(s.base)"
            >
              <span style="font-family: 'IBM Plex Mono', monospace; font-size: 11px; font-weight: 600; flex: 0 0 42px; text-align: left" :style="{ color: s.color }">{{
                s.base
              }}</span>
              <span style="font-size: 11.5px; line-height: 1.45; color: oklch(0.84 0.01 255); text-align: left">{{ s.msg }}</span>
            </button>
            <span v-if="issueRows.length === 0" style="font-size: 12px; color: oklch(0.72 0.13 155)">No conflicts detected.</span>
          </div>
        </div>
      </aside>
    </div>

    <!-- Workbench -->
    <div style="background: oklch(0.225 0.013 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 12px; padding: 14px">
      <div style="display: flex; flex-wrap: wrap; gap: 10px; align-items: center; margin-bottom: 12px">
        <div style="display: flex; gap: 2px; padding: 2px; background: oklch(0.19 0.012 255); border-radius: 7px">
          <button v-for="t in workTabs" :key="t.id" type="button" :style="t.style" @click="tab = t.id as typeof tab">
            {{ t.label }}
          </button>
        </div>
        <span style="font-family: 'IBM Plex Mono', monospace; font-size: 11px; color: oklch(0.62 0.012 255)">{{ mapStatus }}</span>
        <div style="margin-left: auto; display: flex; gap: 6px">
          <button
            type="button"
            style="padding: 6px 11px; background: oklch(0.27 0.013 255); border: 1px solid oklch(0.36 0.014 255); border-radius: 6px; color: oklch(0.88 0.01 255); font-size: 12px; cursor: pointer"
            @click="onResetFw"
          >
            Reset to firmware
          </button>
          <button
            v-if="showCopy"
            type="button"
            style="padding: 6px 11px; background: oklch(0.8 0.15 195); border: none; border-radius: 6px; color: oklch(0.19 0.03 195); font-weight: 600; font-size: 12px; cursor: pointer"
            @click="onCopy"
          >
            {{ copyLabel }}
          </button>
        </div>
      </div>

      <div v-if="tab === 'map'">
        <div style="overflow: auto; max-height: 52vh">
          <table style="width: 100%; border-collapse: separate; border-spacing: 0 4px; font-size: 12px">
            <thead>
              <tr
                style="
                  text-align: left;
                  color: oklch(0.64 0.012 255);
                  font-family: 'IBM Plex Mono', monospace;
                  font-size: 10px;
                  letter-spacing: 0.08em;
                  text-transform: uppercase;
                "
              >
                <th style="padding: 0 8px 4px">Peripheral</th>
                <th style="padding: 0 8px 4px">Bus</th>
                <th style="padding: 0 8px 4px">Pin</th>
                <th style="padding: 0 8px 4px">Role</th>
                <th style="padding: 0 8px 4px">Macro</th>
                <th style="padding: 0 8px 4px">Check</th>
                <th></th>
              </tr>
            </thead>
            <tbody>
              <tr v-for="r in editRows" :key="r.i" :style="r.style">
                <td style="padding: 3px 4px">
                  <input
                    type="text"
                    :value="r.p"
                    style="width: 100%; min-width: 110px; padding: 5px 7px; background: oklch(0.19 0.012 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 5px; color: oklch(0.94 0.006 255); font-size: 12px"
                    @input="onCell(r.i, 'p', ($event.target as HTMLInputElement).value)"
                  />
                </td>
                <td style="padding: 3px 4px">
                  <input
                    type="text"
                    :value="r.bus"
                    style="width: 100%; min-width: 90px; padding: 5px 7px; background: oklch(0.19 0.012 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 5px; color: oklch(0.86 0.01 255); font-family: 'IBM Plex Mono', monospace; font-size: 11px"
                    @input="onCell(r.i, 'bus', ($event.target as HTMLInputElement).value)"
                  />
                </td>
                <td style="padding: 3px 4px">
                  <input
                    type="text"
                    list="nucleo-pins"
                    :value="r.pin"
                    style="width: 100%; min-width: 82px; padding: 5px 7px; background: oklch(0.19 0.012 255); border-radius: 5px; color: oklch(0.94 0.006 255); font-family: 'IBM Plex Mono', monospace; font-size: 11px"
                    :style="{ border: `1px solid ${r.pinBorder}` }"
                    @input="onCell(r.i, 'pin', ($event.target as HTMLInputElement).value)"
                  />
                </td>
                <td style="padding: 3px 4px">
                  <input
                    type="text"
                    :value="r.role"
                    style="width: 100%; min-width: 150px; padding: 5px 7px; background: oklch(0.19 0.012 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 5px; color: oklch(0.94 0.006 255); font-family: 'IBM Plex Mono', monospace; font-size: 11px"
                    @input="onCell(r.i, 'role', ($event.target as HTMLInputElement).value)"
                  />
                </td>
                <td style="padding: 3px 4px">
                  <input
                    type="text"
                    :value="r.macro"
                    placeholder="—"
                    style="width: 100%; min-width: 110px; padding: 5px 7px; background: oklch(0.19 0.012 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 5px; color: oklch(0.86 0.01 255); font-family: 'IBM Plex Mono', monospace; font-size: 11px"
                    @input="onCell(r.i, 'macro', ($event.target as HTMLInputElement).value)"
                  />
                </td>
                <td style="padding: 3px 8px; font-size: 11px; max-width: 220px; line-height: 1.4" :style="{ color: r.checkColor }">
                  {{ r.check }}
                </td>
                <td style="padding: 3px 4px">
                  <button
                    type="button"
                    title="Delete row"
                    style="border: 1px solid oklch(0.32 0.014 255); background: oklch(0.22 0.013 255); color: oklch(0.7 0.012 255); border-radius: 5px; cursor: pointer; padding: 4px 8px; font-size: 12px"
                    @click="onDelRow(r.i)"
                  >
                    ×
                  </button>
                </td>
              </tr>
            </tbody>
          </table>
        </div>
        <datalist id="nucleo-pins">
          <option v-for="n in pinNames" :key="n" :value="n"></option>
        </datalist>
        <button
          type="button"
          style="margin-top: 10px; padding: 7px 12px; background: oklch(0.27 0.013 255); border: 1px dashed oklch(0.4 0.014 255); border-radius: 7px; color: oklch(0.86 0.01 255); font-size: 12px; cursor: pointer"
          @click="onAddRow"
        >
          + Add mapping row
        </button>
      </div>

      <div v-else-if="tab === 'json'" style="display: flex; flex-direction: column; gap: 8px">
        <textarea
          spellcheck="false"
          rows="16"
          :value="jsonText"
          style="width: 100%; padding: 12px; background: oklch(0.17 0.012 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 8px; color: oklch(0.9 0.01 255); font-family: 'IBM Plex Mono', monospace; font-size: 12px; line-height: 1.55; resize: vertical"
          @input="onJsonInput"
        ></textarea>
        <div style="display: flex; gap: 8px; align-items: center">
          <button
            type="button"
            style="padding: 7px 14px; background: oklch(0.8 0.15 195); border: none; border-radius: 6px; color: oklch(0.19 0.03 195); font-weight: 600; font-size: 12px; cursor: pointer"
            @click="onApplyJson"
          >
            Apply
          </button>
          <span style="font-family: 'IBM Plex Mono', monospace; font-size: 11px" :style="{ color: jsonErrColor }">{{ jsonErr }}</span>
        </div>
      </div>

      <pre
        v-else
        style="margin: 0; padding: 14px; background: oklch(0.17 0.012 255); border: 1px solid oklch(0.3 0.014 255); border-radius: 8px; color: oklch(0.9 0.01 255); font-family: 'IBM Plex Mono', monospace; font-size: 12px; line-height: 1.6; overflow: auto; max-height: 52vh"
        >{{ codeOut }}</pre
      >
    </div>
  </div>
</template>

<style scoped>
.pinout-studio :deep(a) {
  color: oklch(0.8 0.15 195);
  text-decoration: none;
}
.pinout-studio :deep(input),
.pinout-studio :deep(select),
.pinout-studio :deep(textarea),
.pinout-studio :deep(button) {
  font-family: inherit;
  font-size: inherit;
}
.pinout-studio :deep(button) {
  box-sizing: border-box;
}
.pinout-studio :deep(*)::selection {
  background: oklch(0.8 0.15 195 / 0.28);
}
.pinout-studio :deep(*)::-webkit-scrollbar {
  width: 10px;
  height: 10px;
}
.pinout-studio :deep(*)::-webkit-scrollbar-thumb {
  background: oklch(0.34 0.015 255);
  border-radius: 6px;
}
.pinout-studio :deep(*)::-webkit-scrollbar-track {
  background: transparent;
}

.board-container{
  display: grid;
  grid-template-columns: repeat(4, 4fr);
  grid-template-rows: 1fr 1fr;
  align-items: start;
  gap: 12px;
}
.CN7{
  grid-area: 1 / 1 / 3 / 2 ;
}
.CN6{
 grid-area: 1 / 2 / 2 / 3;
}
.CN8{
 grid-area: 2 / 2 / 3 / 3;

}

.CN5{
 grid-area: 1 / 3 / 2 / 4;

}
.CN9{
 grid-area: 2 / 3 / 3 / 4;

}
.CN10{
  grid-area: 1 / 4 / 3 / 5 ;

}
</style>

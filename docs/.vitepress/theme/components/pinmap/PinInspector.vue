<script setup lang="ts">
import { computed } from "vue";
import { busFor } from "./buses";
import { CAT, FAMS, SEV_COLOR, famRe, mix, periphColor } from "./studio";
import { usePinout } from "./usePinout";

const s = usePinout();
const pin = s.selPin;

const ioText = computed(() => {
  const t = pin.value?.type;
  return t === "FT"
    ? "FT: 5 V tolerant"
    : t === "TTa"
      ? "TTa: 3.3 V only, analog capable"
      : t === "FTf"
        ? "FTf: 5 V tolerant, FM+ I2C"
        : "supply / control";
});
const badgeColor = computed(() => (pin.value ? CAT[pin.value.cat] : ""));
const afs = computed(() =>
  (pin.value?.af ?? []).map((t) => {
    const fam = FAMS.find((f) => famRe(f).test(t)) || "";
    return { t, fam, on: !!fam && (s.hoverFn.value || s.fn.value) === fam };
  })
);
const assignments = computed(() =>
  s.selUsedRows.value.map((u) => ({ ...u, color: periphColor(u.p, s.periphNames.value) }))
);
/** Bus limits of every peripheral this pin can carry (buses.json). */
const buses = computed(() => {
  if (!pin.value) return [];
  const blob = pin.value.af.concat(pin.value.add).join(" ");
  return FAMS.filter((f) => famRe(f).test(blob))
    .map((f) => busFor(f))
    .filter((b) => !!b)
    .map((b) => ({ ...b!, title: `${b!.detail} (${b!.source})` }));
});
const issues = computed(() => s.issues.value.map((i) => ({ ...i, color: SEV_COLOR[i.sev] })));
</script>

<template>
  <aside class="inspector">
    <details class="card" open>
      <summary>
        <span class="label">Pin</span>
        <span v-if="pin" class="mono muted small">{{ pin.fullName }}</span>
      </summary>
      <template v-if="pin">
        <div class="head">
          <span class="pin-num">Pin {{ pin.num }}</span>
          <h3>{{ pin.fullName }}</h3>
          <span class="badge" :style="{ background: mix(badgeColor, 24), borderColor: mix(badgeColor, 60) }">
            {{ pin.cat === "io" ? `I/O · ${pin.type}` : pin.cat.toUpperCase() }}
          </span>
        </div>

        <div class="assign">
          <div v-for="u in assignments" :key="u.i" class="assign-row" :style="{ '--c': u.color }">
            <span class="swatch"></span>
            <span class="assign-p">{{ u.p }}</span>
            <span class="assign-role">{{ u.role }}</span>
            <code v-if="u.macro" class="assign-macro">{{ u.macro }}</code>
          </div>
          <span v-if="assignments.length === 0" class="muted">Not used: free for future use.</span>
        </div>

        <dl>
          <template v-if="pin.ard || pin.morpho">
            <dt>Board</dt>
            <dd class="mono">{{ [pin.ard, pin.morpho].filter(Boolean).join(" · ") }}</dd>
          </template>
          <dt>Default</dt>
          <dd>{{ pin.dflt }}</dd>
          <dt>I/O</dt>
          <dd>{{ ioText }}</dd>
          <template v-if="pin.maxSpeed">
            <dt>Speed</dt>
            <dd>{{ pin.maxSpeed }}</dd>
          </template>
          <template v-if="pin.domain">
            <dt>Supply</dt>
            <dd>{{ pin.domain }}</dd>
          </template>
          <template v-if="afs.length">
            <dt>Alternate</dt>
            <dd class="afs">
              <button
                v-for="a in afs"
                :key="a.t"
                type="button"
                class="af"
                :class="{ on: a.on, clickable: !!a.fam }"
                @mouseenter="s.hoverFn.value = a.fam || null"
                @mouseleave="s.hoverFn.value = null"
                @click="s.onAfClick(a.fam)"
              >
                {{ a.t }}
              </button>
            </dd>
          </template>
          <template v-if="pin.add.length">
            <dt>Extra</dt>
            <dd class="mono small">{{ pin.add.join(", ") }}</dd>
          </template>
          <template v-if="buses.length">
            <dt>Buses</dt>
            <dd class="buses">
              <span v-for="b in buses" :key="b.family" :title="b.title" class="mono small">
                {{ b.family }} · {{ b.bus }} · <span class="accent">{{ b.maxRate }}</span>
              </span>
            </dd>
          </template>
          <template v-if="pin.note">
            <dt>Note</dt>
            <dd class="warn small">{{ pin.note }}</dd>
          </template>
          <template v-if="pin.remarks.length">
            <dt>Datasheet</dt>
            <dd class="warn small">
              <ul>
                <li v-for="r in pin.remarks" :key="r">{{ r }}</li>
              </ul>
            </dd>
          </template>
        </dl>
      </template>
      <p v-else class="muted">Select a pin to see its functions.</p>
    </details>

    <details class="card">
      <summary>
        <span class="label">Design checks</span>
        <span class="mono muted small">{{ issues.length ? `${issues.length} notes` : "none" }}</span>
      </summary>
      <div class="checks">
        <button
          v-for="i in issues"
          :key="`${i.base}-${i.msg}`"
          type="button"
          class="check"
          :class="[i.sev, { sel: s.sel.value === i.base }]"
          @mouseenter="s.onPinEnter(i.base)"
          @mouseleave="s.onPinLeave()"
          @click="s.onPinClick(i.base)"
        >
          <span class="check-pin" :style="{ color: i.color }">{{ i.base }}</span>
          <span class="check-msg">{{ i.msg }}</span>
        </button>
        <span v-if="issues.length === 0" class="ok small">No conflicts detected.</span>
      </div>
    </details>
  </aside>
</template>

<style scoped>
.inspector {
  display: flex;
  flex-direction: column;
  gap: 12px;
  min-width: 0;
}
.card {
  background: var(--pv-panel-2);
  border: 1px solid var(--pv-border);
  border-radius: 12px;
  padding: 14px;
}
.head {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
  margin-bottom: 12px;
}
.head h3 {
  margin: 0;
  font-family: var(--pv-mono);
  font-size: 19px;
  font-weight: 600;
  letter-spacing: -0.01em;
  word-break: break-all;
}
.pin-num {
  padding: 2px 6px;
  border-radius: 4px;
  background: var(--pv-border);
  color: var(--pv-text-2);
  font-family: var(--pv-mono);
  font-size: 10px;
}
.badge {
  padding: 3px 7px;
  border: 1px solid;
  border-radius: 4px;
  font-family: var(--pv-mono);
  font-size: 10px;
}
.assign {
  display: flex;
  flex-direction: column;
  gap: 5px;
  margin-bottom: 14px;
}
.assign-row {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 6px 8px;
  padding: 6px 8px;
  border-radius: 6px;
  background: color-mix(in oklab, var(--c) 16%, var(--pv-bg));
  border: 1px solid color-mix(in oklab, var(--c) 40%, var(--pv-bg));
}
.swatch {
  flex: 0 0 auto;
  width: 8px;
  height: 8px;
  border-radius: 2px;
  background: var(--c);
}
.assign-p {
  font-size: 12px;
  font-weight: 600;
}
.assign-role {
  flex: 1;
  min-width: 0;
  font-family: var(--pv-mono);
  font-size: 11px;
  color: var(--pv-text-2);
}
.assign-macro {
  font-size: 10.5px;
  color: var(--pv-accent);
  background: none;
  padding: 0;
}
dl {
  display: grid;
  grid-template-columns: 78px minmax(0, 1fr);
  gap: 7px 10px;
  margin: 0;
  font-size: 12px;
}
dt {
  padding-top: 1px;
  color: var(--pv-muted);
  font-size: 11px;
  letter-spacing: 0.06em;
  text-transform: uppercase;
}
dd {
  margin: 0;
  min-width: 0;
}
dd ul {
  margin: 0;
  padding-left: 14px;
}
.afs {
  display: flex;
  flex-wrap: wrap;
  gap: 4px;
}
.af {
  padding: 3px 7px;
  border-radius: 5px;
  font-family: var(--pv-mono);
  font-size: 10.5px;
  background: var(--pv-panel);
  border: 1px solid var(--pv-border);
  color: var(--pv-text-2);
  cursor: default;
}
.af.clickable {
  cursor: pointer;
}
.af.on {
  background: var(--pv-accent);
  border-color: transparent;
  color: var(--pv-on-accent);
}
.buses {
  display: flex;
  flex-direction: column;
  gap: 3px;
}
.mono {
  font-family: var(--pv-mono);
}
.small {
  font-size: 11.5px;
  line-height: 1.45;
}
.muted {
  margin: 0;
  color: var(--pv-muted);
  font-size: 12px;
}
.accent {
  color: var(--pv-accent);
}
.warn {
  color: var(--pv-note);
}
.ok {
  color: var(--pv-ok);
}
.label {
  font-size: 11px;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--pv-muted);
}
summary {
  display: flex;
  align-items: baseline;
  gap: 8px;
  cursor: pointer;
  list-style: none;
  user-select: none;
}
summary::-webkit-details-marker {
  display: none;
}
summary::before {
  content: "";
  align-self: center;
  width: 6px;
  height: 6px;
  margin-right: 2px;
  border-right: 1.5px solid var(--pv-muted);
  border-bottom: 1.5px solid var(--pv-muted);
  transform: rotate(-45deg);
  transition: transform 0.15s;
}
details[open] > summary::before {
  transform: rotate(45deg);
}
summary .small {
  margin-left: auto;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
details[open] > summary {
  margin-bottom: 12px;
}
.checks {
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.check {
  display: flex;
  align-items: flex-start;
  gap: 8px;
  width: 100%;
  padding: 7px 8px;
  border-radius: 7px;
  text-align: left;
  background: var(--pv-bg);
  border: 1px solid var(--pv-border);
}
.check.error {
  border-color: color-mix(in oklab, var(--pv-error) 45%, var(--pv-bg));
}
.check.warn {
  border-color: color-mix(in oklab, var(--pv-warn) 40%, var(--pv-bg));
}
.check.sel {
  background: var(--pv-sel);
}
.check-pin {
  flex: 0 0 42px;
  font-family: var(--pv-mono);
  font-size: 11px;
  font-weight: 600;
}
.check-msg {
  font-size: 11.5px;
  line-height: 1.45;
  color: var(--pv-text-2);
}
</style>

<template>
  <div class="bl-kpis">
    <div class="bl-kpi">
      <div class="bl-kpi-k">Screen</div>
      <div class="bl-kpi-v text">{{ st.header || routeName(st.route) }}</div>
      <div class="bl-kpi-s">{{ st.item ? `▸ ${st.item}` : " " }}</div>
    </div>
    <div class="bl-kpi">
      <div class="bl-kpi-k">Now playing</div>
      <div class="bl-kpi-play">
        <span class="bl-kpi-glyph">{{ st.playing === "1" ? "▶" : "❚❚" }}</span>
        <span class="bl-kpi-v track">{{ st.now || "—" }}</span>
      </div>
      <div class="bl-kpi-s bl-mono">{{ clock(+st.pos) }} / {{ clock(+st.dur) }} · vol {{ st.vol ?? "–" }}</div>
    </div>
    <div class="bl-kpi">
      <div class="bl-kpi-k">
        Buffer runway
        <span v-if="lastAudio?.refilling" class="bl-badge brand">refilling</span>
      </div>
      <div class="bl-kpi-v num">{{ lastAudio ? lastAudio.runwayS.toFixed(1) : "—" }}<span>s</span></div>
      <div class="bl-kpi-s bl-mono">{{ lastAudio ? Math.round(lastAudio.rateKBs) : "—" }} KB/s in</div>
    </div>
    <div class="bl-kpi">
      <div class="bl-kpi-k">Underruns</div>
      <div class="bl-kpi-v num" :class="{ bad: underrunTotal > 0 }">
        {{ underrunTotal }}<span v-if="underrunTotal > 0" class="bl-kpi-arrow" aria-hidden="true">▲</span>
      </div>
      <div class="bl-kpi-s">{{ underrunTotal > 0 ? "glitches since connecting" : "none since connecting" }}</div>
    </div>
    <div class="bl-kpi">
      <div class="bl-kpi-k">Library</div>
      <div class="bl-kpi-v num">{{ info.tracks ? (+info.tracks).toLocaleString("en-US") : "—" }}<span>tracks</span></div>
      <div class="bl-kpi-s bl-mono">
        {{ info.bootMs ? `boot ${(+info.bootMs / 1000).toFixed(2)} s · lookup ${info.lookupMs} ms` : " " }}
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { clock } from "../format.ts";
import { useBenchLive } from "./state.ts";

const { st, info, lastAudio, underrunTotal } = useBenchLive();

const ROUTES = ["Main menu", "Search", "Genres", "Artists", "Albums", "Tracks", "Now Playing", "Settings"];
const routeName = (r?: string) => (r === undefined ? "—" : (ROUTES[+r] ?? `screen ${r}`));
</script>

<style scoped>
/* KPIs */
.bl-kpis {
  display: flex;
  flex-wrap: wrap;
  gap: 1px;
  background: var(--bl-line);
  border: 1px solid var(--bl-line);
  border-radius: 12px;
  overflow: hidden;
}
.bl-kpi {
  flex: 1 1 190px;
  display: flex;
  flex-direction: column;
  gap: 4px;
  min-width: 0;
  padding: 16px 18px;
  background: var(--bl-surface);
}
.bl-kpi-k {
  display: flex;
  justify-content: space-between;
  align-items: center;
  min-height: 20px;
  font-size: 12px;
  color: var(--bl-muted);
}
.bl-kpi-v,
.bl-kpi-s {
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}
.bl-kpi-v.text {
  font-size: 22px;
  font-weight: 600;
  letter-spacing: -0.02em;
  line-height: 1.3;
}
.bl-kpi-v.track {
  font-size: 20px;
  font-weight: 600;
  letter-spacing: -0.02em;
  line-height: 1.43;
}
.bl-kpi-v.num {
  font-family: var(--bl-mono);
  font-size: 28px;
  font-weight: 500;
  line-height: 1.15;
}
.bl-kpi-v.num span {
  margin-left: 4px;
  font-size: 15px;
  color: var(--bl-muted);
}
.bl-kpi-v.bad,
.bl-kpi-v.bad .bl-kpi-arrow {
  color: var(--bl-red);
}
.bl-kpi-arrow {
  margin-left: 6px !important;
}
.bl-kpi-play {
  display: flex;
  align-items: center;
  gap: 8px;
  min-width: 0;
}
.bl-kpi-glyph {
  width: 16px;
  flex: none;
  font-size: 13px;
  color: var(--bl-brand);
}
.bl-kpi-s {
  font-size: 12px;
  color: var(--bl-muted);
}
</style>

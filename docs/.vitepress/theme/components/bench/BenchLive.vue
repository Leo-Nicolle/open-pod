<template>
  <div class="bl vp-raw" :style="{ '--bl-status-h': `${statusH}px` }">
    <!-- Status bar -->
    <header ref="statusEl" class="bl-status">
      <div class="bl-brand">
        <img :src="withBase('/openpod_logo.svg')" alt="" />
        <span><b>OpenPod</b> <span class="bl-muted">BenchLive</span></span>
      </div>
      <span class="bl-divider" />
      <template v-if="!supported">
        <span class="bl-live"><i />Unavailable in this browser</span>
      </template>
      <template v-else-if="!device">
        <button class="bl-primary" :disabled="connecting" @click="connect()">
          {{ connecting ? "Connecting…" : "Connect to the Nucleo" }}
        </button>
        <span class="bl-small bl-muted">Flash the bench firmware first: <code class="bl-mono">pio run -e bench -t upload</code></span>
      </template>
      <template v-else>
        <span class="bl-live" :class="{ on: alive }">
          <i /><b>{{ alive ? "Live" : "No data for 3 s" }}</b>
          <span class="bl-mono bl-muted">{{ portLabel }}</span>
        </span>
        <span v-if="info.rev" class="bl-small bl-muted">firmware <span class="bl-mono bl-text">{{ info.rev }}</span></span>
      </template>
      <span class="bl-spacer" />
      <div v-if="device" class="bl-window">
        <span class="bl-small bl-muted">Window</span>
        <Seg v-model="windowS" :options="WINDOWS" label="Time window" size="md" />
      </div>
      <div class="bl-actions">
        <button class="bl-ghost" @click="isDark = !isDark">{{ isDark ? "Light mode" : "Dark mode" }}</button>
        <template v-if="device">
          <button class="bl-ghost" @click="clearCharts">Clear</button>
          <button class="bl-ghost" @click="disconnect">Disconnect</button>
        </template>
      </div>
    </header>

    <div class="bl-page">
      <div v-if="!supported" class="bl-note red">
        <b>Web Serial isn't available.</b> This page talks to the Nucleo over Web Serial, which only Chrome and Edge
        have (on desktop). <a href="?demo">Try the demo</a> instead.
      </div>

      <!-- Errors -->
      <div v-for="e in errors" :key="e.id" class="bl-error" role="alert">
        <span class="bl-error-dot" />
        <div class="bl-error-text">
          <div class="bl-error-title">{{ e.title }}</div>
          <div v-if="e.body" class="bl-error-body">
            <template v-for="(part, i) in e.body.split('`')" :key="i">
              <code v-if="i % 2" class="bl-chip-code">{{ part }}</code>
              <template v-else>{{ part }}</template>
            </template>
          </div>
          <div v-if="e.details" class="bl-error-details">{{ e.details }}</div>
        </div>
        <div class="bl-error-actions">
          <button v-if="supported" class="bl-ghost sm" :disabled="connecting" @click="reconnect">Reconnect</button>
          <button class="bl-text-btn" @click="dismiss(e.id)">Dismiss</button>
        </div>
      </div>

      <template v-if="device">
        <!-- KPIs -->
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

        <!-- Tabs -->
        <div class="bl-tabs" role="tablist">
          <button
            v-for="t in tabs"
            :key="t.id"
            role="tab"
            :aria-selected="tab === t.id"
            :class="{ on: tab === t.id }"
            @click="tab = t.id"
          >
            {{ t.label }}<span v-if="t.count" class="bl-tab-count">{{ t.count }}</span>
          </button>
        </div>

        <!-- Live: remote + charts -->
        <div v-if="tab === 'live'" class="bl-live-grid">
          <section class="bl-panel bl-remote" @wheel="onWheel">
            <span v-if="busy" class="bl-badge amber bl-locked">locked · run in progress</span>
            <div class="bl-remote-body" :class="{ locked: busy }" :inert="busy">
              <div class="bl-wheel-wrap">
                <div
                  ref="wheelEl"
                  class="bl-wheel"
                  :class="{ dragging: dragAngle !== null }"
                  role="group"
                  aria-label="Click wheel: press a button, or drag around the ring to turn it"
                >
                  <button
                    v-for="w in WEDGES"
                    :key="w.name"
                    class="bl-wedge"
                    :class="[w.cls, { lit: lit(w.name) }]"
                    :aria-label="w.label"
                    @pointerdown="gestureDown(w.name, $event)"
                    @pointermove="gestureMove"
                    @pointerup="gestureUp"
                    @pointercancel="gestureUp"
                    @keydown.enter.prevent="click(w.name)"
                    @keydown.space.prevent="click(w.name)"
                  >
                    {{ w.text }}
                  </button>
                  <button
                    class="bl-center"
                    :class="{ lit: lit('select') }"
                    aria-label="Select (right-click for a long press)"
                    @pointerdown="gestureDown('select', $event)"
                    @pointerup="gestureUp"
                    @pointercancel="gestureUp"
                    @contextmenu.prevent="longPress"
                    @keydown.enter.prevent="click('select')"
                    @keydown.space.prevent="click('select')"
                  >
                    SELECT
                  </button>
                  <svg v-if="trailSegments.length || thumb" class="bl-trail" viewBox="0 0 280 280" aria-hidden="true">
                    <line
                      v-for="(l, i) in trailSegments"
                      :key="i"
                      :x1="l.x1"
                      :y1="l.y1"
                      :x2="l.x2"
                      :y2="l.y2"
                      :stroke-opacity="l.o"
                    />
                    <circle v-if="thumb" :cx="thumb.x" :cy="thumb.y" r="13" />
                  </svg>
                  <span
                    v-if="fingerAngle >= 0 && dragAngle === null"
                    class="bl-finger"
                    :style="{
                      left: `${140 + 97 * Math.cos((fingerAngle * Math.PI) / 180)}px`,
                      top: `${140 - 97 * Math.sin((fingerAngle * Math.PI) / 180)}px`,
                    }"
                    aria-hidden="true"
                  />
                </div>
              </div>
              <div class="bl-scroll">
                <button class="bl-ghost mono" @click="press('scroll', '24')">▲24</button>
                <button class="bl-ghost mono" @click="press('scroll', '1')">▲1</button>
                <button class="bl-ghost mono" @click="press('scroll', '-1')">▼1</button>
                <button class="bl-ghost mono" @click="press('scroll', '-24')">▼24</button>
              </div>
              <form class="bl-field" @submit.prevent="typeQuery">
                <label class="bl-small bl-muted" for="bl-search">Search</label>
                <div class="bl-field-row">
                  <input id="bl-search" v-model="query" placeholder="Artist, album, song" />
                  <button class="bl-ghost md" :disabled="!query">Type</button>
                  <button class="bl-text-btn md" type="button" @click="clearSearch">Clear</button>
                </div>
              </form>
              <form class="bl-field" @submit.prevent="press('seek', String(seekTo))">
                <label class="bl-small bl-muted" for="bl-seek">Seek to</label>
                <div class="bl-field-row">
                  <input id="bl-seek" v-model.number="seekTo" type="number" min="0" class="bl-mono short" />
                  <span class="bl-muted">s</span>
                  <button class="bl-ghost md">Seek</button>
                </div>
              </form>
            </div>
          </section>

          <section class="bl-charts">
            <div class="bl-charts-head">
              <h2>Live charts</h2>
              <div class="bl-chips">
                <button
                  v-for="c in CHIPS"
                  :key="c.key"
                  class="bl-chip"
                  :class="{ on: shown[c.key] }"
                  :aria-pressed="shown[c.key]"
                  @click="shown[c.key] = !shown[c.key]"
                >
                  {{ c.label }}
                </button>
              </div>
            </div>
            <LoopBar v-if="shown.breakdown" :samples="recentProf" :unit="units.breakdown">
              <template #units>
                <Seg v-model="units.breakdown" :options="BREAKDOWN_UNITS" label="Loop breakdown unit" size="sm" />
              </template>
            </LoopBar>
            <EventTimeline v-if="shown.events" :events="wheelEvents" :now="now" />
            <div v-if="nCharts" class="bl-chart-grid" :class="{ single: nCharts === 1 }">
              <LiveChart
                v-if="shown.runway"
                title="Buffer runway"
                sub="Seconds of decoded audio ahead of playback"
                :readout="lastAudio ? lastAudio.runwayS.toFixed(1) : '—'"
                readout-unit="s"
                :legend="[{ name: 'Runway', color: 'var(--bl-brand)', value: lastAudio ? `${lastAudio.runwayS.toFixed(2)} s` : '—' }]"
                :series="[{ name: 'runway', color: 'var(--bl-brand)', points: runway }]"
                :bands="underrunBands"
                :now="now"
                :window-s="windowS"
                :height="chartH"
                :format="(v) => `${v.toFixed(1)} s`"
                :tick="(v) => fmtN(v, 's')"
                empty="Play a track to see the buffer."
              />
              <LiveChart
                v-if="shown.cpu"
                title="CPU time"
                sub="Share of each second. SD is counted inside audio."
                :readout="cpuReadout"
                :readout-unit="units.cpu === 'ms' ? 'ms per s' : '% of 1 s'"
                :legend="cpuLegend"
                :series="cpuSeries"
                :scale="cpuScale"
                :now="now"
                :window-s="windowS"
                :height="chartH"
                :format="(v) => `${fmtN(v, cpuUnit)} ${cpuUnit}`"
                :tick="(v) => fmtN(v, cpuUnit)"
              >
                <template #units>
                  <Seg v-model="units.cpu" :options="CPU_UNITS" label="CPU time unit" size="xs" />
                </template>
              </LiveChart>
              <LiveChart
                v-if="shown.loop"
                title="Longest main loop"
                sub="Worst iteration in each second"
                :readout="fmtN(lastOf(maxLoop) === null ? null : lastOf(maxLoop)! * loopUnit.k, loopUnit.u)"
                :readout-unit="loopUnit.u"
                :legend="[{ name: 'Worst in window', color: 'var(--bl-text)', value: worstLoop }]"
                :series="[{ name: 'loop', color: 'var(--bl-text)', points: maxLoop }]"
                :scale="loopUnit.k"
                :threshold="{ v: FRAME_MS * loopUnit.k, label: `frame ${fmtN(FRAME_MS * loopUnit.k, loopUnit.u)} ${loopUnit.u}` }"
                :now="now"
                :window-s="windowS"
                :height="chartH"
                :format="(v) => `${fmtN(v, loopUnit.u)} ${loopUnit.u}`"
                :tick="(v) => fmtN(v, loopUnit.u)"
              >
                <template #units>
                  <Seg v-model="units.loop" :options="TIME_UNITS" label="Loop time unit" size="xs" />
                </template>
              </LiveChart>
              <LiveChart
                v-if="shown.input"
                title="Input → settled screen"
                sub="One dot per command"
                mode="dots"
                :readout="fmtN(lastOf(latency) === null ? null : lastOf(latency)! * inputUnit.k, inputUnit.u)"
                :readout-unit="inputUnit.u"
                :legend="inputLegend"
                :series="[{ name: 'latency', color: 'var(--bl-brand)', points: latency }]"
                :scale="inputUnit.k"
                :y-floor="SLOW_INPUT_MS * inputUnit.k"
                :bad-above="SLOW_INPUT_MS * inputUnit.k"
                :now="now"
                :window-s="windowS"
                :height="chartH"
                :format="(v) => `${fmtN(v, inputUnit.u)} ${inputUnit.u}`"
                :tick="(v) => fmtN(v, inputUnit.u)"
                empty="Press a wheel button to measure input latency."
              >
                <template #units>
                  <Seg v-model="units.input" :options="TIME_UNITS" label="Input latency unit" size="xs" />
                </template>
              </LiveChart>
              <LiveChart
                v-if="shown.stream"
                title="Stream rate"
                sub="KB/s delivered to the decoder"
                :readout="lastAudio ? String(Math.round(lastAudio.rateKBs)) : '—'"
                readout-unit="KB/s"
                :legend="[{ name: 'Rate', color: 'var(--bl-sgreen)', value: lastAudio ? `${Math.round(lastAudio.rateKBs)} KB/s` : '—' }]"
                :series="[{ name: 'rate', color: 'var(--bl-sgreen)', points: rate }]"
                :now="now"
                :window-s="windowS"
                :height="chartH"
                :format="(v) => `${Math.round(v)} KB/s`"
                :tick="(v) => fmtN(v)"
                empty="Play a track to see the stream rate."
              />
            </div>
            <p v-if="shown.runway && bandsInWindow" class="bl-underrun-note">
              <span class="bl-band-swatch" />Red bands mark seconds where the buffer ran dry and playback glitched.
            </p>
          </section>
        </div>

        <!-- Run the bench -->
        <div v-else-if="tab === 'run'" class="bl-stack">
          <section class="bl-runner">
            <div class="bl-section-head">
              <h2>Run the bench</h2>
              <p class="bl-muted">Same scenarios as the CLI, <code class="bl-cmd">npm run bench -- run</code>.</p>
            </div>
            <div class="bl-note amber">
              <b>Hands off the wheel.</b> Scenarios drive the device themselves, so the remote is locked until the run
              ends. Stopping is safe: the current scenario finishes and its results are kept.
            </div>
            <div v-if="running && !current" class="bl-progress">
              <div class="bl-progress-row"><span>Preparing the run…</span></div>
              <div class="bl-progress-track"><div style="width: 0" /></div>
            </div>
            <div v-if="running && current" class="bl-progress">
              <div class="bl-progress-row">
                <span>Running <b>{{ SCENARIOS[current.name].label }}</b></span>
                <span class="bl-mono bl-muted">{{ current.index + 1 }} of {{ current.total }}</span>
              </div>
              <div class="bl-progress-track"><div :style="{ width: `${runPct}%` }" /></div>
            </div>
            <div class="bl-scenarios">
              <button
                v-for="(s, name) in SCENARIOS"
                :key="name"
                class="bl-sc"
                :class="{ running: progress[name]?.kind === 'running' }"
                role="checkbox"
                :aria-checked="chosen.includes(name)"
                :disabled="busy"
                @click="toggleScenario(name)"
              >
                <span class="bl-check" :class="{ on: chosen.includes(name) }" aria-hidden="true">{{ chosen.includes(name) ? "✓" : "" }}</span>
                <span class="bl-sc-text">
                  <span class="bl-sc-name">{{ s.label }}</span>
                  <span class="bl-sc-desc">{{ s.desc }}</span>
                </span>
                <span class="bl-sc-status" :class="progress[name]?.kind" :title="progress[name]?.message">{{ scenarioStatus(name) }}</span>
                <span v-if="progress[name]?.kind === 'running'" class="bl-sc-bar" :style="{ width: `${currentPct * 100}%` }" />
              </button>
            </div>
            <div class="bl-run-foot">
              <button class="bl-checkbox" role="checkbox" :aria-checked="background" :disabled="busy" :title="BACKGROUND_SAMPLE" @click="background = !background">
                <span class="bl-check" :class="{ on: background }" aria-hidden="true">{{ background ? "✓" : "" }}</span>
                Loop a sample track in the background
              </button>
              <span class="bl-spacer" />
              <button v-if="running" class="bl-primary quiet" :disabled="stopping" @click="stopRun">
                {{ stopping ? "Stopping after this scenario…" : "Stop after this scenario" }}
              </button>
              <button v-else class="bl-primary" :disabled="!chosen.length || testing" @click="runScenarios">
                {{
                  !chosen.length
                    ? "Select a scenario"
                    : chosen.length === Object.keys(SCENARIOS).length
                      ? "Run everything"
                      : `Run ${chosen.length} scenario${chosen.length > 1 ? "s" : ""}`
                }}
              </button>
            </div>
          </section>

          <section v-if="report" class="bl-panel bl-results">
            <div class="bl-results-main">
              <div class="bl-section-head">
                <div class="bl-results-title">
                  <h2>Results</h2>
                  <span class="bl-mono bl-small bl-muted">
                    {{ report.rev }} · {{ hhmm(report.date) }}
                    <template v-if="reference"> vs published {{ reference.rev }}<template v-if="reference.date"> · {{ shortDate(reference.date) }}</template></template>
                    <template v-else> · nothing published to compare with</template>
                  </span>
                </div>
                <div v-if="reference" class="bl-badges">
                  <span class="bl-badge red">{{ counts.worse }} worse</span>
                  <span class="bl-badge green">{{ counts.better }} better</span>
                  <span class="bl-badge muted">{{ counts.same }} same (±10%)</span>
                </div>
              </div>
              <div class="bl-table-wrap">
                <table>
                  <thead>
                    <tr>
                      <th>Metric</th>
                      <th class="r">This run</th>
                      <th v-if="reference" class="r">Published</th>
                      <th v-if="reference" class="r">Change</th>
                    </tr>
                  </thead>
                  <tbody>
                    <tr v-for="m in shownMetrics" :key="m.key">
                      <td>
                        <div>{{ m.label }}</div>
                        <div class="bl-small bl-muted">{{ m.group }}</div>
                      </td>
                      <td class="r bl-mono">
                        {{ fmtParts(summary[m.key], m.unit)[0] }} <span class="bl-muted">{{ fmtParts(summary[m.key], m.unit)[1] }}</span>
                      </td>
                      <td v-if="reference" class="r bl-mono bl-muted">{{ fmtParts(reference.summary[m.key], m.unit).join(" ") }}</td>
                      <td v-if="reference" class="r">
                        <span v-if="change(m)" class="bl-change" :class="change(m)!.kind">{{ change(m)!.text }}</span>
                      </td>
                    </tr>
                  </tbody>
                </table>
              </div>
              <div v-for="e in report.errors" :key="e.scenario" class="bl-note red">
                <div class="bl-note-title">
                  {{ e.scenario === "background" ? "Background track" : (SCENARIOS[e.scenario]?.label ?? e.scenario) }} {{ e.message === "stopped" ? "didn't run" : "failed" }}
                </div>
                <div>{{ e.message === "stopped" ? "The run was stopped before it started." : e.message }}</div>
              </div>
            </div>
            <aside class="bl-publish">
              <div class="bl-label">Publish this run</div>
              <div class="bl-step">
                <span class="bl-step-n">1</span>
                <div class="bl-step-body">
                  <div>Download the report</div>
                  <button class="bl-primary wide" @click="download">Download report</button>
                  <div class="bl-mono bl-xs bl-muted">{{ fileName }} · {{ reportKB }} KB</div>
                </div>
              </div>
              <div class="bl-step">
                <span class="bl-step-n">2</span>
                <div class="bl-step-body">
                  <div>Publish it from the repo</div>
                  <div class="bl-code">
                    <span>{{ publishCmd }}</span>
                    <button @click="copyPublish">{{ copied ? "Copied" : "Copy" }}</button>
                  </div>
                  <div class="bl-small bl-muted">Run it in <code class="bl-mono">software/</code>. It becomes the new baseline for the next comparison.</div>
                </div>
              </div>
            </aside>
          </section>

          <section class="bl-runner">
            <div class="bl-section-head">
              <h2>Run the e2e tests</h2>
              <p class="bl-muted">
                The tests in <code class="bl-cmd">software/bench/e2e/</code>, as <code class="bl-cmd">npm run bench -- e2e</code>
                runs them: gestures played on the board through the wheel's sensor layer, then checks on what the UI did.
              </p>
            </div>
            <div class="bl-run-foot">
              <span v-if="testSummary" class="bl-mono bl-small bl-muted">{{ testSummary }}</span>
              <span class="bl-spacer" />
              <button v-if="testing" class="bl-primary quiet" @click="testAbort?.abort()">Stop after this test</button>
              <button v-else class="bl-primary" :disabled="running" @click="runTests()">Run all {{ TESTS.length }} tests</button>
            </div>
            <div class="bl-test-groups">
              <div v-for="(group, g) in testGroups" :key="g" class="bl-test-group">
                <div class="bl-label">{{ g }}</div>
                <div v-for="test in group" :key="test.name" class="bl-test" :class="testState[test.name]?.kind">
                  <span class="bl-test-icon" aria-hidden="true">{{ testIcon(test.name) }}</span>
                  <span class="bl-test-name">{{ test.name }}</span>
                  <span class="bl-mono bl-small bl-muted">{{ testMeta(test.name) }}</span>
                  <button class="bl-ghost xs" :disabled="busy" @click="runTests([test])">Run</button>
                  <div v-if="testState[test.name]?.kind === 'failed'" class="bl-test-error">
                    <div v-if="testState[test.name].result?.step">while: {{ testState[test.name].result?.step }}</div>
                    <div>{{ testState[test.name].result?.error }}</div>
                    <div class="bl-muted">events: <code class="bl-mono">{{ sequence(testState[test.name].result?.events ?? []) || "none" }}</code></div>
                  </div>
                </div>
              </div>
            </div>
          </section>
        </div>

        <!-- Serial log -->
        <section v-else class="bl-panel bl-log">
          <button class="bl-log-head" :aria-expanded="logOpen" @click="logOpen = !logOpen">
            <span class="bl-muted bl-chevron">{{ logOpen ? "▾" : "▸" }}</span>
            <span class="bl-log-title">Serial log</span>
            <span class="bl-mono bl-small bl-muted">{{ shownLog.length }} lines</span>
          </button>
          <div v-if="logOpen" class="bl-log-body">
            <div class="bl-log-tools">
              <input v-model="logFilter" class="bl-mono" placeholder="Filter lines" aria-label="Filter the log" />
              <button class="bl-checkbox" role="checkbox" :aria-checked="hideMonitor" @click="hideMonitor = !hideMonitor">
                <span class="bl-check" :class="{ on: hideMonitor }" aria-hidden="true">{{ hideMonitor ? "✓" : "" }}</span>
                Hide once-a-second monitor lines
              </button>
            </div>
            <div ref="logEl" class="bl-log-lines" @scroll="onLogScroll">
              <div v-for="(l, i) in shownLog" :key="i" :class="l.kind">{{ l.text }}</div>
            </div>
          </div>
        </section>
      </template>
    </div>
  </div>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, reactive, ref, shallowRef, watch, nextTick } from "vue";
import { useData, withBase } from "vitepress";
import type { ButtonName, Device, Fields, ProfSample, WheelEvent } from "@bench/device.ts";
import { parseAudio, parseEvent, parseFields, parseProf } from "@bench/device.ts";
import { TESTS, runE2E, sequence, type E2EResult, type E2ETest } from "@bench/e2e/index.ts";
import { SCENARIOS, runBench } from "@bench/bench.ts";
import { BACKGROUND_SAMPLE } from "@bench/sample-list.ts";
import { METRICS, revFile, summarize, type Metric, type Report } from "@bench/metrics.ts";
import LiveChart, { type LegendItem, type Point } from "./LiveChart.vue";
import LoopBar from "./LoopBar.vue";
import EventTimeline from "./EventTimeline.vue";
import Seg from "./Seg.vue";
import { FRAME_MS, clock, duration, fmtN, fmtParts } from "./format.ts";
import { connectWebSerial, webSerialSupported } from "./web-serial.ts";
import { demoDevice } from "./demo-device.ts";

const { isDark } = useData();

// The status bar's height (it wraps on narrow screens), so the remote can stick under it.
const statusEl = ref<HTMLElement | null>(null);
const statusH = ref(60);
let statusObserver: ResizeObserver | null = null;

const supported = ref(true);
const demo = ref(false);
const device = shallowRef<Device | null>(null);
const portLabel = ref("");
const connecting = ref(false);
const now = ref(0);
const wallNow = ref(Date.now());
const lastLineAt = ref(0);
const windowS = ref(60);
const tab = ref<"live" | "run" | "log">("live");

const WINDOWS = [
  { value: 30, label: "30 s" },
  { value: 60, label: "1 min" },
  { value: 300, label: "5 min" },
];

// --- Errors ------------------------------------------------------------------

type BenchErr = { id: number; title: string; body: string; details: string };
const errors = ref<BenchErr[]>([]);
let errorId = 0;

/** Shows an error callout; the same title again replaces the older one. */
function fail(title: string, body = "", cause = "", lastLine = "") {
  const details = [new Date().toTimeString().slice(0, 8), cause, lastLine && `last line “${lastLine}”`].filter(Boolean).join(" · ");
  errors.value = [...errors.value.filter((e) => e.title !== title), { id: ++errorId, title, body, details }];
}
/** A long message as a title (its first clause) and a body (the rest). */
function failMessage(message: string, cause = "") {
  const m = /^(.+?)(?:[.:] |$)([\s\S]*)$/.exec(message.trim());
  fail(m ? m[1] : message, m?.[2] ?? "", cause);
}
const dismiss = (id: number) => (errors.value = errors.value.filter((e) => e.id !== id));

// --- Live series (device ms). Plain arrays behind refs, replaced in batches. ---

const runway = ref<Point[]>([]);
const rate = ref<Point[]>([]);
const maxLoop = ref<Point[]>([]);
const latency = ref<Point[]>([]);
const cpu = reactive<Record<string, Point[]>>({ feeder: [], SD: [], UI: [], audio: [] });
const underrunBands = ref<[number, number][]>([]);
const underrunTotal = ref(0);
const lastAudio = shallowRef<ReturnType<typeof parseAudio>>(null);
const recentProf = shallowRef<ProfSample[]>([]); // the last few seconds, for the loop breakdown
const wheelEvents = shallowRef<WheelEvent[]>([]); // what the UI handled (no finger positions)
const fingerAngle = ref(-1); // where the finger is on the wheel (real or virtual), -1 = none
const held = ref(new Set<string>()); // buttons down, from the board's events

const st = ref<Fields>({});
const info = reactive<Record<string, string>>({});
const query = ref("");
const seekTo = ref(25);

const running = ref(false);
const testing = ref(false);
const busy = computed(() => running.value || testing.value);

const alive = computed(() => now.value - lastLineAt.value < 3000);
const KEEP_MS = 10 * 60 * 1000;

// --- Charts: which are shown, and their units (kept between visits) -------

const CHIPS = [
  { key: "breakdown", label: "Loop breakdown" },
  { key: "events", label: "Wheel events" },
  { key: "runway", label: "Buffer runway" },
  { key: "cpu", label: "CPU time" },
  { key: "loop", label: "Longest loop" },
  { key: "input", label: "Input → settled" },
  { key: "stream", label: "Stream rate" },
] as const;
type ChartKey = (typeof CHIPS)[number]["key"];
const shown = reactive<Record<ChartKey, boolean>>({
  breakdown: true,
  events: true,
  runway: true,
  cpu: true,
  loop: true,
  input: true,
  stream: true,
});
const units = reactive({
  cpu: "pct" as "pct" | "ms",
  loop: "ms" as "ms" | "us" | "frame",
  input: "ms" as "ms" | "us" | "frame",
  breakdown: "pct" as "pct" | "us" | "frame",
});
const CPU_UNITS = [
  { value: "pct" as const, label: "% of 1 s" },
  { value: "ms" as const, label: "ms per s" },
];
const TIME_UNITS = [
  { value: "ms" as const, label: "ms" },
  { value: "us" as const, label: "µs" },
  { value: "frame" as const, label: "% of frame" },
];
const BREAKDOWN_UNITS = [
  { value: "pct" as const, label: "% of loop" },
  { value: "us" as const, label: "µs per loop" },
  { value: "frame" as const, label: "% of frame" },
];
const STORE = { charts: "bench-live.charts", units: "bench-live.units" };

function restore() {
  try {
    Object.assign(shown, JSON.parse(localStorage.getItem(STORE.charts) ?? "{}"));
    Object.assign(units, JSON.parse(localStorage.getItem(STORE.units) ?? "{}"));
  } catch {
    // Storage blocked or garbled: the defaults do.
  }
}
function save(key: string, value: object) {
  try {
    localStorage.setItem(key, JSON.stringify(value));
  } catch {
    // Not kept: fine.
  }
}
watch(shown, (v) => save(STORE.charts, v));
watch(units, (v) => save(STORE.units, v));

/** Inputs slower than this are drawn red. */
const SLOW_INPUT_MS = 100;

const nCharts = computed(() => (["runway", "cpu", "loop", "input", "stream"] as const).filter((k) => shown[k]).length);
const chartH = computed(() => (nCharts.value <= 1 ? 380 : nCharts.value <= 2 ? 300 : nCharts.value <= 4 ? 230 : 190));

const inWindow = (a: Point[]) => a.filter((p) => p.t >= now.value - windowS.value * 1000);
const lastOf = (a: Point[]) => (a.length ? a[a.length - 1].v : null);

const timeUnit = (u: "ms" | "us" | "frame") =>
  u === "us" ? { k: 1000, u: "µs" } : u === "frame" ? { k: 100 / FRAME_MS, u: "% frame" } : { k: 1, u: "ms" };
const loopUnit = computed(() => timeUnit(units.loop));
const inputUnit = computed(() => timeUnit(units.input));

const worstLoop = computed(() => {
  const v = inWindow(maxLoop.value);
  return v.length ? `${fmtN(Math.max(...v.map((p) => p.v)) * loopUnit.value.k, loopUnit.value.u)} ${loopUnit.value.u}` : "—";
});

const inputLegend = computed<LegendItem[]>(() => {
  const v = inWindow(latency.value).map((p) => p.v);
  if (!v.length) return [];
  const s = [...v].sort((a, b) => a - b);
  const { k, u } = inputUnit.value;
  const at = (q: number) => fmtN(s[Math.min(s.length - 1, Math.floor(s.length * q))] * k, u);
  return [
    { name: "p50", color: "var(--bl-brand)", value: at(0.5), dot: true },
    { name: "p95", color: "var(--bl-brand)", value: at(0.95), dot: true },
    { name: `> ${SLOW_INPUT_MS} ms`, color: "var(--bl-red)", value: String(v.filter((x) => x > SLOW_INPUT_MS).length), dot: true },
  ];
});

// CPU categories: firmware key, name, and how each line is drawn.
const CPU = [
  { key: "audio", name: "audio", label: "Audio", color: "var(--bl-brand)", swatch: "var(--bl-brand)", area: true },
  { key: "sd", name: "SD", label: "SD", color: "var(--bl-brand)", swatch: "var(--bl-brand2)", dash: "3 3", width: 1.5 },
  { key: "ui", name: "UI", label: "UI", color: "var(--bl-sgreen)", swatch: "var(--bl-sgreen)", width: 1.75 },
  { key: "isr", name: "feeder", label: "Feeder", color: "var(--bl-muted)", swatch: "var(--bl-muted)", width: 1.5 },
] as const;
const cpuScale = computed(() => (units.cpu === "ms" ? 10 : 1));
const cpuUnit = computed(() => (units.cpu === "ms" ? "ms" : "%"));
const cpuSeries = computed(() =>
  CPU.map((c) => ({
    name: c.label,
    color: c.color,
    points: cpu[c.name],
    area: "area" in c,
    dash: "dash" in c ? c.dash : undefined,
    width: "width" in c ? c.width : 2,
  })),
);
const cpuLegend = computed<LegendItem[]>(() =>
  CPU.map((c) => {
    const v = lastOf(cpu[c.name]);
    return { name: c.label, color: c.swatch, value: v === null ? "—" : fmtN(v * cpuScale.value, cpuUnit.value) };
  }),
);
const cpuReadout = computed(() => {
  const parts = ["audio", "UI", "feeder"].map((n) => lastOf(cpu[n]));
  return parts.some((v) => v === null) ? "—" : fmtN(parts.reduce((a, v) => a! + v!, 0)! * cpuScale.value, cpuUnit.value);
});

const bandsInWindow = computed(() => underrunBands.value.some(([, b]) => b >= now.value - windowS.value * 1000));

// --- Serial log ----------------------------------------------------------------

type LogKind = "mon" | "cmd" | "ans" | "evt" | "line" | "bad";
type LogLine = { text: string; kind: LogKind };
const LOG_KEEP = 2000;
const LOG_SHOWN = 400;
const logLines = shallowRef<LogLine[]>([]);
const logFilter = ref("");
const hideMonitor = ref(true);
const logOpen = ref(true);
const logEl = ref<HTMLDivElement | null>(null);
let logStick = true;

/** The once-a-second chatter: profiler, audio monitor, state polls, finger positions. */
const isMonitor = (line: string) =>
  line.includes("[prof ") ||
  line.includes("runway=") ||
  /^[<>] #(ok )?(state|finger)\b/.test(line) ||
  line.startsWith("#ev finger");

function log(t: number, mark: string, line: string, kind: LogKind) {
  const text = `${(t / 1000).toFixed(1).padStart(7)}  ${mark}  ${line}`;
  const next = [...logLines.value, { text, kind }];
  logLines.value = next.length > LOG_KEEP ? next.slice(-LOG_KEEP) : next;
}

const shownLog = computed(() => {
  const f = logFilter.value.toLowerCase();
  return logLines.value
    .filter((l) => !(hideMonitor.value && l.kind === "mon") && (!f || l.text.toLowerCase().includes(f)))
    .slice(-LOG_SHOWN);
});
function onLogScroll() {
  const el = logEl.value;
  if (el) logStick = el.scrollTop + el.clientHeight >= el.scrollHeight - 8;
}
watch(
  () => (tab.value === "log" && logOpen.value ? shownLog.value : null),
  async () => {
    await nextTick();
    if (logEl.value && logStick) logEl.value.scrollTop = logEl.value.scrollHeight;
  },
);

const tabs = computed(() => [
  { id: "live" as const, label: "Live", count: "" },
  {
    id: "run" as const,
    label: "Run the bench",
    count: running.value && current.value ? `${current.value.index + 1}/${current.value.total}` : testing.value ? "e2e" : "",
  },
  { id: "log" as const, label: "Serial log", count: String(logLines.value.length) },
]);

// --- Connection ------------------------------------------------------------------

let unsubscribe: (() => void) | null = null;
let ticker: ReturnType<typeof setInterval> | null = null;
let poll: ReturnType<typeof setInterval> | null = null;

onMounted(() => {
  restore();
  statusObserver = new ResizeObserver(([e]) => (statusH.value = e.borderBoxSize[0].blockSize));
  if (statusEl.value) statusObserver.observe(statusEl.value);
  supported.value = webSerialSupported();
  if (new URLSearchParams(location.search).has("demo")) {
    supported.value = true;
    void connect(true);
  }
  window.addEventListener("keydown", onKey);
});
onBeforeUnmount(() => {
  cancelAnimationFrame(trailFrame);
  if (wheelTimer) clearTimeout(wheelTimer);
  statusObserver?.disconnect();
  window.removeEventListener("keydown", onKey);
  void disconnect();
});

const trim = <P extends { t: number }>(a: P[], t: number) => (a.length && a[0].t < t - KEEP_MS ? a.filter((p) => p.t >= t - KEEP_MS) : a);

function onLine(line: string, t: number) {
  lastLineAt.value = t;
  line = line.replace(/\x1b\[[0-9;]*m/g, "");
  const answer = /^#(ok|err) /.test(line);

  const a = parseAudio(line, t);
  log(t, answer ? "<" : " ", line, a && a.underruns > 0 ? "bad" : isMonitor(answer ? `< ${line}` : line) ? "mon" : answer ? "ans" : "line");
  if (a) {
    lastAudio.value = a;
    runway.value = trim([...runway.value, { t, v: a.runwayS }], t);
    rate.value = trim([...rate.value, { t, v: a.rateKBs }], t);
    if (a.underruns > 0) {
      underrunTotal.value += a.underruns;
      underrunBands.value = [...underrunBands.value, [t - 1000, t]];
    }
    return;
  }
  const ev = parseEvent(line, t);
  if (ev) {
    if (ev.type === "finger") fingerAngle.value = ev.angle ?? -1;
    else {
      wheelEvents.value = trim([...wheelEvents.value, ev], t);
      if (ev.type === "down" || ev.type === "up") {
        const next = new Set(held.value);
        if (ev.type === "down") next.add(ev.button!);
        else next.delete(ev.button!);
        held.value = next;
      }
    }
    return;
  }
  const p = parseProf(line, t);
  if (p) {
    maxLoop.value = trim([...maxLoop.value, { t, v: p.maxLoopMs }], t);
    recentProf.value = [...recentProf.value, p].slice(-5);
    // busyMs and windowMs are both ms: the ratio is the share of CPU time over the window.
    for (const c of CPU) cpu[c.name] = trim([...cpu[c.name], { t, v: (100 * (p.busyMs[c.key] ?? 0)) / p.windowMs }], t);
    return;
  }
  const m = /^#(ok|err|boot) ?(\S*)\s*(.*)$/.exec(line);
  if (!m) return;
  if (m[1] === "boot") {
    applyBoot(parseFields(line));
    return;
  }
  const f = parseFields(m[3]);
  if (m[1] === "ok" && m[2] === "state") {
    st.value = f;
    return;
  }
  if (m[1] === "ok" && m[2] === "boot") applyBoot(f);
  if (m[1] === "ok" && m[2] === "dataset") {
    info.tracks = f.tracks_loaded;
    info.albums = f.albums_loaded;
  }
  if (m[1] === "ok" && f.us && ["click", "long", "scroll", "type", "seek"].includes(m[2])) {
    latency.value = trim([...latency.value, { t, v: +f.us / 1000, label: m[2] }], t);
  }
}

function applyBoot(f: Fields) {
  info.rev = f.rev;
  info.bootMs = f.ms;
  info.lookupMs = f.lookup_ms;
  info.searchIndexMs = f.search_index_ms;
}

/** Logs every command sent, whoever sends it (the remote, the polling, the runner). */
function logSends(d: Device) {
  const send = d.send.bind(d);
  d.send = (cmd: string, args = "", timeoutMs?: number) => {
    const line = `#${cmd}${args ? " " + args : ""}`;
    log(d.now(), ">", line, isMonitor(`> ${line}`) ? "mon" : "cmd");
    return send(cmd, args, timeoutMs);
  };
}

async function connect(useDemo = demo.value) {
  errors.value = [];
  connecting.value = true;
  demo.value = useDemo;
  try {
    const { device: d, label, stats } = useDemo
      ? { device: demoDevice(), label: "demo board (simulated)", stats: null }
      : await connectWebSerial((e) => {
          if (e instanceof Error && !(e instanceof DOMException)) failMessage(e.message, "port closed");
          else fail("Connection lost", "Reconnect when the board is back.", e ? String(e) : "port closed");
          void disconnect();
        });
    logSends(d);
    device.value = d;
    portLabel.value = label;
    unsubscribe = d.subscribe(onLine);
    ticker = setInterval(() => {
      now.value = d.now();
      wallNow.value = Date.now();
    }, 250);
    // The board may be running for a while already: ask what a fresh boot prints.
    // Asked twice: a board still booting (or old firmware) misses the first.
    await d
      .send("boot", "", 3000)
      .catch(() => d.send("boot", "", 5000))
      .catch(() => {
        const link = stats
          ? `received ${stats.bytesIn} bytes / ${stats.linesIn} lines, sent ${stats.bytesOut} bytes` +
            (stats.writeError ? `, write failed: ${stats.writeError}` : "")
          : "";
        if (stats && stats.bytesIn === 0) fail("Nothing received from the board", "Wrong port, or the browser can't read it.", link);
        else if (stats && stats.bytesOut === 0) fail("The board talks but the browser couldn't send to it", "", link, stats.lastLine);
        else
          fail(
            "The board doesn't answer bench commands",
            "Is the bench firmware flashed? `pio run -e bench -t upload`",
            link,
            stats?.lastLine ?? "",
          );
      });
    await d.send("dataset", "", 3000).catch(() => {});
    startPolling();
  } catch (e) {
    if (e instanceof DOMException && e.name === "NotFoundError") {
      // Picker closed: nothing chosen, or nothing listed.
      fail(
        "No port chosen",
        "If the Nucleo wasn't listed, check its USB cable. With Chromium from snap, run `sudo snap connect chromium:raw-usb` then restart Chromium.",
      );
    } else failMessage(e instanceof Error ? e.message : String(e), "connect");
  } finally {
    connecting.value = false;
  }
}

async function disconnect() {
  stopPolling();
  if (ticker) clearInterval(ticker);
  ticker = null;
  unsubscribe?.();
  unsubscribe = null;
  const d = device.value;
  device.value = null;
  await d?.close().catch(() => {});
}

async function reconnect() {
  await disconnect();
  await connect();
}

function startPolling() {
  stopPolling();
  poll = setInterval(() => {
    if (!busy.value) device.value?.send("state", "", 2000).catch(() => {});
  }, 700);
}
function stopPolling() {
  if (poll) clearInterval(poll);
  poll = null;
}

function clearCharts() {
  runway.value = [];
  rate.value = [];
  maxLoop.value = [];
  recentProf.value = [];
  wheelEvents.value = [];
  latency.value = [];
  for (const c of CPU) cpu[c.name] = [];
  underrunBands.value = [];
  underrunTotal.value = 0;
}

// --- Remote ------------------------------------------------------------------------

const ROUTES = ["Main menu", "Search", "Genres", "Artists", "Albums", "Tracks", "Now Playing", "Settings"];
const routeName = (r?: string) => (r === undefined ? "—" : (ROUTES[+r] ?? `screen ${r}`));

async function press(cmd: string, arg: string) {
  const d = device.value;
  if (!d || busy.value) return;
  try {
    await d.send(cmd, arg);
    void d.send("state", "", 2000).catch(() => {});
  } catch (e) {
    fail(`#${cmd}${arg ? ` ${arg}` : ""} failed`, e instanceof Error ? e.message : String(e));
  }
}

async function typeQuery() {
  await press("type", query.value);
  query.value = "";
}
function clearSearch() {
  query.value = "";
  void press("clear", "");
}

// Briefly lights a wheel label when it's pressed from here.
const flashed = ref<string | null>(null);
let flashTimer: ReturnType<typeof setTimeout> | null = null;
function flash(name: string) {
  flashed.value = name;
  if (flashTimer) clearTimeout(flashTimer);
  flashTimer = setTimeout(() => (flashed.value = null), 180);
}
const lit = (name: string) => flashed.value === name || held.value.has(name);

function click(name: ButtonName) {
  flash(name);
  void press("click", name);
}
function longPress() {
  flash("select");
  void press("long", "select");
}

function onKey(e: KeyboardEvent) {
  if (!device.value || busy.value) return;
  const el = e.target as HTMLElement;
  if (el && (el.tagName === "INPUT" || el.tagName === "SELECT" || el.tagName === "TEXTAREA" || el.tagName === "BUTTON")) return;
  const keys: Record<string, [string, string]> = {
    ArrowUp: ["scroll", e.shiftKey ? "24" : "1"],
    ArrowDown: ["scroll", e.shiftKey ? "-24" : "-1"],
    ArrowLeft: ["click", "prev"],
    ArrowRight: ["click", "next"],
    Enter: ["click", "select"],
    Escape: ["click", "menu"],
    " ": ["click", "play"],
  };
  const k = keys[e.key];
  if (!k) return;
  e.preventDefault();
  if (k[0] === "click") flash(k[1]);
  void press(...k);
}

// --- The wheel: buttons, the virtual finger, the mouse wheel -------------------------

const WEDGES: { name: ButtonName; cls: string; label: string; text: string }[] = [
  { name: "menu", cls: "w-top", label: "Menu", text: "MENU" },
  { name: "next", cls: "w-right", label: "Next", text: "⏭︎" },
  { name: "play", cls: "w-bottom", label: "Play or pause", text: "⏯︎" },
  { name: "prev", cls: "w-left", label: "Previous", text: "⏮︎" },
];
const wheelEl = ref<HTMLDivElement | null>(null);

/** Held this long without moving, a press becomes a hold (the wheel times it, 800 ms is a long press). */
const HOLD_AFTER_MS = 150;
/** Moved this far first, it becomes a finger dragged around the ring. */
const DRAG_PX = 6;

type Gesture = { name: ButtonName; x: number; y: number; mode: "pending" | "hold" | "drag"; timer: ReturnType<typeof setTimeout> };
let gesture: Gesture | null = null;

/** The pointer's angle around the wheel's center: 0 = right, counter-clockwise, like the firmware. */
function pointerAngle(e: PointerEvent): number | null {
  const r = wheelEl.value!.getBoundingClientRect();
  const dx = e.clientX - (r.left + r.width / 2);
  const dy = e.clientY - (r.top + r.height / 2);
  const dist = Math.hypot(dx, dy) / (r.width / 2);
  if (dist < 0.3 || dist > 1.1) return null; // the center button, or off the wheel
  return (Math.round((Math.atan2(-dy, dx) * 180) / Math.PI) + 360) % 360;
}

let lastSentAt = 0;
let pendingAngle: number | null = null;
let sendTimer: ReturnType<typeof setTimeout> | null = null;
// At most ~60 updates a second: the board samples the wheel once a loop pass.
function sendFinger(angle: number | null) {
  pendingAngle = angle;
  if (sendTimer) return;
  const wait = Math.max(0, 16 - (Date.now() - lastSentAt));
  sendTimer = setTimeout(() => {
    sendTimer = null;
    lastSentAt = Date.now();
    void device.value?.finger(pendingAngle).catch(() => {});
  }, wait);
}

function gestureDown(name: ButtonName, e: PointerEvent) {
  if (!device.value || busy.value || e.button !== 0) return;
  try {
    (e.currentTarget as HTMLElement).setPointerCapture(e.pointerId);
  } catch {
    // Not an active pointer (a synthetic event): the press still counts.
  }
  const g: Gesture = {
    name,
    x: e.clientX,
    y: e.clientY,
    mode: "pending",
    timer: setTimeout(() => {
      if (gesture !== g || g.mode !== "pending") return;
      g.mode = "hold";
      void device.value?.button(name, true).catch((err) => fail(`#btn ${name} failed`, String(err)));
    }, HOLD_AFTER_MS),
  };
  gesture = g;
}
function gestureMove(e: PointerEvent) {
  const g = gesture;
  if (!g) return;
  if (g.mode === "pending" && g.name !== "select" && Math.hypot(e.clientX - g.x, e.clientY - g.y) > DRAG_PX) {
    clearTimeout(g.timer);
    g.mode = "drag";
  }
  if (g.mode === "drag") {
    const a = pointerAngle(e);
    if (a === null) return;
    dragAngle.value = a;
    addTrail(a);
    sendFinger(a);
  }
}
function gestureUp() {
  const g = gesture;
  gesture = null;
  if (!g) return;
  clearTimeout(g.timer);
  const refresh = (ms: number) => setTimeout(() => void device.value?.send("state", "", 2000).catch(() => {}), ms);
  if (g.mode === "pending") click(g.name);
  else if (g.mode === "hold")
    void device.value
      ?.button(g.name, false)
      .then(() => refresh(100))
      .catch(() => {});
  else {
    dragAngle.value = null;
    sendFinger(null);
    refresh(150);
  }
}

// --- Drag feedback: the thumb on the ring, and a trail fading behind it ------

const RING_R = 97; // the middle of the ring, in the wheel's 280 px
const TRAIL_MS = 600;
/** Where the page's own finger is while dragging, null otherwise. */
const dragAngle = ref<number | null>(null);
const trail = shallowRef<{ a: number; t: number }[]>([]);
const trailNow = ref(0);
let trailFrame = 0;

function ringPoint(a: number) {
  const r = (a * Math.PI) / 180;
  return { x: 140 + RING_R * Math.cos(r), y: 140 - RING_R * Math.sin(r) };
}
function addTrail(a: number) {
  const t = performance.now();
  trail.value = [...trail.value.filter((p) => t - p.t < TRAIL_MS), { a, t }];
  if (!trailFrame) trailFrame = requestAnimationFrame(fadeTrail);
}
// Runs while the trail has points, so it fades out after the finger stops or lifts.
function fadeTrail() {
  trailNow.value = performance.now();
  trail.value = trail.value.filter((p) => trailNow.value - p.t < TRAIL_MS);
  trailFrame = trail.value.length ? requestAnimationFrame(fadeTrail) : 0;
}
const trailSegments = computed(() => {
  const pts = trail.value;
  const out = [];
  for (let i = 1; i < pts.length; i++) {
    const a = ringPoint(pts[i - 1].a);
    const b = ringPoint(pts[i].a);
    out.push({ x1: a.x, y1: a.y, x2: b.x, y2: b.y, o: 0.45 * Math.max(0, 1 - (trailNow.value - pts[i].t) / TRAIL_MS) });
  }
  return out;
});
const thumb = computed(() => (dragAngle.value === null ? null : ringPoint(dragAngle.value)));

// --- The mouse wheel: a finger gliding round the ring ------------------------
// Over the remote panel, mouse-wheel / trackpad scrolling drags the virtual
// finger round the ring, so it goes through the wheel's own scroll path
// (acceleration, fast scroll, liftoff) like a real thumb. Scrolling down
// turns clockwise: down the list, as on an iPod.

/** Ring degrees per scrolled pixel: a mouse notch (~100 px) is one 15° click. */
const WHEEL_DEG_PER_PX = 0.15;
/** Per 16 ms step. The board reads the turn the short way round once a loop pass (~50 ms, more while a screen slides), so a step stays far below 180°. */
const WHEEL_MAX_STEP = 20;
/** Lift the finger this long after the last scroll event (more than the wheel's 80 ms liftoff hold). */
const WHEEL_LIFT_MS = 250;

let wheelTarget = 0; // unwrapped degrees the scrolling asks for
let wheelAt: number | null = null; // where the finger is, null = not touching
let wheelLastEventAt = 0;
let wheelTimer: ReturnType<typeof setTimeout> | null = null;

/** The pointer's angle around the wheel's center, wherever it is in the panel. */
function panelAngle(e: MouseEvent): number {
  const r = wheelEl.value!.getBoundingClientRect();
  const dx = e.clientX - (r.left + r.width / 2);
  const dy = e.clientY - (r.top + r.height / 2);
  if (Math.hypot(dx, dy) < 1) return 90;
  return (Math.atan2(-dy, dx) * 180) / Math.PI;
}
const wrap = (a: number) => ((Math.round(a) % 360) + 360) % 360;

function onWheel(e: globalThis.WheelEvent) {
  if (!device.value || busy.value || gesture || !wheelEl.value) return;
  const px = e.deltaY * (e.deltaMode === 1 ? 40 : e.deltaMode === 2 ? 800 : 1);
  if (!px) return;
  e.preventDefault();
  if (wheelAt === null) {
    // Touch down under the cursor first: the wheel only counts motion from there.
    wheelAt = wheelTarget = panelAngle(e);
    lastSentAt = Date.now();
    void device.value.finger(wrap(wheelAt)).catch(() => {});
  }
  wheelTarget -= px * WHEEL_DEG_PER_PX;
  wheelLastEventAt = Date.now();
  if (!wheelTimer) wheelTimer = setTimeout(wheelStep, 16);
}
function wheelStep() {
  wheelTimer = null;
  if (wheelAt === null) return;
  const left = wheelTarget - wheelAt;
  if (Math.abs(left) >= 0.5) {
    wheelAt += Math.max(-WHEEL_MAX_STEP, Math.min(WHEEL_MAX_STEP, left));
    const a = wrap(wheelAt);
    dragAngle.value = a;
    addTrail(a);
    sendFinger(a);
  } else if (Date.now() - wheelLastEventAt >= WHEEL_LIFT_MS) {
    wheelAt = null;
    dragAngle.value = null;
    sendFinger(null);
    setTimeout(() => void device.value?.send("state", "", 2000).catch(() => {}), 150);
    return;
  }
  wheelTimer = setTimeout(wheelStep, 16);
}

// --- E2E tests ---------------------------------------------------------------

const testState = reactive<Record<string, { kind: "running" | "passed" | "failed"; result?: E2EResult }>>({});
const testAbort = shallowRef<AbortController | null>(null);
const testGroups = computed(() => {
  const out: Record<string, E2ETest[]> = {};
  for (const t of TESTS) (out[t.group] ??= []).push(t);
  return out;
});
const testSummary = computed(() => {
  const done = Object.values(testState).filter((s) => s.kind !== "running");
  if (!done.length) return "";
  const failed = done.filter((s) => s.kind === "failed").length;
  return `${done.length - failed} passed · ${failed} failed`;
});
const testIcon = (name: string) => ({ running: "…", passed: "✓", failed: "✕" })[testState[name]?.kind ?? ""] ?? "○";
const testMeta = (name: string) => {
  const s = testState[name];
  if (!s) return "";
  if (s.kind === "running") return "running…";
  return `${((s.result?.ms ?? 0) / 1000).toFixed(1)} s`;
};

async function runTests(tests: E2ETest[] = TESTS) {
  const d = device.value;
  if (!d || busy.value) return;
  testing.value = true;
  for (const t of tests) delete testState[t.name];
  testAbort.value = new AbortController();
  log(d.now(), " ", `run  e2e ${tests.length === TESTS.length ? "all" : tests.map((t) => t.name).join(", ")}`, "evt");
  try {
    await runE2E(d, tests, {
      signal: testAbort.value.signal,
      onProgress: (p) => {
        if (p.kind === "start") testState[p.name] = { kind: "running" };
        else {
          testState[p.result.name] = { kind: p.result.ok ? "passed" : "failed", result: p.result };
          log(d.now(), " ", `run  ${p.result.ok ? "passed" : "failed"} ${p.result.name}`, "evt");
        }
      },
    });
  } catch (e) {
    failMessage(e instanceof Error ? e.message : String(e), "e2e");
  } finally {
    testing.value = false;
    testAbort.value = null;
  }
}

// --- Scenario runner -----------------------------------------------------

type ScenarioProgress = { kind: "queued" | "running" | "done" | "failed"; seconds?: number; message?: string };
const chosen = ref<string[]>(Object.keys(SCENARIOS));
const background = ref(true);
const progress = reactive<Record<string, ScenarioProgress>>({});
const abort = shallowRef<AbortController | null>(null);
const stopping = ref(false);
const current = ref<{ name: string; index: number; total: number; startedAt: number } | null>(null);
const report = shallowRef<Report | null>(null);
const reportJson = ref("");
const reference = ref<{ rev: string; date?: string; summary: Record<string, number> } | null>(null);

function toggleScenario(name: string) {
  chosen.value = chosen.value.includes(name) ? chosen.value.filter((n) => n !== name) : [...chosen.value, name];
}

function scenarioStatus(name: string) {
  const p = progress[name];
  if (!p) {
    const m = SCENARIOS[name].minutes;
    return `~${m < 1 ? `${Math.round(m * 60)} s` : `${m} min`}`;
  }
  if (p.kind === "queued") return "queued";
  if (p.kind === "running") return "running…";
  if (p.kind === "done") return `✓ ${duration(p.seconds ?? 0)}`;
  return `✕ ${p.message}`;
}

/** The running scenario's progress, from its usual duration (it never claims to be done). */
const currentPct = computed(() => {
  const c = current.value;
  if (!c) return 0;
  return Math.min(0.95, (wallNow.value - c.startedAt) / (SCENARIOS[c.name].minutes * 60_000));
});
const runPct = computed(() => (current.value ? (100 * (current.value.index + currentPct.value)) / current.value.total : 0));

function stopRun() {
  stopping.value = true;
  abort.value?.abort();
}

async function runScenarios() {
  const d = device.value;
  if (!d) return;
  const names = Object.keys(SCENARIOS).filter((n) => chosen.value.includes(n));
  running.value = true;
  stopping.value = false;
  report.value = null;
  tab.value = "run";
  for (const k of Object.keys(progress)) delete progress[k];
  for (const n of names) progress[n] = { kind: "queued" };
  abort.value = new AbortController();
  log(d.now(), " ", `run  ${names.join(", ")}`, "evt");
  try {
    const r = await runBench(d, {
      scenarios: names,
      host: "browser",
      background: background.value,
      signal: abort.value.signal,
      onProgress: (p) => {
        if (p.kind === "start") {
          progress[p.scenario] = { kind: "running" };
          current.value = { name: p.scenario, index: p.index, total: p.total, startedAt: Date.now() };
        } else if (p.kind === "done") progress[p.scenario] = { kind: "done", seconds: p.seconds };
        else progress[p.scenario] = { kind: "failed", message: p.message };
        log(d.now(), " ", `run  ${p.kind} ${p.scenario}${p.kind === "failed" ? `: ${p.message}` : ""}`, "evt");
      },
    });
    reportJson.value = JSON.stringify({ ...r, log: d.log }, null, 2);
    report.value = r;
    await loadReference(r.rev);
  } catch (e) {
    failMessage(e instanceof Error ? e.message : String(e), "bench run");
  } finally {
    // Stopped before their turn: back to idle.
    for (const n of names) if (progress[n]?.kind === "queued") delete progress[n];
    running.value = false;
    stopping.value = false;
    current.value = null;
    abort.value = null;
  }
}

/** The latest published report other than this revision, to compare with. */
async function loadReference(rev: string) {
  try {
    const index = await (await fetch(withBase("/bench/index.json"))).json();
    const runs = (index.runs ?? []).filter((r: any) => r.rev !== rev);
    reference.value = runs.length ? runs[runs.length - 1] : null;
  } catch {
    reference.value = null;
  }
}

const summary = computed(() => (report.value ? summarize(report.value) : {}));
const shownMetrics = computed(() => METRICS.filter((m) => summary.value[m.key] !== undefined));
const fileName = computed(() => (report.value ? `${revFile(report.value.rev)}.json` : ""));
const reportKB = computed(() => Math.max(1, Math.round(new Blob([reportJson.value]).size / 1024)));
const publishCmd = computed(() => `npm run bench -- import ~/Downloads/${fileName.value}`);

function change(m: Metric) {
  const a = reference.value?.summary[m.key];
  const b = summary.value[m.key];
  if (a === undefined || b === undefined) return null;
  const delta = b - a;
  const rel = a === 0 ? (delta ? Infinity : 0) : delta / Math.abs(a);
  const worse = m.better === "lower" ? delta > 0 : delta < 0;
  const text = a === 0 ? (delta ? `${delta > 0 ? "+" : ""}${delta}` : "same") : `${rel > 0 ? "+" : "−"}${Math.abs(Math.round(rel * 100))}%`;
  const significant = Math.abs(rel) > 0.1 || (m.unit === "count" && a === 0 && b > 0);
  if (!significant) return { kind: "same", text: "same" };
  return worse ? { kind: "worse", text: `▲ ${text} worse` } : { kind: "better", text: `▼ ${text} better` };
}
const counts = computed(() => {
  const c = { worse: 0, better: 0, same: 0 };
  for (const m of shownMetrics.value) {
    const k = change(m)?.kind as keyof typeof c | undefined;
    if (k) c[k]++;
  }
  return c;
});

function download() {
  if (!report.value) return;
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([reportJson.value], { type: "application/json" }));
  a.download = fileName.value;
  a.click();
  URL.revokeObjectURL(a.href);
}

const copied = ref(false);
async function copyPublish() {
  try {
    await navigator.clipboard.writeText(publishCmd.value);
    copied.value = true;
    setTimeout(() => (copied.value = false), 1400);
  } catch {
    // No clipboard access: the command stays selectable.
  }
}

const hhmm = (iso: string) => new Date(iso).toTimeString().slice(0, 5);
const shortDate = (iso: string) => new Date(iso).toLocaleDateString("en-US", { month: "short", day: "numeric" });
</script>

<style scoped>
/* Light first; dark below. Tokens from the OpenPod Sync identity. */
.bl {
  --bl-bg: var(--vp-c-bg);
  --bl-chrome: #f6f6f7;
  --bl-surface: var(--vp-c-bg-soft);
  --bl-surface2: #ebebef;
  --bl-line: var(--vp-c-divider);
  --bl-line2: #c2c2c4;
  --bl-text: var(--vp-c-text-1);
  --bl-muted: var(--vp-c-text-2);
  --bl-brand: #972d6f;
  --bl-brandbtn: #972d6f;
  --bl-brandhover: #b03d83;
  --bl-brand2: #d98fbb;
  --bl-brandsoft: rgba(151, 45, 111, 0.11);
  --bl-grad: linear-gradient(90deg, #972d6f, #60a534);
  --bl-green: #3d7419;
  --bl-greensoft: rgba(96, 165, 52, 0.14);
  --bl-sgreen: #60a534;
  --bl-amber: #9a5b00;
  --bl-ambersoft: rgba(154, 91, 0, 0.11);
  --bl-red: #a9392f;
  --bl-redsoft: rgba(169, 57, 47, 0.11);
  --bl-band: rgba(169, 57, 47, 0.12);
  --bl-mono: "IBM Plex Mono", var(--vp-font-family-mono);

  /* Full width, out of the doc column. */
  margin: 24px calc(50% - 50vw) 0;
  font-family: "Space Grotesk", var(--vp-font-family-base);
  font-size: 14px;
  line-height: 1.45;
  font-variant-numeric: tabular-nums;
  color: var(--bl-text);
  background: var(--bl-bg);
  container-type: inline-size;
}
:root.dark .bl {
  --bl-chrome: #161618;
  --bl-surface2: #32363f;
  --bl-line2: #3c3f44;
  --bl-brand: #cb5297;
  --bl-brand2: #80406a;
  --bl-brandsoft: rgba(203, 82, 151, 0.16);
  --bl-grad: linear-gradient(90deg, #cb5297, #96dd42);
  --bl-green: #96dd42;
  --bl-greensoft: rgba(150, 221, 66, 0.13);
  --bl-sgreen: #96dd42;
  --bl-amber: #f0a83a;
  --bl-ambersoft: rgba(240, 168, 58, 0.13);
  --bl-red: #e8857b;
  --bl-redsoft: rgba(232, 133, 123, 0.14);
  --bl-band: rgba(232, 133, 123, 0.18);
}
.bl button,
.bl input {
  font-family: inherit;
}
.bl h2 {
  margin: 0;
  padding: 0;
  border: 0;
  font-size: 24px;
  font-weight: 600;
  letter-spacing: -0.02em;
  line-height: 1.25;
}
.bl p {
  margin: 0;
}
.bl a {
  color: var(--bl-brand);
}
.bl code {
  padding: 0;
  background: none;
  font-family: var(--bl-mono);
  font-size: 12px;
  color: var(--bl-text);
}

/* --- Bits ---------------------------------------------------------------- */
.bl-mono {
  font-family: var(--bl-mono);
}
.bl-muted {
  color: var(--bl-muted);
}
.bl-text {
  color: var(--bl-text);
}
.bl-small {
  font-size: 12px;
}
.bl-xs {
  font-size: 11px;
}
.bl-spacer {
  flex: 1;
}
.bl-label {
  font-size: 12px;
  font-weight: 600;
  letter-spacing: 0.08em;
  text-transform: uppercase;
  color: var(--bl-muted);
}
.bl-cmd {
  font-family: var(--bl-mono);
  font-size: 12px;
  color: var(--bl-text);
}
.bl-panel {
  background: var(--bl-surface);
  border: 1px solid var(--bl-line);
  border-radius: 12px;
}
.bl-badge {
  display: inline-flex;
  align-items: center;
  height: 20px;
  padding: 0 7px;
  border-radius: 4px;
  font-family: var(--bl-mono);
  font-size: 11px;
  font-weight: 500;
  text-transform: none;
  letter-spacing: 0;
  white-space: nowrap;
}
.bl-badge.brand {
  background: var(--bl-brandsoft);
  color: var(--bl-brand);
}
.bl-badge.amber {
  background: var(--bl-ambersoft);
  color: var(--bl-amber);
}
.bl-badge.red {
  background: var(--bl-redsoft);
  color: var(--bl-red);
}
.bl-badge.green {
  background: var(--bl-greensoft);
  color: var(--bl-green);
}
.bl-badge.muted {
  background: var(--bl-surface2);
  color: var(--bl-muted);
}
.bl-note {
  padding: 12px 14px;
  border-radius: 10px;
  font-size: 13px;
  text-wrap: pretty;
}
.bl-note.amber {
  background: var(--bl-ambersoft);
}
.bl-note.amber b {
  font-weight: 600;
  color: var(--bl-amber);
}
.bl-note.red {
  display: flex;
  flex-direction: column;
  gap: 4px;
  padding: 14px 16px;
  background: var(--bl-redsoft);
}
.bl-note.red b,
.bl-note-title {
  font-weight: 600;
  color: var(--bl-red);
}

/* Buttons */
.bl-ghost,
.bl-primary,
.bl-text-btn {
  height: 34px;
  padding: 0 14px;
  border-radius: 8px;
  font-size: 13px;
  font-weight: 500;
  white-space: nowrap;
  cursor: pointer;
}
.bl-ghost {
  border: 1px solid var(--bl-line2);
  background: transparent;
  color: var(--bl-text);
}
.bl-ghost:hover:not(:disabled) {
  background: var(--bl-surface2);
}
.bl-ghost.sm {
  height: 32px;
  padding: 0 12px;
}
.bl-ghost.md,
.bl-text-btn.md {
  height: 36px;
  padding: 0 12px;
}
.bl-ghost.xs {
  height: 26px;
  padding: 0 10px;
  font-size: 12px;
}
.bl-ghost.mono {
  height: 38px;
  padding: 0;
  font-family: var(--bl-mono);
}
.bl-text-btn {
  padding: 0 10px;
  border: 0;
  background: transparent;
  color: var(--bl-muted);
}
.bl-text-btn:hover {
  color: var(--bl-text);
}
.bl-primary {
  height: 38px;
  padding: 0 18px;
  border: 0;
  background: var(--bl-brandbtn);
  color: #fff;
  font-size: 14px;
  font-weight: 600;
}
.bl-primary:hover:not(:disabled) {
  background: var(--bl-brandhover);
}
.bl-primary:disabled,
.bl-primary.quiet {
  background: var(--bl-surface2);
  color: var(--bl-muted);
  cursor: default;
}
.bl-primary.quiet:not(:disabled) {
  color: var(--bl-text);
  cursor: pointer;
}
.bl-primary.wide {
  width: 100%;
}
.bl-ghost:disabled,
.bl-text-btn:disabled {
  opacity: 0.5;
  cursor: default;
}
.bl-check {
  display: grid;
  place-items: center;
  width: 18px;
  height: 18px;
  flex: none;
  box-sizing: border-box;
  border: 1.5px solid var(--bl-line2);
  border-radius: 5px;
  color: #fff;
  font-size: 12px;
  font-weight: 700;
  line-height: 1;
}
.bl-check.on {
  border-color: var(--bl-brandbtn);
  background: var(--bl-brandbtn);
}
.bl-checkbox {
  display: flex;
  align-items: center;
  gap: 10px;
  padding: 0;
  border: 0;
  background: none;
  color: var(--bl-text);
  font-size: 13px;
  cursor: pointer;
}
.bl-checkbox:disabled {
  opacity: 0.5;
  cursor: default;
}
.bl input:not([type="checkbox"]) {
  height: 36px;
  padding: 0 10px;
  box-sizing: border-box;
  border: 1px solid var(--bl-line2);
  border-radius: 8px;
  background: transparent;
  color: var(--bl-text);
  font-size: 13px;
  outline: none;
}
.bl input:focus {
  border-color: var(--bl-text);
}

/* --- Status bar ---------------------------------------------------------- */
.bl-status {
  position: sticky;
  top: var(--vp-nav-height, 64px);
  z-index: 10;
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 10px 18px;
  padding: 12px 32px;
  background: var(--bl-chrome);
  border-top: 1px solid var(--bl-line);
  border-bottom: 1px solid var(--bl-line);
}
.bl-brand {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 15px;
}
.bl-brand img {
  width: 22px;
  height: 22px;
  border-radius: 6px;
}
.bl-brand b {
  font-weight: 700;
}
.bl-divider {
  width: 1px;
  height: 22px;
  background: var(--bl-line);
}
.bl-live {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 13px;
}
.bl-live b {
  font-weight: 600;
}
.bl-live .bl-mono {
  font-size: 12px;
}
.bl-live i {
  width: 7px;
  height: 7px;
  border-radius: 50%;
  background: var(--bl-muted);
}
.bl-live.on i {
  background: var(--bl-green);
}
.bl-window,
.bl-actions {
  display: flex;
  align-items: center;
  gap: 8px;
}

/* --- Page ------------------------------------------------------------------- */
.bl-page {
  display: flex;
  flex-direction: column;
  gap: 20px;
  max-width: 1720px;
  margin: 0 auto;
  padding: 24px 32px 56px;
  box-sizing: border-box;
}
.bl-stack {
  display: flex;
  flex-direction: column;
  gap: 28px;
}

/* Errors */
.bl-error {
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto;
  gap: 14px;
  align-items: start;
  padding: 14px 16px;
  border-radius: 10px;
  background: var(--bl-redsoft);
}
.bl-error-dot {
  width: 8px;
  height: 8px;
  margin-top: 6px;
  border-radius: 50%;
  background: var(--bl-red);
}
.bl-error-text {
  display: flex;
  flex-direction: column;
  gap: 4px;
}
.bl-error-title {
  font-weight: 600;
  color: var(--bl-red);
}
.bl-error-body {
  max-width: 820px;
  text-wrap: pretty;
}
.bl .bl-chip-code {
  padding: 1px 5px;
  border-radius: 4px;
  background: var(--bl-surface2);
  font-family: var(--bl-mono);
  font-size: 12px;
}
.bl-error-details {
  font-family: var(--bl-mono);
  font-size: 12px;
  color: var(--bl-muted);
  overflow-wrap: anywhere;
}
.bl-error-actions {
  display: flex;
  gap: 8px;
}
.bl-error-actions .bl-text-btn {
  height: 32px;
}

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

/* Tabs */
.bl-tabs {
  display: flex;
  gap: 24px;
  border-bottom: 1px solid var(--bl-line);
  overflow-x: auto;
}
.bl-tabs button {
  display: flex;
  align-items: center;
  gap: 8px;
  height: 42px;
  padding: 0 2px;
  border: 0;
  background: transparent;
  color: var(--bl-muted);
  font-size: 14px;
  font-weight: 600;
  white-space: nowrap;
  cursor: pointer;
}
.bl-tabs button.on {
  color: var(--bl-text);
  box-shadow: inset 0 -2px 0 var(--bl-brand);
}
.bl-tab-count {
  font-family: var(--bl-mono);
  font-size: 11px;
  font-weight: 500;
  color: var(--bl-muted);
}

/* Live tab */
.bl-live-grid {
  display: grid;
  grid-template-columns: 340px minmax(0, 1fr);
  gap: 20px;
  align-items: start;
}
@container (max-width: 999px) {
  .bl-live-grid {
    grid-template-columns: minmax(0, 1fr);
  }
  .bl-remote {
    position: static !important;
  }
}

/* Remote */
.bl-remote {
  /* Stays in view under the nav and the status bar while the charts scroll. */
  --bl-stick: calc(var(--vp-nav-height, 64px) + var(--bl-status-h) + 16px);
  position: sticky;
  top: var(--bl-stick);
  display: flex;
  flex-direction: column;
  gap: 18px;
  padding: 22px;
}
.bl-locked {
  align-self: center;
}
.bl-remote-body {
  display: flex;
  flex-direction: column;
  gap: 18px;
}
.bl-remote-body.locked {
  opacity: 0.4;
  pointer-events: none;
}
.bl-wheel-wrap {
  display: flex;
  justify-content: center;
}
.bl-wheel {
  position: relative;
  width: 280px;
  height: 280px;
  flex: none;
  border-radius: 50%;
  background: var(--bl-surface2);
  overflow: hidden;
  user-select: none;
  touch-action: none;
}
.bl-wedge {
  position: absolute;
  inset: 0;
  display: flex;
  border: 0;
  background: transparent;
  color: var(--bl-text);
  font-size: 18px;
  cursor: pointer;
}
.bl-wedge:hover {
  background: var(--bl-line2);
}
/* Dragging around the ring: no button is being pressed, so none lights up. */
.bl-wheel.dragging {
  cursor: grabbing;
}
.bl-wheel.dragging .bl-wedge,
.bl-wheel.dragging .bl-wedge:hover {
  background: transparent;
  cursor: grabbing;
}
.bl-trail {
  position: absolute;
  inset: 0;
  width: 100%;
  height: 100%;
  pointer-events: none;
}
.bl-trail line {
  stroke: var(--bl-brand);
  stroke-width: 18;
  stroke-linecap: round;
}
.bl-trail circle {
  fill: var(--bl-brand);
  stroke: var(--bl-surface);
  stroke-width: 3;
}
.bl-wedge.lit,
.bl-center.lit {
  color: var(--bl-brand);
}
.w-top {
  clip-path: polygon(0 0, 100% 0, 50% 50%);
  justify-content: center;
  align-items: flex-start;
  padding-top: 26px;
  font-size: 13px;
  font-weight: 700;
  letter-spacing: 0.1em;
}
.w-right {
  clip-path: polygon(100% 0, 100% 100%, 50% 50%);
  justify-content: flex-end;
  align-items: center;
  padding-right: 28px;
}
.w-bottom {
  clip-path: polygon(0 100%, 100% 100%, 50% 50%);
  justify-content: center;
  align-items: flex-end;
  padding-bottom: 26px;
}
.w-left {
  clip-path: polygon(0 0, 0 100%, 50% 50%);
  justify-content: flex-start;
  align-items: center;
  padding-left: 28px;
}
.bl-center {
  position: absolute;
  left: 86px;
  top: 86px;
  width: 108px;
  height: 108px;
  border: 1px solid var(--bl-line2);
  border-radius: 50%;
  background: var(--bl-surface);
  color: var(--bl-text);
  font-size: 12px;
  font-weight: 600;
  letter-spacing: 0.06em;
  cursor: pointer;
}
.bl-center:hover {
  background: var(--bl-bg);
}
.bl-finger {
  position: absolute;
  width: 22px;
  height: 22px;
  margin: -11px 0 0 -11px;
  border-radius: 50%;
  background: var(--bl-brand);
  opacity: 0.55;
  pointer-events: none;
}
.bl-scroll {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 6px;
}
.bl-field {
  display: flex;
  flex-direction: column;
  gap: 8px;
}
.bl-field-row {
  display: flex;
  align-items: center;
  gap: 6px;
}
.bl-field-row input:not(.short) {
  flex: 1;
  min-width: 0;
}
.bl input.short {
  width: 80px;
}

/* Charts */
.bl-charts {
  display: flex;
  flex-direction: column;
  gap: 16px;
  min-width: 0;
}
.bl-charts-head {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 12px 20px;
}
.bl-chips {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.bl-chip {
  height: 28px;
  padding: 0 12px;
  border: 1px solid var(--bl-line2);
  border-radius: 999px;
  background: transparent;
  color: var(--bl-muted);
  font-size: 12px;
  font-weight: 500;
  cursor: pointer;
}
.bl-chip.on {
  border-color: var(--bl-text);
  background: var(--bl-text);
  color: var(--bl-bg);
}
.bl-chart-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(min(100%, 420px), 1fr));
  gap: 16px;
}
.bl-chart-grid.single {
  grid-template-columns: minmax(0, 1fr);
}
.bl-underrun-note {
  display: flex;
  align-items: center;
  gap: 10px;
  font-size: 12px;
  color: var(--bl-muted);
}
.bl-band-swatch {
  width: 14px;
  height: 12px;
  border-radius: 2px;
  background: var(--bl-band);
}

/* Runner */
.bl-runner {
  display: flex;
  flex-direction: column;
  gap: 16px;
  min-width: 0;
}
.bl-section-head {
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.bl-section-head p {
  text-wrap: pretty;
}
.bl-progress {
  display: flex;
  flex-direction: column;
  gap: 8px;
}
.bl-progress-row {
  display: flex;
  justify-content: space-between;
  font-size: 13px;
}
.bl-progress-row b {
  font-weight: 600;
}
.bl-progress-track {
  height: 10px;
  border-radius: 5px;
  background: var(--bl-surface2);
  overflow: hidden;
}
.bl-progress-track div {
  height: 100%;
  border-radius: 5px;
  background: var(--bl-grad);
  transition: width 0.9s linear;
}
.bl-scenarios {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
  gap: 10px;
}
.bl-sc {
  position: relative;
  display: grid;
  grid-template-columns: auto minmax(0, 1fr) auto;
  gap: 12px;
  align-items: start;
  padding: 14px 14px 16px;
  overflow: hidden;
  border: 1px solid var(--bl-line);
  border-radius: 10px;
  background: var(--bl-surface);
  color: var(--bl-text);
  font-size: 14px;
  text-align: left;
  cursor: pointer;
}
.bl-sc:hover:not(:disabled) {
  background: var(--bl-surface2);
}
.bl-sc:disabled {
  cursor: default;
}
.bl-sc.running {
  border-color: var(--bl-brand);
}
.bl-sc .bl-check {
  margin-top: 1px;
}
.bl-sc-text {
  display: flex;
  flex-direction: column;
  min-width: 0;
}
.bl-sc-name {
  font-weight: 600;
}
.bl-sc-desc {
  font-size: 12px;
  color: var(--bl-muted);
}
.bl-sc-status {
  max-width: 130px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-family: var(--bl-mono);
  font-size: 12px;
  color: var(--bl-muted);
}
.bl-sc-status.running {
  color: var(--bl-brand);
}
.bl-sc-status.done {
  color: var(--bl-green);
}
.bl-sc-status.failed {
  color: var(--bl-red);
}
.bl-sc-bar {
  position: absolute;
  left: 0;
  bottom: 0;
  height: 3px;
  background: var(--bl-grad);
  transition: width 0.9s linear;
}
.bl-run-foot {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 16px;
}

/* Results */
.bl-results {
  display: grid;
  grid-template-columns: minmax(0, 1fr) 320px;
  gap: 24px;
  padding: 24px;
}
@container (max-width: 1099px) {
  .bl-results {
    grid-template-columns: minmax(0, 1fr);
  }
}
.bl-results-main {
  display: flex;
  flex-direction: column;
  gap: 16px;
  min-width: 0;
}
.bl-results-title {
  display: flex;
  align-items: baseline;
  flex-wrap: wrap;
  gap: 12px;
}
.bl-badges {
  display: flex;
  flex-wrap: wrap;
  gap: 6px;
}
.bl-table-wrap {
  overflow-x: auto;
}
.bl-table-wrap table {
  display: table;
  width: 100%;
  margin: 0;
  min-width: 520px;
  border-collapse: collapse;
  font-size: 13px;
}
.bl-table-wrap tr {
  border: 0;
  background: none;
}
.bl-table-wrap th {
  padding: 0 12px 8px;
  border: 0;
  border-bottom: 1px solid var(--bl-line);
  background: none;
  font-size: 12px;
  font-weight: 500;
  text-align: left;
  color: var(--bl-muted);
}
.bl-table-wrap td {
  padding: 10px 12px;
  border: 0;
  border-bottom: 1px solid var(--bl-line);
  background: none;
}
.bl-table-wrap th:first-child,
.bl-table-wrap td:first-child {
  padding-left: 0;
}
.bl-table-wrap th:last-child,
.bl-table-wrap td:last-child {
  padding-right: 0;
}
.bl-table-wrap .r {
  text-align: right;
  white-space: nowrap;
}
.bl-change {
  display: inline-flex;
  align-items: center;
  height: 22px;
  padding: 0 8px;
  border-radius: 4px;
  font-family: var(--bl-mono);
  font-size: 12px;
  font-weight: 500;
  color: var(--bl-muted);
}
.bl-change.worse {
  background: var(--bl-redsoft);
  color: var(--bl-red);
  font-weight: 600;
}
.bl-change.better {
  background: var(--bl-greensoft);
  color: var(--bl-green);
}
.bl-publish {
  display: flex;
  flex-direction: column;
  gap: 18px;
  align-self: start;
  padding: 20px;
  border: 1px solid var(--bl-line);
  border-radius: 10px;
  background: var(--bl-bg);
}
.bl-step {
  display: grid;
  grid-template-columns: 24px minmax(0, 1fr);
  gap: 10px;
}
.bl-step-n {
  display: grid;
  place-items: center;
  width: 24px;
  height: 24px;
  border-radius: 50%;
  background: var(--bl-surface2);
  font-family: var(--bl-mono);
  font-size: 12px;
}
.bl-step-body {
  display: flex;
  flex-direction: column;
  gap: 10px;
  min-width: 0;
}
.bl-code {
  display: flex;
  align-items: center;
  gap: 8px;
  padding: 10px 12px;
  border-radius: 8px;
  background: var(--bl-surface2);
  font-family: var(--bl-mono);
  font-size: 12px;
}
.bl-code span {
  flex: 1;
  min-width: 0;
  word-break: break-all;
}
.bl-code button {
  height: 26px;
  padding: 0 8px;
  border: 1px solid var(--bl-line2);
  border-radius: 6px;
  background: transparent;
  color: var(--bl-text);
  font-size: 11px;
  cursor: pointer;
}

/* E2E tests */
.bl-test-groups {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(320px, 1fr));
  gap: 16px 20px;
}
.bl-test-group {
  display: flex;
  flex-direction: column;
  gap: 6px;
}
.bl-test {
  display: grid;
  grid-template-columns: 18px minmax(0, 1fr) auto auto;
  align-items: center;
  gap: 4px 10px;
  padding: 8px 10px 8px 12px;
  border: 1px solid var(--bl-line);
  border-radius: 10px;
  background: var(--bl-surface);
}
.bl-test.running {
  border-color: var(--bl-brand);
}
.bl-test.failed {
  border-color: var(--bl-red);
}
.bl-test-icon {
  font-weight: 700;
  text-align: center;
  color: var(--bl-muted);
}
.bl-test.running .bl-test-icon {
  color: var(--bl-brand);
}
.bl-test.passed .bl-test-icon {
  color: var(--bl-green);
}
.bl-test.failed .bl-test-icon {
  color: var(--bl-red);
}
.bl-test-name {
  overflow: hidden;
  text-overflow: ellipsis;
}
.bl-test-error {
  grid-column: 2 / -1;
  font-size: 12px;
  color: var(--bl-red);
  overflow-wrap: anywhere;
}
.bl-test-error code {
  font-size: 11px;
}

/* Log */
.bl-log {
  overflow: hidden;
}
.bl-log-head {
  display: flex;
  align-items: center;
  gap: 12px;
  width: 100%;
  padding: 16px 22px;
  border: 0;
  background: none;
  color: var(--bl-text);
  text-align: left;
  cursor: pointer;
}
.bl-chevron {
  width: 12px;
}
.bl-log-title {
  font-size: 16px;
  font-weight: 600;
}
.bl-log-body {
  display: flex;
  flex-direction: column;
  gap: 12px;
  padding: 14px 22px 18px;
  border-top: 1px solid var(--bl-line);
}
.bl-log-tools {
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 16px;
}
.bl-log-tools input {
  width: 260px;
  max-width: 100%;
  height: 34px !important;
  font-size: 12px !important;
}
.bl-log-lines {
  max-height: 360px;
  overflow: auto;
  padding: 12px 14px;
  border-radius: 8px;
  background: var(--bl-bg);
  font-family: var(--bl-mono);
  font-size: 12px;
  line-height: 1.6;
  white-space: pre;
}
.bl-log-lines .mon {
  color: var(--bl-muted);
}
.bl-log-lines .cmd {
  color: var(--bl-brand);
}
.bl-log-lines .evt {
  color: var(--bl-green);
}
.bl-log-lines .bad {
  color: var(--bl-red);
}

@container (max-width: 640px) {
  .bl-status,
  .bl-page {
    padding-left: 16px;
    padding-right: 16px;
  }
}
</style>

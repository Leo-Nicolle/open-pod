// Buttons held with the virtual finger: debounce, click vs long vs very long
// press (src/ClickWheel.hpp: LONG_PRESS_MS 800, VERY_LONG_PRESS_MS 2000), and
// what they do (src/app/input.cpp).
import { ensureBackground, Route } from "../scenarios.ts";
import { check, equal, sequence, type E2ETest } from "./e2e.ts";

const group = "Buttons";

export const buttonTests: E2ETest[] = [
  {
    group,
    name: "A short press is a click and opens the row",
    async run(t) {
      await t.goRoot();
      await t.selectLabel("Tracks");
      const mark = t.mark();
      t.step("hold select 120 ms");
      await t.d.hold("select", 120);
      equal(sequence(t.eventsSince(mark)), "down:select up:select click:select", "events");
      await t.waitFor("on the track list", (s) => +s.route === Route.TRACKS && s.anim === "0");
    },
  },
  {
    group,
    name: "Menu goes back one level",
    async run(t) {
      await t.openRootItem("Tracks");
      await t.waitFor("transition done", (s) => s.anim === "0");
      t.step("hold menu 120 ms");
      await t.d.hold("menu", 120);
      await t.waitFor("back on the root menu", (s) => +s.route === Route.ROOT);
    },
  },
  {
    group,
    name: "A press shorter than the debounce does nothing",
    async run(t) {
      await t.goRoot();
      const before = await t.state();
      const mark = t.mark();
      t.step("hold select 8 ms (debounce is 20 ms)");
      await t.d.hold("select", 8);
      // The pin interrupt still catches it and replays a click: see
      // ClickWheel::updateButtons. The virtual finger has no interrupt, so
      // nothing should come out.
      equal(sequence(t.eventsSince(mark)), "", "events");
      equal((await t.state()).route, before.route, "route");
    },
  },
  {
    group,
    name: "Holding select 1 s jumps to Now Playing, with no click",
    async run(t) {
      t.step("make sure something plays");
      await ensureBackground(t.d);
      await t.openRootItem("Tracks");
      await t.waitFor("transition done", (s) => s.anim === "0");
      const mark = t.mark();
      t.step("hold select 1000 ms");
      await t.d.hold("select", 1000);
      equal(sequence(t.eventsSince(mark)), "down:select long:select up:select", "events");
      await t.waitFor("on Now Playing", (s) => +s.route === Route.NOW_PLAYING);
    },
  },
  {
    group,
    name: "A very long press fires long, then very long",
    async run(t) {
      await t.goRoot();
      const mark = t.mark();
      t.step("hold menu 2200 ms");
      await t.d.hold("menu", 2200);
      const seq = sequence(t.eventsSince(mark));
      equal(seq, "down:menu long:menu verylong:menu up:menu", "events");
      check(!seq.includes("click"), "no click after a long press");
    },
  },
];

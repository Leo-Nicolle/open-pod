// Scrolling with the virtual finger: one click per 15°, the lift-off filter,
// and when the fast-scroll overlay kicks in (src/app/input.cpp,
// trackFastScroll: 6 clicks within 300 ms, long lists only).
import { Route } from "../scenarios.ts";
import { check, equal, scrolled, type E2ETest } from "./e2e.ts";

const group = "Scroll";

export const scrollTests: E2ETest[] = [
  {
    group,
    name: "15° moves one row",
    async run(t) {
      await t.openRootItem("Tracks");
      const before = +(await t.state()).sel;
      const mark = t.mark();
      t.step("turn 15° clockwise");
      await t.d.spin(-15, 150);
      equal(scrolled(t.eventsSince(mark)), -1, "scroll clicks");
      equal(+(await t.state()).sel, before + 1, "selected row");
    },
  },
  {
    group,
    name: "A slow turn scrolls row by row, no overlay",
    async run(t) {
      await t.openRootItem("Tracks");
      const before = +(await t.state()).sel;
      const mark = t.mark();
      t.step("turn 90° (6 clicks) over 1.5 s");
      await t.d.spin(-90, 1500);
      const ev = t.eventsSince(mark).filter((e) => e.type === "scroll");
      equal(scrolled(ev), -6, "scroll clicks");
      check(ev.every((e) => Math.abs(e.value!) === 1), `one click per event, got ${ev.map((e) => e.value).join(" ")}`);
      const s = await t.state();
      equal(+s.sel, before + 6, "selected row");
      equal(s.fast, "0", "fast-scroll overlay");
    },
  },
  {
    group,
    name: "A fast turn on a long list shows the overlay, then hides it",
    async run(t) {
      await t.openRootItem("Tracks");
      const s0 = await t.state();
      check(+s0.total > 21, `needs a list longer than 3 screens, Tracks has ${s0.total}`);
      t.step("turn a full circle in 400 ms");
      await t.d.spin(-360, 400);
      const s1 = await t.state();
      equal(s1.fast, "1", "fast-scroll overlay right after the turn");
      check(+s1.sel >= +s0.sel + 24, `moved at least 24 rows (from ${s0.sel} to ${s1.sel})`);
      t.step("wait for the overlay to hide (600 ms after the wheel stops)");
      await t.waitFor("overlay hidden", (s) => s.fast === "0", 2000);
    },
  },
  {
    group,
    name: "A short list never goes fast",
    async run(t) {
      await t.goRoot();
      const s0 = await t.state();
      check(+s0.total <= 21, `root menu should be short, has ${s0.total} rows`);
      t.step("turn a full circle in 400 ms on the root menu");
      await t.d.spin(-360, 400);
      const s1 = await t.state();
      equal(s1.fast, "0", "fast-scroll overlay");
      equal(+s1.sel, +s1.total - 1, "selection clamped at the last row");
    },
  },
  {
    group,
    name: "Turning back lands on the same row",
    async run(t) {
      await t.openRootItem("Tracks");
      const before = +(await t.state()).sel;
      t.step("turn 90° clockwise, then 90° back");
      await t.d.spin(-90, 800);
      await t.d.spin(90, 800);
      equal(+(await t.state()).sel, before, "selected row");
    },
  },
  {
    group,
    name: "A twitch while lifting the finger is ignored",
    async run(t) {
      await t.openRootItem("Tracks");
      const before = await t.state();
      equal(+before.route, Route.TRACKS, "route");
      const mark = t.mark();
      t.step("rest the finger 300 ms, slide 15° in 20 ms and lift at once");
      // Lifting smears the finger across the pads: the wheel holds clicks
      // 80 ms and drops them if the touch ends meanwhile.
      await t.d.gesture([
        { at: 0, angle: 90 },
        { at: 300, angle: 90 },
        { at: 320, angle: 75 },
        { at: 330, angle: null },
      ]);
      equal(scrolled(t.eventsSince(mark)), 0, "scroll clicks");
      equal((await t.state()).sel, before.sel, "selected row");
    },
  },
];

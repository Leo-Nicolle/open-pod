// Scrolling with the virtual finger: one click per 15°, the lift-off filter,
// and the fast-scroll tiers (src/state/fast_scroll.h, long lists only): rows
// below 12 clicks/s, then 3-, 2- and 1-letter jumps from 12, 25 and 40
// clicks/s (24 clicks a turn), with the letter overlay.
import { Route } from "../scenarios.ts";
import type { WheelEvent } from "../device.ts";
import { check, equal, scrolled, type E2ETest } from "./e2e.ts";

/** The letters of each fast-scroll tier change (`#ev fast`), in order. */
const tiers = (ev: WheelEvent[]) => ev.filter((e) => e.type === "fast").map((e) => e.letters);

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
    name: "A steady turn jumps by 3 letters",
    async run(t) {
      await t.openRootItem("Tracks");
      const s0 = await t.state();
      check(+s0.total > 1500, `needs a list of 1500+ rows for 3 letters, Tracks has ${s0.total}`);
      const mark = t.mark();
      t.step("turn a full circle in 1.2 s (20 clicks/s)");
      await t.d.spin(-360, 1200);
      const s1 = await t.state();
      equal(tiers(t.eventsSince(mark)).join(" "), "3", "tiers");
      equal(s1.letters, "3", "overlay letters");
      check(+s1.sel > +s0.sel + 24, `jumped further than 24 rows (from ${s0.sel} to ${s1.sel})`);
      t.step("wait for the overlay to hide (600 ms after the last jump)");
      await t.waitFor("overlay hidden", (s) => s.fast === "0", 2000);
    },
  },
  {
    group,
    name: "A flick jumps by first letter, onto a bucket start",
    async run(t) {
      await t.openRootItem("Tracks");
      const s0 = await t.state();
      check(+s0.total > 21, `needs a list longer than 3 screens, Tracks has ${s0.total}`);
      const mark = t.mark();
      t.step("turn a full circle in 400 ms (60 clicks/s)");
      await t.d.spin(-360, 400);
      const s1 = await t.state();
      const seen = tiers(t.eventsSince(mark));
      equal(seen[seen.length - 1], 1, `last tier (tiers ${seen.join(" ")})`);
      equal(s1.fast, "1", "overlay right after the turn");
      equal(s1.letters, "1", "overlay letters");
      t.step("one click up: the row before is in another bucket");
      await t.d.scroll(1);
      const s2 = await t.state();
      check(s2.key[0] !== s1.key[0] || +s1.sel === 0, `"${s2.item}" (${s2.key}) and "${s1.item}" (${s1.key}) share a first letter`);
      await t.waitFor("overlay hidden", (s) => s.fast === "0", 2000);
    },
  },
  {
    group,
    name: "Turning back after a flick scrolls by rows",
    async run(t) {
      await t.openRootItem("Tracks");
      t.step("flick down, then at once turn back 45° slowly");
      await t.d.spin(-360, 400, 0, 0);
      const mark = t.mark();
      const s0 = await t.state();
      await t.d.spin(45, 600);
      equal(tiers(t.eventsSince(mark)).join(" "), "0", "tiers after turning back");
      equal(+(await t.state()).sel, +s0.sel - 3, "selected row");
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

// Seek mode on Now Playing (src/app/input.cpp): select enters it, turning the
// wheel previews a position (clockwise = forward), the file seek is committed
// once the wheel rests 400 ms, and seek mode ends after 3 s idle.
import { ensureBackground, Route } from "../scenarios.ts";
import { check, equal, type E2ETest } from "./e2e.ts";

const group = "Seek";

async function toNowPlaying(t: Parameters<E2ETest["run"]>[0]) {
  t.step("make sure something plays, open Now Playing");
  await ensureBackground(t.d);
  await t.goRoot();
  await t.selectLabel("Now Playing");
  await t.d.click("select");
  await t.waitFor("on Now Playing", (s) => +s.route === Route.NOW_PLAYING && s.anim === "0");
}

export const seekTests: E2ETest[] = [
  {
    group,
    name: "Turning in seek mode moves the position forward",
    async run(t) {
      await toNowPlaying(t);
      t.step("select: enter seek mode");
      await t.d.hold("select", 100);
      const s0 = await t.waitFor("seek mode on", (s) => s.seek === "1");
      t.step("turn 30° clockwise (2 clicks, 3 s or more each)");
      await t.d.spin(-30, 300);
      // #state's pos is the decoder's: it moves once the seek is committed,
      // 400 ms after the wheel rests (the screen previews it before that).
      t.step("wait for the seek to be committed");
      const s1 = await t.waitFor(`position past ${+s0.pos + 5} s`, (s) => +s.pos >= +s0.pos + 5, 2000);
      check(s1.seek === "1", "still in seek mode right after the scrub");
      t.step("wait for seek mode to end (3 s idle)");
      await t.waitFor("seek mode off", (s) => s.seek === "0", 5000);
      equal(+(await t.state()).route, Route.NOW_PLAYING, "still on Now Playing");
    },
  },
  {
    group,
    name: "Out of seek mode, turning changes the volume",
    async run(t) {
      await toNowPlaying(t);
      const s0 = await t.state();
      equal(s0.seek, "0", "seek mode");
      t.step("turn 30° counter-clockwise (2 clicks: volume down)");
      await t.d.spin(30, 300);
      const s1 = await t.state();
      check(+s1.vol < +s0.vol || +s0.vol === 0, `volume went down (from ${s0.vol} to ${s1.vol})`);
      t.step("turn back");
      await t.d.spin(-30, 300);
      equal((await t.state()).vol, s0.vol, "volume restored");
    },
  },
];

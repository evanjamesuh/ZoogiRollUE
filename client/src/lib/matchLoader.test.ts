import test from "node:test";
import assert from "node:assert/strict";
import {
  MATCH_LOADER_FADE_MS,
  MATCH_LOADER_MIN_VISIBLE_MS,
  MATCH_LOADER_SHOW_DELAY_MS,
  decideMatchLoader,
  matchLoadingTips,
  worldLabelsCovered,
} from "./matchLoader.ts";
import { BUMPER_SCORE, KNOCKOUT_PENALTY, KNOCKOUT_SCORE_ORB, KNOCKOUT_SCORE_PLAYER, ZONE_SCORE_ORB } from "./arenaConstants.ts";

const startedAt = 10_000;

test("a load that finishes before the show delay never mounts the screen", () => {
  const sceneReadyAt = startedAt + 200;
  const whileLoading = decideMatchLoader({
    now: startedAt + 120,
    startedAt,
    sceneReadyAt: null,
    shownAt: null,
  });
  assert.equal(whileLoading.phase, "waiting");
  assert.equal(whileLoading.mounted, false);
  assert.equal(whileLoading.opacity, 0);

  const after = decideMatchLoader({
    now: startedAt + 800,
    startedAt,
    sceneReadyAt,
    shownAt: null,
  });
  assert.equal(after.phase, "gone");
  assert.equal(after.mounted, false);
  assert.equal(after.opacity, 0);
  assert.equal(after.shownAt, null);
});

test("the screen stays hidden through the show delay and appears once loading runs longer", () => {
  const justBefore = decideMatchLoader({
    now: startedAt + MATCH_LOADER_SHOW_DELAY_MS - 1,
    startedAt,
    sceneReadyAt: null,
    shownAt: null,
  });
  assert.equal(justBefore.mounted, false);
  assert.equal(justBefore.phase, "waiting");

  const atDelay = decideMatchLoader({
    now: startedAt + MATCH_LOADER_SHOW_DELAY_MS,
    startedAt,
    sceneReadyAt: null,
    shownAt: null,
  });
  assert.equal(atDelay.mounted, true);
  assert.equal(atDelay.phase, "visible");
  assert.equal(atDelay.opacity, 1);
  assert.equal(atDelay.shownAt, startedAt + MATCH_LOADER_SHOW_DELAY_MS);
});

test("finishing under about 300ms does not flash, and finishing just after the delay does", () => {
  const under = decideMatchLoader({
    now: startedAt + 290,
    startedAt,
    sceneReadyAt: startedAt + 240,
    shownAt: null,
  });
  assert.equal(under.mounted, false);
  assert.equal(under.phase, "gone");

  const over = decideMatchLoader({
    now: startedAt + 280,
    startedAt,
    sceneReadyAt: startedAt + 280,
    shownAt: null,
  });
  assert.equal(over.mounted, true);
  assert.equal(over.phase, "visible");
  assert.equal(over.shownAt, startedAt + 280);
});

test("once shown, the screen stays up for the minimum time even if the arena is already ready", () => {
  const shownAt = startedAt + MATCH_LOADER_SHOW_DELAY_MS;
  const sceneReadyAt = shownAt + 40;
  const fadeStart = shownAt + MATCH_LOADER_MIN_VISIBLE_MS;

  const stillCovered = decideMatchLoader({
    now: fadeStart,
    startedAt,
    sceneReadyAt,
    shownAt,
  });
  assert.equal(stillCovered.phase, "visible");
  assert.equal(stillCovered.opacity, 1);
  assert.equal(stillCovered.mounted, true);

  const fading = decideMatchLoader({
    now: fadeStart + MATCH_LOADER_FADE_MS / 2,
    startedAt,
    sceneReadyAt,
    shownAt,
  });
  assert.equal(fading.phase, "fading");
  assert.equal(fading.mounted, true);
  assert.ok(fading.opacity > 0.2 && fading.opacity < 0.8, `opacity ${fading.opacity}`);

  const gone = decideMatchLoader({
    now: fadeStart + MATCH_LOADER_FADE_MS,
    startedAt,
    sceneReadyAt,
    shownAt,
  });
  assert.equal(gone.phase, "gone");
  assert.equal(gone.mounted, false);
  assert.equal(gone.opacity, 0);
  assert.equal(gone.shownAt, shownAt);
});

test("a late arena frame holds the screen until that frame, then fades", () => {
  const shownAt = startedAt + MATCH_LOADER_SHOW_DELAY_MS;
  const sceneReadyAt = shownAt + 2_000;

  const waitingOnFrame = decideMatchLoader({
    now: sceneReadyAt,
    startedAt,
    sceneReadyAt,
    shownAt,
  });
  assert.equal(waitingOnFrame.phase, "visible");
  assert.equal(waitingOnFrame.opacity, 1);

  const fading = decideMatchLoader({
    now: sceneReadyAt + 1,
    startedAt,
    sceneReadyAt,
    shownAt,
  });
  assert.equal(fading.phase, "fading");
  assert.ok(fading.opacity < 1);

  const earlier = decideMatchLoader({
    now: sceneReadyAt - 1,
    startedAt,
    sceneReadyAt: null,
    shownAt,
  });
  assert.equal(earlier.phase, "visible");
});

test("loading tips quote the real scores and powers", () => {
  const tips = matchLoadingTips({
    id: "hotstreak",
    ability: "Instant Explosion",
    abilityDescription: "Create a fiery explosion around you",
  });
  assert.equal(tips.length > 1, true);
  assert.match(tips[0], /star orb/);
  assert.match(tips[0], /Instant Explosion/);
  assert.match(tips.join("\n"), new RegExp(String(BUMPER_SCORE)));
  assert.match(tips.join("\n"), new RegExp(String(KNOCKOUT_PENALTY)));
  assert.match(tips.join("\n"), new RegExp(String(KNOCKOUT_SCORE_ORB)));
  assert.match(tips.join("\n"), new RegExp(String(KNOCKOUT_SCORE_PLAYER)));
  assert.match(tips.join("\n"), new RegExp(String(ZONE_SCORE_ORB)));

  const pinpoint = matchLoadingTips({
    id: "pinpoint",
    ability: "Laser Trajectory",
    abilityDescription: "unused",
  });
  assert.match(pinpoint[0], /Lock-On/);
  assert.equal(pinpoint[0].includes("unused"), false);
});

test("in-world labels stay hidden until the loader has fully faded", () => {
  const shownAt = startedAt + MATCH_LOADER_SHOW_DELAY_MS;
  const sceneReadyAt = shownAt;
  const fadeStart = shownAt + MATCH_LOADER_MIN_VISIBLE_MS;

  for (const now of [startedAt, fadeStart, fadeStart + MATCH_LOADER_FADE_MS / 2]) {
    const decision = decideMatchLoader({ now, startedAt, sceneReadyAt, shownAt });
    assert.equal(worldLabelsCovered(decision.phase), true, decision.phase);
  }

  const gone = decideMatchLoader({
    now: fadeStart + MATCH_LOADER_FADE_MS,
    startedAt,
    sceneReadyAt,
    shownAt,
  });
  assert.equal(gone.phase, "gone");
  assert.equal(worldLabelsCovered(gone.phase), false);
});

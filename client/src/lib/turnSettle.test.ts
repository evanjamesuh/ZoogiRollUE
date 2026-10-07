import test from "node:test";
import assert from "node:assert/strict";
import { SETTLE_DELAY_STEPS, SETTLE_TIMEOUT_STEPS } from "./simFeel.ts";
import { turnHandoffReady } from "./turnSettle.ts";

test("the turn waits for a continuous rest, then a short pause", () => {
  let settleHoldSteps = 0;
  let unsettledSteps = 0;
  for (let step = 0; step < SETTLE_DELAY_STEPS + 5; step++) {
    const handoff = turnHandoffReady({
      shotHappened: true,
      courtStill: false,
      settleHoldSteps,
      unsettledSteps,
    });
    assert.equal(handoff.pass, false);
    settleHoldSteps = handoff.settleHoldSteps;
    unsettledSteps = handoff.unsettledSteps;
  }
  for (let step = 0; step < SETTLE_DELAY_STEPS - 1; step++) {
    const handoff = turnHandoffReady({
      shotHappened: true,
      courtStill: true,
      settleHoldSteps,
      unsettledSteps,
    });
    assert.equal(handoff.pass, false, `step ${step} should still be waiting`);
    settleHoldSteps = handoff.settleHoldSteps;
    unsettledSteps = handoff.unsettledSteps;
  }
  const passed = turnHandoffReady({
    shotHappened: true,
    courtStill: true,
    settleHoldSteps,
    unsettledSteps,
  });
  assert.equal(passed.pass, true);
});

test("a jitter that never rests still hands the turn off", () => {
  let settleHoldSteps = 0;
  let unsettledSteps = 0;
  for (let step = 0; step < SETTLE_TIMEOUT_STEPS - 1; step++) {
    const handoff = turnHandoffReady({
      shotHappened: true,
      courtStill: step % 2 === 0,
      settleHoldSteps,
      unsettledSteps,
    });
    assert.equal(handoff.pass, false);
    settleHoldSteps = handoff.settleHoldSteps;
    unsettledSteps = handoff.unsettledSteps;
  }
  const forced = turnHandoffReady({
    shotHappened: true,
    courtStill: false,
    settleHoldSteps,
    unsettledSteps,
  });
  assert.equal(forced.pass, true);
});

test("no shot means the opening drop does not pass the turn", () => {
  const handoff = turnHandoffReady({
    shotHappened: false,
    courtStill: true,
    settleHoldSteps: SETTLE_DELAY_STEPS,
    unsettledSteps: SETTLE_TIMEOUT_STEPS,
  });
  assert.equal(handoff.pass, false);
  assert.equal(handoff.settleHoldSteps, 0);
  assert.equal(handoff.unsettledSteps, 0);
});

import test from "node:test";
import assert from "node:assert/strict";
import { FIXED_DT, MAX_SUBSTEPS, stepAccumulator } from "./fixedTimestep.ts";

test("one 60 Hz frame is one sim step and leaves no leftover", () => {
  const step = stepAccumulator(0, FIXED_DT);
  assert.equal(step.steps, 1);
  assert.equal(step.accumulator, 0);
  assert.equal(step.alpha, 0);
});

test("two 120 Hz frames equal one 60 Hz step", () => {
  const first = stepAccumulator(0, 1 / 120);
  assert.equal(first.steps, 0);
  assert.ok(Math.abs(first.alpha - 0.5) < 1e-6, `alpha ${first.alpha}`);
  const second = stepAccumulator(first.accumulator, 1 / 120);
  assert.equal(second.steps, 1);
  assert.ok(Math.abs(second.accumulator) < 1e-9);
  assert.equal(second.alpha, 0);
});

test("a 30 Hz frame takes two steps", () => {
  const step = stepAccumulator(0, 1 / 30);
  assert.equal(step.steps, 2);
  assert.ok(step.alpha < 1e-9);
});

test("60 Hz and 120 Hz play the same number of steps over one second", () => {
  const count = (frameDt: number, frames: number) => {
    let acc = 0;
    let steps = 0;
    for (let i = 0; i < frames; i++) {
      const step = stepAccumulator(acc, frameDt);
      acc = step.accumulator;
      steps += step.steps;
    }
    return steps;
  };
  assert.equal(count(1 / 60, 60), 60);
  assert.equal(count(1 / 120, 120), 60);
  const at144 = count(1 / 144, 144);
  assert.ok(Math.abs(at144 - 60) <= 1, `144 Hz ran ${at144} steps`);
});

test("time scale slows the sim and a hitch cannot spiral", () => {
  const half = stepAccumulator(0, FIXED_DT, 0.5);
  assert.equal(half.steps, 0);
  assert.ok(Math.abs(half.alpha - 0.5) < 1e-6);

  const paused = stepAccumulator(0, FIXED_DT, 0);
  assert.equal(paused.steps, 0);
  assert.equal(paused.accumulator, 0);

  const hitch = stepAccumulator(0, 1);
  assert.equal(hitch.steps, MAX_SUBSTEPS);
  assert.ok(hitch.alpha < 1);
});

test("alpha stays inside 0..1 for uneven frames", () => {
  let acc = 0;
  const frames = [0.004, 0.02, 0.011, 0.033, 0.008, 0.017];
  for (const frame of frames) {
    const step = stepAccumulator(acc, frame);
    acc = step.accumulator;
    assert.ok(step.alpha >= 0 && step.alpha <= 1, `alpha ${step.alpha}`);
    assert.ok(step.steps >= 0 && step.steps <= MAX_SUBSTEPS);
  }
});

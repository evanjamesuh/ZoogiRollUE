import test from "node:test";
import assert from "node:assert/strict";
import {
  ARENA_FOV_DEG,
  ARENA_PITCH_DEG,
  SHAKE_LIMIT,
  actionBounds,
  addTrauma,
  cameraOffset,
  clampDistance,
  damp,
  fitDistance,
  getTrauma,
  resetTrauma,
  smoothShake,
} from "./cameraRig.ts";

test("the match camera looks down between 50 and 55 degrees", () => {
  assert.ok(ARENA_PITCH_DEG >= 50 && ARENA_PITCH_DEG <= 55);
  const offset = cameraOffset(20);
  const pitch = Math.atan2(offset.y, offset.z) * (180 / Math.PI);
  assert.ok(Math.abs(pitch - ARENA_PITCH_DEG) < 0.01);
  assert.equal(offset.x, 0, "heading stays on the Z axis");
});

test("damping matches across one slow frame and two fast frames", () => {
  const once = damp(0, 10, 6, 1 / 30);
  const mid = damp(0, 10, 6, 1 / 60);
  const twice = damp(mid, 10, 6, 1 / 60);
  assert.ok(Math.abs(once - twice) < 1e-9, `once ${once} twice ${twice}`);
  assert.ok(twice > 0 && twice < 10);
});

test("fit distance grows with the action and stays finite", () => {
  const tight = actionBounds([{ x: 0, z: 0 }, { x: 1, z: 1 }]);
  const wide = actionBounds([{ x: -8, z: -5 }, { x: 8, z: 5 }]);
  const near = fitDistance(tight, (ARENA_PITCH_DEG * Math.PI) / 180, ARENA_FOV_DEG, 16 / 9, 1);
  const far = fitDistance(wide, (ARENA_PITCH_DEG * Math.PI) / 180, ARENA_FOV_DEG, 16 / 9, 1);
  assert.ok(far > near, `wide ${far} should outzoom tight ${near}`);
  assert.ok(near >= 6 && far < 80 && Number.isFinite(far));
  const clamped = clampDistance(far, 13.5, 34);
  assert.ok(clamped >= 13.5 && clamped <= 34);
});

test("shake is zero without trauma and never passes the cap", () => {
  resetTrauma();
  assert.equal(getTrauma(), 0);
  const still = smoothShake(0, 1.25);
  assert.deepEqual(still, { x: 0, y: 0, z: 0 });

  addTrauma(10);
  assert.equal(getTrauma(), 1);
  for (let i = 0; i < 40; i++) {
    const shake = smoothShake(getTrauma(), i * 0.016);
    assert.ok(Math.abs(shake.x) <= SHAKE_LIMIT + 1e-9);
    assert.ok(Math.abs(shake.y) <= SHAKE_LIMIT + 1e-9);
    assert.ok(Math.abs(shake.z) <= SHAKE_LIMIT + 1e-9);
  }
  resetTrauma();
});

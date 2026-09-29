import test from "node:test";
import assert from "node:assert/strict";
import { BUMPER_RADIUS, MARBLE_RADIUS, getMapLayout } from "./arenaColliders.ts";
import {
  NEON_CORNER_GAP,
  NEON_HALF_X,
  NEON_HALF_Z,
  isOutsideNeonCourt,
  neonBumpers,
  neonCourtLayout,
  neonObstacles,
  neonRails,
  resolveNeonRails,
} from "./neonCourt.ts";

test("night circuit is a closed rectangle with corner mouths and posts inside", () => {
  const layout = getMapLayout("neon");
  assert.ok(layout);
  assert.equal(layout.id, "neon");
  assert.equal(neonRails().length, 4);
  assert.equal(neonBumpers().length, layout.bumpers.length);

  for (const bumper of neonBumpers()) {
    assert.ok(Math.abs(bumper.x) + BUMPER_RADIUS < NEON_HALF_X - 1);
    assert.ok(Math.abs(bumper.z) + BUMPER_RADIUS < NEON_HALF_Z - 1);
  }

  const spawns = layout.zones.filter((zone) => zone.isSpawn);
  assert.equal(spawns.length, 4);
  for (const spawn of spawns) {
    const x = Math.cos(spawn.angle) * spawn.distance;
    const z = Math.sin(spawn.angle) * spawn.distance;
    assert.equal(isOutsideNeonCourt(x, z), false, spawn.id);
    for (const bumper of neonBumpers()) {
      const gap = Math.hypot(x - bumper.x, z - bumper.z);
      assert.ok(gap > BUMPER_RADIUS + MARBLE_RADIUS + 0.3, `${spawn.id} overlaps ${bumper.id}`);
    }
  }
});

test("a marble aimed at a rail bounces back in", () => {
  let pos: [number, number, number] = [0, 0.5, -6.2];
  let vel: [number, number, number] = [0, 0, -0.8];
  let bounced = false;
  for (let frame = 0; frame < 12; frame++) {
    const prev = pos;
    const next: [number, number, number] = [pos[0] + vel[0], pos[1], pos[2] + vel[2]];
    const resolved = resolveNeonRails(prev, next, vel, MARBLE_RADIUS);
    if (resolved.hits.some((hit) => hit.id === "rail-south") && resolved.vel[2] > 0) bounced = true;
    pos = resolved.pos;
    vel = resolved.vel;
  }
  assert.equal(bounced, true);
  assert.ok(pos[2] > -NEON_HALF_Z, "the bounce should leave the marble on the floor");
  assert.equal(isOutsideNeonCourt(pos[0], pos[2]), false);
});

test("a corner mouth is a real knockout, not a hidden wall", () => {
  let pos: [number, number, number] = [-8.4, 0.5, -4.4];
  let vel: [number, number, number] = [-0.7, 0, -0.7];
  let exited = false;
  for (let frame = 0; frame < 16; frame++) {
    const prev = pos;
    const next: [number, number, number] = [pos[0] + vel[0], pos[1], pos[2] + vel[2]];
    const resolved = resolveNeonRails(prev, next, vel, MARBLE_RADIUS);
    pos = resolved.pos;
    vel = resolved.vel;
    if (isOutsideNeonCourt(pos[0], pos[2])) {
      exited = true;
      assert.equal(resolved.hits.length, 0, "the mouth should not bounce the exit");
      break;
    }
  }
  assert.equal(exited, true);
  assert.equal(isOutsideNeonCourt(0, 0), false);
});

test("layout helper matches getMapLayout", () => {
  assert.deepEqual(getMapLayout("neon"), neonCourtLayout());
});

test("raised pads and the center channel bounce, and the corner mouths stay open", () => {
  const gapX = NEON_HALF_X - NEON_CORNER_GAP;
  const gapZ = NEON_HALF_Z - NEON_CORNER_GAP;
  const boxes = neonObstacles();
  assert.ok(boxes.length >= 4);

  const mouths = [
    [gapX, NEON_HALF_X, gapZ, NEON_HALF_Z],
    [-NEON_HALF_X, -gapX, gapZ, NEON_HALF_Z],
    [gapX, NEON_HALF_X, -NEON_HALF_Z, -gapZ],
    [-NEON_HALF_X, -gapX, -NEON_HALF_Z, -gapZ],
  ];
  for (const box of boxes) {
    assert.ok(box.maxX < NEON_HALF_X && box.minX > -NEON_HALF_X, box.id);
    assert.ok(box.maxZ < NEON_HALF_Z && box.minZ > -NEON_HALF_Z, box.id);
    for (const [x0, x1, z0, z1] of mouths) {
      const overlaps = box.minX < x1 && box.maxX > x0 && box.minZ < z1 && box.maxZ > z0;
      assert.equal(overlaps, false, `${box.id} blocks a knockout mouth`);
    }
  }

  const spawns = neonCourtLayout().zones.filter((zone) => zone.isSpawn);
  for (const spawn of spawns) {
    const x = Math.cos(spawn.angle) * spawn.distance;
    const z = Math.sin(spawn.angle) * spawn.distance;
    for (const box of boxes) {
      const nearestX = Math.min(box.maxX, Math.max(box.minX, x));
      const nearestZ = Math.min(box.maxZ, Math.max(box.minZ, z));
      const gap = Math.hypot(x - nearestX, z - nearestZ);
      assert.ok(gap > MARBLE_RADIUS + 0.35, `${spawn.id} overlaps ${box.id}`);
    }
  }

  let pos: [number, number, number] = [0, 0.5, 2.2];
  let vel: [number, number, number] = [0, 0, -0.9];
  let bounced = false;
  for (let frame = 0; frame < 8; frame++) {
    const prev = pos;
    const next: [number, number, number] = [pos[0] + vel[0], pos[1], pos[2] + vel[2]];
    const resolved = resolveNeonRails(prev, next, vel, MARBLE_RADIUS);
    if (resolved.hits.some((hit) => hit.id === "channel-north") && resolved.vel[2] > 0) bounced = true;
    pos = resolved.pos;
    vel = resolved.vel;
  }
  assert.equal(bounced, true);
});

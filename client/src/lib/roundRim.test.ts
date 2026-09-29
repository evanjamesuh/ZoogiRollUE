import test from "node:test";
import assert from "node:assert/strict";
import { ARABIAN_RIM, ROUND_KNOCKOFF_RADIUS, rimPosition } from "./roundRim.ts";
import { BUMPER_RADIUS, MARBLE_RADIUS, getMapLayout } from "./arenaColliders.ts";

const PLANTER_ANGLES = [22.5, 67.5, 112.5, 157.5, 202.5, 247.5, 292.5, 337.5];
const PLANTER_DISTANCE = 13;
const PLANTER_RADIUS = 0.72;
const CARDINAL_SPAWN_DISTANCE = 8;

test("arabian planters are eight pots at radius 13 on the 22.5 degree lattice", () => {
  assert.equal(ARABIAN_RIM.length, 8);
  ARABIAN_RIM.forEach((mark, i) => {
    assert.equal(mark.id, `planter-${i}`);
    assert.equal(mark.angleDeg, PLANTER_ANGLES[i]);
    assert.equal(mark.distance, PLANTER_DISTANCE);
    assert.equal(mark.radius, PLANTER_RADIUS);
  });

  const layout = getMapLayout("saturn");
  assert.ok(layout);
  const pots = layout.scenery.filter((solid) => solid.kind === "prop");
  assert.equal(pots.length, 8);
  for (const pot of pots) {
    const mark = ARABIAN_RIM.find((item) => item.id === pot.id);
    assert.ok(mark);
    const placed = rimPosition(mark);
    assert.equal(pot.radius, mark.radius);
    assert.ok(Math.abs(pot.x - placed.x) < 1e-9);
    assert.ok(Math.abs(pot.z - placed.z) < 1e-9);
    assert.ok(Math.abs(Math.hypot(pot.x, pot.z) - PLANTER_DISTANCE) < 1e-9);
    const lane = layout.knockoffRadius - (mark.distance + mark.radius);
    assert.ok(Math.abs(lane - 1.78) < 1e-9, `${pot.id} lane is ${lane}`);
    assert.ok(lane > MARBLE_RADIUS * 2, `${pot.id} lane is narrower than a marble`);
  }
  assert.equal(layout.knockoffRadius, ROUND_KNOCKOFF_RADIUS);
});

test("the four arabian spawns are equidistant from the nearest planter", () => {
  const layout = getMapLayout("saturn");
  assert.ok(layout);
  const spawns = [0, 90, 180, 270].map((deg) => {
    const angle = (deg * Math.PI) / 180;
    return {
      x: Math.cos(angle) * CARDINAL_SPAWN_DISTANCE,
      z: Math.sin(angle) * CARDINAL_SPAWN_DISTANCE,
    };
  });

  const nearest = (x: number, z: number, padding: number) => {
    let best = Infinity;
    for (const mark of ARABIAN_RIM) {
      const pot = rimPosition(mark);
      best = Math.min(best, Math.hypot(x - pot.x, z - pot.z) - mark.radius - padding);
    }
    return best;
  };

  const spawnGaps = spawns.map((spawn) => nearest(spawn.x, spawn.z, MARBLE_RADIUS));
  for (const gap of spawnGaps) {
    assert.ok(Math.abs(gap - spawnGaps[0]) < 1e-9, `spawn gaps differ: ${spawnGaps.join(", ")}`);
    assert.ok(Math.abs(gap - 5.17) < 0.01, `spawn clearance ${gap}`);
  }

  const bumperGaps = layout.bumpers.map((bumper) => nearest(bumper.x, bumper.z, BUMPER_RADIUS));
  assert.equal(bumperGaps.length, 4);
  for (const gap of bumperGaps) {
    assert.ok(Math.abs(gap - bumperGaps[0]) < 1e-9, `bumper gaps differ: ${bumperGaps.join(", ")}`);
    assert.ok(Math.abs(gap - 5) < 0.05, `bumper clearance ${gap}`);
  }
});

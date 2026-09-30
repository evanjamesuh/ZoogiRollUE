import test from "node:test";
import assert from "node:assert/strict";
import { ARABIAN_RIM, ROUND_KNOCKOFF_RADIUS, rimPosition } from "./roundRim.ts";
import { BUMPER_RADIUS, MARBLE_RADIUS, getMapLayout } from "./arenaColliders.ts";
import { arenaScaleFor } from "./arenaScale.ts";

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
  const scale = arenaScaleFor("saturn");
  const pots = layout.scenery.filter((solid) => solid.kind === "prop");
  assert.equal(pots.length, 8);
  const ordered = [...pots].sort((a, b) => Math.atan2(a.z, a.x) - Math.atan2(b.z, b.x));
  for (const pot of ordered) {
    const mark = ARABIAN_RIM.find((item) => item.id === pot.id);
    assert.ok(mark);
    const authored = rimPosition(mark);
    const distance = Math.hypot(pot.x, pot.z);
    assert.equal(pot.radius, mark.radius);
    assert.ok(Math.abs(pot.x - authored.x * scale) < 1e-9, `${pot.id} x moved`);
    assert.ok(Math.abs(pot.z - authored.z * scale) < 1e-9, `${pot.id} z moved`);
    assert.ok(Math.abs(distance / scale - PLANTER_DISTANCE) < 1e-9, `${pot.id} authored distance ${distance / scale}`);
    assert.ok(Math.abs(distance - 13) < 1e-9, `${pot.id} layout distance ${distance}`);
    const lane = layout.knockoffRadius - distance - pot.radius;
    assert.ok(Math.abs(lane - 1.78) < 1e-9, `${pot.id} lane is ${lane}`);
    assert.ok(lane > MARBLE_RADIUS * 2, `${pot.id} lane is narrower than a marble`);
  }
  for (let i = 0; i < ordered.length; i++) {
    const a = ordered[i];
    const b = ordered[(i + 1) % ordered.length];
    const mouth = Math.hypot(a.x - b.x, a.z - b.z) - a.radius - b.radius;
    assert.ok(mouth > MARBLE_RADIUS * 2, `planter mouth ${mouth} is narrower than a marble`);
  }
  assert.equal(layout.knockoffRadius, ROUND_KNOCKOFF_RADIUS * scale);
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

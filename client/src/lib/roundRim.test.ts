import test from "node:test";
import assert from "node:assert/strict";
import { GRASS_RIM, ROUND_KNOCKOFF_RADIUS, rimPosition } from "./roundRim.ts";
import { getMapLayout, laneIsClear } from "./arenaColliders.ts";

const SCORE_DISTANCE = 9.4;
const SCORE_RADIUS = 4;
const SCORE_ANGLES = [45, 135, 225, 315];
const SPAWN_DISTANCE = 8;
const SPAWN_ANGLES = [0, 90, 180, 270];

function centerGap(angleDeg: number, distance: number, otherAngleDeg: number, otherDistance: number): number {
  const a = (angleDeg * Math.PI) / 180;
  const b = (otherAngleDeg * Math.PI) / 180;
  const dx = Math.cos(a) * distance - Math.cos(b) * otherDistance;
  const dz = Math.sin(a) * distance - Math.sin(b) * otherDistance;
  return Math.hypot(dx, dz);
}

test("meadow rim rocks leave a marble-width slot inside the knockoff", () => {
  assert.equal(GRASS_RIM.length, 6);
  for (const mark of GRASS_RIM) {
    const slot = ROUND_KNOCKOFF_RADIUS - (mark.distance + mark.radius);
    const inner = mark.distance - mark.radius;
    assert.ok(slot >= 1.5, `${mark.id} slot ${slot.toFixed(2)} is under 1.5`);
    assert.ok(inner >= 11, `${mark.id} inner radius ${inner.toFixed(2)} is under 11`);
    assert.ok(mark.distance + mark.radius <= ROUND_KNOCKOFF_RADIUS, `${mark.id} crosses the knockoff`);
  }
});

test("meadow rim rocks clear every score disk and every spawn", () => {
  for (const mark of GRASS_RIM) {
    for (const angle of SCORE_ANGLES) {
      const clearance = centerGap(mark.angleDeg, mark.distance, angle, SCORE_DISTANCE) - mark.radius - SCORE_RADIUS;
      assert.ok(clearance >= 0.5, `${mark.id} overlaps the score disk at ${angle}° by ${(-clearance).toFixed(2)}`);
    }
    let nearestSpawn = Infinity;
    for (const angle of SPAWN_ANGLES) {
      const gap = centerGap(mark.angleDeg, mark.distance, angle, SPAWN_DISTANCE) - mark.radius;
      if (gap < nearestSpawn) nearestSpawn = gap;
    }
    assert.ok(nearestSpawn >= 3, `${mark.id} spawn clearance ${nearestSpawn.toFixed(2)} is under 3`);
  }
});

test("meadow cardinal lanes stay open, including the east spawn", () => {
  const layout = getMapLayout("grass");
  for (const angleDeg of SPAWN_ANGLES) {
    assert.equal(laneIsClear(layout, (angleDeg * Math.PI) / 180), true, `${angleDeg}° lane is blocked`);
  }
  const east = GRASS_RIM.map((mark) => {
    const { x, z } = rimPosition(mark);
    return { id: mark.id, x, z };
  });
  assert.ok(east.every((rock) => Math.hypot(rock.x - SPAWN_DISTANCE, rock.z) > 3), "a rock sits on the east spawn");
});

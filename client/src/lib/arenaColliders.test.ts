import test from "node:test";
import assert from "node:assert/strict";
import { SCORE_ZONE_RADIUS } from "./arenaConstants.ts";
import {
  BUMPER_RADIUS,
  MARBLE_RADIUS,
  ORB_RADIUS,
  bumperSolids,
  collectMatchSolids,
  countClearLanes,
  fitPropFootprints,
  getIcePatches,
  getMapLayout,
  getWinterCampSolids,
  laneIsClear,
  pointInsideSolid,
  resolveSolidCollision,
  segmentHitsSolid,
  setWinterCampSolids,
} from "./arenaColliders.ts";

const MAPS = ["grass", "ice", "lava", "space", "saturn"] as const;

test("every map keeps goals, spawns, and orbs off the solids", () => {
  for (const id of MAPS) {
    const layout = getMapLayout(id);
    assert.ok(layout, id);
    assert.ok(countClearLanes(layout, 36) >= 6, `${id} should have open lanes to the drop`);
    const solids = [...layout.scenery, ...bumperSolids(layout.bumpers)];
    for (const solid of solids) {
      assert.ok(solid.radius > 0, `${id} ${solid.id} radius`);
      assert.ok(["bumper", "rock", "bush", "snowman", "hoodoo", "prop"].includes(solid.kind));
    }
    for (const zone of layout.zones) {
      const x = Math.cos(zone.angle) * zone.distance;
      const z = Math.sin(zone.angle) * zone.distance;
      const pad = zone.isSpawn ? MARBLE_RADIUS : ORB_RADIUS;
      assert.equal(pointInsideSolid(x, z, solids, pad), null, `${id} ${zone.id} sits in a solid`);
      assert.ok(
        Math.hypot(x, z) + pad < layout.knockoffRadius,
        `${id} ${zone.id} is past the knockoff`,
      );
      if (!zone.isSpawn) {
        assert.ok(
          zone.distance + SCORE_ZONE_RADIUS <= layout.floorRadius + 0.05,
          `${id} ${zone.id} hangs off the floor`,
        );
      }
    }
    const scores = layout.zones.filter((zone) => !zone.isSpawn);
    const spawns = layout.zones.filter((zone) => zone.isSpawn);
    for (const score of scores) {
      const cx = Math.cos(score.angle) * score.distance;
      const cz = Math.sin(score.angle) * score.distance;
      for (const spawn of spawns) {
        const sx = Math.cos(spawn.angle) * spawn.distance;
        const sz = Math.sin(spawn.angle) * spawn.distance;
        const dist = Math.hypot(cx - sx, cz - sz);
        assert.ok(dist > SCORE_ZONE_RADIUS, `${id} score zone covers ${spawn.id} (${dist.toFixed(2)})`);
      }
    }
    for (let i = 0; i < 15; i++) {
      const angle = (i / 15) * Math.PI * 2;
      const hit = pointInsideSolid(
        Math.cos(angle) * layout.orbRingRadius,
        Math.sin(angle) * layout.orbRingRadius,
        solids,
        ORB_RADIUS,
      );
      assert.equal(hit, null, `${id} orb ${i} overlaps a solid`);
    }
    assert.ok(layout.knockoffRadius > layout.floorRadius);
    assert.ok(layout.knockoffRadius < layout.floorRadius + 6);
  }
});

test("a map with no selection adds no scenery", () => {
  const solids = collectMatchSolids({
    map: null,
    bumpers: [],
    landedRocks: [],
    editorModels: [],
  });
  assert.equal(solids.length, 0);
});

test("bumper colliders use the measured bumber1 radius", () => {
  const layout = getMapLayout("space");
  assert.ok(layout);
  for (const solid of bumperSolids(layout.bumpers)) {
    assert.equal(solid.radius, BUMPER_RADIUS);
    assert.equal(solid.kind, "bumper");
  }
  assert.equal(BUMPER_RADIUS, 0.955);
});

test("ice speed patches are the four disks that are drawn", () => {
  assert.equal(getIcePatches().length, 4);
});

test("a marble launched down a clear grass lane does not hit", () => {
  const layout = getMapLayout("grass");
  assert.ok(layout);
  const angle = (20 * Math.PI) / 180;
  assert.equal(laneIsClear(layout, angle), true);
  const solids = [...layout.scenery, ...bumperSolids(layout.bumpers)];
  let pos: [number, number, number] = [0, 0.5, 0];
  let vel: [number, number, number] = [Math.cos(angle) * 0.25, 0, Math.sin(angle) * 0.25];
  let hits = 0;
  for (let frame = 0; frame < 40; frame++) {
    const prev = pos;
    const next: [number, number, number] = [pos[0] + vel[0], pos[1], pos[2] + vel[2]];
    const resolved = resolveSolidCollision(prev, next, vel, MARBLE_RADIUS, solids);
    hits += resolved.hits.length;
    pos = resolved.pos;
    vel = resolved.vel;
  }
  assert.equal(hits, 0);
  assert.ok(Math.hypot(pos[0], pos[2]) > 2, "the marble should keep rolling");
});

test("a marble aimed at a visible grass rock bounces off it", () => {
  const layout = getMapLayout("grass");
  assert.ok(layout);
  const rock = layout.scenery.find((solid) => solid.id.startsWith("S_8"));
  assert.ok(rock);
  const startX = rock.x - rock.radius - MARBLE_RADIUS - 0.3;
  let pos: [number, number, number] = [startX, 0.5, rock.z];
  let vel: [number, number, number] = [0.4, 0, 0];
  let bounced = false;
  for (let frame = 0; frame < 20; frame++) {
    const prev = pos;
    const next: [number, number, number] = [pos[0] + vel[0], 0.5, pos[2] + vel[2]];
    const resolved = resolveSolidCollision(prev, next, vel, MARBLE_RADIUS, [rock]);
    if (resolved.hits.includes(rock.id) && resolved.vel[0] <= 0) bounced = true;
    pos = resolved.pos;
    vel = resolved.vel;
  }
  assert.equal(bounced, true);
  assert.ok(pos[0] < rock.x - rock.radius, "the marble should stay outside the rock");
});

test("fitted stages keep the knockoff on the measured floor", () => {
  const space = getMapLayout("space");
  const saturn = getMapLayout("saturn");
  const lava = getMapLayout("lava");
  assert.ok(space);
  assert.ok(saturn);
  assert.ok(lava);
  assert.ok(space.floorRadius > 14 && space.knockoffRadius < 20);
  assert.ok(saturn.floorRadius > 14 && saturn.knockoffRadius < 22);
  assert.ok(saturn.knockoffRadius - saturn.floorRadius < 0.6, "arabian out line sits on the floor edge");
  for (const hoodoo of lava.scenery) {
    assert.ok(hoodoo.radius < 0.9, `${hoodoo.id} still uses the fallback cap radius`);
  }
});

test("ice scoring spots are reachable, and camp walls exist only after the model is measured", () => {
  setWinterCampSolids(null);
  const layout = getMapLayout("ice");
  assert.ok(layout);
  assert.equal(layout.scenery.some((solid) => solid.kind === "prop"), false);
  const scores = layout.zones.filter((zone) => !zone.isSpawn);
  assert.equal(scores.length, 4);
  for (const zone of scores) {
    assert.equal(
      laneIsClear(layout, zone.angle, MARBLE_RADIUS, zone.distance),
      true,
      `${zone.id} is blocked`,
    );
  }
  const without = collectMatchSolids({ map: "ice", bumpers: [], landedRocks: [], editorModels: [] });
  assert.equal(without.some((solid) => solid.kind === "prop"), false);

  setWinterCampSolids([{ id: "wall-16", x: 11, z: 11.5, radius: 0.4, kind: "prop" }]);
  const withCamp = collectMatchSolids({ map: "ice", bumpers: [], landedRocks: [], editorModels: [] });
  assert.ok(withCamp.some((solid) => solid.id === "wall-16"));
  const grass = collectMatchSolids({ map: "grass", bumpers: [], landedRocks: [], editorModels: [] });
  assert.equal(grass.some((solid) => solid.id === "wall-16"), false);
  setWinterCampSolids(null);
  assert.equal(getWinterCampSolids().length, 0);
});

function fillBox(
  x0: number, x1: number, z0: number, z1: number, y0: number, y1: number, step = 0.2,
): { x: number; y: number; z: number }[] {
  const points: { x: number; y: number; z: number }[] = [];
  for (let x = x0; x <= x1 + 1e-6; x += step) {
    for (let z = z0; z <= z1 + 1e-6; z += step) {
      for (let y = y0; y <= y1 + 1e-6; y += 0.45) {
        points.push({ x, y, z });
      }
    }
  }
  return points;
}

test("a thin wall collider stays on the wall, not on the open ice beside it", () => {
  const wall = fitPropFootprints([{
    name: "wall_001.003",
    points: [
      ...fillBox(8, 14, 11.35, 11.65, 0, 1.4, 0.25),
      { x: 8, y: 0.5, z: 11.35 },
      { x: 14, y: 0.5, z: 11.65 },
    ],
  }]);
  assert.ok(wall.length >= 2, "a long wall needs more than one circle");
  for (const solid of wall) {
    assert.ok(solid.radius < 0.35, `${solid.id} radius ${solid.radius} covers open ice`);
    assert.ok(Math.abs(solid.z - 11.5) < 0.2, `${solid.id} is off the wall`);
  }
  const beside = pointInsideSolid(11, 12.4, wall, MARBLE_RADIUS);
  assert.equal(beside, null);
  const onWall = pointInsideSolid(11, 11.5, wall, 0);
  assert.ok(onWall, "the middle of the wall should still stop a marble");

  const layout = getMapLayout("ice");
  assert.ok(layout);
  const solids = [...layout.scenery, ...bumperSolids(layout.bumpers), ...wall];
  for (const zone of layout.zones.filter((zone) => !zone.isSpawn)) {
    assert.equal(segmentHitsSolid(zone.angle, zone.distance, solids), null, zone.id);
  }
});

test("round props, flat ground, floating crates, and penguins are filtered", () => {
  const disk: { x: number; y: number; z: number }[] = [];
  for (let i = 0; i < 80; i++) {
    const angle = (i / 80) * Math.PI * 2;
    for (const radius of [0.4, 0.8, 1.1]) {
      disk.push({ x: 15 + Math.cos(angle) * radius, y: 0.4 + (i % 3) * 0.3, z: 8 + Math.sin(angle) * radius });
    }
  }
  const fitted = fitPropFootprints([
    { name: "tower_003", points: disk },
    { name: "ground_006", points: fillBox(-20, 20, -20, 20, 0, 0.05, 2) },
    { name: "box_high", points: fillBox(4, 5, 4, 5, 1.6, 2.4, 0.2) },
    { name: "penguin_001", points: fillBox(1, 2, 1, 2, 0, 1, 0.2) },
  ]);
  assert.equal(fitted.length, 1);
  const tower = fitted[0];
  assert.ok(tower);
  assert.ok(tower.radius > 0.7 && tower.radius < 1.4, `tower radius ${tower.radius}`);
  assert.equal(pointInsideSolid(15, 10.5, fitted, MARBLE_RADIUS), null);
});

test("an editor-placed bumper collides at the bumper radius", () => {
  const solids = collectMatchSolids({
    map: null,
    bumpers: [],
    landedRocks: [],
    editorModels: [{
      id: "placed-bumper",
      modelUrl: "/models/bumber1.glb",
      position: [3, 0, 1],
      scale: [1, 1, 1],
    }],
  });
  assert.equal(solids.length, 1);
  assert.equal(solids[0]?.kind, "bumper");
  assert.equal(solids[0]?.radius, BUMPER_RADIUS);
  assert.equal(solids[0]?.x, 3);
  assert.equal(solids[0]?.z, 1);
});

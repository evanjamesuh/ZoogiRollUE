import test from "node:test";
import assert from "node:assert/strict";
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
  setWinterCampSolids,
} from "./arenaColliders.ts";

const MAPS = ["grass", "ice", "lava", "space", "saturn"] as const;
const SCORE_ZONE_RADIUS = 4;

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

test("ice camp walls exist only after the winter model is measured", () => {
  setWinterCampSolids(null);
  const layout = getMapLayout("ice");
  assert.ok(layout);
  assert.equal(layout.scenery.some((solid) => solid.kind === "prop"), false);
  assert.equal(getWinterCampSolids().length, 0);
  const idle = collectMatchSolids({ map: "ice", bumpers: [], landedRocks: [], editorModels: [] });
  assert.equal(idle.some((solid) => solid.kind === "prop"), false);
  for (const score of layout.zones.filter((zone) => !zone.isSpawn)) {
    assert.equal(laneIsClear(layout, score.angle), true, `score ray ${score.angle} is blocked`);
  }

  setWinterCampSolids([{ id: "wall-16", x: 11, z: 11.5, radius: 0.4, kind: "prop" }]);
  const withCamp = collectMatchSolids({ map: "ice", bumpers: [], landedRocks: [], editorModels: [] });
  assert.ok(withCamp.some((solid) => solid.id === "wall-16"));
  setWinterCampSolids(null);
});

test("a thin wall collider stays on the wall, not on the open ice beside it", () => {
  const points = [];
  for (let i = 0; i <= 12; i++) {
    const along = -4 + (8 * i) / 12;
    for (const side of [-0.15, 0, 0.15]) {
      points.push({ x: 8 + along * 0.7 + side * -0.7, y: 0.6, z: 8 + along * 0.7 + side * 0.7 });
    }
  }
  const solids = fitPropFootprints([{ name: "wall-16", points }]);
  assert.ok(solids.length > 1, "a long wall needs more than one circle");
  for (const solid of solids) {
    assert.ok(solid.radius < 0.5, `${solid.id} radius ${solid.radius} spills onto open ice`);
    const along = ((solid.x - 8) + (solid.z - 8)) / 2;
    const side = Math.abs((solid.z - 8) - (solid.x - 8));
    assert.ok(Math.abs(along) < 5, "circle left the wall");
    assert.ok(side < 0.4, "circle sits beside the wall");
  }
  const layout = getMapLayout("ice");
  assert.ok(layout);
  for (const score of layout.zones.filter((zone) => !zone.isSpawn)) {
    let blocked = false;
    for (let traveled = 0; traveled <= score.distance; traveled += 0.35) {
      const x = Math.cos(score.angle) * traveled;
      const z = Math.sin(score.angle) * traveled;
      if (solids.some((solid) => Math.hypot(x - solid.x, z - solid.z) < solid.radius + MARBLE_RADIUS)) blocked = true;
    }
    assert.equal(blocked, false, "fitted wall blocks a scoring spot");
  }
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

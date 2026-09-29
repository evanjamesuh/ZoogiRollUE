import test from "node:test";
import assert from "node:assert/strict";
import {
  ARABIAN_STAGE,
  BUMPER_RADIUS,
  MARBLE_RADIUS,
  ORB_RADIUS,
  arabianPlayTransform,
  bumperSolids,
  collectMatchSolids,
  countClearLanes,
  getIcePatches,
  getMapLayout,
  getTombBackdrop,
  getTombBlocks,
  getWinterCampVersion,
  knockoffOffsetForMap,
  laneIsClear,
  pointInsideSolid,
  resolveSolidCollision,
  setWinterCampActive,
  subscribeWinterCamp,
} from "./arenaColliders.ts";
import { foliageBlocksRingView, GAMEPLAY_CAM_DISTANCE, GAMEPLAY_CAM_HEIGHT, ringPieceAction, shouldHideRingPiece, translationToClear, type Aabb } from "./ringPlacement.ts";

const MAPS = ["grass", "ice", "lava", "space", "saturn", "tomb"] as const;
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
    if (id === "tomb") {
      assert.equal(layout.knockoffRadius, layout.floorRadius, "tomb knockoff is the sandstone edge");
    } else {
      assert.ok(layout.knockoffRadius > layout.floorRadius);
    }
    assert.ok(layout.knockoffRadius <= layout.floorRadius + 6);
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
  const angle = 0;
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
  const rock = layout.scenery.find((solid) => solid.kind === "rock");
  assert.ok(rock);
  const angle = Math.atan2(rock.z, rock.x);
  const startDist = Math.hypot(rock.x, rock.z) - rock.radius - MARBLE_RADIUS - 0.3;
  let pos: [number, number, number] = [Math.cos(angle) * startDist, 0.5, Math.sin(angle) * startDist];
  let vel: [number, number, number] = [Math.cos(angle) * 0.4, 0, Math.sin(angle) * 0.4];
  let bounced = false;
  for (let frame = 0; frame < 30; frame++) {
    const prev = pos;
    const next: [number, number, number] = [pos[0] + vel[0], 0.5, pos[2] + vel[2]];
    const resolved = resolveSolidCollision(prev, next, vel, MARBLE_RADIUS, [rock]);
    const radial = resolved.vel[0] * Math.cos(angle) + resolved.vel[2] * Math.sin(angle);
    if (resolved.hits.includes(rock.id) && radial <= 0) bounced = true;
    pos = resolved.pos;
    vel = resolved.vel;
  }
  assert.equal(bounced, true);
  assert.ok(Math.hypot(pos[0] - rock.x, pos[2] - rock.z) >= rock.radius, "the marble should stay outside the rock");
});

test("fitted stages keep the knockoff on the measured floor", () => {
  const space = getMapLayout("space");
  const saturn = getMapLayout("saturn");
  const lava = getMapLayout("lava");
  const grass = getMapLayout("grass");
  assert.ok(space);
  assert.ok(saturn);
  assert.ok(lava);
  assert.ok(grass);
  assert.ok(space.floorRadius > 14 && space.floorRadius < space.knockoffRadius);
  assert.ok(Math.abs(space.knockoffRadius - 15.6) < 0.02, "cosmic out line is the inner face of the lip");
  assert.deepEqual(ARABIAN_STAGE.plazaCenter, [0, 0]);
  assert.ok(Math.abs(saturn.knockoffRadius - 15.5) < 0.02, "arabian plaza circle sits on the origin");
  assert.ok(saturn.knockoffRadius - saturn.floorRadius < 0.6, "fallback disk ends at the out line");
  const placed = arabianPlayTransform();
  assert.ok(Math.abs(placed.x - 315.12) < 0.05, "courtyard centred on x");
  assert.ok(Math.abs(placed.y + 267.168) < 0.05, "floor sits on y=0");
  assert.ok(Math.abs(placed.z + 5.28) < 0.05, "courtyard centred on z");
  assert.equal(lava.scenery.length, 6, "south outer hoodoo is present");
  for (const hoodoo of lava.scenery) {
    assert.ok(hoodoo.radius <= 0.25 * 2.8 + 1e-6, `${hoodoo.id} is trimmed to the stone`);
    assert.ok(hoodoo.radius >= 0.25 * 2 - 1e-6, `${hoodoo.id} radius`);
  }
  assert.equal(grass.floorRadius, 15.2);
  assert.equal(grass.knockoffRadius, 15.5);
  assert.equal(grass.scenery.length, 6);
  for (const rock of grass.scenery) {
    assert.equal(rock.kind, "rock");
    assert.ok(rock.radius <= 1.25, `${rock.id} is a rim stone`);
    assert.ok(Math.hypot(rock.x, rock.z) - rock.radius >= 11, `${rock.id} sits in the middle`);
  }
});

test("knockoff centre stays on the origin for every map", () => {
  for (const id of [...MAPS, null]) {
    assert.deepEqual(knockoffOffsetForMap(id), { x: 0, y: 0, z: 0 });
  }
});

test("frozen ring camp props stay scenery even after the winter mesh mounts", () => {
  setWinterCampActive(false);
  const idle = collectMatchSolids({ map: "ice", bumpers: [], landedRocks: [], editorModels: [] });
  assert.equal(idle.some((solid) => solid.kind === "prop"), false);

  setWinterCampActive(true);
  const live = collectMatchSolids({ map: "ice", bumpers: [], landedRocks: [], editorModels: [] });
  assert.equal(live.some((solid) => solid.kind === "prop"), false);
  assert.equal(live.some((solid) => solid.id.startsWith("tower-")), false);
  assert.equal(live.some((solid) => solid.id.startsWith("wall-")), false);
  assert.equal(live.some((solid) => solid.id.startsWith("box-")), false);
  for (const solid of live) {
    const inner = Math.hypot(solid.x, solid.z) - solid.radius;
    assert.ok(inner >= 11, `${solid.id} sits in the central play area`);
  }
  setWinterCampActive(false);
});

test("the collider overlay can hear when camp solids turn on", () => {
  setWinterCampActive(false);
  let notices = 0;
  const unsubscribe = subscribeWinterCamp(() => {
    notices += 1;
  });
  const before = getWinterCampVersion();
  setWinterCampActive(true);
  assert.equal(notices, 1);
  assert.ok(getWinterCampVersion() > before);
  setWinterCampActive(true);
  assert.equal(notices, 1);
  setWinterCampActive(false);
  assert.equal(notices, 2);
  unsubscribe();
  setWinterCampActive(true);
  assert.equal(notices, 2);
  setWinterCampActive(false);
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

const PR9_LAYOUTS: Record<string, { knockoffRadius: number; scenery: number; zones: { id: string; angle: number; distance: number; isSpawn: boolean }[] }> = {
  lava: {
    knockoffRadius: 18.6,
    scenery: 6,
    zones: [
      { id: "spawn-0", angle: 0, distance: 7.6, isSpawn: true },
      { id: "spawn-1", angle: Math.PI / 2, distance: 7.6, isSpawn: true },
      { id: "spawn-2", angle: Math.PI, distance: 7.6, isSpawn: true },
      { id: "spawn-3", angle: Math.PI * 1.5, distance: 7.6, isSpawn: true },
      { id: "score-0", angle: (55 * Math.PI) / 180, distance: 11.2, isSpawn: false },
      { id: "score-1", angle: (115 * Math.PI) / 180, distance: 11.2, isSpawn: false },
      { id: "score-2", angle: (210 * Math.PI) / 180, distance: 11.2, isSpawn: false },
      { id: "score-3", angle: (300 * Math.PI) / 180, distance: 11.2, isSpawn: false },
    ],
  },
  space: {
    knockoffRadius: 15.6,
    scenery: 0,
    zones: [
      { id: "spawn-0", angle: 0, distance: 7.6, isSpawn: true },
      { id: "spawn-1", angle: Math.PI / 2, distance: 7.6, isSpawn: true },
      { id: "spawn-2", angle: Math.PI, distance: 7.6, isSpawn: true },
      { id: "spawn-3", angle: Math.PI * 1.5, distance: 7.6, isSpawn: true },
      { id: "score-0", angle: 0.35, distance: 11.2, isSpawn: false },
      { id: "score-1", angle: 1.9, distance: 11.2, isSpawn: false },
      { id: "score-2", angle: 3.5, distance: 11.2, isSpawn: false },
      { id: "score-3", angle: 5.1, distance: 11.2, isSpawn: false },
    ],
  },
};

test("existing maps keep the PR #9 knockoff, zones, and scenery count", () => {
  for (const [id, expected] of Object.entries(PR9_LAYOUTS)) {
    const layout = getMapLayout(id);
    assert.ok(layout, id);
    assert.equal(layout.knockoffRadius, expected.knockoffRadius, id);
    assert.equal(layout.scenery.length, expected.scenery, id);
    assert.equal(layout.zones.length, expected.zones.length, id);
    expected.zones.forEach((zone, i) => {
      const actual = layout.zones[i];
      assert.ok(actual, `${id} zone ${i}`);
      assert.equal(actual.id, zone.id);
      assert.ok(Math.abs(actual.angle - zone.angle) < 1e-9, `${id} ${zone.id} angle`);
      assert.equal(actual.distance, zone.distance);
      assert.equal(actual.isSpawn, zone.isSpawn);
    });
  }
});

test("pharaoh's tomb is a centred sandstone ring with matching rim blocks", () => {
  const layout = getMapLayout("tomb");
  assert.ok(layout);
  assert.equal(layout.floorRadius, 15.5);
  assert.equal(layout.knockoffRadius, 15.5);
  assert.deepEqual(knockoffOffsetForMap("tomb"), { x: 0, y: 0, z: 0 });
  assert.equal(layout.zones.filter((zone) => zone.isSpawn).length, 4);
  const scores = layout.zones.filter((zone) => !zone.isSpawn);
  assert.equal(scores.length, 4);
  for (const score of scores) {
    assert.ok(score.distance + SCORE_ZONE_RADIUS <= layout.floorRadius, `${score.id} leaves the sandstone`);
  }
  const blocks = getTombBlocks();
  assert.equal(layout.scenery.length, blocks.length);
  for (const block of blocks) {
    const solid = layout.scenery.find((item) => item.id === block.id);
    assert.ok(solid, block.id);
    assert.equal(solid.kind, "rock");
    assert.equal(solid.radius, block.radius);
    assert.ok(Math.abs(solid.x - block.x) < 1e-9);
    assert.ok(Math.abs(solid.z - block.z) < 1e-9);
    assert.ok(Math.hypot(solid.x, solid.z) + solid.radius <= layout.knockoffRadius + 1e-6, `${block.id} crosses the edge`);
  }
  for (const piece of getTombBackdrop()) {
    const inner = Math.hypot(piece.x, piece.z) - piece.radius;
    assert.ok(inner > layout.knockoffRadius + 0.4, `${piece.id} enters the ring (${inner.toFixed(2)})`);
  }
  for (const spawn of layout.zones.filter((zone) => zone.isSpawn)) {
    assert.equal(laneIsClear(layout, spawn.angle), true, `${spawn.id} lane is blocked`);
  }
  assert.ok(countClearLanes(layout, 36) >= 6, "tomb should keep open lanes to the drop");
});

const ROUND_MAPS = ["grass", "ice", "saturn"] as const;

test("meadow, frozen ring, and arabian nights are centred round arenas", () => {
  for (const id of ROUND_MAPS) {
    const layout = getMapLayout(id);
    assert.ok(layout, id);
    assert.equal(layout.floorRadius, 15.2, id);
    assert.equal(layout.knockoffRadius, 15.5, id);
    assert.deepEqual(knockoffOffsetForMap(id), { x: 0, y: 0, z: 0 });
    const spawns = layout.zones.filter((zone) => zone.isSpawn);
    const scores = layout.zones.filter((zone) => !zone.isSpawn);
    assert.equal(spawns.length, 4, id);
    assert.equal(scores.length, 4, id);
    const spawnAngles = spawns.map((zone) => zone.angle).sort((a, b) => a - b);
    for (let i = 0; i < spawnAngles.length; i++) {
      const next = i === spawnAngles.length - 1 ? spawnAngles[0] + Math.PI * 2 : spawnAngles[i + 1];
      assert.ok(Math.abs(next - spawnAngles[i] - Math.PI / 2) < 0.02, `${id} spawns are evenly spaced`);
    }
    for (const spawn of spawns) {
      assert.equal(spawn.distance, 8, id);
      assert.ok(spawn.distance + MARBLE_RADIUS < layout.floorRadius, `${id} ${spawn.id} leaves the floor`);
      assert.equal(laneIsClear(layout, spawn.angle), true, `${id} ${spawn.id} lane is blocked`);
    }
    for (const score of scores) {
      assert.equal(score.distance, 9.4, id);
      assert.ok(score.distance + SCORE_ZONE_RADIUS <= layout.floorRadius, `${id} ${score.id} leaves the floor`);
    }
    for (const bumper of layout.bumpers) {
      assert.ok(Math.hypot(bumper.x, bumper.z) + BUMPER_RADIUS < layout.floorRadius, `${id} ${bumper.id} leaves the floor`);
    }
    assert.ok(layout.scenery.length > 0, `${id} has rim obstacles`);
    for (const solid of layout.scenery) {
      const inner = Math.hypot(solid.x, solid.z) - solid.radius;
      const outer = Math.hypot(solid.x, solid.z) + solid.radius;
      assert.ok(inner >= 11, `${id} ${solid.id} sits in the central play area (${inner.toFixed(2)})`);
      assert.ok(outer <= layout.knockoffRadius + 1e-6, `${id} ${solid.id} crosses the knockoff`);
    }
    assert.ok(countClearLanes(layout, 36) >= 6, `${id} should keep open lanes to the drop`);
  }
});

function box(minX: number, minY: number, minZ: number, maxX: number, maxY: number, maxZ: number): Aabb {
  return { minX, minY, minZ, maxX, maxY, maxZ };
}

test("ring placement hides the playfield slab and pushes props outside", () => {
  const floor = box(-16, 0, -16, 16, 0.4, 16);
  assert.equal(shouldHideRingPiece("PlazaFloor", floor), true);
  assert.equal(shouldHideRingPiece("FrontSide_5", box(-80, 0, -80, 80, 40, 80)), false);
  assert.equal(shouldHideRingPiece("BackSide_2", box(-800, -50, -800, 800, 400, 800)), true);
  assert.equal(shouldHideRingPiece("FrontSide_20", box(0, 0, 0, 1, 1, 1)), true);
  const pool = box(2, 0, 4, 7, 0.3, 9);
  assert.equal(shouldHideRingPiece("Pool", pool), false);
  assert.equal(ringPieceAction("box", box(10, 0, -1, 14, 2, 1), 10), "push");
  assert.equal(ringPieceAction("ground", box(-30, 0, -30, 30, 4, 30), 0), "clip");
  assert.equal(ringPieceAction("fir", box(20, 0, -2, 26, 8, 4), 20), "keep");
  const pushed = translationToClear(pool, 16.35, 32);
  const moved = {
    minX: pool.minX + pushed.dx,
    maxX: pool.maxX + pushed.dx,
    minY: pool.minY,
    maxY: pool.maxY,
    minZ: pool.minZ + pushed.dz,
    maxZ: pool.maxZ + pushed.dz,
  };
  const dx = moved.minX > 0 ? moved.minX : moved.maxX < 0 ? -moved.maxX : 0;
  const dz = moved.minZ > 0 ? moved.minZ : moved.maxZ < 0 ? -moved.maxZ : 0;
  assert.ok(Math.hypot(dx, dz) >= 16.35 - 1e-4, "pool stays inside the ring");
  const outside = box(20, 0, -1, 24, 3, 1);
  const stay = translationToClear(outside, 16.35, 32);
  assert.deepEqual(stay, { dx: 0, dz: 0 });
});

test("meadow canopies must stay under the gameplay camera", () => {
  const elevation = Math.atan2(GAMEPLAY_CAM_HEIGHT, GAMEPLAY_CAM_DISTANCE) * (180 / Math.PI);
  assert.ok(elevation > 45 && elevation < 60, `camera sits at ${elevation.toFixed(1)} degrees`);
  assert.equal(foliageBlocksRingView(0, 12, 18), true, "a tall crown just outside the ring covers the grass");
  assert.equal(foliageBlocksRingView(0, 0.8, 18), false, "a low shrub stays under the sightline");
  assert.equal(foliageBlocksRingView(0, 6, 42), false, "a crown well past the camera frames the far side");
});

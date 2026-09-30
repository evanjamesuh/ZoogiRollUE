import test from "node:test";
import assert from "node:assert/strict";
import {
  ARABIAN_STAGE,
  BUMPER_RADIUS,
  COSMOS_CURB_INNER,
  COSMOS_FLOOR_MESH,
  COSMOS_LIP_SIDES,
  COSMOS_STAGE,
  COSMOS_STANDS_INNER,
  COSMOS_WALL_INNER,
  GRASS_STAGE,
  MARBLE_RADIUS,
  SNOWMAN_RADIUS,
  WINTER_STAGE,
  arenaVisualEdge,
  centerPastOpenEdge,
  collectMatchSolids,
  cosmosDrawnLip,
  cosmosLipRadius,
  cosmosPlayTransform,
  cosmosRingPush,
  embeddedObstaclePose,
  getHoodooDecor,
  getMapLayout,
  listRimGapAdjustments,
  setWinterCampActive,
} from "./arenaColliders.ts";
import { arenaScaleFor } from "./arenaScale.ts";
import {
  NEON_HALF_X,
  NEON_HALF_Z,
  NEON_RAIL_DEPTH,
  neonAabbGap,
  neonBumperBoxGap,
  neonBumpers,
  neonMouthWidth,
  neonObstacles,
  neonPlayHalfX,
  neonPlayHalfZ,
  neonRails,
} from "./neonCourt.ts";
import { ZOOGI_DIAMETER, ZOOGI_DRAW_RADIUS, ORB_DRAW_RADIUS } from "./restHeight.ts";
import {
  MARBLE_WIDTH,
  RIM_GAP_OPEN,
  applyEmbeddedObstaclePose,
  counterScaleFor,
  embeddedWorldScale,
  nodeMatchesMeshKey,
  obstacleMeshKey,
  rimGapAllowed,
  rimGapInBand,
  snapRimGap,
  type ObstacleNudge,
} from "./obstaclePlacement.ts";

const MAPS = ["grass", "ice", "lava", "space", "saturn", "neon"] as const;

function matchSolids(map: string) {
  const layout = getMapLayout(map);
  assert.ok(layout, map);
  return collectMatchSolids({
    map,
    bumpers: layout.bumpers.map((bumper) => ({ id: bumper.id, position: [bumper.x, 0, bumper.z] as [number, number, number] })),
    landedRocks: [],
    editorModels: [],
  });
}

test("a marble width is the collision diameter", () => {
  assert.equal(MARBLE_RADIUS, ZOOGI_DRAW_RADIUS);
  assert.equal(MARBLE_WIDTH, MARBLE_RADIUS * 2);
  assert.equal(MARBLE_WIDTH, ZOOGI_DIAMETER);
  assert.equal(ORB_DRAW_RADIUS < ZOOGI_DRAW_RADIUS, true);
});

test("every arena draws each obstacle on its collider", () => {
  setWinterCampActive(true);
  try {
    for (const map of MAPS) {
      const scale = arenaScaleFor(map);
      const pose = embeddedObstaclePose(map);
      if (map === "neon") {
        const layout = getMapLayout("neon");
        assert.ok(layout);
        assert.equal(neonBumpers().length, layout.bumpers.length);
        for (const post of neonBumpers()) {
          const bumper = layout.bumpers.find((item) => item.id === post.id);
          assert.ok(bumper, post.id);
          assert.equal(bumper.x, post.x, `${post.id} x`);
          assert.equal(bumper.z, post.z, `${post.id} z`);
          const world = embeddedWorldScale(1, 1);
          assert.equal(BUMPER_RADIUS * world, BUMPER_RADIUS);
        }
        const authoredSize: Record<string, [number, number]> = {
          "pad-west": [1.7, 3.1],
          "pad-east": [1.5, 3.1],
          "pad-north": [2.8, 1.15],
          "pad-south": [2.2, 1.15],
          "channel-north": [4.7, 0.36],
          "channel-south": [4.7, 0.36],
        };
        for (const box of neonObstacles()) {
          const [width, depth] = authoredSize[box.id];
          assert.ok(Math.abs(box.maxX - box.minX - width) < 1e-6, `${box.id} width`);
          assert.ok(Math.abs(box.maxZ - box.minZ - depth) < 1e-6, `${box.id} depth`);
        }
        continue;
      }

      const layout = getMapLayout(map);
      assert.ok(layout);
      const solids = matchSolids(map);
      for (const solid of solids) {
        const embedded = pose.meshKeys.includes(obstacleMeshKey(solid.id));
        const parentScale = embedded ? scale : 1;
        const nodeScale = embedded ? counterScaleFor(scale) : 1;
        const world = embeddedWorldScale(parentScale, nodeScale);
        assert.equal(world, 1, `${map} ${solid.id} is drawn at its collider size`);
        assert.equal(solid.radius * world, solid.radius);
      }
      for (const bumper of layout.bumpers) {
        const solid = solids.find((item) => item.id === bumper.id);
        assert.ok(solid, `${map} ${bumper.id}`);
        assert.equal(solid.x, bumper.x);
        assert.equal(solid.z, bumper.z);
        assert.equal(solid.radius, BUMPER_RADIUS);
        assert.equal(pose.meshKeys.includes(bumper.id), false, `${map} bumpers stay outside the dress group`);
      }
      if (map === "lava") {
        for (const hoodoo of getHoodooDecor()) {
          const solid = solids.find((item) => item.id === hoodoo.id);
          assert.ok(solid);
          assert.equal(solid.x, hoodoo.position[0]);
          assert.equal(solid.z, hoodoo.position[2]);
          assert.equal(solid.radius, hoodoo.radius);
        }
      }
      if (map === "ice") {
        for (const snowman of layout.scenery.filter((solid) => solid.kind === "snowman")) {
          assert.equal(snowman.radius, SNOWMAN_RADIUS);
          assert.equal(pose.meshKeys.includes(snowman.id), false);
        }
      }
    }
  } finally {
    setWinterCampActive(false);
  }
});

test("the drawn floor edge and knockout rim match the colliders", () => {
  assert.equal(GRASS_STAGE.groundRadius * arenaScaleFor("grass"), getMapLayout("grass")?.floorRadius);
  assert.equal(WINTER_STAGE.floorRadius * arenaScaleFor("ice"), getMapLayout("ice")?.floorRadius);
  assert.equal(COSMOS_STAGE.floorRadius * arenaScaleFor("space"), getMapLayout("space")?.floorRadius);
  assert.equal(ARABIAN_STAGE.floorRadius * arenaScaleFor("saturn"), getMapLayout("saturn")?.floorRadius);

  for (const map of ["grass", "ice", "lava", "space", "saturn", "tomb"] as const) {
    const edge = arenaVisualEdge(map);
    const layout = getMapLayout(map);
    assert.ok(layout);
    assert.equal(edge.shape, "circle");
    if (edge.shape !== "circle") continue;
    assert.equal(edge.floorRadius, layout.floorRadius, `${map} floor`);
    assert.equal(edge.knockoffRadius, layout.knockoffRadius, `${map} knockout`);
  }

  const space = getMapLayout("space");
  assert.ok(space);
  const lip = cosmosDrawnLip();
  const placed = cosmosPlayTransform();
  assert.ok(Math.abs(lip.centerX) < 1e-6, "cosmic platform is centred on x");
  assert.ok(Math.abs(lip.centerZ) < 1e-6, "cosmic platform is centred on z");
  assert.equal(lip.radius, space.knockoffRadius);
  assert.ok(placed.x < -0.2, "the model shifts so the +X lip meets the collider");
  const rawPlusX = COSMOS_FLOOR_MESH.centerX + COSMOS_FLOOR_MESH.radius;
  const rawMinusX = COSMOS_FLOOR_MESH.radius - COSMOS_FLOOR_MESH.centerX;
  assert.ok(rawPlusX - rawMinusX > 0.4, "the glb floor itself is shifted toward +X");
  // polygon56_Arena Floor corners, measured from cosmos_arena.glb in model space.
  const floorLip: Array<[number, number]> = [
    [-14.54295, -5.40801],
    [-11.80924, -10.14294],
    [-7.62095, -13.65734],
    [-2.48325, -15.52731],
    [2.98418, -15.52731],
    [8.12189, -13.65734],
    [12.31018, -10.14294],
    [15.04389, -5.40801],
    [15.9933, -0.02365],
    [15.04389, 5.36073],
    [12.31017, 10.09566],
    [8.12188, 13.61005],
    [2.98418, 15.48002],
    [-2.48325, 15.48002],
    [-7.62095, 13.61005],
    [-11.80924, 10.09565],
    [-14.54296, 5.36072],
    [-15.49237, -0.02365],
  ];
  floorLip.sort(
    (a, b) =>
      Math.atan2(a[1] - COSMOS_FLOOR_MESH.centerZ, a[0] - COSMOS_FLOOR_MESH.centerX) -
      Math.atan2(b[1] - COSMOS_FLOOR_MESH.centerZ, b[0] - COSMOS_FLOOR_MESH.centerX),
  );
  assert.equal(floorLip.length, COSMOS_LIP_SIDES);
  const worldLip = floorLip.map(([mx, mz]) => ({
    x: placed.x + placed.scale * mx,
    z: placed.z + placed.scale * mz,
  }));
  let flat = 0;
  for (let side = 0; side < worldLip.length; side++) {
    const a = worldLip[side];
    const b = worldLip[(side + 1) % worldLip.length];
    const samples = [
      ["corner", a.x, a.z],
      ["edge", (a.x + b.x) / 2, (a.z + b.z) / 2],
    ] as const;
    for (const [label, x, z] of samples) {
      const drawn = Math.hypot(x - lip.centerX, z - lip.centerZ);
      const angle = Math.atan2(z - lip.centerZ, x - lip.centerX);
      const fall = cosmosLipRadius(angle);
      assert.ok(
        Math.abs(drawn - fall) < 0.05,
        `space ${label} ${side} mesh ${drawn.toFixed(3)} vs fall ${fall.toFixed(3)}`,
      );
      assert.equal(centerPastOpenEdge("space", x, z, space.knockoffRadius, 0, []), false, `${label} ${side} on the lip stays in`);
      const past = 0.08;
      assert.equal(
        centerPastOpenEdge("space", x + Math.cos(angle) * past, z + Math.sin(angle) * past, space.knockoffRadius, 0, []),
        true,
        `${label} ${side} past the lip falls`,
      );
      if (label === "edge" && Math.abs(angle - Math.PI / 2) < Math.PI / COSMOS_LIP_SIDES) flat = drawn;
    }
  }
  assert.ok(space.knockoffRadius - flat > 0.2, "a flat is inside the corner circle");
  assert.equal(centerPastOpenEdge("space", 0, flat + 0.1, space.knockoffRadius, 0, []), true, "+Z past the flat falls inside the corner radius");
  const ringFaces = [
    ["curb", COSMOS_CURB_INNER, 14.544],
    ["wall", COSMOS_WALL_INNER, 14.834],
    ["stands", COSMOS_STANDS_INNER, 14.885],
  ] as const;
  for (const [name, inner, measured] of ringFaces) {
    assert.equal(inner, measured, `${name} hard-coded inner face`);
    const pushed = measured * cosmosRingPush(measured);
    assert.ok(Math.abs(pushed - 16.1) < 1e-6, `${name} ${measured} * push = ${pushed.toFixed(4)}, expected 16.1`);
  }

  const neon = arenaVisualEdge("neon");
  assert.equal(neon.shape, "rect");
  if (neon.shape === "rect") {
    assert.equal(neon.halfX, NEON_HALF_X * arenaScaleFor("neon"));
    assert.equal(neon.halfZ, NEON_HALF_Z * arenaScaleFor("neon"));
    assert.equal(neon.halfX, neonPlayHalfX());
    assert.equal(neon.halfZ, neonPlayHalfZ());
  }
  for (const rail of neonRails()) {
    const thickness = rail.id === "rail-east" || rail.id === "rail-west"
      ? rail.maxX - rail.minX
      : rail.maxZ - rail.minZ;
    assert.ok(Math.abs(thickness - NEON_RAIL_DEPTH) < 1e-6, `${rail.id} thickness`);
  }
  assert.equal(neonRails().find((rail) => rail.id === "rail-north")?.maxZ, neonPlayHalfZ());
  assert.equal(neonRails().find((rail) => rail.id === "rail-south")?.minZ, -neonPlayHalfZ());
  assert.equal(neonRails().find((rail) => rail.id === "rail-east")?.maxX, neonPlayHalfX());
  assert.equal(neonRails().find((rail) => rail.id === "rail-west")?.minX, -neonPlayHalfX());
});

test("rim gaps are sealed or clearly wide enough for a marble", () => {
  setWinterCampActive(true);
  try {
    for (const map of ["grass", "ice", "lava", "space", "saturn", "tomb"] as const) {
      const layout = getMapLayout(map);
      assert.ok(layout);
      const solids = matchSolids(map);
      for (const solid of solids) {
        const gap = layout.knockoffRadius - Math.hypot(solid.x, solid.z) - solid.radius;
        assert.equal(rimGapAllowed(gap), true, `${map} ${solid.id} edge gap ${gap.toFixed(3)} marble widths`);
      }
      const rim = solids
        .filter((solid) => layout.knockoffRadius - Math.hypot(solid.x, solid.z) - solid.radius < RIM_GAP_OPEN + 0.5)
        .sort((a, b) => Math.atan2(a.z, a.x) - Math.atan2(b.z, b.x));
      for (let i = 0; i < rim.length; i++) {
        const a = rim[i];
        const b = rim[(i + 1) % rim.length];
        if (obstacleMeshKey(a.id) === obstacleMeshKey(b.id)) continue;
        const gap = Math.hypot(a.x - b.x, a.z - b.z) - a.radius - b.radius;
        if (gap < 0) continue;
        assert.equal(rimGapAllowed(gap), true, `${map} mouth ${a.id}..${b.id} is ${gap.toFixed(3)} marble widths`);
      }
    }
    for (const box of neonObstacles()) {
      const gap = Math.min(
        box.minX + (neonPlayHalfX() - NEON_RAIL_DEPTH),
        (neonPlayHalfX() - NEON_RAIL_DEPTH) - box.maxX,
        box.minZ + (neonPlayHalfZ() - NEON_RAIL_DEPTH),
        (neonPlayHalfZ() - NEON_RAIL_DEPTH) - box.maxZ,
      );
      assert.equal(rimGapAllowed(gap), true, `${box.id} rail-face gap ${gap.toFixed(3)}`);
    }
    assert.equal(rimGapAllowed(neonMouthWidth()), true, "corner mouth");
    const neonBoxes = neonObstacles();
    const pads = neonBoxes.filter((box) => box.id.startsWith("pad"));
    for (const pad of pads) {
      for (const other of neonBoxes) {
        if (other.id === pad.id) continue;
        if (other.id.startsWith("pad") && other.id <= pad.id) continue;
        const gap = neonAabbGap(pad, other);
        assert.equal(rimGapAllowed(gap), true, `${pad.id} to ${other.id} gap ${gap.toFixed(3)}`);
      }
      for (const bumper of neonBumpers()) {
        const gap = neonBumperBoxGap(pad, bumper);
        assert.equal(rimGapAllowed(gap), true, `${pad.id} to ${bumper.id} gap ${gap.toFixed(3)}`);
      }
    }
  } finally {
    setWinterCampActive(false);
  }
});

test("rim snaps land on a flush or a lane, and the midpoint does not flip", () => {
  assert.ok(Math.abs(snapRimGap(0.8) - 0.3) < 1e-9);
  assert.ok(Math.abs(snapRimGap(1.2) - 1.8) < 1e-9);
  assert.ok(Math.abs(snapRimGap(1.05) - 1.8) < 1e-9);
  assert.ok(Math.abs(snapRimGap(1.05 - 1e-9) - 1.8) < 1e-9);
  assert.equal(rimGapInBand(0.3), false);
  assert.equal(rimGapInBand(1.8), false);
});

test("counter-scaling an embedded mesh restores its size and keeps a rim nudge", () => {
  const scale = 1.5;
  const modelScale = GRASS_STAGE.modelScale;
  const nudge: ObstacleNudge = { meshKey: "Tree2_Leavs_0", dx: 0.25, dz: -0.1 };

  function make(name: string, children: ReturnType<typeof make>[] = []) {
    const node = {
      name,
      parent: null as ReturnType<typeof make> | null,
      children,
      position: {
        x: 0.2,
        y: 0,
        z: -0.4,
        set(x: number, y: number, z: number) {
          this.x = x;
          this.y = y;
          this.z = z;
        },
        clone() {
          return { x: this.x, y: this.y, z: this.z };
        },
      },
      scale: {
        x: 1,
        y: 1,
        z: 1,
        set(x: number, y: number, z: number) {
          this.x = x;
          this.y = y;
          this.z = z;
        },
        clone() {
          return { x: this.x, y: this.y, z: this.z };
        },
      },
      userData: {} as Record<string, unknown>,
      traverse(callback: (child: ReturnType<typeof make>) => void) {
        callback(node);
        for (const child of node.children) child.traverse(callback);
      },
    };
    for (const child of children) child.parent = node;
    return node;
  }

  const rockChild = make("S_7_rock_a");
  const rock = make("S_7_rock", [rockChild]);
  const tree = make("Tree2_Leavs_0");
  const floor = make("GroundDisk");
  const root = make("stage", [floor, rock, tree]);
  const meshKeys = ["S_7_rock", "Tree2_Leavs_0"];
  assert.ok(meshKeys.some((key) => nodeMatchesMeshKey("S_7_rock", key)));
  assert.ok(meshKeys.some((key) => nodeMatchesMeshKey("Tree2", key)));
  assert.deepEqual(embeddedObstaclePose("grass").meshKeys, []);

  const restore = applyEmbeddedObstaclePose(root, scale, modelScale, meshKeys, [nudge]);
  assert.equal(floor.scale.x, 1, "the floor stays on the dress group");
  assert.ok(Math.abs(rock.scale.x - counterScaleFor(scale)) < 1e-9);
  assert.equal(rockChild.scale.x, 1, "a child of a counter-scaled mesh is not scaled again");
  assert.equal(embeddedWorldScale(scale, rock.scale.x), 1);
  assert.ok(Math.abs(tree.scale.x - counterScaleFor(scale)) < 1e-9);
  const worldX = scale * modelScale * (tree.position.x - 0.2);
  const worldZ = scale * modelScale * (tree.position.z - (-0.4));
  assert.ok(Math.abs(worldX - nudge.dx) < 1e-6, `tree nudge x ${worldX}`);
  assert.ok(Math.abs(worldZ - nudge.dz) < 1e-6, `tree nudge z ${worldZ}`);

  restore();
  assert.equal(rock.scale.x, 1);
  assert.equal(tree.position.x, 0.2);
  assert.equal(tree.position.z, -0.4);
});

import assert from "node:assert/strict";
import test from "node:test";
import {
  MARBLE_RADIUS,
  centerPastOpenEdge,
  collectMatchSolids,
  getMapLayout,
} from "./arenaColliders.ts";
import { ARENA_FOV_DEG, ARENA_PITCH, actionBounds, fitDistance, matchCameraMax } from "./cameraRig.ts";
import { launchPadsFor } from "./launchPads.ts";
import { meadowForestPieces } from "./meadowDressing.ts";
import { arenaScaleFor } from "./arenaScale.ts";
import { BOWL_DECKS } from "../components/game/nightCircuitBowl.ts";
import { isOutsideNeonCourt, leftNeonOpenEdge, neonObstacles, neonPlayHalfX, neonPlayHalfZ, neonRails } from "./neonCourt.ts";
import { ZOOGI_REST_Y } from "./restHeight.ts";

const MAPS = ["grass", "ice", "lava", "space", "saturn", "neon"] as const;

function overlapsBox(
  x: number,
  z: number,
  radius: number,
  box: { minX: number; maxX: number; minZ: number; maxZ: number },
): boolean {
  const nearestX = Math.min(box.maxX, Math.max(box.minX, x));
  const nearestZ = Math.min(box.maxZ, Math.max(box.minZ, z));
  return Math.hypot(x - nearestX, z - nearestZ) < radius;
}

test("each map has four launch pads outside the lip and clear of scenery", () => {
  for (const map of MAPS) {
    const layout = getMapLayout(map);
    assert.ok(layout, map);
    const pads = launchPadsFor(map);
    assert.equal(pads.length, 4, map);
    const solids = collectMatchSolids({ map, bumpers: [], landedRocks: [], editorModels: [] });
    for (const pad of pads) {
      if (map === "neon") {
        assert.equal(isOutsideNeonCourt(pad.x, pad.z), true, `${map} pad ${pad.index} is on the floor`);
        assert.equal(leftNeonOpenEdge(pad.x, pad.z, MARBLE_RADIUS), true, `${map} pad ${pad.index} is blocked by a rail`);
        for (const box of [...neonRails(), ...neonObstacles()]) {
          assert.equal(overlapsBox(pad.x, pad.z, MARBLE_RADIUS + 0.15, box), false, `${map} ${box.id}`);
        }
        for (const deck of BOWL_DECKS) {
          const [cx, , cz] = deck.pos;
          const [sx, , sz] = deck.size;
          const footprint = {
            minX: cx - sx / 2,
            maxX: cx + sx / 2,
            minZ: cz - sz / 2,
            maxZ: cz + sz / 2,
          };
          assert.equal(
            overlapsBox(pad.x, pad.z, MARBLE_RADIUS + 0.15, footprint),
            false,
            `neon pad ${pad.index} sits in the ${deck.face} bowl`,
          );
        }
      } else {
        assert.equal(
          centerPastOpenEdge(map, pad.x, pad.z, layout.knockoffRadius, MARBLE_RADIUS, solids),
          true,
          `${map} pad ${pad.index} is still on the floor`,
        );
        for (const solid of solids) {
          const gap = Math.hypot(pad.x - solid.x, pad.z - solid.z) - solid.radius - MARBLE_RADIUS;
          assert.ok(gap >= 0.15, `${map} ${solid.id} gap ${gap.toFixed(2)}`);
        }
      }
      if (map === "grass") {
        for (const piece of meadowForestPieces()) {
          if (Math.hypot(piece.x, piece.z) > 40) continue;
          const reach = Math.max(piece.canopy, piece.trunk);
          const gap = Math.hypot(pad.x - piece.x, pad.z - piece.z) - reach - MARBLE_RADIUS;
          assert.ok(gap >= 0.15, `${piece.id} gap ${gap.toFixed(2)}`);
        }
      }
    }
    for (let i = 0; i < pads.length; i++) {
      for (let j = i + 1; j < pads.length; j++) {
        const apart = Math.hypot(pads[i].x - pads[j].x, pads[i].z - pads[j].z);
        assert.ok(apart > MARBLE_RADIUS * 4, `${map} pads ${i} and ${j} overlap`);
      }
    }
  }
});

test("portrait framing can see the pads without exceeding the camera clamp", () => {
  const aspect = 390 / 844;
  for (const map of MAPS) {
    const layout = getMapLayout(map);
    assert.ok(layout);
    const points = launchPadsFor(map).map((pad) => ({ x: pad.x, z: pad.z }));
    if (map === "neon") {
      points.push(
        { x: neonPlayHalfX(), z: neonPlayHalfZ() },
        { x: -neonPlayHalfX(), z: -neonPlayHalfZ() },
      );
    } else {
      const ring = layout.floorRadius;
      points.push({ x: ring, z: ring }, { x: -ring, z: -ring });
    }
    const pad = (aspect < 0.9 ? 1.3 : 2.6) * arenaScaleFor(map);
    const needed = fitDistance(actionBounds(points, 0), ARENA_PITCH, ARENA_FOV_DEG, aspect, pad);
    assert.ok(needed <= matchCameraMax(aspect, arenaScaleFor(map)), `${map} portrait needs ${needed.toFixed(1)}`);
    const wide = fitDistance(actionBounds(points, 0), ARENA_PITCH, ARENA_FOV_DEG, 1920 / 1080, 2.6 * arenaScaleFor(map));
    assert.ok(wide <= matchCameraMax(1920 / 1080, arenaScaleFor(map)), `${map} desktop needs ${wide.toFixed(1)}`);
  }
});

test("a Zoogi on its pad is not knocked out, and comes back to that pad next turn", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await import("./stores/useZoogiGame.tsx");
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", selectedMap: "grass" });
  useZoogiGame.getState().startGame();
  const started = useZoogiGame.getState();
  assert.ok(started.playerEntity);
  assert.equal(started.playerEntity.onLaunchPad, true);
  assert.equal(started.enemies[0]?.onLaunchPad, true);
  const home = [...started.playerEntity.position] as [number, number, number];
  const enemyParked = [3, ZOOGI_REST_Y, -2] as [number, number, number];
  assert.ok(Math.hypot(home[0], home[2]) > (getMapLayout("grass")?.knockoffRadius ?? 0));

  for (let step = 0; step < 40; step++) useZoogiGame.getState().physicsTick(1 / 60);
  const sitting = useZoogiGame.getState().playerEntity;
  assert.ok(sitting);
  assert.equal(sitting.isKnockedOut, false);
  assert.equal(sitting.offTheFloor ?? false, false);
  assert.equal(sitting.onLaunchPad, true);
  assert.ok(Math.hypot(sitting.position[0] - home[0], sitting.position[2] - home[2]) < 0.05);

  const orbSpot = started.orbs[0]?.position;
  useZoogiGame.setState({
    phase: "playing",
    currentRound: 12,
    isPlayerTurn: true,
    turnHasLaunched: true,
    firstTickProcessed: true,
    orbs: started.orbs.map((orb) => ({ ...orb, velocity: [0, 0, 0] as [number, number, number], position: [orb.position[0], 0.4, orb.position[2]] as [number, number, number] })),
    playerEntity: {
      ...sitting,
      isKnockedOut: true,
      isRespawning: true,
      onLaunchPad: false,
      offTheFloor: false,
      position: [0, -8, 0],
    },
    enemies: [{
      ...started.enemies[0],
      position: enemyParked,
      velocity: [0, 0, 0],
      onLaunchPad: false,
      isKnockedOut: false,
      isRespawning: false,
      offTheFloor: false,
    }],
  });
  const before = useZoogiGame.getState();
  const keptOrb = before.orbs[0] ? [...before.orbs[0].position] : null;
  const realNow = Date.now;
  const clock = { now: realNow() + 8000 };
  Date.now = () => clock.now;
  try {
    useZoogiGame.getState().endTurn();
    const enemyTurn = useZoogiGame.getState();
    assert.equal(enemyTurn.isPlayerTurn, false);
    assert.equal(enemyTurn.playerEntity?.isKnockedOut || enemyTurn.playerEntity?.isRespawning, true);
    assert.ok(Math.hypot((enemyTurn.enemies[0]?.position[0] ?? 0) - enemyParked[0], (enemyTurn.enemies[0]?.position[2] ?? 0) - enemyParked[2]) < 0.05);
    clock.now += 800;
    useZoogiGame.getState().endTurn();
    const back = useZoogiGame.getState();
    assert.equal(back.isPlayerTurn, true);
    assert.ok(back.playerEntity);
    assert.equal(back.playerEntity.onLaunchPad, true);
    assert.equal(back.playerEntity.isKnockedOut, false);
    assert.ok(Math.hypot(back.playerEntity.position[0] - home[0], back.playerEntity.position[2] - home[2]) < 0.05, "returns to their own pad");
    assert.ok(Math.hypot((back.enemies[0]?.position[0] ?? 0) - enemyParked[0], (back.enemies[0]?.position[2] ?? 0) - enemyParked[2]) < 0.05, "the other Zoogi stays");
    if (keptOrb && back.orbs[0]) {
      assert.ok(Math.hypot(back.orbs[0].position[0] - keptOrb[0], back.orbs[0].position[2] - keptOrb[2]) < 0.05);
    }
    assert.ok(orbSpot);
  } finally {
    Date.now = realNow;
  }
});

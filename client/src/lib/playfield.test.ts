import test from "node:test";
import assert from "node:assert/strict";
import { getMapLayout } from "./arenaColliders.ts";
import { ARENA_SCALE } from "./arenaScale.ts";
import { isOutsideNeonCourt, NEON_HALF_X, NEON_HALF_Z } from "./neonCourt.ts";
import { marbleIsShown } from "./marblePresence.ts";
import { resetInterp, setInterpFrame, visualPosition } from "./renderInterp.ts";
import { MAX_LAUNCH_SPEED } from "./simFeel.ts";
import { ORB_REST_Y, ZOOGI_REST_Y } from "./restHeight.ts";

const memory = new Map<string, string>();
const storage = {
  getItem: (key: string) => memory.get(key) ?? null,
  setItem: (key: string, value: string) => {
    memory.set(key, String(value));
  },
  removeItem: (key: string) => {
    memory.delete(key);
  },
  clear: () => memory.clear(),
  key: (index: number) => Array.from(memory.keys())[index] ?? null,
  get length() {
    return memory.size;
  },
};

Object.defineProperty(globalThis, "localStorage", { value: storage, configurable: true });
Object.defineProperty(globalThis, "window", { value: globalThis, configurable: true });

const MAPS = ["grass", "ice", "lava", "space", "saturn", "neon"] as const;

async function loadGame() {
  return import("./stores/useZoogiGame.tsx");
}

test("orbs and zoogis rest on the floor", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({
    aiPlayerCount: 1,
    gameMode: "classic",
    selectedMap: "grass",
    zoneEditorConfigs: [],
  });
  useZoogiGame.getState().startGame();
  const started = useZoogiGame.getState();
  assert.ok(started.playerEntity);
  assert.ok(started.orbs.length > 0);
  assert.equal(started.playerEntity.position[1], ZOOGI_REST_Y);
  assert.equal(started.orbs[0].position[1], ORB_REST_Y);

  useZoogiGame.setState({
    phase: "playing",
    playerEntity: {
      ...started.playerEntity,
      position: [1, 0.5, 1],
      velocity: [0, 0, 0],
      arcMovement: null,
      isStunned: false,
    },
    orbs: started.orbs.map((orb, index) => (
      index === 0 ? { ...orb, position: [2, 0.5, 2] as [number, number, number], velocity: [0, 0, 0] as [number, number, number] } : orb
    )),
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const rested = useZoogiGame.getState();
  assert.equal(rested.playerEntity?.position[1], ZOOGI_REST_Y);
  assert.equal(rested.orbs[0]?.position[1], ORB_REST_Y);
  assert.ok(Math.abs((rested.playerEntity?.position[0] ?? 0) - 1) < 0.05);
  assert.ok(Math.abs((rested.orbs[0]?.position[0] ?? 0) - 2) < 0.05);
});

test("zoogis stay in play across several turns", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({
    aiPlayerCount: 3,
    gameMode: "classic",
    selectedMap: "grass",
  });
  useZoogiGame.getState().startGame();
  useZoogiGame.setState({ phase: "playing" });

  const clock = { now: 1_000_000 };
  const realNow = Date.now;
  Date.now = () => clock.now;
  try {
    for (let step = 0; step < 8; step++) useZoogiGame.getState().physicsTick(1 / 60);
    const before = useZoogiGame.getState();
    assert.ok(before.playerEntity);
    assert.equal(before.enemies.length, 3);
    const bodies = [before.playerEntity, ...before.enemies];
    for (const body of bodies) {
      assert.equal(body.isKnockedOut, false);
      assert.equal(body.isRespawning, false);
      assert.equal(marbleIsShown(body), true);
    }
    const snapshot = bodies.map((body) => ({
      id: body.id,
      x: body.position[0],
      z: body.position[2],
    }));

    resetInterp();
    setInterpFrame(
      bodies.map((body) => ({ id: body.id, prev: body.position, curr: body.position })),
      0,
    );
    for (const body of bodies) {
      const drawn = visualPosition(body.id, body.position);
      assert.ok(Math.hypot(drawn[0] - body.position[0], drawn[2] - body.position[2]) < 1e-6, body.id);
    }

    for (let turn = 0; turn < 4; turn++) {
      clock.now += 500;
      useZoogiGame.getState().endTurn();
      for (let step = 0; step < 6; step++) useZoogiGame.getState().physicsTick(1 / 60);
    }

    const after = useZoogiGame.getState();
    assert.equal(after.phase, "playing");
    assert.ok(after.playerEntity);
    assert.equal(after.enemies.length, 3);
    const afterBodies = [after.playerEntity, ...after.enemies];
    assert.deepEqual(afterBodies.map((body) => body.id), snapshot.map((body) => body.id));
    for (let i = 0; i < afterBodies.length; i++) {
      const body = afterBodies[i];
      assert.equal(body.isKnockedOut, false, body.id);
      assert.equal(body.isRespawning, false, body.id);
      assert.equal(marbleIsShown(body), true, body.id);
      assert.ok(Math.hypot(body.position[0] - snapshot[i].x, body.position[2] - snapshot[i].z) < 0.2, `${body.id} drifted`);
      assert.equal(body.position[1], ZOOGI_REST_Y);
    }

    setInterpFrame(
      afterBodies.map((body) => ({
        id: body.id,
        prev: [body.position[0] - 30, body.position[1], body.position[2]] as [number, number, number],
        curr: body.position,
      })),
      0,
    );
    const snapped = visualPosition(after.playerEntity.id, after.playerEntity.position);
    assert.ok(Math.abs(snapped[0] - after.playerEntity.position[0]) < 1e-6, "a respawn jump draws the store position");
  } finally {
    Date.now = realNow;
    resetInterp();
  }
});

test("a knocked-out marble hands the turn off instead of staying hidden", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", selectedMap: "grass" });
  useZoogiGame.getState().startGame();
  const live = useZoogiGame.getState();
  assert.ok(live.playerEntity);
  useZoogiGame.setState({
    phase: "playing",
    isPlayerTurn: true,
    turnHasLaunched: true,
    playerEntity: { ...live.playerEntity, isKnockedOut: true, isRespawning: true },
  });
  const clock = { now: 5_000_000 };
  const realNow = Date.now;
  Date.now = () => clock.now;
  try {
    useZoogiGame.getState().endTurn();
    clock.now += 10;
    useZoogiGame.getState().endTurn();
    const after = useZoogiGame.getState();
    assert.equal(after.isPlayerTurn, false);
    assert.equal(after.turnIndex, 0);
    assert.equal(marbleIsShown(after.playerEntity), false);
    assert.equal(marbleIsShown(after.enemies[0]), true);
  } finally {
    Date.now = realNow;
  }
});

test("knockout uses the enlarged edge on every arena", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  for (const map of MAPS) {
    useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
    useZoogiGame.setState({ aiPlayerCount: 0, gameMode: "classic", selectedMap: map });
    useZoogiGame.getState().startGame();
    const started = useZoogiGame.getState();
    assert.ok(started.playerEntity, map);
    const layout = getMapLayout(map);
    assert.ok(layout, map);
    assert.ok(layout.floorRadius >= 8 * ARENA_SCALE, map);

    const inside: [number, number, number] = map === "neon"
      ? [NEON_HALF_X - 1.5, ZOOGI_REST_Y, 0]
      : [layout.knockoffRadius * 0.45, ZOOGI_REST_Y, 0];
    useZoogiGame.setState({
      phase: "playing",
      firstTickProcessed: true,
      playerEntity: {
        ...started.playerEntity,
        position: inside,
        velocity: [0, 0, 0],
        isKnockedOut: false,
        isRespawning: false,
        spawnImmunity: false,
        invulnerableUntil: null,
        arcMovement: null,
      },
      enemies: [],
      orbs: [],
    });
    useZoogiGame.getState().physicsTick(1 / 60);
    assert.equal(useZoogiGame.getState().playerEntity?.isKnockedOut, false, `${map} inside`);

    assert.ok(
      Math.abs((useZoogiGame.getState().wallSettings.knockoffBoundaryRadius ?? 0) - layout.knockoffRadius) < 0.05,
      `${map} live edge ${useZoogiGame.getState().wallSettings.knockoffBoundaryRadius} vs ${layout.knockoffRadius}`,
    );
    if (map === "neon") {
      assert.equal(isOutsideNeonCourt(0, NEON_HALF_Z + 0.2), true);
      assert.equal(isOutsideNeonCourt(0, NEON_HALF_Z - 0.2), false);
    }
    const outside: [number, number, number] = map === "neon"
      ? [NEON_HALF_X + 0.35, ZOOGI_REST_Y, 0]
      : [layout.knockoffRadius + 1.2, ZOOGI_REST_Y, 0];
    const still = useZoogiGame.getState().playerEntity;
    assert.ok(still);
    useZoogiGame.setState({
      playerEntity: {
        ...still,
        position: outside,
        velocity: [0, 0, 0],
        isKnockedOut: false,
        isRespawning: false,
        spawnImmunity: false,
        invulnerableUntil: null,
      },
    });
    useZoogiGame.getState().physicsTick(1 / 60);
    const knocked = useZoogiGame.getState().playerEntity;
    assert.equal(knocked?.isKnockedOut, true, `${map} outside`);
    if (map === "neon") assert.equal(isOutsideNeonCourt(outside[0], outside[2]), true);
  }
});

test("a full flick still crosses the enlarged arena", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 0, gameMode: "classic", selectedMap: "grass" });
  useZoogiGame.getState().startGame();
  const started = useZoogiGame.getState();
  assert.ok(started.playerEntity);
  const layout = getMapLayout("grass");
  assert.ok(layout);
  const startX = -layout.floorRadius * 0.45;
  useZoogiGame.setState({
    phase: "playing",
    firstTickProcessed: true,
    enemies: [],
    orbs: [],
    playerEntity: {
      ...started.playerEntity,
      position: [startX, ZOOGI_REST_Y, 0],
      velocity: [MAX_LAUNCH_SPEED, 0, 0],
      spawnImmunity: false,
      invulnerableUntil: null,
      arcMovement: null,
      isStunned: false,
    },
  });
  let farthest = startX;
  for (let step = 0; step < 400; step++) {
    const marble = useZoogiGame.getState().playerEntity;
    if (marble) farthest = Math.max(farthest, marble.position[0]);
    const speed = Math.hypot(marble?.velocity[0] ?? 0, marble?.velocity[2] ?? 0);
    if (step > 5 && speed < 0.02) break;
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  const ended = useZoogiGame.getState().playerEntity;
  assert.ok(ended);
  const traveled = farthest - startX;
  assert.ok(traveled > layout.floorRadius, `full flick reached ${traveled} on a floor of radius ${layout.floorRadius}`);
  assert.equal(ended.isKnockedOut, false);
});

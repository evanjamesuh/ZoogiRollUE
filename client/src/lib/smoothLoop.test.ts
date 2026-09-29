import test from "node:test";
import assert from "node:assert/strict";
import { motionOnly, overlayRecord, type MotionBaseline, type MotionNext } from "./simPublish.ts";
import { resetInterp, setInterpAlpha, setInterpFrame, visualPosition } from "./renderInterp.ts";
import { MAX_LAUNCH_SPEED } from "./simFeel.ts";

function body(id: string, x: number, flags: Record<string, unknown> = {}) {
  return {
    id,
    position: [x, 0.86, 0] as [number, number, number],
    velocity: [0, 0, 0] as [number, number, number],
    isKnockedOut: false,
    isRespawning: false,
    isActive: true,
    score: 0,
    ...flags,
  };
}

function baseline(extra: Partial<MotionBaseline> = {}): MotionBaseline {
  return {
    phase: "playing",
    showExplosion: null,
    player: body("p", 0),
    enemies: [body("e", 2)],
    orbs: [body("o", 4)],
    wolfClones: [],
    fallingEntities: [],
    pinballBumpers: [],
    ...extra,
  };
}

test("a roll with the same bodies does not need a React publish", () => {
  const live = baseline();
  const next: MotionNext = {
    player: { ...live.player!, position: [1, 0.86, 0], velocity: [0.4, 0, 0] },
    enemies: live.enemies,
    orbs: live.orbs,
    wolfClones: [],
    fallingEntities: [],
    pinballBumpers: [],
  };
  assert.equal(motionOnly(live, next), true);
});

test("a knockout or a spent orb still publishes", () => {
  const live = baseline();
  assert.equal(motionOnly(live, {
    player: { ...live.player!, isKnockedOut: true },
    enemies: live.enemies,
    orbs: live.orbs,
    wolfClones: [],
    fallingEntities: [],
    pinballBumpers: [],
  }), false);
  assert.equal(motionOnly(live, {
    player: live.player!,
    enemies: live.enemies,
    orbs: [{ ...live.orbs[0], isActive: false }],
    wolfClones: [],
    fallingEntities: [],
    pinballBumpers: [],
  }), false);
});

test("overlay keeps the position array the mesh already holds", () => {
  const live = body("p", 0);
  const held = live.position;
  overlayRecord(live, { ...live, position: [3, 0.86, 1], velocity: [0.2, 0, 0] });
  assert.equal(live.position, held);
  assert.equal(live.position[0], 3);
  assert.equal(live.position[2], 1);
  assert.equal(live.velocity[0], 0.2);
});

test("render interpolation blends the previous and current physics step", () => {
  resetInterp();
  setInterpFrame([{
    id: "p",
    prev: [0, 1, 0],
    curr: [1, 1, 0],
  }], 0.5);
  const mid = visualPosition("p", [1, 1, 0]);
  assert.ok(Math.abs(mid[0] - 0.5) < 1e-6, `x ${mid[0]}`);
  assert.ok(Math.abs(mid[1] - 1) < 1e-6, `y ${mid[1]}`);
  setInterpAlpha(1);
  const end = visualPosition("p", [1, 1, 0]);
  assert.ok(Math.abs(end[0] - 1) < 1e-6);
  resetInterp();
});

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

test("a full-power step moves the marble without replacing its object", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await import("./stores/useZoogiGame.tsx");
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", selectedMap: "grass" });
  useZoogiGame.getState().startGame();
  const live = useZoogiGame.getState();
  assert.ok(live.playerEntity);
  const player = live.playerEntity;
  const position = player.position;
  const startX = position[0];
  player.velocity[0] = MAX_LAUNCH_SPEED;
  player.velocity[1] = 0;
  player.velocity[2] = 0;
  useZoogiGame.getState().physicsTick(1 / 60);
  const after = useZoogiGame.getState();
  assert.equal(after.playerEntity, player);
  assert.equal(after.playerEntity?.position, position);
  assert.ok((after.playerEntity?.position[0] ?? startX) > startX + 0.2);
  assert.equal(after.enemies, live.enemies);
});

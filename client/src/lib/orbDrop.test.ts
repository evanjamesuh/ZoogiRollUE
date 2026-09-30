import test from "node:test";
import assert from "node:assert/strict";
import { getMapLayout } from "./arenaColliders.ts";
import { ZONE_SCORE_ORB } from "./arenaConstants.ts";
import { orbDropY, orbsHaveLanded } from "./orbDrop.ts";
import { ORB_REST_Y } from "./restHeight.ts";
import { FALL_GRAVITY, ORB_DROP_HEIGHT, ORB_ORB_RESTITUTION, SETTLE_DELAY_STEPS } from "./simFeel.ts";

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

async function playing() {
  const { useZoogiGame, ZOOGI_ROSTER } = await import("./stores/useZoogiGame.tsx");
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({
    aiPlayerCount: 1,
    gameMode: "classic",
    selectedMap: "grass",
    phase: "menu",
  });
  useZoogiGame.getState().startGame();
  return useZoogiGame;
}

test("orbs start above the floor, bounce, and rest on it", async () => {
  const useZoogiGame = await playing();
  const started = useZoogiGame.getState();
  assert.ok(started.orbs.length > 2);
  const heights = started.orbs.map((orb) => orb.position[1]);
  assert.ok(Math.min(...heights) > ORB_REST_Y + ORB_DROP_HEIGHT - 0.01);
  assert.ok(heights[heights.length - 1] > heights[0], "later orbs start higher");
  assert.equal(orbDropY(0), heights[0]);
  for (const orb of started.orbs) {
    assert.equal(orb.velocity[1], 0);
    assert.equal(orbsHaveLanded([orb]), false);
  }
  assert.equal(FALL_GRAVITY, 9.81);
  assert.equal(ORB_ORB_RESTITUTION, 0.55);

  const home = started.orbs.map((orb) => [orb.position[0], orb.position[2]]);
  let sawFall = false;
  let sawBounce = false;
  let prevY = heights[0];
  let rested = false;
  for (let step = 0; step < 900 && !rested; step++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    const orbs = useZoogiGame.getState().orbs;
    const y = orbs[0]?.position[1] ?? 0;
    if (y < prevY - 0.01) sawFall = true;
    if (sawFall && y > prevY + 0.005) sawBounce = true;
    prevY = y;
    for (const orb of orbs) {
      assert.ok(orb.position[1] >= ORB_REST_Y - 1e-6, "an orb over the floor does not sink");
      assert.ok(Math.abs(orb.position[0] - home[orbs.indexOf(orb)][0]) < 0.05);
    }
    rested = orbsHaveLanded(orbs) && orbs.every((orb) => orb.position[1] === ORB_REST_Y && orb.velocity[1] === 0);
  }
  assert.equal(sawFall, true);
  assert.equal(sawBounce, true, "the drop should bounce before it settles");
  assert.equal(rested, true, "every orb should finish on the floor");
  assert.equal(useZoogiGame.getState().isPlayerTurn, true, "the opening drop does not pass the turn");
});

test("a shot is locked until the orbs have landed", async () => {
  const useZoogiGame = await playing();
  const before = useZoogiGame.getState().playerEntity;
  assert.ok(before);
  useZoogiGame.getState().updatePlayerVelocity([0.2, 0, 0]);
  const blocked = useZoogiGame.getState();
  assert.equal(blocked.turnHasLaunched, false);
  assert.equal(blocked.playerEntity?.velocity[0], 0);

  for (let step = 0; step < 900 && !orbsHaveLanded(useZoogiGame.getState().orbs); step++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  assert.equal(orbsHaveLanded(useZoogiGame.getState().orbs), true);
  useZoogiGame.getState().updatePlayerVelocity([0.2, 0, 0]);
  assert.equal(useZoogiGame.getState().turnHasLaunched, true);
  assert.ok((useZoogiGame.getState().playerEntity?.velocity[0] ?? 0) > 0);
});

test("an orb scores only by leaving the edge, not by sitting in a zone", async () => {
  const useZoogiGame = await playing();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  const layout = getMapLayout("grass");
  assert.ok(layout);
  const zone = layout.zones.find((entry) => !entry.isSpawn);
  assert.ok(zone);
  const zx = Math.cos(zone.angle) * zone.distance;
  const zz = Math.sin(zone.angle) * zone.distance;
  useZoogiGame.setState({
    phase: "playing",
    score: 0,
    isPlayerTurn: true,
    turnHasLaunched: true,
    firstTickProcessed: true,
    enemies: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    playerEntity: {
      ...player,
      position: [0, player.position[1], 0],
      velocity: [0, 0, 0],
      score: 0,
    },
    orbs: [{
      id: "zone-orb",
      position: [zx, ORB_REST_Y, zz],
      velocity: [0, 0, 0],
      color: "#0341CA",
      points: 50,
      isActive: true,
      lastHitBy: "player",
      lastHitByEnemyId: null,
      lastHitByLocalPlayerIndex: null,
      lastHitTimestamp: null,
      isStarOrb: false,
      starOrbType: null,
      isOutOfRing: false,
    }],
  });
  for (let step = 0; step < 30; step++) useZoogiGame.getState().physicsTick(1 / 60);
  assert.equal(useZoogiGame.getState().score, 0, "stopping in a zone does not score");
  assert.equal(useZoogiGame.getState().orbs[0]?.isActive, true);
  useZoogiGame.getState().endTurn();
  assert.equal(useZoogiGame.getState().score, 0, "ending the turn does not claim the zone");
  assert.notEqual(ZONE_SCORE_ORB, 50);

  useZoogiGame.setState({
    phase: "playing",
    score: 0,
    isPlayerTurn: true,
    orbs: [{
      id: "edge-orb",
      position: [layout.knockoffRadius + 2, ORB_REST_Y, 0],
      velocity: [0.05, 0, 0],
      color: "#0341CA",
      points: 50,
      isActive: true,
      lastHitBy: "player",
      lastHitByEnemyId: null,
      lastHitByLocalPlayerIndex: null,
      lastHitTimestamp: null,
      isStarOrb: false,
      starOrbType: null,
      isOutOfRing: false,
    }],
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const falling = useZoogiGame.getState();
  assert.equal(falling.score, 0, "crossing the edge does not score yet");
  assert.equal(falling.orbs[0]?.isOutOfRing, true);
  assert.ok((falling.orbs[0]?.position[1] ?? 1) < ORB_REST_Y, "past the edge the orb falls away");
  let climbed = false;
  let prev = falling.orbs[0]?.position[1] ?? 0;
  for (let step = 0; step < 180 && useZoogiGame.getState().orbs[0]?.isActive; step++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    const y = useZoogiGame.getState().orbs[0]?.position[1] ?? prev;
    if (y > prev + 0.01) climbed = true;
    prev = y;
  }
  assert.equal(climbed, false, "an orb past the edge does not bounce back onto the floor");
  assert.equal(useZoogiGame.getState().orbs[0]?.isActive, false);
  assert.equal(useZoogiGame.getState().score, 50);
});

test("the turn stays with the shooter while an orb is still in the air", async () => {
  const useZoogiGame = await playing();
  const player = useZoogiGame.getState().playerEntity;
  const enemy = useZoogiGame.getState().enemies[0];
  assert.ok(player && enemy);
  const parked: [number, number, number] = [-3, player.position[1], 2];
  const realNow = Date.now;
  const clock = { now: realNow() + 5000 };
  Date.now = () => clock.now;
  try {
  useZoogiGame.setState({
    phase: "playing",
    currentRound: 70,
    isPlayerTurn: true,
    turnHasLaunched: true,
    firstTickProcessed: true,
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 80 },
    playerEntity: { ...player, position: parked, velocity: [0, 0, 0], offTheFloor: false, isKnockedOut: false, isRespawning: false },
    enemies: [{ ...enemy, position: [4, enemy.position[1], -2], velocity: [0, 0, 0], offTheFloor: false, isKnockedOut: false, isRespawning: false }],
    orbs: [{
      id: "air-orb",
      position: [1, ORB_REST_Y + 18, 1],
      velocity: [0, 0, 0],
      color: "#0341CA",
      points: 50,
      isActive: true,
      lastHitBy: null,
      lastHitByEnemyId: null,
      lastHitByLocalPlayerIndex: null,
      lastHitTimestamp: null,
      isStarOrb: false,
      starOrbType: null,
      isOutOfRing: false,
    }],
  });
  for (let step = 0; step < SETTLE_DELAY_STEPS + 20; step++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  assert.equal(useZoogiGame.getState().isPlayerTurn, true, "an orb still dropping holds the turn");
  for (let step = 0; step < 900 && useZoogiGame.getState().isPlayerTurn; step++) {
    if (step % 30 === 0) clock.now += 500;
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  const handed = useZoogiGame.getState();
  assert.equal(handed.isPlayerTurn, false);
  assert.equal(handed.orbs[0]?.position[1], ORB_REST_Y);
  assert.ok(Math.hypot((handed.playerEntity?.position[0] ?? 0) - parked[0], (handed.playerEntity?.position[2] ?? 0) - parked[2]) < 0.05);
  } finally {
    Date.now = realNow;
  }
});

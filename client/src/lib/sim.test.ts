import test from "node:test";
import assert from "node:assert/strict";

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

type GameStore = typeof import("./stores/useZoogiGame.tsx");

async function loadGame(): Promise<GameStore> {
  return import("./stores/useZoogiGame.tsx");
}

test("one simulation: launch, collision, and zone score", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  const store = useZoogiGame.getState();

  store.selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({
    aiPlayerCount: 0,
    gameMode: "classic",
    zoneEditorConfigs: [],
  });
  useZoogiGame.getState().startGame();

  const started = useZoogiGame.getState().playerEntity;
  assert.ok(started, "startGame should create the player marble");

  useZoogiGame.setState({
    phase: "playing",
    score: 0,
    enemies: [],
    orbs: [],
    mushrooms: [],
    pinballBumpers: [],
    playerEntity: {
      ...started,
      position: [0, 0.5, 0],
      velocity: [0.8, 0, 0],
      arcMovement: null,
      isStunned: false,
    },
  });

  useZoogiGame.getState().physicsTick(1 / 60);
  const launched = useZoogiGame.getState().playerEntity;
  assert.ok(launched);
  assert.ok(launched.position[0] > 0.5, `flick should move the marble forward, x=${launched.position[0]}`);

  const enemyZoogi = ZOOGI_ROSTER.find((zoogi) => zoogi.id !== launched.zoogi.id) ?? ZOOGI_ROSTER[1];
  useZoogiGame.setState({
    score: 0,
    orbs: [],
    playerEntity: {
      ...launched,
      position: [0, 0.5, 0],
      velocity: [0.5, 0, 0],
      arcMovement: null,
      isStunned: false,
    },
    enemies: [{
      ...launched,
      id: "enemy-collision",
      isPlayer: false,
      zoogi: enemyZoogi,
      position: [1.0, 0.5, 0],
      velocity: [0, 0, 0],
      arcMovement: null,
      isStunned: false,
      isKnockedOut: false,
      spawnImmunity: false,
    }],
  });

  useZoogiGame.getState().physicsTick(1 / 60);
  const collided = useZoogiGame.getState();
  const enemy = collided.enemies[0];
  assert.ok(enemy);
  const separation = Math.hypot(
    enemy.position[0] - (collided.playerEntity?.position[0] ?? 0),
    enemy.position[2] - (collided.playerEntity?.position[2] ?? 0)
  );
  assert.ok(separation >= 0.9, `marbles should separate on impact, distance=${separation}`);
  assert.ok(enemy.velocity[0] > 0, "the hit marble should be pushed in the launch direction");

  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    score: 0,
    enemies: [],
    mushrooms: [],
    pinballBumpers: [],
    zoneEditorConfigs: [{
      id: "score-zone",
      angle: 0,
      distance: 4,
      visible: true,
      isSpawn: false,
    }],
    playerEntity: {
      ...player,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      arcMovement: null,
    },
    orbs: [{
      id: "orb-score",
      position: [4, 0.5, 0],
      velocity: [0, 0, 0],
      color: "#87CEEB",
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
  const scored = useZoogiGame.getState();
  assert.equal(scored.score, 15, "a stopped orb in a scoring zone should award 15 points");
  assert.equal(scored.phase, "playing", "one scored orb should not end the round");
  assert.equal(scored.orbs[0]?.capturedInZone, "score-zone");
});

function planarSpeed(velocity: [number, number, number]): number {
  return Math.hypot(velocity[0], velocity[2]);
}

function planarEnergy(velocity: [number, number, number]): number {
  return 0.5 * (velocity[0] ** 2 + velocity[2] ** 2);
}

function makeStillOrb(id: string, x: number, z: number) {
  return {
    id,
    position: [x, 0.5, z] as [number, number, number],
    velocity: [0, 0, 0] as [number, number, number],
    color: "#87CEEB",
    points: 50,
    isActive: true,
    lastHitBy: null,
    lastHitByEnemyId: null,
    lastHitByLocalPlayerIndex: null,
    lastHitTimestamp: null,
    isStarOrb: false,
    starOrbType: null,
    isOutOfRing: false,
  };
}

async function playingMarble() {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  const store = useZoogiGame.getState();
  store.selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({
    aiPlayerCount: 0,
    gameMode: "classic",
    zoneEditorConfigs: [],
    phase: "menu",
  });
  useZoogiGame.getState().startGame();
  const started = useZoogiGame.getState().playerEntity;
  assert.ok(started);
  useZoogiGame.setState({
    phase: "playing",
    score: 0,
    enemies: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    zoneEditorConfigs: [],
    playerEntity: {
      ...started,
      larsRicochetBoost: 1,
      arcMovement: null,
      isStunned: false,
      isKnockedOut: false,
      spawnImmunity: false,
    },
  });
  return useZoogiGame;
}

test("marble near an orb with no ability active does not accelerate toward it", async () => {
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);

  useZoogiGame.setState({
    playerEntity: {
      ...player,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      arcMovement: null,
    },
    orbs: [makeStillOrb("near-orb", 1.6, 0)],
  });

  for (let frame = 0; frame < 180; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }

  const resting = useZoogiGame.getState();
  const restingPlayer = resting.playerEntity;
  const restingOrb = resting.orbs[0];
  assert.ok(restingPlayer && restingOrb);
  assert.equal(planarSpeed(restingPlayer.velocity), 0, "a resting marble should stay stopped");
  assert.ok(
    Math.hypot(restingPlayer.position[0], restingPlayer.position[2]) < 0.001,
    "a resting marble should not drift toward a nearby orb"
  );
  const restingGap = Math.hypot(
    restingOrb.position[0] - restingPlayer.position[0],
    restingOrb.position[2] - restingPlayer.position[2]
  );
  assert.ok(restingGap >= 1.59, `gap to the orb should not shrink, gap=${restingGap}`);

  useZoogiGame.setState({
    playerEntity: {
      ...restingPlayer,
      position: [0, 0.5, 0],
      velocity: [0.15, 0, 0],
      arcMovement: null,
      larsRicochetBoost: 1,
    },
    orbs: [makeStillOrb("contact-orb", 1.3, 0), makeStillOrb("side-orb", 1.3, 2.8)],
  });

  const startSpeed = 0.15;
  let maxSpeed = 0;
  let maxSideways = 0;
  for (let frame = 0; frame < 180; frame++) {
    const before = useZoogiGame.getState();
    const beforePlayer = before.playerEntity;
    assert.ok(beforePlayer);
    const beforeEnergy = planarEnergy(beforePlayer.velocity)
      + before.orbs.reduce((sum, orb) => sum + planarEnergy(orb.velocity), 0);
    useZoogiGame.getState().physicsTick(1 / 60);
    const after = useZoogiGame.getState();
    const afterPlayer = after.playerEntity;
    assert.ok(afterPlayer);
    const speed = planarSpeed(afterPlayer.velocity);
    if (speed > maxSpeed) maxSpeed = speed;
    maxSideways = Math.max(maxSideways, Math.abs(afterPlayer.velocity[2]), Math.abs(afterPlayer.position[2]));
    const afterEnergy = planarEnergy(afterPlayer.velocity)
      + after.orbs.reduce((sum, orb) => sum + planarEnergy(orb.velocity), 0);
    assert.ok(
      afterEnergy <= beforeEnergy + 1e-6,
      `contact must not add energy, before=${beforeEnergy} after=${afterEnergy}`
    );
  }

  assert.ok(maxSpeed <= startSpeed + 0.005, `light contact sped the marble up to ${maxSpeed}`);
  assert.ok(maxSideways < 0.02, `marble steered toward a side orb, sideways=${maxSideways}`);
  const finished = useZoogiGame.getState().playerEntity;
  assert.ok(finished);
  assert.equal(planarSpeed(finished.velocity), 0, "light contact should still come to rest");
  assert.ok(Math.abs(finished.position[2]) < 0.25, `marble drifted off the shot line, z=${finished.position[2]}`);
});

test("marble comes to rest within a reasonable time after a flick and collision", async () => {
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);

  useZoogiGame.setState({
    playerEntity: {
      ...player,
      position: [0, 0.5, 0],
      velocity: [0.9, 0, 0],
      arcMovement: null,
      larsRicochetBoost: 1,
      isStunned: false,
    },
    orbs: [makeStillOrb("hit-orb", 2.4, 0)],
  });

  const contactFrames = 240;
  let restFrame = -1;
  let contactX: number | null = null;

  for (let frame = 0; frame < contactFrames; frame++) {
    const before = useZoogiGame.getState();
    const beforePlayer = before.playerEntity;
    assert.ok(beforePlayer);
    const beforeEnergy = planarEnergy(beforePlayer.velocity)
      + before.orbs.reduce((sum, orb) => sum + planarEnergy(orb.velocity), 0);

    useZoogiGame.getState().physicsTick(1 / 60);

    const after = useZoogiGame.getState();
    const afterPlayer = after.playerEntity;
    const hitOrb = after.orbs[0];
    assert.ok(afterPlayer && hitOrb);
    const speed = planarSpeed(afterPlayer.velocity);
    const afterEnergy = planarEnergy(afterPlayer.velocity) + planarEnergy(hitOrb.velocity);
    assert.ok(
      afterEnergy <= beforeEnergy + 1e-6,
      `flick collision gained energy on frame ${frame + 1}, before=${beforeEnergy} after=${afterEnergy}`
    );

    if (contactX === null && planarSpeed(hitOrb.velocity) > 0) {
      contactX = afterPlayer.position[0];
    }
    if (restFrame < 0 && speed === 0) restFrame = frame + 1;
  }

  const done = useZoogiGame.getState().playerEntity;
  assert.ok(done);
  assert.ok(contactX !== null, "the flick should hit the orb");
  assert.ok(restFrame > 0 && restFrame <= 180, `marble was still moving after ${restFrame} frames`);
  assert.equal(planarSpeed(done.velocity), 0, "marble should be fully stopped");
  const coast = done.position[0] - contactX;
  assert.ok(coast < 1.25, `marble kept rolling after contact, coast=${coast}`);
  assert.ok(Math.abs(done.position[2]) < 0.05, "collision should not steer the marble sideways");

  for (let frame = 0; frame < 60; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  const stayed = useZoogiGame.getState().playerEntity;
  assert.ok(stayed);
  assert.equal(planarSpeed(stayed.velocity), 0, "a stopped marble should not start moving again");
  assert.ok(Math.abs(stayed.position[0] - done.position[0]) < 1e-6, "a stopped marble should hold its ground");
});

test("a knockoff offset does not carry from one map into the next", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  const dirty = {
    ...useZoogiGame.getState().elementTransforms,
    knockoffBoundaryOffset: { x: -9.9, y: 0, z: -4.8 },
  };
  useZoogiGame.setState({
    aiPlayerCount: 0,
    gameMode: "classic",
    selectedMap: "saturn",
    elementTransforms: dirty,
  });
  useZoogiGame.getState().startGame();
  assert.deepEqual(useZoogiGame.getState().elementTransforms.knockoffBoundaryOffset, { x: 0, y: 0, z: 0 });
  assert.equal(useZoogiGame.getState().wallSettings.knockoffBoundaryRadius, 15.5);

  useZoogiGame.setState({
    selectedMap: "grass",
    elementTransforms: {
      ...useZoogiGame.getState().elementTransforms,
      knockoffBoundaryOffset: { x: -9.9, y: 0, z: -4.8 },
    },
  });
  useZoogiGame.getState().startGame();
  assert.deepEqual(useZoogiGame.getState().elementTransforms.knockoffBoundaryOffset, { x: 0, y: 0, z: 0 });
  assert.equal(useZoogiGame.getState().wallSettings.knockoffBoundaryRadius, 15.5);
});

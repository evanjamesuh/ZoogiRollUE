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

test("knockouts score, falling costs points, and a flick off the edge ends the turn", async () => {
  await new Promise((resolve) => setTimeout(resolve, 500));
  const { ZOOGI_ROSTER } = await loadGame();
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  const cpuZoogi = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "hotstreak") ?? ZOOGI_ROSTER[1];
  const lars = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "lars");
  const wraps = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "wraps");
  assert.ok(lars && wraps, "Lars and Wraps should both be on the roster");

  const cpu = {
    ...player,
    id: "cpu",
    isPlayer: false,
    zoogi: cpuZoogi,
    position: [0, 0.5, 0] as [number, number, number],
    velocity: [0, 0, 0] as [number, number, number],
    score: 100,
    isKnockedOut: false,
    isRespawning: false,
    respawnAt: null,
    spawnImmunity: false,
    lastHitByPlayer: false,
  };

  useZoogiGame.setState({
    phase: "playing",
    gameMode: "classic",
    isPlayerTurn: true,
    turnIndex: 0,
    score: 100,
    zoneEditorConfigs: [],
    mushrooms: [],
    pinballBumpers: [],
    orbs: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 50 },
    playerEntity: {
      ...player,
      position: [60, 0.5, 0],
      velocity: [1.2, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
      respawnAt: null,
      spawnImmunity: false,
      lastHitByEnemyId: null,
    },
    enemies: [cpu],
  });

  useZoogiGame.getState().physicsTick(1 / 60);
  const fell = useZoogiGame.getState();
  assert.equal(fell.score, 25, "falling off should cost 75 points");
  assert.equal(fell.isPlayerTurn, false, "a flick off the edge should end the player's turn");
  assert.equal(fell.playerEntity?.isKnockedOut, true);
  assert.equal(fell.playerEntity?.velocity[0], 0, "the marble should stop instead of rolling forever");
  assert.ok((fell.playerEntity?.respawnAt ?? 0) > Date.now(), "respawn waits a short beat");

  useZoogiGame.setState({
    playerEntity: fell.playerEntity
      ? { ...fell.playerEntity, respawnAt: Date.now() - 20 }
      : null,
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const back = useZoogiGame.getState().playerEntity;
  assert.ok(back);
  assert.equal(back.isKnockedOut, false, "the marble should be back in play");
  assert.equal(back.isRespawning, false);
  assert.ok(
    Math.hypot(back.position[0], back.position[2]) < 48,
    `respawn should land on the playfield, distance=${Math.hypot(back.position[0], back.position[2])}`
  );

  useZoogiGame.setState({
    orbs: [{
      ...makeStillOrb("star-out", 60, 0),
      lastHitBy: "player" as const,
      isStarOrb: true,
      starOrbType: "wolfgang" as const,
    }],
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const orbKnock = useZoogiGame.getState();
  assert.equal(orbKnock.score, 75, "knocking an orb off should score 50");
  assert.equal(orbKnock.orbs[0]?.isOutOfRing, true);
  assert.equal(orbKnock.playerEntity?.wolfgangAbilityUnlocked, true, "a star orb unlocks the hitter's ability");

  useZoogiGame.setState({
    orbs: [{
      ...makeStillOrb("cpu-orb", 62, 0),
      lastHitBy: "enemy" as const,
      lastHitByEnemyId: "cpu",
    }],
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const cpuScored = useZoogiGame.getState();
  assert.equal(cpuScored.score, 75, "an opponent's orb should not add to the player's score");
  assert.equal(cpuScored.enemies[0]?.score, 150, "the computer should score for knocking an orb off");

  useZoogiGame.setState({
    orbs: [],
    enemies: cpuScored.enemies.map((enemy) => ({
      ...enemy,
      position: [64, 0.5, 0] as [number, number, number],
      velocity: [0.4, 0, 0] as [number, number, number],
      isKnockedOut: false,
      isRespawning: false,
      respawnAt: null,
      lastHitByPlayer: true,
      score: 150,
    })),
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const ko = useZoogiGame.getState();
  assert.equal(ko.score, 175, "knocking an opponent off should score 100");
  assert.equal(ko.enemies[0]?.score, 75, "the opponent should lose 75 for falling off");
  assert.equal(ko.enemies[0]?.isKnockedOut, true);

  const ready = useZoogiGame.getState().playerEntity;
  assert.ok(ready);
  useZoogiGame.setState({
    isPlayerTurn: true,
    playerEntity: {
      ...ready,
      position: [0, 0.5, 0],
      velocity: [0.4, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
    },
    enemies: ko.enemies.map((enemy) => ({
      ...enemy,
      position: [4, 0.5, 0] as [number, number, number],
      isKnockedOut: false,
      isRespawning: false,
    })),
    wolfClones: [],
    orbs: [makeStillOrb("home-target", 0.8, 1.5)],
  });
  useZoogiGame.getState().activateWolfgangAbility();
  const packed = useZoogiGame.getState();
  assert.equal(packed.wolfClones.length, 3, "Pack should send three clones");
  assert.equal(packed.playerEntity?.wolfgangAbilityUnlocked, false);
  useZoogiGame.getState().physicsTick(1 / 60);
  const clones = useZoogiGame.getState().wolfClones;
  const center = clones.reduce((best, clone) => (
    Math.abs(clone.position[2]) < Math.abs(best.position[2]) ? clone : best
  ));
  assert.ok(center.velocity[2] > 0.05, "the forward clone should steer toward the orb");

  useZoogiGame.setState({
    playerEntity: {
      ...ready,
      zoogi: lars,
      position: [0, 0.5, 0],
      velocity: [0.3, 0, 0],
      larsAbilityUnlocked: true,
      larsRicochetBoost: 1,
    },
    orbs: [makeStillOrb("ricochet-target", 4, 1)],
  });
  useZoogiGame.getState().activateLarsAbility();
  const ricochet = useZoogiGame.getState().playerEntity;
  assert.ok(ricochet);
  assert.ok(ricochet.larsRicochetBoost > 1, "Ricochet should arm a homing bounce");
  assert.equal(ricochet.larsAbilityUnlocked, false);

  useZoogiGame.setState({
    playerEntity: {
      ...ricochet,
      zoogi: wraps,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      wrapsAbilityUnlocked: true,
      wrapsBindUntil: 0,
    },
    enemies: [{
      ...cpu,
      position: [2, 0.5, 0],
      velocity: [0.5, 0, 0],
      slowUntil: 0,
      isKnockedOut: false,
      isRespawning: false,
    }],
  });
  useZoogiGame.getState().activateWrapsAbility();
  const bound = useZoogiGame.getState();
  assert.ok((bound.playerEntity?.wrapsBindUntil ?? 0) > Date.now(), "Bind should arm contact slow");
  assert.ok((bound.enemies[0]?.slowUntil ?? 0) > Date.now(), "a nearby opponent should be slowed");
  assert.ok(planarSpeed(bound.enemies[0]?.velocity ?? [0, 0, 0]) < 0.5, "Bind should cut the opponent's speed");
});

test("an opponent past the ring is knocked out", async () => {
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    gameTimer: 200,
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 21 },
    playerEntity: {
      ...player,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
      spawnImmunity: false,
      invulnerableUntil: null,
    },
    enemies: [{
      ...player,
      id: "edge-enemy",
      isPlayer: false,
      position: [30, 0.5, 0],
      velocity: [0.2, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
      spawnImmunity: false,
      invulnerableUntil: null,
      score: 40,
    }],
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const knocked = useZoogiGame.getState().enemies[0];
  assert.ok(knocked);
  assert.equal(knocked.isKnockedOut, true, "a character at x=30 should leave play when the ring is 21");
  assert.equal(knocked.score, 0, "falling off should cost the opponent 75, floored at 0");
});

test("hotstreak pushes player 1 when another local player casts it", async () => {
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  const hotstreak = (await loadGame()).ZOOGI_ROSTER.find((zoogi) => zoogi.id === "hotstreak");
  assert.ok(hotstreak);
  useZoogiGame.setState({
    gameMode: "local_multiplayer",
    currentLocalPlayerIndex: 1,
    phase: "playing",
    playerEntity: {
      ...player,
      id: "player-1",
      position: [3, 0.5, 0],
      velocity: [0, 0, 0],
    },
    enemies: [{
      ...player,
      id: "player-2",
      isPlayer: false,
      zoogi: hotstreak,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      hotstreakAbilityUnlocked: true,
    }],
  });
  useZoogiGame.getState().activateHotstreakAbility();
  const blasted = useZoogiGame.getState();
  assert.ok((blasted.playerEntity?.velocity[0] ?? 0) > 0.4, "player 1 should be pushed by the explosion");
  assert.equal(blasted.enemies[0]?.velocity[0], 0, "the caster should stay put");
  assert.equal(blasted.enemies[0]?.hotstreakAbilityUnlocked, false);
});

test("a stun lasts until the next turn is skipped", async () => {
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    gameMode: "classic",
    phase: "playing",
    gameTimer: 200,
    isPlayerTurn: true,
    turnIndex: 0,
    playerEntity: {
      ...player,
      isStunned: false,
      stunTimer: 0,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
    },
    enemies: [{
      ...player,
      id: "stunned-enemy",
      isPlayer: false,
      isStunned: true,
      stunTimer: 2,
      position: [3, 0.5, 0],
      velocity: [0, 0, 0],
    }],
  });
  useZoogiGame.getState().tickTimers(3);
  assert.equal(useZoogiGame.getState().enemies[0]?.isStunned, true, "three real seconds should not clear a stun");
  await new Promise((resolve) => setTimeout(resolve, 500));
  useZoogiGame.getState().endTurn();
  const skipped = useZoogiGame.getState();
  assert.equal(skipped.isPlayerTurn, true, "the stunned opponent's turn should be skipped");
  assert.equal(skipped.enemies[0]?.isStunned, false, "skipping the turn should use up the stun");
});

test("practice mode places star orbs", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ orbMultiplier: 1, selectedCustomZoogi: null });
  useZoogiGame.getState().startPracticeGame();
  const stars = useZoogiGame.getState().orbs.filter((orb) => orb.isStarOrb);
  assert.equal(stars.length, 3, "practice should include the same three star orbs as a classic match");
  const kinds = new Set(stars.map((orb) => orb.starOrbType));
  assert.equal(kinds.size, 3);
});

test("a launched flick ends the turn when the marble stops, even if no render saw the slowdown", async () => {
  await new Promise((resolve) => setTimeout(resolve, 500));
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    gameMode: "classic",
    isPlayerTurn: true,
    turnIndex: 0,
    turnHasLaunched: false,
    gameTimer: 200,
    orbs: [],
    mushrooms: [],
    pinballBumpers: [],
    zoneEditorConfigs: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 50 },
    playerEntity: {
      ...player,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
      spawnImmunity: false,
    },
    enemies: [{
      ...player,
      id: "waiting-cpu",
      isPlayer: false,
      position: [4, 0.5, 0],
      velocity: [0, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
    }],
  });

  useZoogiGame.getState().physicsTick(1 / 60);
  assert.equal(useZoogiGame.getState().isPlayerTurn, true, "a marble that has not been launched should keep the turn");

  useZoogiGame.getState().updatePlayerVelocity([0.2, 0, 0]);
  assert.equal(useZoogiGame.getState().turnHasLaunched, true);

  let handedOff = false;
  for (let frame = 0; frame < 120; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    if (!useZoogiGame.getState().isPlayerTurn) {
      handedOff = true;
      break;
    }
  }
  const settled = useZoogiGame.getState();
  assert.equal(handedOff, true, "the turn should pass to the computer once the flick stops");
  assert.equal(settled.turnHasLaunched, false);
  assert.equal(planarSpeed(settled.playerEntity?.velocity ?? [1, 0, 0]), 0);

  await new Promise((resolve) => setTimeout(resolve, 500));
  useZoogiGame.getState().updateEnemy("waiting-cpu", { velocity: [0.2, 0, 0] });
  assert.equal(useZoogiGame.getState().turnHasLaunched, true, "the computer's launch should arm the same turn-end");
  let playerTurnAgain = false;
  for (let frame = 0; frame < 120; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    if (useZoogiGame.getState().isPlayerTurn) {
      playerTurnAgain = true;
      break;
    }
  }
  assert.equal(playerTurnAgain, true, "the computer's turn should end when its marble stops");
});

test("a computer marble that is only coasting does not skip its shot, and a later roll does end the turn", async () => {
  await new Promise((resolve) => setTimeout(resolve, 500));
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  const enemy = {
    ...player,
    id: "coast-cpu",
    isPlayer: false,
    position: [4, 0.5, 0] as [number, number, number],
    velocity: [0.35, 0, 0] as [number, number, number],
    isKnockedOut: false,
    isRespawning: false,
  };
  useZoogiGame.setState({
    phase: "playing",
    gameMode: "classic",
    currentRound: 4,
    isPlayerTurn: false,
    turnIndex: 0,
    turnHasLaunched: false,
    gameTimer: 200,
    orbs: [],
    mushrooms: [],
    pinballBumpers: [],
    zoneEditorConfigs: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 50 },
    playerEntity: { ...player, position: [0, 0.5, 0], velocity: [0, 0, 0], isKnockedOut: false, isRespawning: false },
    enemies: [enemy],
  });

  for (let frame = 0; frame < 80; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  const coasted = useZoogiGame.getState();
  assert.equal(coasted.isPlayerTurn, false, "sliding in from the last hit should not use up the computer's turn");
  assert.equal(planarSpeed(coasted.enemies[0]?.velocity ?? [1, 0, 0]), 0);

  useZoogiGame.setState({
    enemies: coasted.enemies.map((marble) => ({ ...marble, velocity: [0.3, 0, 0] as [number, number, number] })),
  });
  let handedBack = false;
  for (let frame = 0; frame < 100; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    if (useZoogiGame.getState().isPlayerTurn) {
      handedBack = true;
      break;
    }
  }
  assert.equal(handedBack, true, "once the computer actually rolls, stopping should hand the turn back");
});

test("a stop frame cannot end the turn twice, so the computer still gets to roll", async () => {
  await new Promise((resolve) => setTimeout(resolve, 500));
  const { stopFrameMayEndTurn } = await loadGame();
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  const enemyId = "handoff-cpu";
  useZoogiGame.setState({
    phase: "playing",
    gameMode: "classic",
    isPlayerTurn: true,
    turnIndex: 0,
    turnHasLaunched: false,
    gameTimer: 200,
    orbs: [],
    mushrooms: [],
    pinballBumpers: [],
    zoneEditorConfigs: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 50 },
    playerEntity: {
      ...player,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
      spawnImmunity: false,
    },
    enemies: [{
      ...player,
      id: enemyId,
      isPlayer: false,
      position: [4, 0.5, 0],
      velocity: [0, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
    }],
  });

  const repeatStopFrames = (whoJustStopped: "player" | "enemy") => {
    for (let frame = 0; frame < 8; frame++) {
      const live = useZoogiGame.getState();
      if (stopFrameMayEndTurn(live, "player")) live.endTurn();
      if (stopFrameMayEndTurn(live, "enemy", 0)) live.endTurn();
      useZoogiGame.getState().physicsTick(1 / 60);
    }
    const after = useZoogiGame.getState();
    if (whoJustStopped === "player") {
      assert.equal(after.isPlayerTurn, false, "late player stop-frames must leave the computer up");
    } else {
      assert.equal(after.isPlayerTurn, true, "late computer stop-frames must leave the player up");
    }
    assert.equal(after.turnHasLaunched, false, "the handoff clears the launch flag");
    assert.equal(stopFrameMayEndTurn(after, whoJustStopped, 0), false);
  };

  useZoogiGame.getState().updatePlayerVelocity([0.25, 0, 0]);
  assert.equal(useZoogiGame.getState().turnHasLaunched, true);
  for (let frame = 0; frame < 120 && useZoogiGame.getState().isPlayerTurn; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  assert.equal(useZoogiGame.getState().isPlayerTurn, false, "the player's stop hands the turn to the computer");
  repeatStopFrames("player");

  await new Promise((resolve) => setTimeout(resolve, 500));
  useZoogiGame.getState().updateEnemy(enemyId, { velocity: [0.25, 0, 0] });
  assert.equal(useZoogiGame.getState().turnHasLaunched, true, "the computer's launch arms its own turn");
  assert.equal(useZoogiGame.getState().isPlayerTurn, false);
  for (let frame = 0; frame < 120 && !useZoogiGame.getState().isPlayerTurn; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  assert.equal(useZoogiGame.getState().isPlayerTurn, true, "the computer's stop hands the turn back");
  repeatStopFrames("enemy");
});

test("a tied final round is not a loss, and earlier wins stay a victory", async () => {
  await new Promise((resolve) => setTimeout(resolve, 500));
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  const enemy = {
    ...player,
    id: "tied-cpu",
    isPlayer: false,
    score: 0,
  };
  useZoogiGame.setState({
    phase: "playing",
    gameMode: "classic",
    currentRound: 3,
    maxRounds: 3,
    score: 0,
    gameTimer: 0.4,
    isVictory: false,
    playerRoundWins: 0,
    enemyRoundWins: new Map<string, number>(),
    playerEntity: { ...player, score: 0 },
    enemies: [enemy],
  });
  useZoogiGame.getState().tickTimers(1);
  const allTied = useZoogiGame.getState();
  assert.equal(allTied.phase, "game_over");
  assert.equal(allTied.isVictory, true, "a 0–0 match should not show a defeat");
  assert.equal(allTied.playerRoundWins, 1, "the tied round still goes to the player");

  useZoogiGame.setState({
    phase: "playing",
    currentRound: 3,
    maxRounds: 3,
    score: 0,
    gameTimer: 0.4,
    isVictory: false,
    playerRoundWins: 2,
    enemyRoundWins: new Map<string, number>(),
    enemies: [{ ...enemy, score: 0 }],
  });
  useZoogiGame.getState().tickTimers(1);
  const ahead = useZoogiGame.getState();
  const aheadEnemyWins = Math.max(...Array.from(ahead.enemyRoundWins.values()), 0);
  assert.equal(ahead.phase, "game_over");
  assert.equal(ahead.isVictory, true);
  assert.equal(ahead.playerRoundWins, 3);
  assert.ok(ahead.playerRoundWins > aheadEnemyWins, "two earlier wins plus a 0–0 last round is a victory, not a match tie");

  useZoogiGame.setState({
    phase: "playing",
    currentRound: 3,
    maxRounds: 3,
    score: 10,
    gameTimer: 0.4,
    isVictory: false,
    playerRoundWins: 0,
    enemyRoundWins: new Map<string, number>(),
    enemies: [{ ...enemy, score: 40 }],
  });
  useZoogiGame.getState().tickTimers(1);
  const lost = useZoogiGame.getState();
  assert.equal(lost.phase, "game_over");
  assert.equal(lost.isVictory, false, "a match the opponent won on rounds should still be a loss");

  useZoogiGame.setState({
    phase: "playing",
    currentRound: 3,
    maxRounds: 3,
    score: 0,
    gameTimer: 0.4,
    isVictory: false,
    playerRoundWins: 1,
    enemyRoundWins: new Map<string, number>(),
    enemies: [{ ...enemy, score: 40 }],
  });
  useZoogiGame.getState().tickTimers(1);
  const split = useZoogiGame.getState();
  const splitEnemyWins = Math.max(...Array.from(split.enemyRoundWins.values()), 0);
  assert.equal(split.phase, "game_over");
  assert.equal(split.isVictory, true, "an even round total is not a defeat");
  assert.equal(split.playerRoundWins, splitEnemyWins, "the tie screen is for an even match, not a tied last round");
});

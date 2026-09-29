import test from "node:test";
import assert from "node:assert/strict";
import { endCardResult } from "./matchResult.ts";

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

test("one simulation: launch, collision, and no ring score", async () => {
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
  assert.equal(scored.score, 0, "stopping in a painted zone does not collect an orb");
  assert.equal(scored.phase, "playing", "a resting orb should not end the round");
  assert.equal(scored.orbs[0]?.isActive, true);
  assert.equal(scored.orbs[0]?.capturedInZone ?? null, null);

  await new Promise((resolve) => setTimeout(resolve, 500));
  useZoogiGame.getState().endTurn();
  assert.equal(useZoogiGame.getState().score, 0, "standing in a score ring does not award points");
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
      velocity: [0.22, 0, 0],
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
  const nightshade = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "nightshade");
  assert.ok(lars && wraps && nightshade, "Lars, Wraps, and Nightshade should be on the roster");
  assert.equal(nightshade.ability, "Shadow Stun");
  assert.equal(nightshade.color, "#6B46C1");

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
  assert.equal(fell.isPlayerTurn, true, "the turn waits until the fall finishes and the court settles");
  assert.equal(fell.playerEntity?.isKnockedOut, false, "the marble stays in view while it falls");
  assert.ok((fell.playerEntity?.position[1] ?? 1) < 0.5, "gravity should pull the marble down");
  assert.ok((fell.playerEntity?.velocity[0] ?? 0) > 0, "leaving the edge should keep the shot's speed");

  const back = fell.playerEntity;
  assert.ok(back);
  useZoogiGame.setState({
    playerEntity: {
      ...back,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      isKnockedOut: false,
      isRespawning: false,
      offTheFloor: false,
    },
  });

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
  assert.equal(ko.enemies[0]?.isKnockedOut, false, "the opponent falls before they leave play");
  assert.equal(ko.enemies[0]?.offTheFloor, true);

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
  useZoogiGame.getState().activateWolfgangAbility("player");
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
  useZoogiGame.getState().activateLarsAbility("player");
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
  useZoogiGame.getState().activateWrapsAbility("player");
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
  assert.equal(knocked.offTheFloor, true, "a character at x=30 should leave the floor when the ring is 21");
  assert.equal(knocked.isKnockedOut, false, "the fall stays visible before the body is removed");
  assert.ok(knocked.position[1] < 0.5);
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
  useZoogiGame.getState().activateHotstreakAbility("player-2");
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

test("an AI opponent casts an unlocked power", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  const hotstreak = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "hotstreak");
  assert.ok(hotstreak);
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER.find((zoogi) => zoogi.id === "lars") ?? ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", zoneEditorConfigs: [], phase: "menu" });
  useZoogiGame.getState().startGame();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    isPlayerTurn: false,
    turnIndex: 0,
    orbs: [],
    abilityNotice: null,
    showExplosion: null,
    playerEntity: {
      ...player,
      position: [3, 0.5, 0],
      velocity: [0, 0, 0],
      isStunned: false,
    },
    enemies: [{
      ...player,
      id: "ai-hotstreak",
      isPlayer: false,
      zoogi: hotstreak,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      hotstreakAbilityUnlocked: true,
      isStunned: false,
      isKnockedOut: false,
    }],
  });
  const cast = useZoogiGame.getState().useAiPower("ai-hotstreak");
  assert.equal(cast, "hotstreak", "the AI should cast Explosion when a foe is inside the blast");
  const after = useZoogiGame.getState();
  assert.equal(after.enemies[0]?.hotstreakAbilityUnlocked, false, "the power stays one-shot");
  assert.equal(after.abilityNotice?.text, "Explosion!");
  assert.ok(after.showExplosion, "the match should show the blast");
  assert.ok((after.playerEntity?.velocity[0] ?? 0) > 0.4, "the blast should push the human");
  assert.equal(useZoogiGame.getState().useAiPower("ai-hotstreak"), null, "a spent power cannot be cast again");
});

test("an AI opponent cannot cast a locked power", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  const bolt = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "bolt");
  assert.ok(bolt);
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", zoneEditorConfigs: [], phase: "menu" });
  useZoogiGame.getState().startGame();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    orbs: [makeStillOrb("nearby-orb", 2, 0)],
    playerEntity: {
      ...player,
      position: [3, 0.5, 0],
      velocity: [0, 0, 0],
      isStunned: false,
    },
    enemies: [{
      ...player,
      id: "ai-bolt-locked",
      isPlayer: false,
      zoogi: bolt,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      boltAbilityUnlocked: false,
      isStunned: false,
      isKnockedOut: false,
    }],
  });
  assert.equal(useZoogiGame.getState().useAiPower("ai-bolt-locked"), null);
  const after = useZoogiGame.getState();
  assert.equal(after.playerEntity?.isStunned, false, "a locked shock should not stun the human");
  assert.equal(after.playerEntity?.velocity[0], 0);
  assert.equal(after.enemies[0]?.boltAbilityUnlocked, false);
  assert.equal(after.showExplosion, null);
});

test("a power cast by the AI stuns the human", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  const bolt = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "bolt");
  assert.ok(bolt);
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER.find((zoogi) => zoogi.id === "wraps") ?? ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", zoneEditorConfigs: [], phase: "menu" });
  useZoogiGame.getState().startGame();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    isPlayerTurn: false,
    turnIndex: 0,
    orbs: [],
    playerEntity: {
      ...player,
      position: [3, 0.5, 0],
      velocity: [0, 0, 0],
      isStunned: false,
      stunTimer: 0,
    },
    enemies: [{
      ...player,
      id: "ai-bolt",
      isPlayer: false,
      zoogi: bolt,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      boltAbilityUnlocked: true,
      isStunned: false,
      isKnockedOut: false,
    }],
  });
  const cast = useZoogiGame.getState().useAiPower("ai-bolt");
  assert.equal(cast, "bolt");
  const shocked = useZoogiGame.getState();
  assert.equal(shocked.playerEntity?.isStunned, true, "Shock should stun the human");
  assert.ok((shocked.playerEntity?.stunTimer ?? 0) > 0);
  assert.equal(shocked.enemies[0]?.boltAbilityUnlocked, false);
  assert.equal(shocked.enemies[0]?.isStunned, false, "the caster should not stun itself");
});

test("shadow stun freezes a nearby opponent and leaves a distant one rolling", async () => {
  const { useZoogiGame, ZOOGI_ROSTER, SHADOW_STUN_RADIUS } = await loadGame();
  const nightshade = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "nightshade");
  assert.ok(nightshade);
  useZoogiGame.getState().selectZoogi(nightshade);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", zoneEditorConfigs: [], phase: "menu" });
  useZoogiGame.getState().startGame();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    orbs: [],
    showExplosion: null,
    playerEntity: {
      ...player,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      nightshadeAbilityUnlocked: true,
      isStunned: false,
      stunTimer: 0,
    },
    enemies: [
      {
        ...player,
        id: "near",
        isPlayer: false,
        zoogi: ZOOGI_ROSTER[0],
        position: [2, 0.5, 0],
        velocity: [0.4, 0, 0.2],
        isStunned: false,
        stunTimer: 0,
        isKnockedOut: false,
      },
      {
        ...player,
        id: "far",
        isPlayer: false,
        zoogi: ZOOGI_ROSTER[1],
        position: [SHADOW_STUN_RADIUS + 3, 0.5, 0],
        velocity: [0.3, 0, 0],
        isStunned: false,
        stunTimer: 0,
        isKnockedOut: false,
      },
    ],
  });
  useZoogiGame.getState().activateNightshadeAbility("player");
  const after = useZoogiGame.getState();
  assert.equal(after.playerEntity?.nightshadeAbilityUnlocked, false, "Shadow Stun is one shot");
  assert.equal(after.playerEntity?.isStunned, false, "the caster should not freeze himself");
  const near = after.enemies.find((enemy) => enemy.id === "near");
  const far = after.enemies.find((enemy) => enemy.id === "far");
  assert.ok(near && far);
  assert.equal(near.isStunned, true);
  assert.equal(near.stunTimer, 2, "the freeze skips the next turn, same as Bolt");
  assert.equal(near.velocity[0], 0);
  assert.equal(near.velocity[2], 0);
  assert.equal(far.isStunned, false);
  assert.equal(far.velocity[0], 0.3);
  assert.equal(after.showExplosion?.color, "shadow");
  assert.equal(after.showExplosion?.radius, SHADOW_STUN_RADIUS);
  assert.equal(useZoogiGame.getState().activateNightshadeAbility("player"), undefined);
  assert.equal(useZoogiGame.getState().enemies.find((enemy) => enemy.id === "far")?.isStunned, false);
});

test("an AI Nightshade only casts Shadow Stun when an opponent is inside the pulse", async () => {
  const { useZoogiGame, ZOOGI_ROSTER, SHADOW_STUN_RADIUS } = await loadGame();
  const nightshade = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "nightshade");
  assert.ok(nightshade);
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", zoneEditorConfigs: [], phase: "menu" });
  useZoogiGame.getState().startGame();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    isPlayerTurn: false,
    turnIndex: 0,
    orbs: [],
    playerEntity: {
      ...player,
      position: [SHADOW_STUN_RADIUS + 2, 0.5, 0],
      velocity: [0.2, 0, 0],
      isStunned: false,
      stunTimer: 0,
    },
    enemies: [{
      ...player,
      id: "ai-nightshade",
      isPlayer: false,
      zoogi: nightshade,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      nightshadeAbilityUnlocked: true,
      isStunned: false,
      isKnockedOut: false,
    }],
  });
  assert.equal(useZoogiGame.getState().useAiPower("ai-nightshade"), null, "a far opponent is out of the pulse");
  assert.equal(useZoogiGame.getState().enemies[0]?.nightshadeAbilityUnlocked, true);

  useZoogiGame.setState({
    playerEntity: {
      ...useZoogiGame.getState().playerEntity!,
      position: [2, 0.5, 0],
      velocity: [0.5, 0, 0],
      isStunned: false,
    },
    enemies: [{
      ...useZoogiGame.getState().enemies[0],
      nightshadeAbilityUnlocked: false,
    }],
  });
  assert.equal(useZoogiGame.getState().useAiPower("ai-nightshade"), null, "a locked pulse should not fire");
  assert.equal(useZoogiGame.getState().playerEntity?.isStunned, false);

  useZoogiGame.setState({
    enemies: [{
      ...useZoogiGame.getState().enemies[0],
      nightshadeAbilityUnlocked: true,
    }],
  });
  assert.equal(useZoogiGame.getState().useAiPower("ai-nightshade"), "nightshade");
  const frozen = useZoogiGame.getState();
  assert.equal(frozen.playerEntity?.isStunned, true);
  assert.equal(frozen.playerEntity?.velocity[0], 0);
  assert.equal(frozen.enemies[0]?.nightshadeAbilityUnlocked, false);
  assert.equal(frozen.enemies[0]?.isStunned, false);
});

test("a rolling AI blast stays centered on the caster", async () => {
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  const hotstreak = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "hotstreak");
  assert.ok(hotstreak);
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER.find((zoogi) => zoogi.id === "lars") ?? ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", zoneEditorConfigs: [], phase: "menu" });
  useZoogiGame.getState().startGame();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  const castAt: [number, number, number] = [0, 0.5, 16];
  useZoogiGame.setState({
    phase: "playing",
    isPlayerTurn: false,
    turnIndex: 0,
    orbs: [],
    abilityNotice: null,
    showExplosion: null,
    playerEntity: {
      ...player,
      position: [0, 0.5, 10],
      velocity: [0, 0, 0],
      isStunned: false,
    },
    enemies: [{
      ...player,
      id: "ai-hotstreak",
      isPlayer: false,
      zoogi: hotstreak,
      position: castAt,
      velocity: [0.45, 0, 0.2],
      hotstreakAbilityUnlocked: true,
      isStunned: false,
      isKnockedOut: false,
    }],
  });
  const cast = useZoogiGame.getState().useAiPower("ai-hotstreak");
  assert.equal(cast, "hotstreak");
  const after = useZoogiGame.getState();
  const blast = after.showExplosion?.position;
  assert.ok(blast, "the blast should be visible");
  assert.ok(Math.hypot(blast[0] - castAt[0], blast[2] - castAt[2]) < 0.001, "the blast stays on the rolling caster");
  assert.ok((after.playerEntity?.velocity[2] ?? 0) < -0.2, "the push uses that same center, back toward the ring");
  assert.ok(Math.hypot(after.enemies[0]?.velocity[0] ?? 0, after.enemies[0]?.velocity[2] ?? 0) > 0.4, "the caster keeps its roll");
});

test("an AI hotstreak blast during a roll still hands the turn back when the marble stops", async () => {
  await new Promise((resolve) => setTimeout(resolve, 500));
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  const hotstreak = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "hotstreak");
  assert.ok(hotstreak);
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER.find((zoogi) => zoogi.id === "lars") ?? ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", zoneEditorConfigs: [], phase: "menu" });
  useZoogiGame.getState().startGame();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    gameMode: "classic",
    currentRound: 8,
    isPlayerTurn: false,
    turnIndex: 0,
    turnHasLaunched: false,
    gameTimer: 200,
    orbs: [],
    mushrooms: [],
    pinballBumpers: [],
    zoneEditorConfigs: [],
    showExplosion: null,
    wolfClones: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 50 },
    playerEntity: {
      ...player,
      position: [5, 0.5, 0],
      velocity: [0, 0, 0],
      isStunned: false,
      isKnockedOut: false,
      isRespawning: false,
    },
    enemies: [{
      ...player,
      id: "ai-hotstreak",
      isPlayer: false,
      zoogi: hotstreak,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      hotstreakAbilityUnlocked: true,
      isStunned: false,
      isKnockedOut: false,
      isRespawning: false,
    }],
  });

  useZoogiGame.getState().physicsTick(1 / 60);
  assert.equal(useZoogiGame.getState().isPlayerTurn, false, "a computer marble that has not rolled yet should keep the turn");

  useZoogiGame.getState().updateEnemy("ai-hotstreak", { velocity: [0.28, 0, 0] });
  assert.equal(useZoogiGame.getState().turnHasLaunched, true);
  useZoogiGame.getState().physicsTick(1 / 60);

  const cast = useZoogiGame.getState().useAiPower("ai-hotstreak");
  assert.equal(cast, "hotstreak");
  const blasted = useZoogiGame.getState();
  assert.equal(blasted.isPlayerTurn, false, "the blast should not end the computer's turn by itself");
  assert.ok(blasted.showExplosion, "the blast should be visible");
  assert.equal(blasted.turnHasLaunched, true, "the roll that was already in motion should stay armed");

  let handedBack = false;
  for (let frame = 0; frame < 800; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    if (useZoogiGame.getState().isPlayerTurn) {
      handedBack = true;
      break;
    }
  }
  assert.equal(handedBack, true, "the computer's turn should pass once the blasted roll stops");
});

test("an AI wolf pack mid-roll still ends the turn, and the next round clears those clones", async () => {
  await new Promise((resolve) => setTimeout(resolve, 500));
  const { useZoogiGame, ZOOGI_ROSTER } = await loadGame();
  const wolfgang = ZOOGI_ROSTER.find((zoogi) => zoogi.id === "wolfgang");
  assert.ok(wolfgang);
  useZoogiGame.getState().selectZoogi(ZOOGI_ROSTER.find((zoogi) => zoogi.id === "lars") ?? ZOOGI_ROSTER[0]);
  useZoogiGame.setState({ aiPlayerCount: 1, gameMode: "classic", zoneEditorConfigs: [], phase: "menu" });
  useZoogiGame.getState().startGame();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);
  useZoogiGame.setState({
    phase: "playing",
    gameMode: "classic",
    currentRound: 2,
    maxRounds: 5,
    score: 0,
    isPlayerTurn: false,
    turnIndex: 0,
    turnHasLaunched: false,
    gameTimer: 200,
    orbs: [],
    mushrooms: [],
    pinballBumpers: [],
    zoneEditorConfigs: [],
    wolfClones: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 50 },
    playerEntity: {
      ...player,
      position: [0, 0.5, 6],
      velocity: [0, 0, 0],
      score: 0,
      isKnockedOut: false,
      isRespawning: false,
    },
    enemies: [{
      ...player,
      id: "ai-wolf",
      isPlayer: false,
      zoogi: wolfgang,
      position: [0, 0.5, 0],
      velocity: [0, 0, 0],
      score: 0,
      wolfgangAbilityUnlocked: true,
      isKnockedOut: false,
      isRespawning: false,
    }],
  });

  useZoogiGame.getState().physicsTick(1 / 60);
  assert.equal(useZoogiGame.getState().isPlayerTurn, false);

  useZoogiGame.getState().updateEnemy("ai-wolf", { velocity: [0.3, 0, 0] });
  useZoogiGame.getState().physicsTick(1 / 60);
  const cast = useZoogiGame.getState().useAiPower("ai-wolf");
  assert.equal(cast, "wolfgang");
  const packed = useZoogiGame.getState();
  assert.equal(packed.isPlayerTurn, false, "spawning the pack should not end the computer's turn");
  assert.equal(packed.wolfClones.length, 3);
  assert.ok(packed.wolfClones.every((clone) => clone.spawnedByPlayerId === "ai-wolf"));
  const spawned = packed.wolfClones.map((clone) => ({ ...clone, isActive: true }));

  let handedBack = false;
  for (let frame = 0; frame < 800; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    if (useZoogiGame.getState().isPlayerTurn) {
      handedBack = true;
      break;
    }
  }
  assert.equal(handedBack, true, "the computer's turn should pass once the pack roll stops");

  useZoogiGame.setState({ wolfClones: spawned });
  useZoogiGame.getState().startNextRound();
  const next = useZoogiGame.getState();
  assert.equal(next.wolfClones.length, 0, "clones an AI spawned should be gone when the next round starts");
  assert.equal(next.currentRound, 3);
  assert.equal(next.phase, "playing");
});

test("an off-screen star burst anchors on the unlocking marble", async () => {
  const { resolveUnlockSpot } = await loadGame();
  const exit: [number, number, number] = [0, 0.7, 24];
  const marble: [number, number, number] = [2, 0.5, 8];
  assert.deepEqual(resolveUnlockSpot(exit, marble, true), exit);
  assert.deepEqual(resolveUnlockSpot(exit, marble, false), marble);
  assert.deepEqual(resolveUnlockSpot(exit, null, false), exit);
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
  for (let frame = 0; frame < 800; frame++) {
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
  for (let frame = 0; frame < 800; frame++) {
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
    velocity: [0.12, 0, 0] as [number, number, number],
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

  for (let frame = 0; frame < 500; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  const coasted = useZoogiGame.getState();
  assert.equal(coasted.isPlayerTurn, false, "sliding in from the last hit should not use up the computer's turn");
  assert.equal(planarSpeed(coasted.enemies[0]?.velocity ?? [1, 0, 0]), 0);

  useZoogiGame.setState({
    enemies: coasted.enemies.map((marble) => ({ ...marble, velocity: [0.2, 0, 0] as [number, number, number] })),
  });
  let handedBack = false;
  for (let frame = 0; frame < 800; frame++) {
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
  for (let frame = 0; frame < 800 && useZoogiGame.getState().isPlayerTurn; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  assert.equal(useZoogiGame.getState().isPlayerTurn, false, "the player's stop hands the turn to the computer");
  repeatStopFrames("player");

  await new Promise((resolve) => setTimeout(resolve, 500));
  useZoogiGame.getState().updateEnemy(enemyId, { velocity: [0.25, 0, 0] });
  assert.equal(useZoogiGame.getState().turnHasLaunched, true, "the computer's launch arms its own turn");
  assert.equal(useZoogiGame.getState().isPlayerTurn, false);
  for (let frame = 0; frame < 800 && !useZoogiGame.getState().isPlayerTurn; frame++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  assert.equal(useZoogiGame.getState().isPlayerTurn, true, "the computer's stop hands the turn back");
  repeatStopFrames("enemy");
});

function cardFor(state: {
  isVictory: boolean;
  playerRoundWins: number;
  enemyRoundWins: Map<string, number>;
  enemies: { id: string; zoogi?: { name?: string } }[];
}) {
  return endCardResult({
    isVictory: state.isVictory,
    playerRoundWins: state.playerRoundWins,
    enemyRoundWins: state.enemyRoundWins,
    opponents: state.enemies.map((enemy) => ({
      id: enemy.id,
      name: enemy.zoogi?.name || "Computer",
    })),
  });
}

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
  assert.equal(allTied.score, 0, "the last round can end on 0 points");
  const firstRoundCard = cardFor(allTied);
  assert.equal(firstRoundCard.headline, "VICTORY!");
  assert.equal(firstRoundCard.tally, "1 to 0");
  assert.notEqual(firstRoundCard.playerRounds, allTied.score);

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
  assert.equal(ahead.score, 0);
  const sweepCard = cardFor(ahead);
  assert.equal(sweepCard.kind, "victory");
  assert.equal(sweepCard.headline, "VICTORY!");
  assert.equal(sweepCard.tally, "3 to 0");
  assert.equal(sweepCard.playerRounds, 3);
  assert.equal(sweepCard.opponentRounds, 0);

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
  assert.equal(lost.score, 10, "the loss card must not treat this last-round total as the match");
  const lostCard = cardFor(lost);
  assert.equal(lostCard.kind, "defeat");
  assert.equal(lostCard.headline, "GAME OVER");
  assert.equal(lostCard.tally, "0 to 1");
  assert.notEqual(lostCard.playerRounds, lost.score);
  assert.notEqual(lostCard.tally, String(lost.score));

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
  assert.equal(split.score, 0);
  const tieCard = cardFor(split);
  assert.equal(tieCard.kind, "tie");
  assert.equal(tieCard.headline, "IT'S A TIE!");
  assert.equal(tieCard.tally, "1 to 1");
  assert.equal(tieCard.playerRounds, 1);
  assert.equal(tieCard.opponentRounds, 1);

  useZoogiGame.setState({
    phase: "playing",
    currentRound: 3,
    maxRounds: 3,
    score: 10,
    gameTimer: 0.4,
    isVictory: false,
    playerRoundWins: 2,
    enemyRoundWins: new Map<string, number>(),
    enemies: [{ ...enemy, score: 40 }],
  });
  useZoogiGame.getState().tickTimers(1);
  const splitWin = useZoogiGame.getState();
  const splitWinEnemy = Math.max(...Array.from(splitWin.enemyRoundWins.values()), 0);
  assert.equal(splitWin.phase, "game_over");
  assert.equal(splitWin.isVictory, true, "losing the last round does not erase two earlier wins");
  assert.equal(splitWin.playerRoundWins, 2);
  assert.equal(splitWinEnemy, 1);
  assert.equal(splitWin.score, 10, "10 is the last round, not the match");
  const splitWinCard = cardFor(splitWin);
  assert.equal(splitWinCard.kind, "victory");
  assert.equal(splitWinCard.headline, "VICTORY!");
  assert.equal(splitWinCard.tally, "2 to 1");
  assert.equal(splitWinCard.playerRounds, 2);
  assert.equal(splitWinCard.opponentRounds, 1);
  assert.notEqual(splitWinCard.playerRounds, splitWin.score);
  assert.notEqual(splitWinCard.tally, String(splitWin.score));
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
  const { arenaScaleFor } = await import("./arenaScale.ts");
  assert.equal(useZoogiGame.getState().wallSettings.knockoffBoundaryRadius, 15.5 * arenaScaleFor("saturn"));

  useZoogiGame.setState({
    selectedMap: "grass",
    elementTransforms: {
      ...useZoogiGame.getState().elementTransforms,
      knockoffBoundaryOffset: { x: -9.9, y: 0, z: -4.8 },
    },
  });
  useZoogiGame.getState().startGame();
  assert.deepEqual(useZoogiGame.getState().elementTransforms.knockoffBoundaryOffset, { x: 0, y: 0, z: 0 });
  assert.equal(useZoogiGame.getState().wallSettings.knockoffBoundaryRadius, 15.5 * arenaScaleFor("grass"));

  for (const mapId of ["grass", "ice", "saturn"] as const) {
    useZoogiGame.setState({
      selectedMap: mapId,
      elementTransforms: {
        ...useZoogiGame.getState().elementTransforms,
        knockoffBoundaryOffset: { x: -9.9, y: 0, z: -4.8 },
      },
    });
    useZoogiGame.getState().startGame();
    const state = useZoogiGame.getState();
    const scale = arenaScaleFor(mapId);
    assert.deepEqual(state.elementTransforms.knockoffBoundaryOffset, { x: 0, y: 0, z: 0 }, mapId);
    assert.equal(state.wallSettings.knockoffBoundaryRadius, 15.5 * scale, mapId);
    const spawns = state.zoneEditorConfigs.filter((zone) => zone.isSpawn);
    const scores = state.zoneEditorConfigs.filter((zone) => !zone.isSpawn);
    assert.equal(spawns.length, 4, mapId);
    assert.equal(scores.length, 4, mapId);
    for (const spawn of spawns) {
      assert.equal(spawn.distance, 8 * scale, mapId);
      assert.ok(spawn.distance + 0.5 < 15.2 * scale, `${mapId} spawn leaves the floor`);
    }
    for (const score of scores) {
      assert.equal(score.distance, 9.4 * scale, mapId);
      assert.ok(score.distance + 4 <= 15.2 * scale, `${mapId} score zone leaves the floor`);
    }
  }
});

test("frozen ring ice patches coast without speeding a marble up", async () => {
  const { getIcePatches } = await import("./arenaColliders.ts");
  const patch = getIcePatches()[0];
  assert.ok(patch, "Frozen Ring should still draw an ice patch");
  const useZoogiGame = await playingMarble();
  const player = useZoogiGame.getState().playerEntity;
  assert.ok(player);

  const onPatch: [number, number, number] = [patch.x, 0.5, patch.z];
  const onRink: [number, number, number] = [0, 0.5, 0];
  const inwardLen = Math.hypot(patch.x, patch.z) || 1;
  const dirX = -patch.x / inwardLen;
  const dirZ = -patch.z / inwardLen;

  const step = (map: "ice" | "grass", position: [number, number, number], speed: number, asEnemy = false) => {
    const velocity: [number, number, number] = [dirX * speed, 0, dirZ * speed];
    const body = {
      ...player,
      arcMovement: null,
      isStunned: false,
      isKnockedOut: false,
      isRespawning: false,
      spawnImmunity: false,
      slowUntil: 0,
      larsRicochetBoost: 1,
    };
    useZoogiGame.setState({
      phase: "playing",
      selectedMap: map,
      gameMode: "classic",
      isPlayerTurn: true,
      turnHasLaunched: false,
      score: 0,
      orbs: [],
      mushrooms: [],
      pinballBumpers: [],
      landedRocks: [],
      editorPlacedModels: [],
      wolfClones: [],
      zoneEditorConfigs: [],
      playerEntity: asEnemy
        ? { ...body, position: onRink, velocity: [0, 0, 0] }
        : { ...body, position, velocity },
      enemies: asEnemy
        ? [{
          ...body,
          id: "ice-enemy",
          isPlayer: false,
          position,
          velocity,
        }]
        : [],
    });
    useZoogiGame.getState().physicsTick(1 / 60);
  };

  for (const speed of [0.08, 0.2, 0.4, 0.6]) {
    step("ice", onPatch, speed);
    const iced = useZoogiGame.getState().playerEntity;
    assert.ok(iced);
    const iceSpeed = planarSpeed(iced.velocity);
    assert.ok(iceSpeed < speed, `a patch must not add speed: ${speed} -> ${iceSpeed}`);

    step("ice", onRink, speed);
    const rinked = useZoogiGame.getState().playerEntity;
    assert.ok(rinked);
    const rinkSpeed = planarSpeed(rinked.velocity);
    assert.ok(rinkSpeed < speed, `open ice should still slow a marble: ${speed} -> ${rinkSpeed}`);
    if (speed >= 0.2) {
      assert.ok(
        iceSpeed > rinkSpeed,
        `a patch should keep more speed than the rink at ${speed}: patch=${iceSpeed} rink=${rinkSpeed}`,
      );
    }
  }

  step("grass", onPatch, 0.5);
  const grassSpeed = planarSpeed(useZoogiGame.getState().playerEntity?.velocity ?? [0, 0, 0]);
  step("ice", onRink, 0.5);
  const rinkSpeed = planarSpeed(useZoogiGame.getState().playerEntity?.velocity ?? [0, 0, 0]);
  assert.ok(
    Math.abs(grassSpeed - rinkSpeed) < 1e-9,
    `standing on a patch coordinate off Frozen Ring should use normal ground, grass=${grassSpeed} rink=${rinkSpeed}`,
  );

  let patchCoast = 0.5;
  for (let frame = 0; frame < 30; frame++) {
    const before = patchCoast;
    step("ice", onPatch, patchCoast);
    patchCoast = planarSpeed(useZoogiGame.getState().playerEntity?.velocity ?? [0, 0, 0]);
    assert.ok(patchCoast < before, `frame ${frame + 1} on a patch sped up: ${before} -> ${patchCoast}`);
  }
  let rinkCoast = 0.5;
  for (let frame = 0; frame < 30; frame++) {
    step("ice", onRink, rinkCoast);
    rinkCoast = planarSpeed(useZoogiGame.getState().playerEntity?.velocity ?? [0, 0, 0]);
  }
  assert.ok(
    patchCoast > rinkCoast,
    `after the same coast, the patch (${patchCoast}) should still be faster than the rink (${rinkCoast})`,
  );

  step("ice", onPatch, 0.5, true);
  const enemy = useZoogiGame.getState().enemies[0];
  assert.ok(enemy);
  const enemySpeed = planarSpeed(enemy.velocity);
  assert.ok(enemySpeed < 0.5, `an enemy on a patch sped up: ${enemySpeed}`);
  assert.ok(enemySpeed > rinkSpeed, `an enemy on a patch should coast more than the rink: enemy=${enemySpeed} rink=${rinkSpeed}`);
});

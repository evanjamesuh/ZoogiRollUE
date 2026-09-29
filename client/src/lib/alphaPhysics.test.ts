import test from "node:test";
import assert from "node:assert/strict";
import { getMapLayout, resolveSolidCollision, MARBLE_RADIUS, ORB_RADIUS, BUMPER_RADIUS, centerPastOpenEdge, collectMatchSolids } from "./arenaColliders.ts";
import { arenaScaleFor } from "./arenaScale.ts";
import { leftNeonOpenEdge, neonPlayHalfX, neonPlayHalfZ, neonRails } from "./neonCourt.ts";
import { circleTimeOfImpact } from "./sweptHit.ts";
import { ORB_DRAW_RADIUS, ORB_REST_Y, ZOOGI_DIAMETER, ZOOGI_DRAW_RADIUS, ZOOGI_REST_Y } from "./restHeight.ts";
import {
  BUMPER_RESTITUTION,
  FALL_GRAVITY,
  FULL_LAUNCH_SPEED_PER_SEC,
  MARBLE_RESTITUTION,
  MAX_LAUNCH_SPEED,
  MAX_PLANAR_SPEED,
  MOMENTUM_TRANSFER,
  ORB_MASS,
  ZOOGI_MASS,
  NORMAL_LAUNCH_SPEED,
  ROCK_RESTITUTION,
  ROLLING_DECEL,
  SETTLE_DELAY_SECONDS,
  SETTLE_DELAY_STEPS,
  SHOOTER_KEEP,
} from "./simFeel.ts";

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
    selectedMap: "space",
    phase: "menu",
  });
  useZoogiGame.getState().startGame();
  const started = useZoogiGame.getState();
  assert.ok(started.playerEntity);
  assert.ok(started.enemies[0]);
  return { useZoogiGame, player: started.playerEntity, enemy: started.enemies[0] };
}

test("arenas are about 15 to 20 Zoogi diameters across", () => {
  const circles = ["grass", "ice", "lava", "space", "saturn", "tomb"] as const;
  for (const map of circles) {
    const layout = getMapLayout(map);
    assert.ok(layout);
    const diameters = (layout.floorRadius * 2) / ZOOGI_DIAMETER;
    assert.ok(diameters >= 15 && diameters <= 20, `${map} is ${diameters.toFixed(2)} Zoogi diameters`);
  }
  const shortSide = (neonPlayHalfZ() * 2) / ZOOGI_DIAMETER;
  const longSide = (neonPlayHalfX() * 2) / ZOOGI_DIAMETER;
  assert.ok(shortSide >= 15 && shortSide <= 20, `neon short side ${shortSide.toFixed(2)}`);
  assert.ok(longSide > 20 && longSide < 24, `neon long side ${longSide.toFixed(2)}`);
  assert.equal(arenaScaleFor("grass"), 1);
  assert.equal(arenaScaleFor("lava"), 0.836);
  assert.equal(arenaScaleFor("neon"), 1.613);
});

test("a normal shot rolls straight and rests in 2 to 4 seconds", async () => {
  const { useZoogiGame, player } = await playing();
  useZoogiGame.setState({
    phase: "playing",
    currentRound: 41,
    isPlayerTurn: true,
    turnHasLaunched: true,
    firstTickProcessed: true,
    orbs: [],
    enemies: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 80 },
    playerEntity: {
      ...player,
      position: [-12, ZOOGI_REST_Y, 0],
      velocity: [NORMAL_LAUNCH_SPEED, 0, 0],
      arcMovement: null,
      isStunned: false,
      isKnockedOut: false,
      isRespawning: false,
      offTheFloor: false,
      spawnImmunity: false,
      larsRicochetBoost: 1,
    },
  });

  let restStep = -1;
  for (let step = 0; step < 400; step++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    const marble = useZoogiGame.getState().playerEntity;
    assert.ok(marble);
    assert.ok(Math.abs(marble.position[2]) < 0.02, "the shot should stay on a straight line");
    const speed = Math.hypot(marble.velocity[0], marble.velocity[2]);
    if (speed === 0) {
      restStep = step + 1;
      break;
    }
  }
  const rested = useZoogiGame.getState().playerEntity;
  assert.ok(rested);
  const seconds = restStep / 60;
  assert.ok(seconds >= 2 && seconds <= 4, `rested in ${seconds.toFixed(2)}s`);
  assert.equal(rested.isKnockedOut, false);
  assert.equal(rested.offTheFloor, false);
  const traveled = rested.position[0] - (-12);
  assert.ok(traveled > 15 && traveled < 26, `traveled ${traveled.toFixed(2)}`);
  console.log(`MEASURE normalRestSec=${seconds.toFixed(3)} normalTravel=${traveled.toFixed(3)}`);
});

test("full power crosses the arena in under a second and flies off", async () => {
  const { useZoogiGame, player } = await playing();
  const layout = getMapLayout("space");
  assert.ok(layout);
  const startX = -layout.floorRadius + 1.2;
  const diameter = layout.floorRadius * 2;
  useZoogiGame.setState({
    phase: "playing",
    currentRound: 42,
    isPlayerTurn: true,
    turnHasLaunched: true,
    firstTickProcessed: true,
    orbs: [],
    enemies: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    playerEntity: {
      ...player,
      position: [startX, ZOOGI_REST_Y, 0],
      velocity: [MAX_LAUNCH_SPEED, 0, 0],
      arcMovement: null,
      isStunned: false,
      isKnockedOut: false,
      isRespawning: false,
      offTheFloor: false,
      spawnImmunity: false,
      larsRicochetBoost: 1,
    },
  });

  let crossStep = -1;
  for (let step = 0; step < 180; step++) {
    useZoogiGame.getState().physicsTick(1 / 60);
    const marble = useZoogiGame.getState().playerEntity;
    assert.ok(marble);
    if (crossStep < 0 && marble.position[0] - startX >= diameter) crossStep = step + 1;
  }
  const flown = useZoogiGame.getState().playerEntity;
  assert.ok(flown);
  assert.ok(crossStep > 0, "the flick should cross the floor");
  const seconds = crossStep / 60;
  assert.ok(seconds < 1, `crossed in ${seconds.toFixed(3)}s`);
  assert.ok(flown.offTheFloor || flown.isKnockedOut || flown.position[1] < ZOOGI_REST_Y, "an open full shot flies off");
  console.log(`MEASURE fullCrossSec=${seconds.toFixed(3)} fullSpeed=${FULL_LAUNCH_SPEED_PER_SEC} decel=${ROLLING_DECEL}`);
});

test("a head-on hit keeps some shooter speed and gives most of it to the target", async () => {
  const { useZoogiGame, player, enemy } = await playing();
  const incoming = 0.4;
  useZoogiGame.setState({
    phase: "playing",
    currentRound: 43,
    isPlayerTurn: true,
    turnHasLaunched: false,
    firstTickProcessed: true,
    orbs: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 80 },
    playerEntity: {
      ...player,
      position: [0, ZOOGI_REST_Y, 0],
      velocity: [incoming, 0, 0],
      arcMovement: null,
      isStunned: false,
      larsRicochetBoost: 1,
      spawnImmunity: false,
    },
    enemies: [{
      ...enemy,
      position: [MARBLE_RADIUS * 2 + 0.12, ZOOGI_REST_Y, 0],
      velocity: [0, 0, 0],
      arcMovement: null,
      isStunned: false,
      isKnockedOut: false,
      isRespawning: false,
      larsRicochetBoost: 1,
    }],
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const after = useZoogiGame.getState();
  const shooter = after.playerEntity;
  const target = after.enemies[0];
  assert.ok(shooter && target);
  const shooterSpeed = Math.hypot(shooter.velocity[0], shooter.velocity[2]);
  const targetSpeed = Math.hypot(target.velocity[0], target.velocity[2]);
  const transfer = targetSpeed / incoming;
  const kept = shooterSpeed / incoming;
  assert.ok(target.velocity[0] > 0, "the target should be pushed forward");
  assert.ok(shooter.velocity[0] > 0, "the shooter should not stop dead");
  assert.ok(Math.abs(transfer - MOMENTUM_TRANSFER) < 0.08, `transfer ${transfer.toFixed(3)} vs ${MOMENTUM_TRANSFER}`);
  assert.ok(Math.abs(kept - SHOOTER_KEEP) < 0.08, `kept ${kept.toFixed(3)} vs ${SHOOTER_KEEP}`);
  assert.equal(MARBLE_RESTITUTION, 0.75);
  console.log(`MEASURE transfer=${transfer.toFixed(3)} kept=${kept.toFixed(3)}`);
});

test("bumpers kick harder than rocks", () => {
  const approach = 0.5;
  const hit = (restitution: number, radius: number) => {
    const solid = { id: "solid", x: 2, z: 0, radius, kind: "rock" as const };
    const bumper = { ...solid, kind: "bumper" as const };
    const body = restitution === BUMPER_RESTITUTION ? bumper : solid;
    const prev: [number, number, number] = [2 - radius - MARBLE_RADIUS - 0.2, 0.5, 0];
    const next: [number, number, number] = [prev[0] + approach, 0.5, 0];
    const resolved = resolveSolidCollision(prev, next, [approach, 0, 0], MARBLE_RADIUS, [body], restitution === BUMPER_RESTITUTION ? 0.2 : ROCK_RESTITUTION);
    return Math.hypot(resolved.vel[0], resolved.vel[2]);
  };
  const bumperSpeed = hit(BUMPER_RESTITUTION, BUMPER_RADIUS);
  const rockSpeed = hit(ROCK_RESTITUTION, 1.2);
  assert.ok(bumperSpeed > rockSpeed * 1.4, `bumper ${bumperSpeed.toFixed(3)} should kick harder than rock ${rockSpeed.toFixed(3)}`);
  assert.ok(bumperSpeed > approach * 0.9, "a bumper nearly keeps the incoming speed");
  console.log(`MEASURE bumperOut=${bumperSpeed.toFixed(3)} rockOut=${rockSpeed.toFixed(3)}`);
});

test("the turn waits for every body, then the settle delay, and only the next Zoogi returns to the pad", async () => {
  const { useZoogiGame, player, enemy } = await playing();
  const enemyHome = [...enemy.position] as [number, number, number];
  const parkedPlayer: [number, number, number] = [-4, ZOOGI_REST_Y, 2];
  const parkedEnemy: [number, number, number] = [5, ZOOGI_REST_Y, -3];
  const realNow = Date.now;
  const clock = { now: realNow() + 2000 };
  Date.now = () => clock.now;
  try {
    useZoogiGame.setState({
      phase: "playing",
      currentRound: 44,
      isPlayerTurn: true,
      turnIndex: 0,
      turnHasLaunched: true,
      firstTickProcessed: true,
      orbs: [{
        id: "rolling-orb",
        position: [1, ORB_REST_Y, 1],
        velocity: [0.4, 0, 0],
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
      mushrooms: [],
      pinballBumpers: [],
      wolfClones: [],
      wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 80 },
      playerEntity: {
        ...player,
        position: parkedPlayer,
        velocity: [0, 0, 0],
        isKnockedOut: false,
        isRespawning: false,
        offTheFloor: false,
      },
      enemies: [{
        ...enemy,
        position: parkedEnemy,
        velocity: [0, 0, 0],
        isKnockedOut: false,
        isRespawning: false,
        offTheFloor: false,
      }],
    });

    for (let step = 0; step < SETTLE_DELAY_STEPS + 30; step++) {
      useZoogiGame.getState().physicsTick(1 / 60);
    }
    const waiting = useZoogiGame.getState();
    assert.equal(waiting.isPlayerTurn, true, "a rolling orb should hold the turn");
    assert.equal(waiting.playerEntity?.position[0], parkedPlayer[0]);
    assert.equal(waiting.enemies[0]?.position[0], parkedEnemy[0]);

    useZoogiGame.setState({
      orbs: waiting.orbs.map((orb) => ({ ...orb, velocity: [0, 0, 0] as [number, number, number] })),
    });
    for (let step = 0; step < SETTLE_DELAY_STEPS - 1; step++) {
      useZoogiGame.getState().physicsTick(1 / 60);
    }
    assert.equal(useZoogiGame.getState().isPlayerTurn, true, "the settle delay should still be running");
    useZoogiGame.getState().physicsTick(1 / 60);
    const handed = useZoogiGame.getState();
    assert.equal(handed.isPlayerTurn, false, "the turn passes after the delay");
    assert.ok(handed.playerEntity);
    assert.ok(handed.enemies[0]);
    assert.ok(
      Math.hypot(handed.playerEntity.position[0] - parkedPlayer[0], handed.playerEntity.position[2] - parkedPlayer[2]) < 0.05,
      "the shooter stays where they stopped",
    );
    assert.ok(
      Math.hypot(handed.enemies[0].position[0] - enemyHome[0], handed.enemies[0].position[2] - enemyHome[2]) < 0.2,
      "the next Zoogi goes to the start pad",
    );
    assert.equal(handed.playerEntity.isKnockedOut, false);
    assert.equal(handed.enemies[0].isKnockedOut, false);
    console.log(`MEASURE settleSec=${SETTLE_DELAY_SECONDS} settleSteps=${SETTLE_DELAY_STEPS}`);
  } finally {
    Date.now = realNow;
  }
});

test("orbs rest on the floor, stay put when touched, and score only by falling off", async () => {
  const { useZoogiGame, player } = await playing();
  assert.ok(ORB_DRAW_RADIUS < ZOOGI_DRAW_RADIUS);
  assert.ok(ORB_DRAW_RADIUS > ZOOGI_DRAW_RADIUS * 0.65);
  useZoogiGame.setState({
    phase: "playing",
    currentRound: 45,
    score: 0,
    isPlayerTurn: true,
    turnHasLaunched: false,
    firstTickProcessed: true,
    enemies: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 20 },
    playerEntity: {
      ...player,
      position: [0, ZOOGI_REST_Y, 0],
      velocity: [0.2, 0, 0],
      larsRicochetBoost: 1,
      arcMovement: null,
      spawnImmunity: false,
    },
    orbs: [{
      id: "touch-orb",
      position: [MARBLE_RADIUS + ORB_RADIUS + 0.08, ORB_REST_Y, 0],
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
  useZoogiGame.getState().physicsTick(1 / 60);
  const touched = useZoogiGame.getState();
  assert.equal(touched.score, 0, "touching an orb does not score");
  assert.equal(touched.orbs[0]?.isActive, true);
  assert.equal(touched.orbs[0]?.position[1], ORB_REST_Y);

  useZoogiGame.setState({
    score: 0,
    orbs: [{
      ...touched.orbs[0],
      position: [24, ORB_REST_Y, 0],
      velocity: [0.15, 0, 0],
      lastHitBy: "player",
      isOutOfRing: false,
      isActive: true,
    }],
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const falling = useZoogiGame.getState();
  assert.equal(falling.score, 0, "crossing the edge does not score");
  assert.equal(falling.orbs[0]?.isOutOfRing, true);
  assert.equal(falling.orbs[0]?.isActive, true, "the orb is still falling");
  assert.ok((falling.orbs[0]?.position[1] ?? 1) < ORB_REST_Y);
  for (let step = 0; step < 120 && useZoogiGame.getState().orbs[0]?.isActive; step++) {
    useZoogiGame.getState().physicsTick(1 / 60);
  }
  assert.equal(useZoogiGame.getState().orbs[0]?.isActive, false, "the orb leaves play after it falls out");
  assert.equal(useZoogiGame.getState().score, 50, "the point lands when the orb finishes falling");
  assert.equal(FALL_GRAVITY, 9.81);
});

test("the drawn ball matches the collider and rests on the floor", () => {
  assert.equal(MARBLE_RADIUS, ZOOGI_DRAW_RADIUS);
  assert.equal(ORB_RADIUS, ORB_DRAW_RADIUS);
  assert.equal(ZOOGI_REST_Y, MARBLE_RADIUS);
  assert.equal(ORB_REST_Y, ORB_RADIUS);
  assert.ok(ORB_RADIUS < MARBLE_RADIUS);
  const sink = ZOOGI_DRAW_RADIUS - MARBLE_RADIUS;
  assert.equal(sink, 0);
  console.log(`MEASURE drawRadius=${ZOOGI_DRAW_RADIUS} hitRadius=${MARBLE_RADIUS} orbRadius=${ORB_RADIUS} sink=${sink}`);
});

test("an orb is lighter than a Zoogi, so it leaves faster and the shooter keeps more", async () => {
  assert.ok(ORB_MASS < ZOOGI_MASS);
  const { useZoogiGame, player } = await playing();
  const incoming = 0.45;
  useZoogiGame.setState({
    phase: "playing",
    currentRound: 46,
    score: 0,
    isPlayerTurn: true,
    turnHasLaunched: false,
    firstTickProcessed: true,
    enemies: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 80 },
    playerEntity: {
      ...player,
      position: [0, ZOOGI_REST_Y, 0],
      velocity: [incoming, 0, 0],
      arcMovement: null,
      spawnImmunity: false,
      larsRicochetBoost: 1,
    },
    orbs: [{
      id: "light-orb",
      position: [MARBLE_RADIUS + ORB_RADIUS + 0.08, ORB_REST_Y, 0],
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
  useZoogiGame.getState().physicsTick(1 / 60);
  const after = useZoogiGame.getState();
  const shooter = after.playerEntity;
  const orb = after.orbs[0];
  assert.ok(shooter && orb);
  const orbSpeed = Math.hypot(orb.velocity[0], orb.velocity[2]);
  const kept = Math.hypot(shooter.velocity[0], shooter.velocity[2]) / incoming;
  const leave = orbSpeed / incoming;
  assert.ok(orb.velocity[0] > 0, "the orb is pushed forward");
  assert.ok(leave > MOMENTUM_TRANSFER, `orb leave ${leave.toFixed(3)} should beat an equal-mass transfer`);
  assert.ok(kept > SHOOTER_KEEP + 0.05, `shooter kept ${kept.toFixed(3)}`);
  console.log(`MEASURE orbMass=${ORB_MASS.toFixed(3)} zoogiMass=${ZOOGI_MASS} orbLeave=${leave.toFixed(3)} orbKeep=${kept.toFixed(3)}`);
});

test("a crossing that endpoints miss still registers, including a full-power shot", async () => {
  const closing = 4.1;
  const half = closing / 2;
  const startGap = 2.76;
  const a0 = -startGap / 2;
  const a1 = a0 + half;
  const b0 = startGap / 2;
  const b1 = b0 - half;
  const endGap = Math.abs(a1 - b1);
  const hitDiameter = 1;
  assert.ok(startGap > hitDiameter && endGap > hitDiameter);
  assert.equal(Math.abs(a0 - b0) < hitDiameter, false);
  assert.equal(endGap < hitDiameter, false);
  const swept = circleTimeOfImpact(a0, 0, a1, 0, b0, 0, b1, 0, hitDiameter);
  assert.ok(swept, "the swept hit registers");
  assert.ok(swept.t > 0 && swept.t < 1);
  const endpointGap = Math.min(startGap, endGap);
  console.log(`MEASURE tunnelClose=${closing} endpointGap=${endpointGap.toFixed(3)} hitDiameter=${hitDiameter} hitT=${swept.t.toFixed(3)}`);

  const { useZoogiGame, player, enemy } = await playing();
  const gap = MARBLE_RADIUS * 2 + 0.05;
  useZoogiGame.setState({
    phase: "playing",
    currentRound: 47,
    isPlayerTurn: true,
    turnHasLaunched: false,
    firstTickProcessed: true,
    orbs: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    wallSettings: { ...useZoogiGame.getState().wallSettings, knockoffBoundaryRadius: 80 },
    playerEntity: {
      ...player,
      position: [-gap / 2, ZOOGI_REST_Y, 0],
      velocity: [MAX_LAUNCH_SPEED, 0, 0],
      arcMovement: null,
      spawnImmunity: false,
      larsRicochetBoost: 1,
    },
    enemies: [{
      ...enemy,
      position: [gap / 2, ZOOGI_REST_Y, 0],
      velocity: [-MAX_LAUNCH_SPEED, 0, 0],
      arcMovement: null,
      isStunned: false,
      isKnockedOut: false,
      isRespawning: false,
      larsRicochetBoost: 1,
    }],
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  const shot = useZoogiGame.getState();
  const shooterSpeed = Math.hypot(shot.playerEntity?.velocity[0] ?? 0, shot.playerEntity?.velocity[2] ?? 0);
  const targetSpeed = Math.hypot(shot.enemies[0]?.velocity[0] ?? 0, shot.enemies[0]?.velocity[2] ?? 0);
  assert.ok(targetSpeed > MAX_PLANAR_SPEED * 0.4, `full-power target speed ${targetSpeed.toFixed(3)}`);
  assert.ok((shot.enemies[0]?.velocity[0] ?? 0) > 0, "the target is sent back the way the shot came");
  console.log(`MEASURE fullPowerHit target=${targetSpeed.toFixed(3)} shooter=${shooterSpeed.toFixed(3)} cap=${MAX_PLANAR_SPEED.toFixed(3)}`);
});

test("knockout waits for the open edge, not a meadow rock or a night-circuit rail", async () => {
  const grass = getMapLayout("grass");
  assert.ok(grass);
  const solids = collectMatchSolids({ map: "grass", bumpers: [], landedRocks: [], editorModels: [] });
  const rock = grass.scenery.find((solid) => {
    const rockDist = Math.hypot(solid.x, solid.z);
    return rockDist + solid.radius + MARBLE_RADIUS > grass.knockoffRadius;
  });
  assert.ok(rock, "a meadow rim rock reaches the knockout line");
  const rockDist = Math.hypot(rock.x, rock.z);
  const ux = rock.x / rockDist;
  const uz = rock.z / rockDist;
  const against = rockDist + rock.radius + MARBLE_RADIUS * 0.9;
  assert.ok(against > grass.knockoffRadius, "the sample center is past the circle");
  assert.ok(against < rockDist + rock.radius + MARBLE_RADIUS, "the sample still overlaps the rock");
  assert.equal(
    centerPastOpenEdge("grass", ux * against, uz * against, grass.knockoffRadius, MARBLE_RADIUS, solids),
    false,
  );
  assert.equal(
    centerPastOpenEdge("grass", grass.knockoffRadius + 0.35, 0, grass.knockoffRadius, MARBLE_RADIUS, solids),
    true,
    "the +x gap is open",
  );

  const { useZoogiGame, player } = await playing();
  useZoogiGame.getState().selectZoogi(player.zoogi);
  useZoogiGame.setState({ selectedMap: "grass", aiPlayerCount: 0, phase: "menu" });
  useZoogiGame.getState().startGame();
  const grassPlayer = useZoogiGame.getState().playerEntity;
  assert.ok(grassPlayer);
  useZoogiGame.setState({
    phase: "playing",
    firstTickProcessed: true,
    orbs: [],
    enemies: [],
    mushrooms: [],
    pinballBumpers: [],
    wolfClones: [],
    playerEntity: {
      ...grassPlayer,
      position: [ux * against, ZOOGI_REST_Y, uz * against],
      velocity: [0, 0, 0],
      offTheFloor: false,
      isKnockedOut: false,
      isRespawning: false,
      spawnImmunity: false,
      invulnerableUntil: null,
      arcMovement: null,
    },
  });
  useZoogiGame.getState().physicsTick(1 / 60);
  assert.equal(useZoogiGame.getState().playerEntity?.offTheFloor, false, "overlapping a rim rock stays in");

  const halfZ = neonPlayHalfZ();
  const north = neonRails().find((rail) => rail.id === "rail-north");
  assert.ok(north);
  const railCenterZ = (north.minZ + north.maxZ) / 2;
  assert.equal(leftNeonOpenEdge(0, halfZ + MARBLE_RADIUS * 0.25, MARBLE_RADIUS), false);
  assert.ok(railCenterZ < halfZ);
});

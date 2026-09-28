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

import test from "node:test";
import assert from "node:assert/strict";
import { buildMatchFacts, factsEqual, formatClock, type HudEntity, type HudInput } from "./matchHudModel.ts";

function entity(id: string, name: string, score: number, overrides: Partial<HudEntity> = {}): HudEntity {
  return {
    id,
    score,
    zoogi: {
      id: name.toLowerCase(),
      name,
      type: "Marble",
      color: "#ffffff",
      ability: "Pack",
      abilityDescription: "Send clones",
    },
    hasShield: false,
    shieldTimer: 0,
    speedBoost: 1,
    speedBoostTimer: 0,
    wolfgangAbilityUnlocked: false,
    hotstreakAbilityUnlocked: false,
    boltAbilityUnlocked: false,
    larsAbilityUnlocked: false,
    wrapsAbilityUnlocked: false,
    nightshadeAbilityUnlocked: false,
    ...overrides,
  };
}

function state(overrides: Partial<HudInput> = {}): HudInput {
  return {
    gameMode: "classic",
    isPlayerTurn: true,
    turnIndex: 0,
    currentLocalPlayerIndex: 0,
    localPlayers: [],
    playerEntity: entity("player", "Wolfgang", 4),
    enemies: [entity("cpu", "Hotstreak", 2, { zoogi: { id: "hotstreak", name: "Hotstreak", type: "Fire", color: "#f97316", ability: "Explosion", abilityDescription: "Blast" } })],
    orbs: [{ isActive: true }, { isActive: false }, { isActive: true }],
    score: 10,
    currentRound: 2,
    maxRounds: 3,
    playerRoundWins: 1,
    gameTimer: 125.8,
    selectedMap: "grass",
    ...overrides,
  };
}

test("match facts keep a one-second clock and only active orbs", () => {
  const facts = buildMatchFacts(state());
  assert.ok(facts);
  assert.equal(facts.secondsLeft, 125);
  assert.equal(formatClock(facts.secondsLeft), "2:05");
  assert.equal(facts.orbsLeft, 2);
  assert.equal(facts.turnLabel, "Your turn");
  assert.equal(facts.scores[0].score, 10);
  assert.equal(facts.scores[1].name, "Hotstreak");
  assert.equal(facts.scores[1].active, false);
});

test("sub-second physics noise does not change the hud snapshot", () => {
  const first = buildMatchFacts(state({ gameTimer: 125.9 }));
  const second = buildMatchFacts(state({ gameTimer: 125.1 }));
  assert.ok(first && second);
  assert.equal(factsEqual(first, second), true);
});

test("a new second or a new turn changes the hud snapshot", () => {
  const first = buildMatchFacts(state({ gameTimer: 125.2 }));
  const nextSecond = buildMatchFacts(state({ gameTimer: 124.9 }));
  const enemyTurn = buildMatchFacts(state({ isPlayerTurn: false, turnIndex: 0 }));
  assert.ok(first && nextSecond && enemyTurn);
  assert.equal(factsEqual(first, nextSecond), false);
  assert.equal(nextSecond.secondsLeft, 124);
  assert.equal(enemyTurn.turnLabel, "Hotstreak's turn");
  assert.equal(enemyTurn.scores[1].active, true);
  assert.equal(factsEqual(first, enemyTurn), false);
});

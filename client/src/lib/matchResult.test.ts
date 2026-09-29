import test from "node:test";
import assert from "node:assert/strict";
import { endCardResult } from "./matchResult.ts";

const wraps = [{ id: "cpu", name: "Wraps" }];

test("a 3-0 victory does not show the last round's 0 points", () => {
  const card = endCardResult({
    isVictory: true,
    playerRoundWins: 3,
    enemyRoundWins: new Map([["cpu", 0]]),
    opponents: wraps,
  });

  assert.equal(card.kind, "victory");
  assert.equal(card.headline, "VICTORY!");
  assert.equal(card.tally, "3 to 0");
  assert.equal(card.playerRounds, 3);
  assert.equal(card.opponentRounds, 0);
  assert.equal(card.playerLabel, "You");
  assert.equal(card.opponentLabel, "Wraps");
  assert.equal(card.detail, "You won more rounds!");
});

test("a 2-1 victory still shows the match when the last round was lost", () => {
  const card = endCardResult({
    isVictory: true,
    playerRoundWins: 2,
    enemyRoundWins: new Map([["cpu", 1]]),
    opponents: wraps,
  });

  assert.equal(card.kind, "victory");
  assert.equal(card.headline, "VICTORY!");
  assert.equal(card.tally, "2 to 1");
  assert.equal(card.playerRounds, 2);
  assert.equal(card.opponentRounds, 1);
});

test("a defeat shows how many rounds each side won", () => {
  const card = endCardResult({
    isVictory: false,
    playerRoundWins: 0,
    enemyRoundWins: new Map([["cpu", 3]]),
    opponents: wraps,
  });

  assert.equal(card.kind, "defeat");
  assert.equal(card.headline, "GAME OVER");
  assert.equal(card.tally, "0 to 3");
  assert.equal(card.playerRounds, 0);
  assert.equal(card.opponentRounds, 3);
  assert.equal(card.detail, "Better luck next time!");
});

test("a tied match shows the same rounds for both sides", () => {
  const card = endCardResult({
    isVictory: true,
    playerRoundWins: 1,
    enemyRoundWins: new Map([["cpu", 1]]),
    opponents: wraps,
  });

  assert.equal(card.kind, "tie");
  assert.equal(card.headline, "IT'S A TIE!");
  assert.equal(card.tally, "1 to 1");
  assert.equal(card.playerRounds, 1);
  assert.equal(card.opponentRounds, 1);
  assert.equal(card.detail, "You both won the same number of rounds.");
});

test("several computers count as the best one, not a pile of their wins", () => {
  const card = endCardResult({
    isVictory: true,
    playerRoundWins: 2,
    enemyRoundWins: new Map([
      ["a", 1],
      ["b", 0],
    ]),
    opponents: [
      { id: "a", name: "Bolt" },
      { id: "b", name: "Lars" },
    ],
  });

  assert.equal(card.kind, "victory");
  assert.equal(card.tally, "2 to 1");
  assert.equal(card.opponentLabel, "Bolt");
});

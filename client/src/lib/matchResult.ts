/**
 * Copy for the end-of-match card.
 *
 * A classic match is decided by rounds won. The store's `score` is only the
 * last round's points, and that number is cleared at the start of every new
 * round, so a 3–0 win can finish on a 0-point round. The card reads the
 * round totals instead.
 */

export interface EndCardOpponent {
  id: string;
  name: string;
}

export interface EndCardInput {
  isVictory: boolean;
  playerRoundWins: number;
  enemyRoundWins: Iterable<readonly [string, number]>;
  opponents?: EndCardOpponent[];
  playerLabel?: string;
}

export interface EndCardResult {
  kind: "victory" | "tie" | "defeat";
  headline: "VICTORY!" | "IT'S A TIE!" | "GAME OVER";
  detail: string;
  playerRounds: number;
  opponentRounds: number;
  /** Both sides, for example "3 to 0". */
  tally: string;
  playerLabel: string;
  opponentLabel: string;
}

export function endCardResult(input: EndCardInput): EndCardResult {
  const playerRounds = Math.max(0, input.playerRoundWins || 0);
  let opponentRounds = 0;
  let bestOpponentId: string | null = null;

  for (const [id, wins] of input.enemyRoundWins) {
    const count = Math.max(0, wins || 0);
    if (count > opponentRounds) {
      opponentRounds = count;
      bestOpponentId = id;
    }
  }

  const opponents = input.opponents ?? [];
  const namedOpponent =
    opponents.find((opponent) => opponent.id === bestOpponentId)?.name ||
    (opponents.length === 1 ? opponents[0]?.name : "") ||
    "";
  const opponentLabel = namedOpponent.trim() || "Computer";
  const playerLabel = input.playerLabel?.trim() || "You";

  // Equal round wins is the tie card. A tied round still goes to the player,
  // so this is only when the two sides finished with the same number of rounds.
  const tied = input.isVictory && playerRounds === opponentRounds;
  const kind: EndCardResult["kind"] = tied ? "tie" : input.isVictory ? "victory" : "defeat";
  const headline =
    kind === "tie" ? "IT'S A TIE!" : kind === "victory" ? "VICTORY!" : "GAME OVER";
  const detail =
    kind === "tie"
      ? "You both won the same number of rounds."
      : kind === "victory"
        ? "You won more rounds!"
        : "Better luck next time!";

  return {
    kind,
    headline,
    detail,
    playerRounds,
    opponentRounds,
    tally: `${playerRounds} to ${opponentRounds}`,
    playerLabel,
    opponentLabel,
  };
}

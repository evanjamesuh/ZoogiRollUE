/**
 * Glanceable match facts for the HUD.
 * Physics replaces entities every tick, so callers keep the previous object
 * when the words on screen would not change.
 */

export interface HudZoogi {
  id: string;
  name: string;
  type: string;
  color: string;
  ability: string;
  abilityDescription: string;
}

export interface HudEntity {
  id: string;
  score: number;
  zoogi: HudZoogi;
  hasShield: boolean;
  shieldTimer: number;
  speedBoost: number;
  speedBoostTimer: number;
  wolfgangAbilityUnlocked: boolean;
  hotstreakAbilityUnlocked: boolean;
  boltAbilityUnlocked: boolean;
  larsAbilityUnlocked: boolean;
  wrapsAbilityUnlocked: boolean;
  nightshadeAbilityUnlocked: boolean;
}

export interface HudLocalPlayer {
  id: number;
  name: string;
  isEliminated: boolean;
  zoogi: { name: string; color: string } | null;
}

export interface HudInput {
  gameMode: string;
  isPlayerTurn: boolean;
  turnIndex: number;
  currentLocalPlayerIndex: number;
  localPlayers: HudLocalPlayer[];
  playerEntity: HudEntity | null;
  enemies: HudEntity[];
  orbs: { isActive: boolean }[];
  score: number;
  currentRound: number;
  maxRounds: number;
  playerRoundWins: number;
  gameTimer: number;
  selectedMap: string | null;
}

export interface ScoreChip {
  id: string;
  name: string;
  score: number;
  color: string;
  active: boolean;
  eliminated: boolean;
}

export type TurnKind = "you" | "foe" | "local" | "ffa";

export interface MatchFacts {
  turnLabel: string;
  turnKind: TurnKind;
  scores: ScoreChip[];
  orbsLeft: number;
  round: number;
  maxRounds: number;
  wins: number;
  secondsLeft: number;
  map: string;
  gameMode: string;
  zoogiId: string;
  zoogiName: string;
  zoogiType: string;
  abilityTitle: string;
  abilityDetail: string;
  abilityHow: string;
  unlocked: boolean;
  shieldSeconds: number;
  boostSeconds: number;
  modeTitle: string;
  modeBlurb: string;
}

const PLAYER_COLORS = ["#3B82F6", "#EF4444", "#22C55E", "#A855F7"];

export function abilityHow(zoogiId: string): string {
  switch (zoogiId) {
    case "wolfgang":
      return "Tap Pack while moving to send homing clones.";
    case "hotstreak":
      return "Tap Explosion to blast everything nearby.";
    case "lars":
      return "Tap Ricochet, then hit something to home in.";
    case "pinpoint":
      return "Tap Lock-On, then tap an orb or opponent.";
    case "bolt":
      return "Tap Shock to phase through enemies and stun them.";
    case "wraps":
      return "Tap Bind to slow enemies you touch.";
    case "nightshade":
      return "Tap Shadow to freeze nearby opponents.";
    default:
      return "";
  }
}

export function abilityUnlocked(entity: HudEntity): boolean {
  switch (entity.zoogi.id) {
    case "wolfgang":
      return entity.wolfgangAbilityUnlocked;
    case "hotstreak":
      return entity.hotstreakAbilityUnlocked;
    case "lars":
      return entity.larsAbilityUnlocked;
    case "bolt":
      return entity.boltAbilityUnlocked;
    case "wraps":
      return entity.wrapsAbilityUnlocked;
    case "nightshade":
      return entity.nightshadeAbilityUnlocked;
    case "pinpoint":
      return true;
    default:
      return false;
  }
}

export function modeCopy(gameMode: string): { title: string; blurb: string } {
  if (gameMode === "ringer_royale") {
    return { title: "Ringer Royale", blurb: "Knock opponents out of the ring." };
  }
  if (gameMode === "practice") {
    return { title: "Marble Arena", blurb: "Knock orbs and opponents off the island." };
  }
  if (gameMode === "local_multiplayer") {
    return { title: "Local Match", blurb: "Take turns. Knock orbs and opponents off the island." };
  }
  return { title: "Classic Match", blurb: "Take turns. Knock orbs and opponents off the island." };
}

export function formatClock(secondsLeft: number): string {
  const safe = Math.max(0, secondsLeft);
  const minutes = Math.floor(safe / 60);
  const seconds = safe % 60;
  return `${minutes}:${seconds.toString().padStart(2, "0")}`;
}

function actingEntity(state: HudInput): HudEntity | null {
  if (state.gameMode === "local_multiplayer") {
    if (state.currentLocalPlayerIndex === 0) return state.playerEntity;
    return state.enemies[state.currentLocalPlayerIndex - 1] ?? null;
  }
  return state.playerEntity;
}

export function buildMatchFacts(state: HudInput): MatchFacts | null {
  const actor = actingEntity(state);
  if (!actor) return null;

  const local = state.gameMode === "local_multiplayer";
  const ffa = state.gameMode === "ringer_royale";
  const scores: ScoreChip[] = [];
  let turnLabel = "Your turn";
  let turnKind: TurnKind = "you";

  if (local) {
    const index = state.currentLocalPlayerIndex;
    const current = state.localPlayers[index];
    turnLabel = current ? `${current.name}'s turn` : "Your turn";
    turnKind = "local";
    state.localPlayers.forEach((player, playerIndex) => {
      const entity = playerIndex === 0 ? state.playerEntity : state.enemies[playerIndex - 1];
      scores.push({
        id: `local-${player.id}`,
        name: player.name,
        score: entity?.score ?? 0,
        color: player.zoogi?.color || PLAYER_COLORS[playerIndex % PLAYER_COLORS.length],
        active: playerIndex === index,
        eliminated: player.isEliminated,
      });
    });
  } else {
    scores.push({
      id: "you",
      name: "You",
      score: state.score,
      color: state.playerEntity?.zoogi.color || "#60a5fa",
      active: !ffa && state.isPlayerTurn,
      eliminated: false,
    });
    state.enemies.forEach((enemy, index) => {
      const active = !ffa && !state.isPlayerTurn && state.turnIndex === index;
      scores.push({
        id: enemy.id,
        name: enemy.zoogi.name,
        score: enemy.score,
        color: enemy.zoogi.color,
        active,
        eliminated: false,
      });
    });
    if (ffa) {
      turnLabel = "Free for all";
      turnKind = "ffa";
    } else if (state.isPlayerTurn) {
      turnLabel = "Your turn";
      turnKind = "you";
    } else {
      const foe = state.enemies[state.turnIndex];
      turnLabel = foe ? `${foe.zoogi.name}'s turn` : "Enemy turn";
      turnKind = "foe";
    }
  }

  const mode = modeCopy(state.gameMode);
  const secondsLeft = Math.max(0, Math.floor(state.gameTimer));

  return {
    turnLabel,
    turnKind,
    scores,
    orbsLeft: state.orbs.reduce((count, orb) => count + (orb.isActive ? 1 : 0), 0),
    round: state.currentRound,
    maxRounds: state.maxRounds,
    wins: state.playerRoundWins,
    secondsLeft,
    map: state.selectedMap || "",
    gameMode: state.gameMode,
    zoogiId: actor.zoogi.id,
    zoogiName: actor.zoogi.name,
    zoogiType: actor.zoogi.type,
    abilityTitle: actor.zoogi.ability,
    abilityDetail: actor.zoogi.abilityDescription,
    abilityHow: abilityHow(actor.zoogi.id),
    unlocked: abilityUnlocked(actor),
    shieldSeconds: actor.hasShield ? Math.max(0, Math.ceil(actor.shieldTimer)) : 0,
    boostSeconds: actor.speedBoost > 1 ? Math.max(0, Math.ceil(actor.speedBoostTimer)) : 0,
    modeTitle: mode.title,
    modeBlurb: mode.blurb,
  };
}

function sameScores(a: ScoreChip[], b: ScoreChip[]): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    const left = a[i];
    const right = b[i];
    if (
      left.id !== right.id ||
      left.name !== right.name ||
      left.score !== right.score ||
      left.color !== right.color ||
      left.active !== right.active ||
      left.eliminated !== right.eliminated
    ) {
      return false;
    }
  }
  return true;
}

export function factsEqual(a: MatchFacts, b: MatchFacts): boolean {
  return (
    a.turnLabel === b.turnLabel &&
    a.turnKind === b.turnKind &&
    a.orbsLeft === b.orbsLeft &&
    a.round === b.round &&
    a.maxRounds === b.maxRounds &&
    a.wins === b.wins &&
    a.secondsLeft === b.secondsLeft &&
    a.map === b.map &&
    a.gameMode === b.gameMode &&
    a.zoogiId === b.zoogiId &&
    a.zoogiName === b.zoogiName &&
    a.zoogiType === b.zoogiType &&
    a.abilityTitle === b.abilityTitle &&
    a.abilityDetail === b.abilityDetail &&
    a.abilityHow === b.abilityHow &&
    a.unlocked === b.unlocked &&
    a.shieldSeconds === b.shieldSeconds &&
    a.boostSeconds === b.boostSeconds &&
    a.modeTitle === b.modeTitle &&
    a.modeBlurb === b.modeBlurb &&
    sameScores(a.scores, b.scores)
  );
}

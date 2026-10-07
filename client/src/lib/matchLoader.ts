import { useEffect, useState } from "react";
import * as THREE from "three";
import { BUMPER_SCORE, KNOCKOUT_PENALTY, KNOCKOUT_SCORE_ORB, KNOCKOUT_SCORE_PLAYER, ZONE_SCORE_ORB } from "./arenaConstants";

/** Stay hidden until a load has lasted this long, so a fast match does not flash the screen. */
export const MATCH_LOADER_SHOW_DELAY_MS = 250;

/** Once the screen is on, keep it up at least this long so it cannot flicker off. */
export const MATCH_LOADER_MIN_VISIBLE_MS = 600;

/** Fade onto the already-painted arena. The canvas is drawing underneath the whole time. */
export const MATCH_LOADER_FADE_MS = 480;

const SLOW_LOAD_DEFAULT_MS = 6000;

export type MatchLoaderPhase = "waiting" | "visible" | "fading" | "gone";

export interface MatchLoaderSnapshot {
  now: number;
  startedAt: number;
  /** When the arena had finished loading and a frame of it had been drawn. */
  sceneReadyAt: number | null;
  /** When the overlay actually appeared. Null until the show delay has elapsed. */
  shownAt: number | null;
}

export interface MatchLoaderDecision {
  phase: MatchLoaderPhase;
  /** 1 covers the canvas. Falls toward 0 during the fade, then the overlay unmounts. */
  opacity: number;
  mounted: boolean;
  /** Persist this. The minimum visible time is measured from it. */
  shownAt: number | null;
}

function smoothstep(t: number): number {
  const x = Math.min(1, Math.max(0, t));
  return x * x * (3 - 2 * x);
}

/**
 * When the match loader may cover the canvas.
 *
 * A load that finishes at or before the show delay never mounts, so a sub-300ms
 * start does not flash. After the delay, the overlay stays opaque until the
 * arena frame is ready and it has been up for the minimum time, then fades.
 */
export function decideMatchLoader(input: MatchLoaderSnapshot): MatchLoaderDecision {
  const { now, startedAt, sceneReadyAt, shownAt } = input;
  const showAt = startedAt + MATCH_LOADER_SHOW_DELAY_MS;
  const hidden = { phase: "gone" as const, opacity: 0, mounted: false, shownAt: null };

  if (sceneReadyAt !== null && sceneReadyAt <= showAt && shownAt === null) {
    return hidden;
  }

  if (shownAt === null && now < showAt) {
    return { phase: "waiting", opacity: 0, mounted: false, shownAt: null };
  }

  const visibleSince = shownAt ?? now;
  const visible = { phase: "visible" as const, opacity: 1, mounted: true, shownAt: visibleSince };
  if (sceneReadyAt === null) return visible;

  const fadeStart = Math.max(sceneReadyAt, visibleSince + MATCH_LOADER_MIN_VISIBLE_MS);
  if (now <= fadeStart) return visible;

  const t = (now - fadeStart) / MATCH_LOADER_FADE_MS;
  if (t >= 1) {
    return { phase: "gone", opacity: 0, mounted: false, shownAt: visibleSince };
  }

  return {
    phase: "fading",
    opacity: 1 - smoothstep(t),
    mounted: true,
    shownAt: visibleSince,
  };
}

/**
 * drei Html labels (letter badges, Stunned, Slowed, power chips) use a z-index
 * above any overlay. Keep them out of the tree until the loader has fully gone.
 */
export function worldLabelsCovered(phase: MatchLoaderPhase): boolean {
  return phase !== "gone";
}

let matchWorldLabelsHidden = false;
const matchWorldLabelListeners = new Set<(hidden: boolean) => void>();

export function setMatchWorldLabelsHidden(hidden: boolean): void {
  if (matchWorldLabelsHidden === hidden) return;
  matchWorldLabelsHidden = hidden;
  for (const listener of matchWorldLabelListeners) listener(hidden);
}

export function getMatchWorldLabelsHidden(): boolean {
  return matchWorldLabelsHidden;
}

export function subscribeMatchWorldLabels(listener: (hidden: boolean) => void): () => void {
  matchWorldLabelListeners.add(listener);
  return () => {
    matchWorldLabelListeners.delete(listener);
  };
}

export function useMatchWorldLabelsHidden(): boolean {
  const [hidden, setHidden] = useState(getMatchWorldLabelsHidden);
  useEffect(() => subscribeMatchWorldLabels(setHidden), []);
  return hidden;
}

export interface MatchTipSource {
  id: string;
  ability: string;
  abilityDescription: string;
}

/** One tip is shown at a time. The lines are the real rules and powers. */
export function matchLoadingTips(zoogi: MatchTipSource | null): string[] {
  const unlock = zoogi?.id === "pinpoint"
    ? "Pinpoint's Lock-On is always ready. Tap it, then tap an orb or an opponent, and the shot aims itself."
    : zoogi
      ? `Knock a star orb off the ring to unlock ${zoogi.ability}. ${zoogi.abilityDescription}.`
      : "Knock a star orb off the ring to unlock your power.";

  return [
    unlock,
    `Bumpers bounce a marble away and score ${BUMPER_SCORE} points.`,
    `Falling off the arena costs ${KNOCKOUT_PENALTY} points.`,
    `Knock an orb off the edge for ${KNOCKOUT_SCORE_ORB} points.`,
    `Knock an opponent off the arena for ${KNOCKOUT_SCORE_PLAYER} points.`,
    "Drag backward and release to launch. A longer drag means more power: green is low, yellow is medium, red is high.",
    `Leave an orb inside a scoring ring for ${ZONE_SCORE_ORB} points.`,
  ];
}

/** `?slowLoad` or `?slowLoad=1` holds the match load so the screen can be captured. `?slowLoad=2500` is a custom hold. */
export function readSlowLoadMs(search: string): number {
  const params = new URLSearchParams(search);
  if (!params.has("slowLoad")) return 0;
  const raw = params.get("slowLoad");
  if (raw === null || raw === "" || raw === "1" || raw === "true") return SLOW_LOAD_DEFAULT_MS;
  const parsed = Number(raw);
  if (!Number.isFinite(parsed) || parsed <= 0) return SLOW_LOAD_DEFAULT_MS;
  return Math.min(parsed, 20000);
}

/**
 * Debug-only. Spreads real LoadingManager completions across the hold so the
 * bar moves with actual asset counts instead of finishing on the first 404.
 */
export function installSlowLoad(search: string): () => void {
  if (typeof window === "undefined") return () => {};
  const budget = readSlowLoadMs(search);
  if (budget <= 0) return () => {};

  const manager = THREE.DefaultLoadingManager;
  const original = manager.itemEnd.bind(manager);
  let index = 0;
  const wrapped = (url: string) => {
    const fraction = Math.min(1, 0.22 + index * 0.2);
    index += 1;
    window.setTimeout(() => original(url), Math.round(budget * fraction));
  };
  manager.itemEnd = wrapped;
  return () => {
    if (manager.itemEnd === wrapped) manager.itemEnd = original;
  };
}

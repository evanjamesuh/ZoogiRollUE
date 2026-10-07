import { useEffect, useMemo, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import { useProgress } from "@react-three/drei";
import { Wand2 } from "lucide-react";
import { MAP_OPTIONS, useZoogiGame, type MapTheme } from "@/lib/stores/useZoogiGame";
import { useViewportLayout } from "@/lib/mobileGraphics";
import { ZoogiPortrait } from "./ZoogiPortrait";
import {
  decideMatchLoader,
  matchLoadingTips,
  setMatchWorldLabelsHidden,
  worldLabelsCovered,
  type MatchLoaderDecision,
} from "@/lib/matchLoader";

const TIP_INTERVAL_MS = 3800;

function decisionsMatch(a: MatchLoaderDecision, b: MatchLoaderDecision): boolean {
  return a.phase === b.phase
    && a.mounted === b.mounted
    && a.shownAt === b.shownAt
    && Math.abs(a.opacity - b.opacity) < 0.015;
}

/** Follows asset completion, then the minimum-time and fade rules. */
function useMatchLoaderGate(scenePainted: boolean): MatchLoaderDecision {
  const startedAtRef = useRef(performance.now());
  const sceneReadyAtRef = useRef<number | null>(null);
  const shownAtRef = useRef<number | null>(null);
  if (scenePainted && sceneReadyAtRef.current === null) {
    sceneReadyAtRef.current = performance.now();
  }

  const [decision, setDecision] = useState<MatchLoaderDecision>(() => decideMatchLoader({
    now: startedAtRef.current,
    startedAt: startedAtRef.current,
    sceneReadyAt: null,
    shownAt: null,
  }));

  useEffect(() => {
    let raf = 0;
    let stopped = false;
    const tick = () => {
      if (stopped) return;
      const now = performance.now();
      const next = decideMatchLoader({
        now,
        startedAt: startedAtRef.current,
        sceneReadyAt: sceneReadyAtRef.current,
        shownAt: shownAtRef.current,
      });
      if (next.shownAt !== null) shownAtRef.current = next.shownAt;
      setDecision((prev) => (decisionsMatch(prev, next) ? prev : next));
      if (next.phase !== "gone") raf = requestAnimationFrame(tick);
    };
    raf = requestAnimationFrame(tick);
    return () => {
      stopped = true;
      cancelAnimationFrame(raf);
    };
  }, []);

  return decision;
}

/**
 * Signals after the loading manager has gone idle and the canvas has drawn
 * the arena for two frames. Those frames stay under the overlay, so the fade
 * reveals a picture that is already there.
 */
export function MatchSceneProbe({ onPainted, holdMs }: { onPainted: () => void; holdMs: number }) {
  const onPaintedRef = useRef(onPainted);
  onPaintedRef.current = onPainted;
  const holdUntil = useRef(performance.now() + Math.max(0, holdMs));
  const done = useRef(false);
  const idleFrames = useRef(0);
  const frames = useRef(0);

  useFrame(() => {
    if (done.current) return;
    frames.current += 1;
    const state = useProgress.getState();
    const busy = state.active || state.loaded < state.total;
    const held = performance.now() < holdUntil.current;
    if (busy || held) {
      idleFrames.current = 0;
      return;
    }
    if (frames.current < 3 && state.total === 0 && state.loaded === 0) return;
    idleFrames.current += 1;
    if (idleFrames.current < 2) return;
    done.current = true;
    onPaintedRef.current();
  });

  return null;
}

interface ArenaLabel {
  name: string;
  description: string;
  color: string;
}

function readCustomArenaName(customArenaId: string): { name: string; theme?: MapTheme } | null {
  if (typeof localStorage === "undefined") return null;
  try {
    const raw = localStorage.getItem("custom_arenas");
    const arenas = raw ? JSON.parse(raw) : [];
    if (!Array.isArray(arenas)) return null;
    const found = arenas.find((arena) => arena && arena.id === customArenaId);
    if (!found || typeof found.name !== "string" || found.name.trim() === "") return null;
    return { name: found.name.trim(), theme: found.theme };
  } catch {
    return null;
  }
}

function arenaLabel(selectedMap: MapTheme | null, customArenaId: string | null, meshyArenaModelUrl: string | null): ArenaLabel {
  const themeOption = (id: MapTheme | null | undefined) => MAP_OPTIONS.find((map) => map.id === id) ?? null;
  if (customArenaId) {
    const custom = readCustomArenaName(customArenaId);
    if (custom) {
      const theme = themeOption(custom.theme ?? selectedMap);
      return {
        name: custom.name,
        description: theme?.description ?? "Custom arena",
        color: theme?.color ?? "#8B5CF6",
      };
    }
  }
  if (meshyArenaModelUrl) {
    return { name: "Custom Arena", description: "Your generated stage", color: "#8B5CF6" };
  }
  const theme = themeOption(selectedMap) ?? MAP_OPTIONS[0];
  return { name: theme.name, description: theme.description, color: theme.color };
}

function useMatchCard() {
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  const customArenaId = useZoogiGame((state) => state.customArenaId);
  const meshyArenaModelUrl = useZoogiGame((state) => state.meshyArenaModelUrl);
  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const selectedZoogi = useZoogiGame((state) => state.selectedZoogi);
  const selectedCustomZoogi = useZoogiGame((state) => state.selectedCustomZoogi);

  const rosterZoogi = playerEntity?.zoogi ?? selectedZoogi;
  const custom = !rosterZoogi && selectedCustomZoogi
    ? {
        id: selectedCustomZoogi.id,
        name: selectedCustomZoogi.name,
        type: "Custom",
        color: "#8B5CF6",
        secondaryColor: "#A78BFA",
        ability: "Custom Power",
        abilityDescription: "A unique custom ability",
      }
    : null;
  const zoogi = rosterZoogi ?? custom;
  const thumbnailUrl = playerEntity?.customThumbnailUrl || selectedCustomZoogi?.thumbnailUrl;
  const isCustom = Boolean(playerEntity?.isCustomZoogi || custom);

  return {
    arena: arenaLabel(selectedMap, customArenaId, meshyArenaModelUrl),
    zoogi,
    thumbnailUrl,
    isCustom,
  };
}

function MatchPortrait({
  zoogi,
  thumbnailUrl,
  custom,
  className,
}: {
  zoogi: { id: string; name: string; color: string; secondaryColor: string };
  thumbnailUrl?: string;
  custom: boolean;
  className: string;
}) {
  if (thumbnailUrl) {
    return (
      <div className={`relative overflow-hidden rounded-full border-2 border-white/30 bg-black/40 shadow-lg ${className}`}>
        <img src={thumbnailUrl} alt="" className="h-full w-full object-cover" />
      </div>
    );
  }
  if (custom) {
    return (
      <div className={`flex items-center justify-center rounded-full border-2 border-violet-400/50 bg-gradient-to-br from-violet-500 to-fuchsia-500 shadow-lg ${className}`}>
        <Wand2 className="h-6 w-6 text-white" aria-hidden="true" />
      </div>
    );
  }
  return <ZoogiPortrait zoogi={zoogi} className={`border-2 border-white/30 shadow-lg ${className}`} />;
}

function RotatingTip({ tips }: { tips: string[] }) {
  const [index, setIndex] = useState(0);
  useEffect(() => {
    if (tips.length < 2) return;
    const id = window.setInterval(() => {
      setIndex((current) => (current + 1) % tips.length);
    }, TIP_INTERVAL_MS);
    return () => window.clearInterval(id);
  }, [tips]);

  const tip = tips[index] ?? "";
  return (
    <div className="min-h-[3.25rem]" aria-live="polite">
      <p className="text-[10px] font-semibold uppercase tracking-[0.18em] text-amber-200/80">Tip</p>
      <p key={`${index}-${tip}`} className="mt-1 text-sm leading-snug text-white/80 animate-in fade-in duration-500">
        {tip}
      </p>
    </div>
  );
}

export function MatchLoadingScreen({ scenePainted }: { scenePainted: boolean }) {
  const decision = useMatchLoaderGate(scenePainted);
  useEffect(() => {
    setMatchWorldLabelsHidden(worldLabelsCovered(decision.phase));
  }, [decision.phase]);
  useEffect(() => () => setMatchWorldLabelsHidden(false), []);
  const { active, progress } = useProgress();
  const { arena, zoogi, thumbnailUrl, isCustom } = useMatchCard();
  const { phone, portrait } = useViewportLayout();
  const compact = phone && !portrait;
  const tipKey = zoogi ? `${zoogi.id}|${zoogi.ability}|${zoogi.abilityDescription}` : "";
  const tips = useMemo(() => matchLoadingTips(zoogi ? {
    id: zoogi.id,
    ability: zoogi.ability,
    abilityDescription: zoogi.abilityDescription,
  } : null), [tipKey]);

  if (!decision.mounted || !zoogi) return null;

  const safeProgress = Number.isFinite(progress) ? Math.min(100, Math.max(0, progress)) : 0;
  const shownProgress = !active && scenePainted ? 100 : safeProgress;
  const percent = Math.round(shownProgress);

  return (
    <div
      className={`fixed inset-0 z-[80] flex items-center justify-center ${phone ? "phone-safe-x phone-safe-top phone-safe-bottom" : "p-6"}`}
      style={{
        opacity: decision.opacity,
        background: `radial-gradient(90% 70% at 50% 12%, ${arena.color}2e, transparent 58%), linear-gradient(180deg, #1a1024 0%, #0c0812 100%)`,
      }}
      data-match-loader={decision.phase}
      data-loader-progress={percent}
      data-arena-name={arena.name}
      data-zoogi-name={zoogi.name}
      role="status"
      aria-live="polite"
      aria-busy={decision.phase !== "fading"}
      aria-label={`Loading ${arena.name}`}
    >
      <div
        className={`w-full border border-white/10 bg-black/45 shadow-2xl backdrop-blur-xl ${
          compact
            ? "mx-3 flex max-w-3xl items-center gap-4 rounded-2xl px-4 py-3"
            : "mx-4 flex max-w-md flex-col gap-5 rounded-3xl px-6 py-6"
        }`}
        style={{ boxShadow: `0 24px 80px rgba(0,0,0,0.45), inset 0 1px 0 rgba(255,255,255,0.06)` }}
      >
        <div className={`flex items-center ${compact ? "w-36 shrink-0 flex-col gap-2 text-center" : "gap-4"}`}>
          <MatchPortrait
            zoogi={zoogi}
            thumbnailUrl={thumbnailUrl}
            custom={isCustom}
            className={compact ? "h-16 w-16" : "h-20 w-20"}
          />
          <div className={compact ? "" : "min-w-0 flex-1"}>
            <p className="text-[11px] font-semibold uppercase tracking-[0.2em] text-white/45">{zoogi.type}</p>
            <h2 className={`font-bold leading-tight text-white ${compact ? "text-base" : "text-xl"}`}>{zoogi.name}</h2>
            {!compact && <p className="mt-0.5 text-sm font-semibold text-amber-300">{zoogi.ability}</p>}
          </div>
        </div>

        <div className="min-w-0 flex-1">
          <p className="text-[11px] font-semibold uppercase tracking-[0.22em] text-white/45">Entering arena</p>
          <h1 className={`font-bold tracking-tight text-white ${compact ? "text-xl" : "text-3xl"}`}>{arena.name}</h1>
          {!compact && <p className="mt-1 text-sm text-white/60">{arena.description}</p>}

          <div className={compact ? "mt-2" : "mt-5"}>
            <div className="mb-1.5 flex items-baseline justify-between gap-3">
              <span className="text-xs font-semibold uppercase tracking-[0.16em] text-white/55">Loading</span>
              <span className="text-xs font-semibold tabular-nums text-white/80">{percent}%</span>
            </div>
            <div
              className="h-2 overflow-hidden rounded-full bg-white/10"
              role="progressbar"
              aria-valuemin={0}
              aria-valuemax={100}
              aria-valuenow={percent}
              aria-label="Match load progress"
            >
              <div
                className="h-full rounded-full"
                style={{
                  width: `${percent}%`,
                  background: `linear-gradient(90deg, ${arena.color}, #f5c16c)`,
                  transition: "width 180ms linear",
                }}
              />
            </div>
          </div>

          <div className={`rounded-2xl border border-white/10 bg-white/5 ${compact ? "mt-2 px-3 py-2" : "mt-4 px-4 py-3"}`}>
            <RotatingTip tips={tips} />
          </div>
        </div>
      </div>
    </div>
  );
}

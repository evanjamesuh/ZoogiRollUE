import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { BUMPER_SCORE, KNOCKOUT_PENALTY, KNOCKOUT_SCORE_ORB, KNOCKOUT_SCORE_PLAYER, ZONE_SCORE_ORB } from "@/lib/arenaConstants";
import { useViewportLayout } from "@/lib/mobileGraphics";
import { useAudio } from "@/lib/stores/useAudio";
import { useProgression } from "@/lib/stores/useProgression";
import { NeonScoreboard } from "./NeonScoreboard";
import {
  buildMatchFacts,
  factsEqual,
  formatClock,
  type MatchFacts,
} from "./matchHudModel";
import { motion, AnimatePresence } from "framer-motion";
import {
  Eye,
  EyeOff,
  Home,
  HelpCircle,
  User,
  Zap,
  Crosshair,
  Star,
  Trophy,
  Coins,
  Info,
  X,
  Video,
  Move,
  ScanEye,
  Target,
  Lock,
  Menu,
  Minus,
  Plus,
} from "lucide-react";
import { memo, useCallback, useEffect, useRef, useState } from "react";
import Confetti from "react-confetti";

function useDevTools() {
  const [enabled, setEnabled] = useState(() => {
    if (typeof window === "undefined") return false;
    const debug = new URLSearchParams(window.location.search).get("debug");
    return debug !== null && debug !== "colliders";
  });

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.key !== "`") return;
      const target = event.target as HTMLElement | null;
      const tag = target?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA" || target?.isContentEditable) return;
      setEnabled((on) => !on);
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  return enabled;
}

function useMatchFacts(): MatchFacts | null {
  const cache = useRef<MatchFacts | null>(null);
  const select = useCallback((state: Parameters<typeof buildMatchFacts>[0]) => {
    const next = buildMatchFacts(state);
    if (next && cache.current && factsEqual(cache.current, next)) return cache.current;
    cache.current = next;
    return next;
  }, []);
  return useZoogiGame(select);
}

function typingTarget(event: KeyboardEvent): boolean {
  const target = event.target as HTMLElement | null;
  const tag = target?.tagName;
  return tag === "INPUT" || tag === "TEXTAREA" || Boolean(target?.isContentEditable);
}

const turnClass: Record<MatchFacts["turnKind"], string> = {
  you: "text-emerald-300",
  foe: "text-orange-300",
  local: "text-white",
  ffa: "text-purple-200",
};

function TopCluster() {
  const facts = useMatchFacts();
  const { phone } = useViewportLayout();
  if (!facts) return null;
  const clock = formatClock(facts.secondsLeft);
  const clockClass = facts.secondsLeft <= 60 ? "text-red-300" : facts.secondsLeft <= 180 ? "text-yellow-300" : "text-white";

  return (
    <div className={phone ? "max-w-full" : "max-w-xl"}>
      <div className="rounded-2xl bg-black/75 px-3 py-2.5 shadow-lg backdrop-blur-sm">
        <p className={`font-black leading-none tracking-tight ${turnClass[facts.turnKind]} ${phone ? "text-3xl" : "text-5xl"}`}>
          {facts.turnLabel}
        </p>
        <div className="mt-2 flex flex-wrap gap-1.5">
          {facts.scores.map((chip) => (
            <div
              key={chip.id}
              className={`flex min-h-8 items-center gap-1.5 rounded-full bg-white/10 px-2 py-1 ${
                chip.active ? "ring-2 ring-yellow-300" : ""
              } ${chip.eliminated ? "opacity-40" : ""}`}
            >
              <span className="h-3.5 w-3.5 shrink-0 rounded-full" style={{ backgroundColor: chip.color }} />
              <span className="max-w-[5.5rem] truncate text-sm font-semibold text-white">{chip.name}</span>
              <span className="text-base font-black text-yellow-300">{chip.score}</span>
            </div>
          ))}
        </div>
        <div className="mt-2 flex flex-wrap items-center gap-x-3 gap-y-1 text-sm font-semibold">
          <span className="text-purple-200">Round {facts.round}/{facts.maxRounds}</span>
          <span className={clockClass}>{clock}</span>
          <span className="text-pink-300">{facts.orbsLeft} orbs</span>
        </div>
        {(facts.shieldSeconds > 0 || facts.boostSeconds > 0) && (
          <div className="mt-1 flex flex-wrap gap-2 text-sm font-semibold">
            {facts.shieldSeconds > 0 && <span className="text-cyan-200">Shield {facts.shieldSeconds}s</span>}
            {facts.boostSeconds > 0 && <span className="text-sky-200">Boost {facts.boostSeconds}s</span>}
          </div>
        )}
      </div>
    </div>
  );
}

function NeonStrip() {
  const facts = useMatchFacts();
  if (!facts || facts.map !== "neon") return null;
  const foe = facts.scores.filter((chip) => chip.id !== "you").reduce((best, chip) => Math.max(best, chip.score), 0);
  const you = facts.scores.find((chip) => chip.id === "you")?.score ?? 0;
  return (
    <NeonScoreboard
      playerScore={you}
      foeScore={foe}
      round={facts.round}
      maxRounds={facts.maxRounds}
      timer={formatClock(facts.secondsLeft)}
    />
  );
}

function AbilityButton() {
  const facts = useMatchFacts();
  const activateWolfgangAbility = useZoogiGame((state) => state.activateWolfgangAbility);
  const activateHotstreakAbility = useZoogiGame((state) => state.activateHotstreakAbility);
  const activateLarsAbility = useZoogiGame((state) => state.activateLarsAbility);
  const activateBoltAbility = useZoogiGame((state) => state.activateBoltAbility);
  const activateWrapsAbility = useZoogiGame((state) => state.activateWrapsAbility);
  const activateNightshadeAbility = useZoogiGame((state) => state.activateNightshadeAbility);
  const toggleLockOn = useZoogiGame((state) => state.toggleLockOn);
  const lockOnEnabled = useZoogiGame((state) => state.lockOnEnabled);
  const actorId = useZoogiGame((state) => {
    if (state.gameMode === "local_multiplayer") {
      if (state.currentLocalPlayerIndex === 0) return state.playerEntity?.id ?? "";
      return state.enemies[state.currentLocalPlayerIndex - 1]?.id ?? "";
    }
    return state.playerEntity?.id ?? "";
  });

  const fire = useCallback(() => {
    if (!facts?.unlocked) return;
    const id = actorId;
    switch (facts.zoogiId) {
      case "wolfgang":
        activateWolfgangAbility(id);
        break;
      case "hotstreak":
        activateHotstreakAbility(id);
        break;
      case "lars":
        activateLarsAbility(id);
        break;
      case "bolt":
        activateBoltAbility(id);
        break;
      case "wraps":
        activateWrapsAbility(id);
        break;
      case "nightshade":
        activateNightshadeAbility(id);
        break;
      case "pinpoint":
        toggleLockOn();
        break;
      default:
        break;
    }
  }, [
    facts?.unlocked,
    facts?.zoogiId,
    actorId,
    activateWolfgangAbility,
    activateHotstreakAbility,
    activateLarsAbility,
    activateBoltAbility,
    activateWrapsAbility,
    activateNightshadeAbility,
    toggleLockOn,
  ]);

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.repeat || typingTarget(event)) return;
      if (event.key !== "e" && event.key !== "E") return;
      event.preventDefault();
      fire();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [fire]);

  if (!facts || !facts.zoogiId) return null;
  const ready = facts.unlocked;
  const active = facts.zoogiId === "pinpoint" && lockOnEnabled;
  const shortLabel: Record<string, string> = {
    wolfgang: "Pack",
    hotstreak: "Explosion",
    lars: "Ricochet",
    bolt: "Shock",
    wraps: "Bind",
    nightshade: "Shadow",
    pinpoint: "Lock-On",
  };
  const label = shortLabel[facts.zoogiId] || facts.abilityTitle;

  return (
    <button
      type="button"
      onClick={fire}
      disabled={!ready}
      aria-keyshortcuts="E"
      aria-label={ready ? label : `${label} locked`}
      title={ready ? facts.abilityHow : "Knock a star orb off the island to unlock"}
      className={`pointer-events-auto flex items-center justify-center gap-2 rounded-2xl px-4 font-black shadow-lg ${
        ready
          ? "min-h-16 min-w-[7.5rem] bg-amber-400 text-black hover:bg-amber-300"
          : "min-h-11 min-w-11 bg-black/70 text-white/70"
      } ${active ? "ring-2 ring-purple-200" : ""}`}
    >
      {ready ? <Zap size={22} /> : <Lock size={20} />}
      <span className="text-base leading-tight">
        {label}
        <span className="mt-0.5 block text-xs font-bold uppercase tracking-wide opacity-80">
          {ready ? "Ready" : "Locked"}
        </span>
      </span>
    </button>
  );
}

function MenuButton({ open, onClick, flagged }: { open: boolean; onClick: () => void; flagged: boolean }) {
  return (
    <button
      type="button"
      onClick={onClick}
      aria-expanded={open}
      aria-keyshortcuts="M"
      aria-label={open ? "Close match menu" : "Open match menu"}
      className={`pointer-events-auto relative flex h-12 w-12 items-center justify-center rounded-2xl shadow-lg ${
        open ? "bg-white text-black" : "bg-black/75 text-white"
      }`}
    >
      {open ? <X size={22} /> : <Menu size={22} />}
      {flagged && !open && <span className="absolute right-1 top-1 h-2.5 w-2.5 rounded-full bg-amber-300" />}
    </button>
  );
}

function cameraButtonClass(active: boolean, activeClass: string): string {
  return `min-h-11 rounded-xl px-2 text-sm font-bold ${active ? activeClass : "bg-white/10 text-white"}`;
}

function ToolsMenu({
  devTools,
  tall,
  onBirdHelp,
}: {
  devTools: boolean;
  tall: boolean;
  onBirdHelp: () => void;
}) {
  const firstPerson = useZoogiGame((state) => state.firstPersonView);
  const shoulder = useZoogiGame((state) => state.overShoulderView);
  const birds = useZoogiGame((state) => state.birdsEyeView);
  const launch = useZoogiGame((state) => state.launchPadView);
  const toggleFirstPersonView = useZoogiGame((state) => state.toggleFirstPersonView);
  const toggleOverShoulderView = useZoogiGame((state) => state.toggleOverShoulderView);
  const toggleBirdsEyeView = useZoogiGame((state) => state.toggleBirdsEyeView);
  const toggleLaunchPadView = useZoogiGame((state) => state.toggleLaunchPadView);
  const lockOn = useZoogiGame((state) => state.lockOnEnabled);
  const toggleLockOn = useZoogiGame((state) => state.toggleLockOn);
  const zoogiId = useZoogiGame((state) => {
    if (state.gameMode === "local_multiplayer" && state.currentLocalPlayerIndex > 0) {
      return state.enemies[state.currentLocalPlayerIndex - 1]?.zoogi.id ?? "";
    }
    return state.playerEntity?.zoogi.id ?? "";
  });
  const gameMode = useZoogiGame((state) => state.gameMode);
  const orbMultiplier = useZoogiGame((state) => state.orbMultiplier);
  const incrementOrbMultiplier = useZoogiGame((state) => state.incrementOrbMultiplier);
  const decrementOrbMultiplier = useZoogiGame((state) => state.decrementOrbMultiplier);
  const showCollision = useZoogiGame((state) => state.showCollisionTuningPanel);
  const setShowCollision = useZoogiGame((state) => state.setShowCollisionTuningPanel);
  const showAi = useZoogiGame((state) => state.showAiControlsPanel);
  const setShowAi = useZoogiGame((state) => state.setShowAiControlsPanel);
  const devCamera = useZoogiGame((state) => state.developerCamera);
  const devMove = useZoogiGame((state) => state.developerMoveMode);

  return (
    <div
      data-testid="match-menu"
      className={`allow-pan-y pointer-events-auto absolute bottom-full right-0 z-30 mb-2 w-[min(100%,20rem)] overflow-y-auto rounded-2xl bg-black/90 px-3 py-2 text-white shadow-2xl backdrop-blur-md ${tall ? "max-h-[32.75rem]" : "max-h-[calc(100dvh-13.5rem)]"}`}
    >
      <p className="mb-1 text-base font-black">Menu</p>

      <p className="mb-1 text-sm font-bold text-white/60">Camera</p>
      <div className="grid grid-cols-2 gap-1.5">
        <button type="button" onClick={toggleFirstPersonView} className={cameraButtonClass(firstPerson, "bg-green-500 text-white")}>
          <User size={16} className="mr-1 inline" /> First person
        </button>
        <button type="button" onClick={toggleOverShoulderView} className={cameraButtonClass(shoulder, "bg-orange-500 text-white")}>
          <ScanEye size={16} className="mr-1 inline" /> Shoulder
        </button>
        <button type="button" onClick={toggleBirdsEyeView} className={cameraButtonClass(birds, "bg-blue-500 text-white")}>
          {birds ? <EyeOff size={16} className="mr-1 inline" /> : <Eye size={16} className="mr-1 inline" />}
          Bird&apos;s eye
        </button>
        <button type="button" onClick={toggleLaunchPadView} className={cameraButtonClass(launch, "bg-cyan-500 text-white")}>
          <Target size={16} className="mr-1 inline" /> Launch pad
        </button>
      </div>
      {birds && (
        <button type="button" onClick={onBirdHelp} className="mt-2 min-h-11 w-full rounded-xl bg-cyan-700 px-3 text-left text-sm font-bold">
          Bird&apos;s eye help
        </button>
      )}

      <p className="mb-1 mt-3 text-sm font-bold text-white/60">Match</p>
      {zoogiId !== "pinpoint" && (
        <button type="button" onClick={toggleLockOn} className={`mb-2 min-h-11 w-full rounded-xl px-3 text-left text-sm font-bold ${lockOn ? "bg-purple-500" : "bg-white/10"}`}>
          <Crosshair size={16} className="mr-1 inline" /> {lockOn ? "Lock-on aim is on" : "Lock-on aim"}
        </button>
      )}
      <button
        type="button"
        onClick={() => useZoogiGame.getState().openTutorial()}
        className="mb-2 flex min-h-11 w-full items-center gap-2 rounded-xl bg-white/10 px-3 text-left text-sm font-bold"
      >
        <HelpCircle size={18} /> How to play
      </button>
      <button
        type="button"
        onClick={() => useZoogiGame.getState().setPhase("menu")}
        className="flex min-h-11 w-full items-center gap-2 rounded-xl bg-red-600 px-3 text-left text-sm font-bold"
      >
        <Home size={18} /> Quit match
      </button>

      {gameMode === "practice" && (
        <div className="mt-3">
          <p className="mb-1 text-sm font-bold text-white/60">Practice orbs</p>
          <div className="flex items-center gap-2">
            <button type="button" onClick={decrementOrbMultiplier} disabled={orbMultiplier <= 1} className="flex h-11 w-11 items-center justify-center rounded-xl bg-pink-500 disabled:bg-white/10">
              <Minus size={18} />
            </button>
            <span className="min-w-10 text-center text-base font-black text-pink-300">x{orbMultiplier}</span>
            <button type="button" onClick={incrementOrbMultiplier} disabled={orbMultiplier >= 3} className="flex h-11 w-11 items-center justify-center rounded-xl bg-pink-500 disabled:bg-white/10">
              <Plus size={18} />
            </button>
          </div>
        </div>
      )}

      {devTools && (
        <div className="mt-3">
          <p className="mb-1 text-sm font-bold text-amber-200/80">Debug</p>
          <div className="grid grid-cols-2 gap-1.5">
            <button
              type="button"
              className="min-h-11 rounded-xl bg-amber-700 px-2 text-sm font-bold"
              onClick={() => {
                const id = useZoogiGame.getState().playerEntity?.id;
                if (id) useZoogiGame.getState().debugUnlockPower(id);
              }}
            >
              Unlock mine
            </button>
            <button
              type="button"
              className="min-h-11 rounded-xl bg-orange-700 px-2 text-sm font-bold"
              onClick={() => {
                const foe = useZoogiGame.getState().enemies.find((enemy) => !enemy.isKnockedOut);
                if (foe) useZoogiGame.getState().debugUnlockPower(foe.id);
              }}
            >
              Unlock foe
            </button>
            <button type="button" onClick={() => setShowCollision(!showCollision)} className={`min-h-11 rounded-xl px-2 text-sm font-bold ${showCollision ? "bg-cyan-500" : "bg-white/10"}`}>
              <Target size={14} className="mr-1 inline" /> Collision
            </button>
            <button type="button" onClick={() => setShowAi(!showAi)} className={`min-h-11 rounded-xl px-2 text-sm font-bold ${showAi ? "bg-orange-500" : "bg-white/10"}`}>
              <Zap size={14} className="mr-1 inline" /> AI
            </button>
            <button type="button" onClick={() => useZoogiGame.getState().toggleDeveloperCamera()} className={`min-h-11 rounded-xl px-2 text-sm font-bold ${devCamera ? "bg-yellow-400 text-black" : "bg-white/10"}`}>
              <Video size={14} className="mr-1 inline" /> Dev camera
            </button>
            <button type="button" onClick={() => useZoogiGame.getState().toggleDeveloperMoveMode()} className={`min-h-11 rounded-xl px-2 text-sm font-bold ${devMove ? "bg-orange-400 text-black" : "bg-white/10"}`}>
              <Move size={14} className="mr-1 inline" /> Move
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

function InfoPanel({ onClose }: { onClose: () => void }) {
  const facts = useMatchFacts();
  const level = useProgression((state) => state.level);
  const coins = useProgression((state) => state.coins);
  if (!facts) return null;

  return (
    <div className="allow-pan-y pointer-events-auto absolute bottom-full left-0 z-30 mb-2 max-h-[min(32rem,calc(100dvh-14rem))] w-[min(100%,22rem)] overflow-y-auto rounded-2xl bg-black/90 p-4 text-white shadow-2xl backdrop-blur-md">
      <div className="mb-2 flex items-start justify-between gap-2">
        <div>
          <p className="text-lg font-black leading-tight">{facts.zoogiName}</p>
          <p className="text-sm text-white/70">{facts.zoogiType}</p>
        </div>
        <button type="button" onClick={onClose} aria-label="Close match info" className="flex h-11 w-11 items-center justify-center rounded-xl bg-white/10">
          <X size={20} />
        </button>
      </div>
      <div className="mb-3 flex gap-2 text-sm font-bold">
        <span className="rounded-full bg-purple-600 px-2 py-1">Lv.{level}</span>
        <span className="flex items-center gap-1 rounded-full bg-yellow-600 px-2 py-1">
          <Coins size={14} /> {coins}
        </span>
        <span className="rounded-full bg-white/10 px-2 py-1">Wins {facts.wins}</span>
      </div>
      <p className="text-sm font-bold text-amber-200">{facts.abilityTitle}</p>
      <p className="mt-1 text-sm leading-snug text-white/90">{facts.abilityDetail}</p>
      {facts.abilityHow && <p className="mt-1 text-sm leading-snug text-white/70">{facts.abilityHow}</p>}
      <p className="mb-1 mt-3 text-sm font-bold text-cyan-200">{facts.modeTitle}</p>
      <p className="text-sm text-white/75">{facts.modeBlurb}</p>
      <p className="mb-1 mt-3 text-sm font-bold text-white/60">Points</p>
      <ul className="space-y-1 text-sm text-white/85">
        <li>Orb knock-off <span className="font-bold text-yellow-300">+{KNOCKOUT_SCORE_ORB}</span></li>
        <li>Opponent knock-off <span className="font-bold text-yellow-300">+{KNOCKOUT_SCORE_PLAYER}</span></li>
        <li>Bumper touch <span className="font-bold text-yellow-300">+{BUMPER_SCORE}</span></li>
        <li>Score zone <span className="font-bold text-yellow-300">+{ZONE_SCORE_ORB}</span></li>
        <li>Fall off the island <span className="font-bold text-red-300">-{KNOCKOUT_PENALTY}</span></li>
      </ul>
    </div>
  );
}

function BirdsEyeHelp({ onClose }: { onClose: () => void }) {
  return (
    <div className="pointer-events-auto fixed inset-0 z-50 flex items-center justify-center bg-black/60 p-4" onClick={onClose}>
      <div className="w-full max-w-sm rounded-2xl border border-cyan-400/30 bg-blue-950 p-5 text-white shadow-2xl" onClick={(event) => event.stopPropagation()}>
        <div className="mb-3 flex items-center justify-between">
          <h3 className="flex items-center gap-2 text-xl font-black">
            <Eye className="h-5 w-5 text-cyan-300" /> Bird&apos;s eye
          </h3>
          <button type="button" onClick={onClose} aria-label="Close bird's eye help" className="flex h-11 w-11 items-center justify-center rounded-full bg-white/10">
            <X className="h-5 w-5" />
          </button>
        </div>
        <ul className="space-y-2 text-sm leading-snug text-white/90">
          <li>Swipe left or right to rotate the arena.</li>
          <li>Pinch with two fingers to zoom.</li>
          <li>Tap near your Zoogi and drag to aim.</li>
        </ul>
        <button type="button" onClick={onClose} className="mt-4 min-h-11 w-full rounded-xl bg-cyan-500 font-black">
          Got it
        </button>
      </div>
    </div>
  );
}

function ArcLauncher() {
  const lockOnEnabled = useZoogiGame((state) => state.lockOnEnabled);
  const lockOnTargetId = useZoogiGame((state) => state.lockOnTargetId);
  const arcType = useZoogiGame((state) => state.arcType);
  const setArcType = useZoogiGame((state) => state.setArcType);
  const straightMode = useZoogiGame((state) => state.straightMode);
  const toggleStraightMode = useZoogiGame((state) => state.toggleStraightMode);
  const tangentOffset = useZoogiGame((state) => state.tangentOffset);
  const setTangentOffset = useZoogiGame((state) => state.setTangentOffset);
  const triggerArcLaunch = useZoogiGame((state) => state.triggerArcLaunch);
  if (!lockOnEnabled || !lockOnTargetId) return null;
  const armed = Boolean(arcType || straightMode);

  return (
    <div className="pointer-events-auto absolute bottom-full left-0 z-20 mb-2 flex flex-col gap-2">
      <div className="flex items-center gap-2">
        <button type="button" onClick={() => setArcType(arcType === "left" ? null : "left")} className={`h-12 w-12 rounded-xl text-lg font-black ${arcType === "left" ? "bg-purple-500 text-white ring-2 ring-white" : "bg-black/75 text-white"}`}>←</button>
        <button type="button" onClick={toggleStraightMode} className={`h-12 w-12 rounded-xl text-lg font-black ${straightMode ? "bg-green-500 text-white ring-2 ring-white" : arcType === "over" ? "bg-purple-500 text-white ring-2 ring-white" : "bg-black/75 text-white"}`} title="Straight, then over">↑</button>
        <button type="button" onClick={() => setArcType(arcType === "right" ? null : "right")} className={`h-12 w-12 rounded-xl text-lg font-black ${arcType === "right" ? "bg-purple-500 text-white ring-2 ring-white" : "bg-black/75 text-white"}`}>→</button>
      </div>
      {straightMode && (
        <div className="flex items-center gap-2">
          <button type="button" onClick={() => setTangentOffset(tangentOffset === "left" ? "none" : "left")} className={`h-11 w-11 rounded-xl font-bold ${tangentOffset === "left" ? "bg-orange-500 text-white" : "bg-black/75 text-white"}`}>↖</button>
          <span className="text-xs font-bold text-white/70">ANGLE</span>
          <button type="button" onClick={() => setTangentOffset(tangentOffset === "right" ? "none" : "right")} className={`h-11 w-11 rounded-xl font-bold ${tangentOffset === "right" ? "bg-orange-500 text-white" : "bg-black/75 text-white"}`}>↗</button>
        </div>
      )}
      <button type="button" onClick={() => armed && triggerArcLaunch()} disabled={!armed} className={`h-12 rounded-xl text-sm font-black ${armed ? "bg-green-500 text-white" : "bg-white/20 text-white/40"}`}>
        LAUNCH
      </button>
    </div>
  );
}

function AbilityNotice() {
  const notice = useZoogiGame((state) => state.abilityNotice);
  useEffect(() => {
    if (!notice) return;
    const wait = Math.max(0, notice.until - Date.now());
    const timer = setTimeout(() => {
      if (useZoogiGame.getState().abilityNotice?.until === notice.until) {
        useZoogiGame.setState({ abilityNotice: null });
      }
    }, wait);
    return () => clearTimeout(timer);
  }, [notice]);
  if (!notice) return null;
  return (
    <div className="rounded-xl border border-yellow-300 bg-black/85 px-4 py-2 text-sm font-bold text-white shadow-lg">
      {notice.text}
    </div>
  );
}

function XpToasts() {
  const recentXpGains = useProgression((state) => state.recentXpGains);
  const removeXpGain = useProgression((state) => state.removeXpGain);
  return (
    <div className="pointer-events-none absolute bottom-full left-16 z-20 mb-2 flex flex-col gap-1">
      <AnimatePresence>
        {recentXpGains.slice(-3).map((gain) => (
          <XpToast key={gain.id} amount={gain.amount} reason={gain.reason} timestamp={gain.timestamp} onDone={removeXpGain} />
        ))}
      </AnimatePresence>
    </div>
  );
}

function XpToast({ amount, reason, timestamp, onDone }: { amount: number; reason: string; timestamp: number; onDone: (timestamp: number) => void }) {
  useEffect(() => {
    const timer = setTimeout(() => onDone(timestamp), 3000);
    return () => clearTimeout(timer);
  }, [timestamp, onDone]);
  return (
    <motion.div
      initial={{ opacity: 0, x: -24 }}
      animate={{ opacity: 1, x: 0 }}
      exit={{ opacity: 0, x: -24 }}
      className="flex items-center gap-2 rounded-lg bg-gradient-to-r from-purple-600/90 to-blue-600/90 px-3 py-2 shadow-lg"
    >
      <Star className="h-4 w-4 text-yellow-300" />
      <span className="text-sm font-bold text-white">+{amount} XP</span>
      <span className="text-sm text-white/75">{reason}</span>
    </motion.div>
  );
}

function LevelUpWatcher() {
  const level = useProgression((state) => state.level);
  const [show, setShow] = useState(false);
  const [celebrated, setCelebrated] = useState(0);
  const prev = useRef(level);
  useEffect(() => {
    if (level > prev.current && level > 1) {
      setCelebrated(level);
      setShow(true);
    }
    prev.current = level;
  }, [level]);
  return (
    <AnimatePresence>
      {show && <LevelUpCelebration level={celebrated} onComplete={() => setShow(false)} />}
    </AnimatePresence>
  );
}

function LevelUpCelebration({ level, onComplete }: { level: number; onComplete: () => void }) {
  const [showConfetti, setShowConfetti] = useState(true);
  const { playSound } = useAudio();
  const onCompleteRef = useRef(onComplete);
  onCompleteRef.current = onComplete;
  const hasPlayedSound = useRef(false);
  useEffect(() => {
    if (!hasPlayedSound.current) {
      playSound("level_up");
      hasPlayedSound.current = true;
    }
    const timer = setTimeout(() => onCompleteRef.current(), 3000);
    const confettiTimer = setTimeout(() => setShowConfetti(false), 2500);
    return () => {
      clearTimeout(timer);
      clearTimeout(confettiTimer);
    };
  }, [playSound]);

  return (
    <>
      {showConfetti && (
        <Confetti width={window.innerWidth} height={window.innerHeight} numberOfPieces={150} recycle={false} gravity={0.3} colors={["#FFD700", "#FFA500", "#FF6347", "#9400D3", "#00CED1"]} />
      )}
      <motion.div initial={{ opacity: 0, scale: 0.5 }} animate={{ opacity: 1, scale: 1 }} exit={{ opacity: 0, scale: 1.5 }} className="pointer-events-none fixed inset-0 z-50 flex items-center justify-center">
        <div className="rounded-2xl bg-gradient-to-b from-yellow-500/95 to-orange-600/95 px-8 py-6 text-center shadow-2xl">
          <Trophy className="mx-auto mb-2 h-16 w-16 text-yellow-100" />
          <p className="text-lg font-medium text-yellow-100">LEVEL UP!</p>
          <p className="mt-1 text-4xl font-black text-white">Level {level}</p>
        </div>
      </motion.div>
    </>
  );
}

const desktopSafeArea = {
  paddingTop: "max(1rem, env(safe-area-inset-top))",
  paddingRight: "max(1rem, env(safe-area-inset-right))",
  paddingBottom: "max(1rem, env(safe-area-inset-bottom))",
  paddingLeft: "max(1rem, env(safe-area-inset-left))",
} as const;

export const MatchHud = memo(function MatchHud() {
  const layout = useViewportLayout();
  const phone = layout.phone;
  const devTools = useDevTools();
  const [menuOpen, setMenuOpen] = useState(false);
  const [infoOpen, setInfoOpen] = useState(false);
  const [birdHelp, setBirdHelp] = useState(false);
  const cameraOn = useZoogiGame((state) => state.firstPersonView || state.overShoulderView || state.birdsEyeView || state.launchPadView);
  const neon = useZoogiGame((state) => state.selectedMap === "neon");
  const showHint = !phone;

  useEffect(() => {
    const onKey = (event: KeyboardEvent) => {
      if (event.repeat || typingTarget(event)) return;
      if (event.key === "m" || event.key === "M") {
        event.preventDefault();
        setMenuOpen((open) => !open);
        setInfoOpen(false);
      } else if (event.key === "Escape") {
        setMenuOpen(false);
        setInfoOpen(false);
        setBirdHelp(false);
      }
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, []);

  return (
    <div className="pointer-events-none fixed inset-0 z-10" data-testid="match-hud">
      <NeonStrip />
      <div
        className={`flex h-full w-full flex-col justify-between ${phone ? "phone-safe-top phone-safe-bottom phone-safe-x" : ""}`}
        style={phone ? undefined : desktopSafeArea}
      >
        <div className={neon ? "mt-14 max-w-full" : "max-w-full"}>
          <TopCluster />
          <div className="mt-2 max-w-sm">
            <AbilityNotice />
          </div>
        </div>
        <div className="relative z-20">
          {(menuOpen || infoOpen) && (
            <button
              type="button"
              aria-label="Close panels"
              className="pointer-events-auto fixed inset-0 z-10"
              onClick={() => {
                setMenuOpen(false);
                setInfoOpen(false);
              }}
            />
          )}
          <XpToasts />
          <ArcLauncher />
          {infoOpen && <InfoPanel onClose={() => setInfoOpen(false)} />}
          {menuOpen && (
            <ToolsMenu
              devTools={devTools}
              tall={!phone || layout.portrait}
              onBirdHelp={() => {
                setBirdHelp(true);
                setMenuOpen(false);
              }}
            />
          )}
          <div className="relative z-20 flex items-end justify-between gap-2">
          <button
            type="button"
            aria-label="Match info"
            aria-expanded={infoOpen}
            onClick={() => {
              setInfoOpen((open) => !open);
              setMenuOpen(false);
            }}
            className={`pointer-events-auto flex h-12 w-12 items-center justify-center rounded-2xl shadow-lg ${infoOpen ? "bg-white text-black" : "bg-black/75 text-white"}`}
          >
            <Info size={22} />
          </button>
          <div className="flex items-end gap-2">
            {showHint && <p className="mb-3 hidden text-sm font-semibold text-white/80 sm:block">E ability · M menu</p>}
            <AbilityButton />
            <MenuButton open={menuOpen} flagged={cameraOn} onClick={() => { setMenuOpen((open) => !open); setInfoOpen(false); }} />
          </div>
        </div>
        </div>
      </div>
      <LevelUpWatcher />
      {birdHelp && <BirdsEyeHelp onClose={() => setBirdHelp(false)} />}
    </div>
  );
});

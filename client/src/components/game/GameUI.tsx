import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { BUMPER_SCORE, KNOCKOUT_PENALTY, KNOCKOUT_SCORE_ORB, KNOCKOUT_SCORE_PLAYER, ZONE_SCORE_ORB } from "@/lib/arenaConstants";
import { motion, AnimatePresence } from "framer-motion";
import { Eye, EyeOff, Home, HelpCircle, Users, User, Zap, Crosshair, Star, Trophy, Coins, Info, X, Phone, Video, Move, Check, ArrowUp, ArrowDown, ArrowLeft, ArrowRight, ChevronUp, ChevronDown, RotateCcw, RotateCw, Minus, Plus, Anchor, Download, ScanEye, Camera, Target, Lock, Flame, Package, MapPin, Palette } from "lucide-react";
import { moveSelectedElementByArrows, confirmPlacements, rotateSelectedElement, snapToGround, getIncrement, setIncrement } from "./DeveloperMoveControls";
import { getGlobalZoogiScale, setGlobalZoogiScale } from "./Zoogi";
import { getGlobalOrbScale, setGlobalOrbScale } from "./Orb";
import { PhysicsControlPanel } from "./PhysicsControlPanel";
import { MapEditorPanel } from "./MapEditorPanel";
import { BackgroundControlPanel, DEFAULT_BACKGROUND_SETTINGS } from "./BackgroundControlPanel";
import { ZoneEditorPanel } from "./ZoneEditorPanel";
import { CollisionTuningPanel } from "./CollisionTuningPanel";
import { AIControlsPanel } from "./AIControlsPanel";
import { exportAllOffsets } from "@/lib/treeOffsets";
import { NeonScoreboard } from "./NeonScoreboard";
import { useAudio } from "@/lib/stores/useAudio";
import { useProgression } from "@/lib/stores/useProgression";
import { useEffect, useState, useRef, useCallback, useMemo, PointerEvent as ReactPointerEvent } from "react";
import Confetti from "react-confetti";

interface HoldButtonProps {
  onAction: () => void;
  className: string;
  title?: string;
  children: React.ReactNode;
}

function HoldButton({ onAction, className, title, children }: HoldButtonProps) {
  const intervalRef = useRef<NodeJS.Timeout | null>(null);
  const buttonRef = useRef<HTMLButtonElement>(null);
  const activePointerRef = useRef<number | null>(null);
  
  const stopHold = useCallback(() => {
    if (intervalRef.current) {
      clearInterval(intervalRef.current);
      intervalRef.current = null;
    }
    if (activePointerRef.current !== null && buttonRef.current) {
      try {
        buttonRef.current.releasePointerCapture(activePointerRef.current);
      } catch (e) {
        // Pointer capture may already be released
      }
      activePointerRef.current = null;
    }
  }, []);
  
  const startHold = useCallback((pointerId: number) => {
    if (intervalRef.current) return;
    activePointerRef.current = pointerId;
    if (buttonRef.current) {
      try {
        buttonRef.current.setPointerCapture(pointerId);
      } catch (e) {
        // Pointer capture may fail on some browsers
      }
    }
    onAction();
    intervalRef.current = setInterval(() => {
      onAction();
    }, 100);
  }, [onAction]);
  
  useEffect(() => {
    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current);
      }
    };
  }, []);
  
  return (
    <button
      ref={buttonRef}
      onPointerDown={(e: ReactPointerEvent) => {
        e.preventDefault();
        startHold(e.pointerId);
      }}
      onPointerUp={stopHold}
      onPointerCancel={stopHold}
      onLostPointerCapture={stopHold}
      className={className}
      title={title}
      style={{ touchAction: "none" }}
    >
      {children}
    </button>
  );
}

const getAbilityTriggerText = (zoogiId: string): string => {
  switch (zoogiId) {
    case "wolfgang": return "Tap Pack while moving to send homing clones";
    case "hotstreak": return "Tap Explosion to blast everything nearby";
    case "lars": return "Tap Ricochet, then hit something to home in";
    case "pinpoint": return "Tap Lock-On, then tap an orb or opponent";
    case "bolt": return "Tap Shock to phase through enemies and stun them";
    case "wraps": return "Tap Bind to slow enemies you touch";
    default: return "";
  }
};

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

function CharacterAbilityButton({ zoogiId, entity }: {
  zoogiId: string;
  entity: {
    id: string;
    wolfgangAbilityUnlocked: boolean;
    hotstreakAbilityUnlocked: boolean;
    boltAbilityUnlocked: boolean;
    larsAbilityUnlocked: boolean;
    wrapsAbilityUnlocked: boolean;
  };
}) {
  const activateWolfgangAbility = useZoogiGame((state) => state.activateWolfgangAbility);
  const activateHotstreakAbility = useZoogiGame((state) => state.activateHotstreakAbility);
  const activateLarsAbility = useZoogiGame((state) => state.activateLarsAbility);
  const activateBoltAbility = useZoogiGame((state) => state.activateBoltAbility);
  const activateWrapsAbility = useZoogiGame((state) => state.activateWrapsAbility);
  const toggleLockOn = useZoogiGame((state) => state.toggleLockOn);
  const lockOnEnabled = useZoogiGame((state) => state.lockOnEnabled);

  const spec: Record<string, { label: string; unlocked: boolean; onClick: () => void; active?: boolean }> = {
    wolfgang: { label: "Pack", unlocked: entity.wolfgangAbilityUnlocked, onClick: () => activateWolfgangAbility(entity.id) },
    hotstreak: { label: "Explosion", unlocked: entity.hotstreakAbilityUnlocked, onClick: () => activateHotstreakAbility(entity.id) },
    lars: { label: "Ricochet", unlocked: entity.larsAbilityUnlocked, onClick: () => activateLarsAbility(entity.id) },
    bolt: { label: "Shock", unlocked: entity.boltAbilityUnlocked, onClick: () => activateBoltAbility(entity.id) },
    wraps: { label: "Bind", unlocked: entity.wrapsAbilityUnlocked, onClick: () => activateWrapsAbility(entity.id) },
    pinpoint: { label: "Lock-On", unlocked: true, onClick: toggleLockOn, active: lockOnEnabled },
  };
  const ability = spec[zoogiId];
  if (!ability) return null;

  const ready = ability.unlocked;
  return (
    <button
      onClick={ready ? ability.onClick : undefined}
      disabled={!ready}
      className={`px-3 py-2 rounded-xl backdrop-blur-sm transition-all flex flex-col items-center min-w-[76px] ${
        !ready
          ? "bg-gray-700/80 text-white/50 cursor-not-allowed"
          : ability.active
            ? "bg-purple-500 text-white ring-2 ring-purple-200"
            : "bg-amber-500 text-black hover:bg-amber-400"
      }`}
      title={ready ? getAbilityTriggerText(zoogiId) : "Knock a star orb off the island to unlock"}
    >
      {ready ? <Zap size={18} /> : <Lock size={18} />}
      <span className="text-[11px] font-bold leading-tight mt-1">{ability.label}</span>
      <span className="text-[9px] uppercase tracking-wide">{ready ? "Ready" : "Locked"}</span>
    </button>
  );
}

function modeRules(gameMode: string): { title: string; blurb: string } {
  if (gameMode === "ringer_royale") {
    return { title: "Ringer Royale", blurb: "Knock opponents out of the ring!" };
  }
  if (gameMode === "practice") {
    return { title: "Marble Arena", blurb: "Knock orbs and opponents off the island." };
  }
  if (gameMode === "local_multiplayer") {
    return { title: "Local Match", blurb: "Take turns. Knock orbs and opponents off the island." };
  }
  return { title: "Classic Match", blurb: "Take turns. Knock orbs and opponents off the island." };
}

function XPToast({ amount, reason, timestamp }: { amount: number; reason: string; timestamp: number }) {
  const { removeXpGain } = useProgression();
  
  useEffect(() => {
    const timer = setTimeout(() => {
      removeXpGain(timestamp);
    }, 3000);
    
    return () => clearTimeout(timer);
  }, [timestamp, removeXpGain]);

  return (
    <motion.div
      initial={{ opacity: 0, x: -50, scale: 0.8 }}
      animate={{ opacity: 1, x: 0, scale: 1 }}
      exit={{ opacity: 0, x: -50, scale: 0.8 }}
      transition={{ duration: 0.2 }}
      className="flex items-center gap-2 bg-gradient-to-r from-purple-600/90 to-blue-600/90 px-3 py-1.5 rounded-lg shadow-lg backdrop-blur-sm"
    >
      <Star className="w-4 h-4 text-yellow-400" />
      <span className="text-white font-bold text-sm">+{amount} XP</span>
      <span className="text-white/70 text-xs">{reason}</span>
    </motion.div>
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
        <Confetti
          width={window.innerWidth}
          height={window.innerHeight}
          numberOfPieces={150}
          recycle={false}
          gravity={0.3}
          colors={['#FFD700', '#FFA500', '#FF6347', '#9400D3', '#00CED1']}
        />
      )}
      <motion.div
        initial={{ opacity: 0, scale: 0.5 }}
        animate={{ opacity: 1, scale: 1 }}
        exit={{ opacity: 0, scale: 1.5 }}
        className="fixed inset-0 flex items-center justify-center pointer-events-none z-50"
      >
        <motion.div
          initial={{ y: 50 }}
          animate={{ y: 0 }}
          className="bg-gradient-to-b from-yellow-500/95 to-orange-600/95 px-8 py-6 rounded-2xl shadow-2xl text-center"
        >
          <motion.div
            animate={{ rotate: [0, -10, 10, -10, 10, 0] }}
            transition={{ duration: 0.5, delay: 0.2 }}
          >
            <Trophy className="w-16 h-16 text-yellow-200 mx-auto mb-2" />
          </motion.div>
          <motion.p
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            transition={{ delay: 0.3 }}
            className="text-yellow-100 text-lg font-medium"
          >
            LEVEL UP!
          </motion.p>
          <motion.p
            initial={{ opacity: 0, scale: 0.5 }}
            animate={{ opacity: 1, scale: 1 }}
            transition={{ delay: 0.5 }}
            className="text-white text-4xl font-bold mt-1"
          >
            Level {level}
          </motion.p>
        </motion.div>
      </motion.div>
    </>
  );
}

function LaunchPadTiltControl() {
  const launchPadTilt = useZoogiGame((state) => state.launchPadTilt);
  const setLaunchPadTilt = useZoogiGame((state) => state.setLaunchPadTilt);
  
  return (
    <div className="flex items-center justify-center gap-2 mb-2">
      <span className="text-white/60 text-xs">Tilt:</span>
      <button
        onClick={() => setLaunchPadTilt(launchPadTilt - 2)}
        className="w-8 h-8 rounded-lg bg-cyan-600/90 text-white hover:bg-cyan-500 transition-all flex items-center justify-center"
        title="Less foreground (lower tilt)"
      >
        <Minus size={14} />
      </button>
      <span className="text-white text-xs w-10 text-center font-mono">{launchPadTilt.toFixed(0)}</span>
      <button
        onClick={() => setLaunchPadTilt(launchPadTilt + 2)}
        className="w-8 h-8 rounded-lg bg-cyan-600/90 text-white hover:bg-cyan-500 transition-all flex items-center justify-center"
        title="More foreground (higher tilt)"
      >
        <Plus size={14} />
      </button>
    </div>
  );
}

function LaunchPadPitchControl() {
  const launchPadPitch = useZoogiGame((state) => state.launchPadPitch);
  const setLaunchPadPitch = useZoogiGame((state) => state.setLaunchPadPitch);
  
  return (
    <div className="flex items-center justify-center gap-2 mb-2">
      <span className="text-white/60 text-xs">Pitch:</span>
      <button
        onClick={() => setLaunchPadPitch(launchPadPitch - 2)}
        className="w-8 h-8 rounded-lg bg-purple-600/90 text-white hover:bg-purple-500 transition-all flex items-center justify-center"
        title="Look down"
      >
        <Minus size={14} />
      </button>
      <span className="text-white text-xs w-10 text-center font-mono">{launchPadPitch.toFixed(0)}</span>
      <button
        onClick={() => setLaunchPadPitch(launchPadPitch + 2)}
        className="w-8 h-8 rounded-lg bg-purple-600/90 text-white hover:bg-purple-500 transition-all flex items-center justify-center"
        title="Look up"
      >
        <Plus size={14} />
      </button>
    </div>
  );
}

function LaunchPadHeightControl() {
  const launchPadHeight = useZoogiGame((state) => state.launchPadHeight);
  const setLaunchPadHeight = useZoogiGame((state) => state.setLaunchPadHeight);
  
  return (
    <div className="flex items-center justify-center gap-2 mb-2">
      <span className="text-white/60 text-xs">Height:</span>
      <button
        onClick={() => setLaunchPadHeight(launchPadHeight - 5)}
        className="w-8 h-8 rounded-lg bg-green-600/90 text-white hover:bg-green-500 transition-all flex items-center justify-center"
        title="Move camera down"
      >
        <Minus size={14} />
      </button>
      <span className="text-white text-xs w-10 text-center font-mono">{launchPadHeight.toFixed(0)}</span>
      <button
        onClick={() => setLaunchPadHeight(launchPadHeight + 5)}
        className="w-8 h-8 rounded-lg bg-green-600/90 text-white hover:bg-green-500 transition-all flex items-center justify-center"
        title="Move camera up"
      >
        <Plus size={14} />
      </button>
    </div>
  );
}

export function GameUI() {
  const { playerEntity, enemies, score, orbs, currentRound, maxRounds, playerRoundWins, isPlayerTurn, birdsEyeView, toggleBirdsEyeView, firstPersonView, toggleFirstPersonView, overShoulderView, toggleOverShoulderView, launchPadView, toggleLaunchPadView, setPhase, gameTimer, openTutorial, gameMode, localPlayers, currentLocalPlayerIndex, activateWolfgangAbility, canUseWolfgangAbility, activateHotstreakAbility, canUseHotstreakAbility, activateBoltAbility, canUseBoltAbility, lockOnEnabled, toggleLockOn, sessionId, arcType, setArcType, straightMode, toggleStraightMode, tangentOffset, setTangentOffset, lockOnTargetId, triggerArcLaunch, orbMultiplier, incrementOrbMultiplier, decrementOrbMultiplier, restrictionPhaseActive, restrictionPhaseStartTime } = useZoogiGame();
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  const showCollisionTuningPanel = useZoogiGame((state) => state.showCollisionTuningPanel);
  const setShowCollisionTuningPanel = useZoogiGame((state) => state.setShowCollisionTuningPanel);
  const showAiControlsPanel = useZoogiGame((state) => state.showAiControlsPanel);
  const setShowAiControlsPanel = useZoogiGame((state) => state.setShowAiControlsPanel);
  const { level, coins, getXpProgress, recentXpGains, equippedTitle } = useProgression();
  
  const [showLevelUp, setShowLevelUp] = useState(false);
  const [celebratedLevel, setCelebratedLevel] = useState(0);
  const [showBirdsEyeTutorial, setShowBirdsEyeTutorial] = useState(false);
  const [showExportMenu, setShowExportMenu] = useState(false);
  const isLocalMultiplayerMode = gameMode === "local_multiplayer";
  const [showPlayerPanel, setShowPlayerPanel] = useState(true);
  const [showPanelHint, setShowPanelHint] = useState(false);
  const devTools = useDevTools();
  const abilityNotice = useZoogiGame((state) => state.abilityNotice);
  const [showMapEditorPanel, setShowMapEditorPanel] = useState(false);
  const [showBackgroundPanel, setShowBackgroundPanel] = useState(false);
  const prevLevelRef = useRef(level);
  const prevPanelStateRef = useRef(true);
  const wasLocalMultiplayerRef = useRef(false);
  
  const isMapEditor = gameMode === "map_editor";

  useEffect(() => {
    if (!abilityNotice) return;
    const wait = Math.max(0, abilityNotice.until - Date.now());
    const timer = setTimeout(() => {
      if (useZoogiGame.getState().abilityNotice?.until === abilityNotice.until) {
        useZoogiGame.setState({ abilityNotice: null });
      }
    }, wait);
    return () => clearTimeout(timer);
  }, [abilityNotice]);
  
  useEffect(() => {
    if (level > prevLevelRef.current && level > 1) {
      setCelebratedLevel(level);
      setShowLevelUp(true);
    }
    prevLevelRef.current = level;
  }, [level]);
  
  useEffect(() => {
    wasLocalMultiplayerRef.current = isLocalMultiplayerMode;
  }, [isLocalMultiplayerMode]);
  
  const xpProgress = getXpProgress();

  if (!playerEntity && !isMapEditor) return null;

  // Map Editor mode - get reactive state with hooks
  const selectedEditorModelId = useZoogiGame(state => state.selectedEditorModelId);
  const editorSnapEnabled = useZoogiGame(state => state.editorSnapEnabled);
  const editorPlacedModels = useZoogiGame(state => state.editorPlacedModels);
  const developerCamera = useZoogiGame(state => state.developerCamera);
  const backgroundSettings = useZoogiGame(state => state.backgroundSettings);
  const setBackgroundSettings = useZoogiGame(state => state.setBackgroundSettings);
  const wallSettings = useZoogiGame(state => state.wallSettings);
  const setWallSettings = useZoogiGame(state => state.setWallSettings);
  const zoneSettings = useZoogiGame(state => state.zoneSettings);
  const setZoneSettings = useZoogiGame(state => state.setZoneSettings);
  
  // Map Editor mode - show simplified UI
  if (isMapEditor) {
    const { 
      toggleEditorSnap, 
      moveSelectedEditorModel, 
      rotateSelectedEditorModel,
      scaleSelectedEditorModel,
      selectEditorModel,
      removeEditorPlacedModel,
      toggleDeveloperCamera
    } = useZoogiGame.getState();
    
    const selectedModel = editorPlacedModels.find(m => m.id === selectedEditorModelId);
    
    return (
      <>
      <div className="fixed inset-0 pointer-events-none z-10">
        {/* Map Editor Header */}
        <div className="absolute top-4 left-4 right-4 flex justify-between items-start pointer-events-auto">
          <div className="bg-black/70 backdrop-blur-md rounded-xl p-3">
            <div className="flex items-center gap-2">
              <MapPin className="w-5 h-5 text-purple-400" />
              <h2 className="text-white font-bold">Map Editor</h2>
            </div>
            <p className="text-white/60 text-xs mt-1">Place and arrange 3D models</p>
          </div>
          
          <button
            onClick={() => useZoogiGame.getState().returnToMenu()}
            className="bg-red-600/80 hover:bg-red-500 text-white px-4 py-2 rounded-lg font-medium transition-colors"
          >
            Exit Editor
          </button>
        </div>
        
        {/* All Buttons - Left Side */}
        <div className={`absolute top-1/2 -translate-y-1/2 flex flex-col gap-2 pointer-events-auto transition-all ${showBackgroundPanel ? 'left-72' : 'left-4'}`}>
          {/* Models, Save, Export */}
          <button
            onClick={() => setShowMapEditorPanel(!showMapEditorPanel)}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${showMapEditorPanel ? 'bg-purple-500' : 'bg-purple-600 hover:bg-purple-500'} text-white`}
            title="Models"
          >
            <Package size={20} />
          </button>
          <button
            onClick={async () => {
              try {
                const success = await useZoogiGame.getState().saveMapDecorations();
                if (success) {
                  alert("Map saved successfully!");
                } else {
                  alert("Failed to save map");
                }
              } catch (err) {
                console.error("Save error:", err);
                alert("Failed to save map - please try again");
              }
            }}
            className="p-3 rounded-xl backdrop-blur-sm bg-green-600 hover:bg-green-500 text-white transition-all"
            title="Save"
          >
            <Check size={20} />
          </button>
          <button
            onClick={() => useZoogiGame.getState().exportBackgroundSettings()}
            className="p-3 rounded-xl backdrop-blur-sm bg-blue-600 hover:bg-blue-500 text-white transition-all"
            title="Export"
          >
            <Download size={20} />
          </button>
          <button
            onClick={() => {
              console.log("[GameUI] Pink button clicked, current showBackgroundPanel:", showBackgroundPanel);
              setShowBackgroundPanel(!showBackgroundPanel);
            }}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${showBackgroundPanel ? 'bg-pink-500' : 'bg-pink-600 hover:bg-pink-500'} text-white`}
            title="Background Controls"
          >
            <Palette size={20} />
          </button>
          <button
            onClick={() => useZoogiGame.getState().setShowCollisionTuningPanel(!useZoogiGame.getState().showCollisionTuningPanel)}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${useZoogiGame.getState().showCollisionTuningPanel ? 'bg-cyan-500' : 'bg-cyan-600 hover:bg-cyan-500'} text-white`}
            title="Collision Tuning"
          >
            <Target size={20} />
          </button>
          
          <div className="h-2" /> {/* Spacer */}
          
          {/* Camera View Buttons */}
          <button
            onClick={toggleFirstPersonView}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              firstPersonView 
                ? "bg-green-500/80 text-white" 
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="First person view"
          >
            <User size={20} />
          </button>
          
          <button
            onClick={toggleOverShoulderView}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              overShoulderView 
                ? "bg-orange-500/80 text-white" 
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="Over the shoulder view"
          >
            <ScanEye size={20} />
          </button>
          
          <button
            onClick={toggleBirdsEyeView}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              birdsEyeView 
                ? "bg-blue-500/80 text-white" 
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="Bird's eye view"
          >
            {birdsEyeView ? <EyeOff size={20} /> : <Eye size={20} />}
          </button>
          
          <button
            onClick={toggleLaunchPadView}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              launchPadView 
                ? "bg-cyan-500/80 text-white" 
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="Launch Pad view"
          >
            <Target size={20} />
          </button>
          
          <button
            onClick={toggleDeveloperCamera}
            className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
              developerCamera
                ? "bg-yellow-500/90 text-black ring-2 ring-white"
                : "bg-black/70 text-white/80 hover:bg-black/90"
            }`}
            title="Developer Camera (free roam)"
          >
            <Video size={20} />
          </button>
        </div>
        
        {/* Background Control Panel - Only show when not in map editor mode (map editor has its own) */}
        {showBackgroundPanel && !isMapEditor && (
          <div className="pointer-events-auto">
            <BackgroundControlPanel
              settings={backgroundSettings}
              onSettingsChange={setBackgroundSettings}
              onImageUpload={(imageDataUrl) => {
                setBackgroundSettings({
                  ...backgroundSettings,
                  customImage: imageDataUrl
                });
              }}
              wallSettings={wallSettings}
              onWallSettingsChange={setWallSettings}
              zoneSettings={zoneSettings}
              onZoneSettingsChange={setZoneSettings}
            />
          </div>
        )}
        
        {/* Transform Controls - Bottom Right */}
        {selectedModel && (
          <div className="absolute bottom-4 right-4 bg-black/80 backdrop-blur-md rounded-xl p-4 pointer-events-auto">
            <div className="text-white text-sm font-medium mb-2">{selectedModel.name}</div>
            <div className="text-white/60 text-xs mb-3">
              Pos: {selectedModel.position.map(v => v.toFixed(1)).join(", ")}
            </div>
            
            {/* Movement Controls */}
            <div className="grid grid-cols-3 gap-1 mb-2">
              <div />
              <HoldButton
                onAction={() => moveSelectedEditorModel(0, 0, -0.5)}
                className="w-10 h-10 bg-slate-700 hover:bg-slate-600 rounded-lg flex items-center justify-center text-white"
              >
                <ArrowUp size={16} />
              </HoldButton>
              <div />
              <HoldButton
                onAction={() => moveSelectedEditorModel(-0.5, 0, 0)}
                className="w-10 h-10 bg-slate-700 hover:bg-slate-600 rounded-lg flex items-center justify-center text-white"
              >
                <ArrowLeft size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => moveSelectedEditorModel(0, 0, 0.5)}
                className="w-10 h-10 bg-slate-700 hover:bg-slate-600 rounded-lg flex items-center justify-center text-white"
              >
                <ArrowDown size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => moveSelectedEditorModel(0.5, 0, 0)}
                className="w-10 h-10 bg-slate-700 hover:bg-slate-600 rounded-lg flex items-center justify-center text-white"
              >
                <ArrowRight size={16} />
              </HoldButton>
            </div>
            
            {/* Height & Rotation Controls */}
            <div className="flex gap-1 mb-2">
              <HoldButton
                onAction={() => moveSelectedEditorModel(0, 0.5, 0)}
                className="flex-1 h-10 bg-blue-600 hover:bg-blue-500 rounded-lg flex items-center justify-center text-white"
                title="Raise"
              >
                <ChevronUp size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => moveSelectedEditorModel(0, -0.5, 0)}
                className="flex-1 h-10 bg-blue-600 hover:bg-blue-500 rounded-lg flex items-center justify-center text-white"
                title="Lower"
              >
                <ChevronDown size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => rotateSelectedEditorModel("y", -15)}
                className="flex-1 h-10 bg-orange-600 hover:bg-orange-500 rounded-lg flex items-center justify-center text-white"
                title="Rotate Left"
              >
                <RotateCcw size={16} />
              </HoldButton>
              <HoldButton
                onAction={() => rotateSelectedEditorModel("y", 15)}
                className="flex-1 h-10 bg-orange-600 hover:bg-orange-500 rounded-lg flex items-center justify-center text-white"
                title="Rotate Right"
              >
                <RotateCw size={16} />
              </HoldButton>
            </div>
            
            {/* Scale Controls */}
            <div className="flex gap-1 mb-2">
              <HoldButton
                onAction={() => scaleSelectedEditorModel(-0.1)}
                className="flex-1 h-10 bg-purple-600 hover:bg-purple-500 rounded-lg flex items-center justify-center text-white"
                title="Scale Down"
              >
                <Minus size={16} />
              </HoldButton>
              <div className="flex-1 h-10 bg-slate-700 rounded-lg flex items-center justify-center text-white text-xs">
                {selectedModel?.scale[0].toFixed(1)}x
              </div>
              <HoldButton
                onAction={() => scaleSelectedEditorModel(0.1)}
                className="flex-1 h-10 bg-purple-600 hover:bg-purple-500 rounded-lg flex items-center justify-center text-white"
                title="Scale Up"
              >
                <Plus size={16} />
              </HoldButton>
            </div>
            
            {/* Action Buttons */}
            <div className="flex gap-1">
              <button
                onClick={toggleEditorSnap}
                className={`flex-1 py-2 rounded-lg flex items-center justify-center gap-1 text-white text-xs font-medium ${
                  editorSnapEnabled ? "bg-cyan-600" : "bg-slate-600"
                }`}
              >
                <Anchor size={12} />
                Snap
              </button>
              <button
                onClick={() => selectEditorModel(null)}
                className="flex-1 py-2 bg-green-600 hover:bg-green-500 rounded-lg flex items-center justify-center gap-1 text-white text-xs font-medium"
              >
                <Check size={12} />
                Confirm
              </button>
              <button
                onClick={() => {
                  removeEditorPlacedModel(selectedEditorModelId!);
                  selectEditorModel(null);
                }}
                className="flex-1 py-2 bg-red-600 hover:bg-red-500 rounded-lg flex items-center justify-center gap-1 text-white text-xs font-medium"
              >
                <X size={12} />
                Delete
              </button>
            </div>
          </div>
        )}
        
        {/* Camera Controls hint */}
        <div className="absolute bottom-4 left-4 bg-black/70 backdrop-blur-md rounded-xl p-3 pointer-events-auto">
          <p className="text-white/80 text-sm font-medium">Camera Controls</p>
          <p className="text-white/50 text-xs">Drag to rotate • Pinch to zoom</p>
        </div>
      </div>
      
      {/* Map Editor Panel - fixed position with pointer-events enabled */}
      <div className="fixed inset-0 pointer-events-none z-50">
        <div className="pointer-events-auto">
          <MapEditorPanel isOpen={showMapEditorPanel} onClose={() => setShowMapEditorPanel(false)} />
        </div>
      </div>
      
      {/* Background Control Panel for Map Editor */}
      <BackgroundControlPanel
        settings={backgroundSettings}
        onSettingsChange={setBackgroundSettings}
        onImageUpload={(imageDataUrl) => {
          setBackgroundSettings({
            ...backgroundSettings,
            customImage: imageDataUrl
          });
        }}
        wallSettings={wallSettings}
        onWallSettingsChange={setWallSettings}
        zoneSettings={zoneSettings}
        onZoneSettingsChange={setZoneSettings}
        isOpen={showBackgroundPanel}
        onOpenChange={setShowBackgroundPanel}
      />
      </>
    );
  }

  const activeOrbs = orbs.filter(o => o.isActive).length;
  
  const minutes = Math.floor(gameTimer / 60);
  const seconds = Math.floor(gameTimer % 60);
  const timerDisplay = `${minutes}:${seconds.toString().padStart(2, '0')}`;
  const timerColor = gameTimer <= 60 ? "text-red-400" : gameTimer <= 180 ? "text-yellow-400" : "text-white";
  
  const wallOwnershipMode = useZoogiGame(state => state.wallOwnershipMode);
  const ownershipScores = useZoogiGame(state => state.ownershipScores);
  const controlZones = useZoogiGame(state => state.controlZones);
  
  const isLocalMultiplayer = gameMode === "local_multiplayer";
  const isFreeForAll = gameMode === "ringer_royale";
  const currentPlayer = isLocalMultiplayer ? localPlayers[currentLocalPlayerIndex] : null;
  const playerColors = ["#3B82F6", "#EF4444", "#22C55E", "#A855F7"];
  
  const currentEntity = isLocalMultiplayer 
    ? (currentLocalPlayerIndex === 0 ? playerEntity : enemies[currentLocalPlayerIndex - 1])
    : playerEntity;
  
  const displayEntity = currentEntity || playerEntity;
  
  // Safety check - should not happen due to early return above
  if (!displayEntity) return null;

  return (
    <div className="fixed inset-0 pointer-events-none z-10">
      {selectedMap === "neon" && (
        <NeonScoreboard
          playerScore={score}
          foeScore={enemies.reduce((best, enemy) => Math.max(best, enemy.score), 0)}
          round={currentRound}
          maxRounds={maxRounds}
          timer={timerDisplay}
        />
      )}
      
      {abilityNotice && (
        <div className="absolute top-16 left-1/2 -translate-x-1/2 z-20 pointer-events-none">
          <div className="bg-black/85 text-white font-bold px-4 py-2 rounded-xl border border-yellow-400 shadow-lg text-sm">
            {abilityNotice.text}
          </div>
        </div>
      )}

      <div className="absolute top-0 left-0 right-0 h-1.5 bg-black/30">
        <motion.div 
          className="h-full bg-gradient-to-r from-purple-500 to-blue-500"
          initial={{ width: 0 }}
          animate={{ width: `${xpProgress.percent}%` }}
          transition={{ duration: 0.3 }}
        />
      </div>
      
      <AnimatePresence>
        {showLevelUp && (
          <LevelUpCelebration 
            level={celebratedLevel} 
            onComplete={() => setShowLevelUp(false)} 
          />
        )}
      </AnimatePresence>
      
      <AnimatePresence>
        {showBirdsEyeTutorial && birdsEyeView && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            exit={{ opacity: 0 }}
            className="fixed inset-0 flex items-center justify-center bg-black/60 z-50 pointer-events-auto"
            onClick={() => setShowBirdsEyeTutorial(false)}
          >
            <motion.div
              initial={{ scale: 0.9, y: 20 }}
              animate={{ scale: 1, y: 0 }}
              exit={{ scale: 0.9, y: 20 }}
              className="bg-gradient-to-b from-cyan-800/95 to-blue-900/95 rounded-2xl p-6 max-w-sm mx-4 shadow-2xl border border-cyan-400/30"
              onClick={(e) => e.stopPropagation()}
            >
              <div className="flex items-center justify-between mb-4">
                <h3 className="text-xl font-bold text-white flex items-center gap-2">
                  <Eye className="w-5 h-5 text-cyan-400" />
                  Bird's Eye Controls
                </h3>
                <button
                  onClick={() => setShowBirdsEyeTutorial(false)}
                  className="p-1 rounded-full bg-white/10 hover:bg-white/20 transition-colors"
                >
                  <X className="w-5 h-5 text-white" />
                </button>
              </div>
              
              <div className="space-y-4 text-white/90">
                <div className="flex items-start gap-3 bg-white/10 rounded-xl p-3">
                  <div className="w-10 h-10 bg-cyan-500/30 rounded-full flex items-center justify-center flex-shrink-0">
                    <span className="text-lg">👆</span>
                  </div>
                  <div>
                    <p className="font-semibold text-cyan-300">Swipe to Rotate</p>
                    <p className="text-sm text-white/70">Swipe left or right to rotate the arena view</p>
                  </div>
                </div>
                
                <div className="flex items-start gap-3 bg-white/10 rounded-xl p-3">
                  <div className="w-10 h-10 bg-cyan-500/30 rounded-full flex items-center justify-center flex-shrink-0">
                    <span className="text-lg">🤏</span>
                  </div>
                  <div>
                    <p className="font-semibold text-cyan-300">Pinch to Zoom</p>
                    <p className="text-sm text-white/70">Pinch with two fingers to zoom in or out</p>
                  </div>
                </div>
                
                <div className="flex items-start gap-3 bg-white/10 rounded-xl p-3">
                  <div className="w-10 h-10 bg-cyan-500/30 rounded-full flex items-center justify-center flex-shrink-0">
                    <span className="text-lg">🎯</span>
                  </div>
                  <div>
                    <p className="font-semibold text-cyan-300">Tap Near Zoogi to Aim</p>
                    <p className="text-sm text-white/70">Tap anywhere near your Zoogi and drag to aim your launch</p>
                  </div>
                </div>
              </div>
              
              <button
                onClick={() => setShowBirdsEyeTutorial(false)}
                className="w-full mt-5 py-3 bg-cyan-500 hover:bg-cyan-400 text-white font-bold rounded-xl transition-colors"
              >
                Got it!
              </button>
            </motion.div>
          </motion.div>
        )}
      </AnimatePresence>
      
      <div className="absolute top-4 sm:top-6 left-2 sm:left-4 right-2 sm:right-4 flex flex-wrap sm:flex-nowrap justify-between items-start gap-2 pointer-events-auto">
        <div className="bg-black/70 rounded-lg sm:rounded-xl p-2 sm:p-4 backdrop-blur-sm transition-all">
          <div className="flex items-center gap-2 sm:gap-3">
            <div className="relative">
              <div
                className="w-8 h-8 sm:w-10 sm:h-10 rounded-full shadow-lg flex-shrink-0 cursor-pointer ring-2 ring-transparent hover:ring-white/30 active:scale-95 transition-all"
                style={{
                  background: `radial-gradient(circle at 30% 30%, ${displayEntity.zoogi.secondaryColor}, ${displayEntity.zoogi.color})`,
                  border: isLocalMultiplayer ? `3px solid ${playerColors[currentLocalPlayerIndex]}` : 'none'
                }}
                onPointerDown={(e) => {
                  e.stopPropagation();
                  setShowPlayerPanel(!showPlayerPanel);
                  setShowPanelHint(false);
                }}
              />
              <AnimatePresence>
                {showPanelHint && (
                  <motion.div
                    initial={{ opacity: 0, x: 10 }}
                    animate={{ opacity: 1, x: 0 }}
                    exit={{ opacity: 0, x: -10 }}
                    className="absolute left-full top-1/2 -translate-y-1/2 ml-2 whitespace-nowrap"
                  >
                    <div className="flex items-center gap-1 bg-yellow-500/90 text-black text-xs font-bold px-2 py-1 rounded-lg shadow-lg">
                      <span>👆</span>
                      <span>{showPlayerPanel ? "Tap to hide" : "Tap to expand"}</span>
                    </div>
                  </motion.div>
                )}
              </AnimatePresence>
            </div>
            <div className="flex-1">
              <div className="flex items-center gap-2">
                <h3 className="text-white font-bold">{displayEntity.zoogi.name}</h3>
                <span className="px-1.5 py-0.5 bg-purple-600/80 rounded text-xs text-white font-bold">Lv.{level}</span>
                <span className="flex items-center gap-0.5 px-1.5 py-0.5 bg-yellow-600/80 rounded text-xs text-white font-bold">
                  <Coins className="w-3 h-3" />
                  {coins}
                </span>
              </div>
              <p className="text-white/60 text-xs">{displayEntity.zoogi.type}</p>
            </div>
          </div>

          <AnimatePresence>
            {showPlayerPanel && (
              <motion.div
                initial={{ height: 0, opacity: 0 }}
                animate={{ height: "auto", opacity: 1 }}
                exit={{ height: 0, opacity: 0 }}
                transition={{ duration: 0.2 }}
                className="overflow-hidden"
              >
                {isLocalMultiplayer && currentPlayer ? (
                  <div 
                    className="mt-2 px-3 py-1 rounded text-sm font-bold"
                    style={{ 
                      backgroundColor: `${playerColors[currentLocalPlayerIndex]}40`,
                      color: playerColors[currentLocalPlayerIndex]
                    }}
                  >
                    {currentPlayer.name}'s Turn!
                  </div>
                ) : isFreeForAll ? (
                  <div className="mt-2 px-3 py-1 rounded text-sm font-bold bg-purple-500/30 text-purple-300">
                    Free for All!
                  </div>
                ) : (
                  <div className={`mt-2 px-3 py-1 rounded text-sm font-bold ${
                    isPlayerTurn ? "bg-green-500/30 text-green-300" : "bg-orange-500/30 text-orange-300"
                  }`}>
                    {isPlayerTurn ? "Your Turn!" : "Enemy Turn..."}
                  </div>
                )}

                {displayEntity.hasShield && (
                  <div className="mt-2 px-2 py-1 bg-cyan-500/30 rounded text-cyan-300 text-xs">
                    Shield Active ({Math.ceil(displayEntity.shieldTimer)}s)
                  </div>
                )}
                
                {displayEntity.speedBoost > 1 && (
                  <div className="mt-2 px-2 py-1 bg-blue-500/30 rounded text-blue-300 text-xs">
                    Speed Boost ({Math.ceil(displayEntity.speedBoostTimer)}s)
                  </div>
                )}
                
                <div className="mt-3 pt-2 border-t border-white/20">
                  <p className="text-white/50 text-xs mb-1">Ability</p>
                  <p className="text-white/90 text-sm font-semibold">{displayEntity.zoogi.ability}</p>
                  <p className="text-white/60 text-xs">{getAbilityTriggerText(displayEntity.zoogi.id)}</p>
                  {displayEntity.zoogi.id === "lars" && displayEntity.larsRicochetBoost > 1 && (
                    <div className="mt-1 px-2 py-1 bg-blue-500/40 rounded text-blue-200 text-xs">
                      Ricochet Boost: {((displayEntity.larsRicochetBoost - 1) * 100).toFixed(0)}%
                    </div>
                  )}
                </div>
                
                {selectedMap !== "neon" && <div className="mt-3 pt-2 border-t border-white/20 flex items-center justify-between gap-3">
                  <div className="text-center">
                    <p className="text-white/50 text-[10px] uppercase">Round</p>
                    <p className="text-lg font-bold text-purple-400">{currentRound}/{maxRounds}</p>
                    <p className="text-[9px] text-green-400">Wins: {playerRoundWins}</p>
                  </div>
                  <div className="text-center">
                    <p className="text-white/50 text-[10px] uppercase">Time</p>
                    <p className={`text-lg font-bold ${timerColor}`}>{timerDisplay}</p>
                  </div>
                  <div className="text-center">
                    <p className="text-white/50 text-[10px] uppercase">Score</p>
                    <p className="text-lg font-bold text-yellow-400">{score}</p>
                  </div>
                  <div className="text-center">
                    <p className="text-white/50 text-[10px] uppercase">Orbs</p>
                    <div className="flex items-center gap-1">
                      <span className="text-lg font-bold text-pink-400">{activeOrbs}</span>
                      <div className="flex gap-0.5">
                        {orbs.filter(o => o.isActive).slice(0, 4).map((orb) => (
                          <div 
                            key={orb.id}
                            className="w-2 h-2 rounded-full"
                            style={{ backgroundColor: orb.color }}
                          />
                        ))}
                      </div>
                    </div>
                  </div>
                </div>}
                
                {wallOwnershipMode && (gameMode === "ringer_royale" || gameMode === "local_multiplayer") && (
                  <div className="mt-3 pt-2 border-t border-cyan-400/30">
                    <div className="flex items-center gap-2 mb-2">
                      <div className="w-2 h-2 rounded-full bg-cyan-400 animate-pulse" />
                      <p className="text-cyan-300 text-xs font-bold uppercase">Ringer Royale Active</p>
                    </div>
                    <div className="flex gap-2 flex-wrap">
                      {Object.entries(ownershipScores).map(([playerId, ownershipScore]) => {
                        const playerIndex = parseInt(playerId.replace('player-', ''));
                        const player = localPlayers[playerIndex];
                        const enemy = playerIndex > 0 ? enemies[playerIndex - 1] : null;
                        const color = player?.zoogi?.color || enemy?.zoogi?.color || playerColors[playerIndex % playerColors.length];
                        return (
                          <div key={playerId} className="flex items-center gap-1 px-2 py-1 rounded bg-white/10">
                            <div 
                              className="w-3 h-3 rounded-full"
                              style={{ backgroundColor: color }}
                            />
                            <span className="text-white text-xs font-bold">{ownershipScore}</span>
                          </div>
                        );
                      })}
                    </div>
                  </div>
                )}
                
                <div className="mt-3 pt-2 border-t border-cyan-400/30">
                  <div className="flex items-center gap-2">
                    <div className="w-2 h-2 rounded-full bg-cyan-400" />
                    <p className="text-cyan-300 text-xs font-bold uppercase">{modeRules(gameMode).title}</p>
                  </div>
                  <p className="text-cyan-200/70 text-[10px]">
                    {modeRules(gameMode).blurb}
                  </p>
                </div>
              </motion.div>
            )}
          </AnimatePresence>
        </div>
      </div>

      <div className="absolute bottom-24 left-2 sm:left-4 flex flex-col gap-1 pointer-events-none">
        <AnimatePresence>
          {recentXpGains.slice(-3).map((gain) => (
            <XPToast 
              key={gain.id} 
              amount={gain.amount} 
              reason={gain.reason}
              timestamp={gain.timestamp}
            />
          ))}
        </AnimatePresence>
      </div>

      <div className="absolute bottom-2 sm:bottom-4 left-2 sm:left-4 right-2 sm:right-4 flex justify-between items-end gap-2">
        <div className="bg-black/70 rounded-lg sm:rounded-xl p-2 sm:p-4 backdrop-blur-sm">
          {isLocalMultiplayer ? (
            <div>
              <p className="text-white/60 text-[10px] sm:text-xs uppercase mb-2">All Players</p>
              <div className="flex gap-3">
                {localPlayers.map((player, index) => (
                  <div 
                    key={player.id} 
                    className={`flex flex-col items-center p-2 rounded-lg transition-all ${
                      index === currentLocalPlayerIndex ? "bg-white/20 ring-2 ring-yellow-400" : ""
                    } ${player.isEliminated ? "opacity-40" : ""}`}
                  >
                    <div
                      className="w-8 h-8 rounded-full shadow-lg border-2"
                      style={{
                        background: player.zoogi ? `radial-gradient(circle at 30% 30%, ${player.zoogi.secondaryColor}, ${player.zoogi.color})` : "#666",
                        borderColor: playerColors[index]
                      }}
                    />
                    <span className="text-xs text-white font-medium mt-1">{player.name}</span>
                    <span className="text-xs text-yellow-400 font-bold">
                      {index === 0 ? (playerEntity?.score || 0) : (enemies[index - 1]?.score || 0)}
                    </span>
                    {player.isEliminated && (
                      <span className="text-xs text-red-400">OUT</span>
                    )}
                  </div>
                ))}
              </div>
            </div>
          ) : (
            <div className="flex items-center gap-2 sm:gap-4">
              <div>
                <p className="text-white/60 text-[10px] sm:text-xs uppercase mb-1 sm:mb-2">Opponents</p>
                <div className="flex gap-2 sm:gap-3">
                  {enemies.map((enemy) => (
                    <div key={enemy.id} className="flex flex-col items-center">
                      <div
                        className="w-6 h-6 sm:w-8 sm:h-8 rounded-full shadow-lg"
                        style={{
                          background: `radial-gradient(circle at 30% 30%, ${enemy.zoogi.secondaryColor}, ${enemy.zoogi.color})`,
                        }}
                      />
                      {selectedMap !== "neon" && <span className="text-xs text-yellow-400 font-bold mt-1">{enemy.score}</span>}
                    </div>
                  ))}
                </div>
              </div>
              {selectedMap !== "neon" && (
              <div className="border-l border-white/20 pl-4">
                <p className="text-white/60 text-xs uppercase">Score</p>
                <p className="text-2xl font-bold text-yellow-400">{score}</p>
              </div>
              )}
            </div>
          )}
        </div>

        {showPlayerPanel && (
          <div className="bg-black/70 rounded-xl p-3 backdrop-blur-sm text-center">
            <p className="text-white/60 text-xs mb-1">Points</p>
            <div className="text-xs text-white/70 space-y-1">
              <p>Orb knock-off: <span className="text-yellow-400">+{KNOCKOUT_SCORE_ORB}</span></p>
              <p>Opponent knock-off: <span className="text-yellow-400">+{KNOCKOUT_SCORE_PLAYER}</span></p>
              <p>Bumper touch: <span className="text-yellow-400">+{BUMPER_SCORE}</span></p>
              <p>Score zone: <span className="text-yellow-400">+{ZONE_SCORE_ORB}</span></p>
              <p>Fall off the island: <span className="text-red-400">-{KNOCKOUT_PENALTY}</span></p>
            </div>
          </div>
        )}
      </div>
      
      <div className="absolute right-4 top-1/2 -translate-y-1/2 flex flex-col gap-3 pointer-events-auto">
        {/* Lock-On Control - Moved to top to avoid overlap with player scores */}
        <button
          onClick={toggleLockOn}
          className={`p-3 rounded-xl backdrop-blur-sm transition-all relative ${
            lockOnEnabled
              ? "bg-purple-500/80 text-white ring-2 ring-purple-300" 
              : "bg-purple-500/50 text-white/80 hover:bg-purple-500/70"
          }`}
          title={lockOnEnabled ? "Lock-On Active - Tap again to disable" : "Enable Lock-On Mode"}
        >
          <Crosshair size={20} />
          {lockOnEnabled && (
            <span className="absolute -top-1 -right-1 w-3 h-3 bg-green-400 rounded-full animate-pulse" />
          )}
        </button>
        
        <CharacterAbilityButton zoogiId={displayEntity.zoogi.id} entity={displayEntity} />
        {devTools && (
          <div className="flex flex-col gap-1">
            <button
              onClick={() => useZoogiGame.getState().debugUnlockPower(displayEntity.id)}
              className="px-2 py-1 rounded-lg bg-amber-700/90 text-white text-[10px] font-bold"
              title="Debug: unlock this marble's power"
            >
              Unlock mine
            </button>
            <button
              onClick={() => {
                const foe = useZoogiGame.getState().enemies.find((enemy) => !enemy.isKnockedOut);
                if (foe) useZoogiGame.getState().debugUnlockPower(foe.id);
              }}
              className="px-2 py-1 rounded-lg bg-orange-700/90 text-white text-[10px] font-bold"
              title="Debug: unlock the next opponent's power"
            >
              Unlock foe
            </button>
          </div>
        )}
        
        {/* Orb Multiplier Control */}
        {gameMode === "practice" && (
          <div className="flex flex-col items-center gap-1 bg-black/70 rounded-xl p-2 backdrop-blur-sm">
            <button
              onClick={incrementOrbMultiplier}
              disabled={orbMultiplier >= 3}
              className={`w-8 h-8 rounded-lg flex items-center justify-center transition-all ${
                orbMultiplier >= 3 
                  ? "bg-gray-600/50 text-gray-500 cursor-not-allowed" 
                  : "bg-pink-500/80 text-white hover:bg-pink-600/80"
              }`}
              title="Add orb ring"
            >
              <Plus size={16} />
            </button>
            <div className="flex items-center gap-1">
              <span className="text-pink-400 font-bold text-sm">x{orbMultiplier}</span>
            </div>
            <button
              onClick={decrementOrbMultiplier}
              disabled={orbMultiplier <= 1}
              className={`w-8 h-8 rounded-lg flex items-center justify-center transition-all ${
                orbMultiplier <= 1 
                  ? "bg-gray-600/50 text-gray-500 cursor-not-allowed" 
                  : "bg-pink-500/80 text-white hover:bg-pink-600/80"
              }`}
              title="Remove orb ring"
            >
              <Minus size={16} />
            </button>
            <span className="text-white/60 text-[10px]">Orbs</span>
          </div>
        )}
        
        <button
          onClick={toggleFirstPersonView}
          className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
            firstPersonView 
              ? "bg-green-500/80 text-white" 
              : "bg-black/70 text-white/80 hover:bg-black/90"
          }`}
          title={firstPersonView ? "Return to normal view" : "First person view"}
        >
          <User size={20} />
        </button>
        
        <button
          onClick={toggleOverShoulderView}
          className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
            overShoulderView 
              ? "bg-orange-500/80 text-white" 
              : "bg-black/70 text-white/80 hover:bg-black/90"
          }`}
          title={overShoulderView ? "Return to normal view" : "Over the shoulder view"}
        >
          <ScanEye size={20} />
        </button>
        
        <button
          onClick={toggleBirdsEyeView}
          className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
            birdsEyeView 
              ? "bg-blue-500/80 text-white" 
              : "bg-black/70 text-white/80 hover:bg-black/90"
          }`}
          title={birdsEyeView ? "Return to normal view" : "Bird's eye view"}
        >
          {birdsEyeView ? <EyeOff size={20} /> : <Eye size={20} />}
        </button>
        
        <button
          onClick={toggleLaunchPadView}
          className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
            launchPadView 
              ? "bg-cyan-500/80 text-white" 
              : "bg-black/70 text-white/80 hover:bg-black/90"
          }`}
          title={launchPadView ? "Return to normal view" : "Launch Pad view (fixed)"}
        >
          <Target size={20} />
        </button>
        
        {birdsEyeView && (
          <button
            onClick={() => setShowBirdsEyeTutorial(true)}
            className="p-3 rounded-xl backdrop-blur-sm transition-all bg-cyan-500/70 text-white hover:bg-cyan-600/80"
            title="Bird's Eye Controls"
          >
            <Info size={20} />
          </button>
        )}
        
        <button
          onClick={() => setPhase("menu")}
          className="p-3 rounded-xl backdrop-blur-sm transition-all bg-red-500/70 text-white hover:bg-red-600/80"
          title="Quit to Main Menu"
        >
          <Home size={20} />
        </button>
        
        <button
          onClick={openTutorial}
          className="p-3 rounded-xl backdrop-blur-sm transition-all bg-purple-500/70 text-white hover:bg-purple-600/80"
          title="How to Play"
        >
          <HelpCircle size={20} />
        </button>
        
        {devTools && (
          <>
            <button
              onClick={() => setShowCollisionTuningPanel(!showCollisionTuningPanel)}
              className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
                showCollisionTuningPanel
                  ? "bg-cyan-500/90 text-white ring-2 ring-white"
                  : "bg-cyan-600/70 text-white hover:bg-cyan-500/80"
              }`}
              title="Collision Tuning"
            >
              <Target size={20} />
            </button>
            
            <button
              onClick={() => setShowAiControlsPanel(!showAiControlsPanel)}
              className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
                showAiControlsPanel
                  ? "bg-orange-500/90 text-white ring-2 ring-white"
                  : "bg-orange-600/70 text-white hover:bg-orange-500/80"
              }`}
              title="AI Controls"
            >
              <Zap size={20} />
            </button>
            
            <button
              onClick={() => useZoogiGame.getState().toggleDeveloperCamera()}
              className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
                useZoogiGame.getState().developerCamera
                  ? "bg-yellow-500/90 text-black ring-2 ring-white"
                  : "bg-gray-700/70 text-white/80 hover:bg-gray-600/80"
              }`}
              title="Developer Camera (free roam)"
            >
              <Video size={20} />
            </button>
            
            <button
              onClick={() => useZoogiGame.getState().toggleDeveloperMoveMode()}
              className={`p-3 rounded-xl backdrop-blur-sm transition-all ${
                useZoogiGame.getState().developerMoveMode
                  ? "bg-orange-500/90 text-black ring-2 ring-white"
                  : "bg-gray-700/70 text-white/80 hover:bg-gray-600/80"
              }`}
              title="Developer Move (click and drag elements)"
            >
              <Move size={20} />
            </button>
          </>
        )}
        
      </div>
      
      {useZoogiGame.getState().developerMoveMode && (
        <div className="absolute top-4 right-4 flex items-center gap-2 pointer-events-auto">
          <button
            onClick={() => useZoogiGame.getState().toggleIoControlsVisible()}
            className={`w-10 h-10 rounded-lg flex items-center justify-center transition-all ${
              useZoogiGame.getState().ioControlsVisible
                ? "bg-blue-600 hover:bg-blue-500"
                : "bg-gray-700/70 hover:bg-gray-600"
            }`}
            title={useZoogiGame.getState().ioControlsVisible ? "Hide Element Controls" : "Show Element Controls"}
          >
            <Anchor size={18} className="text-white" />
          </button>
          <button
            onClick={() => useZoogiGame.getState().toggleDevToolsVisible()}
            className={`w-10 h-10 rounded-lg flex items-center justify-center transition-all ${
              useZoogiGame.getState().devToolsVisible
                ? "bg-green-600 hover:bg-green-500"
                : "bg-gray-700/70 hover:bg-gray-600"
            }`}
            title={useZoogiGame.getState().devToolsVisible ? "Hide Physics Panel" : "Show Physics Panel"}
          >
            {useZoogiGame.getState().devToolsVisible ? <Eye size={18} className="text-white" /> : <EyeOff size={18} className="text-white" />}
          </button>
          {useZoogiGame.getState().devToolsVisible && <PhysicsControlPanel />}
        </div>
      )}
      
      {useZoogiGame.getState().developerMoveMode && useZoogiGame.getState().ioControlsVisible && (
        <div className="absolute bottom-4 left-1/2 transform -translate-x-1/2 flex flex-col items-center gap-2 pointer-events-auto">
          <div className="bg-black/80 rounded-xl p-3 backdrop-blur-sm">
            <div className="text-white/70 text-xs text-center mb-2">
              {useZoogiGame.getState().selectedMoveElement 
                ? `Selected: Tree #${useZoogiGame.getState().selectedMoveElement?.index}` 
                : "Click an element to select"}
            </div>
            
            <div className="flex items-center justify-center gap-2 mb-2">
              <button
                onClick={() => setIncrement(getIncrement() - 0.05)}
                className="w-8 h-8 rounded-lg bg-purple-600/90 text-white hover:bg-purple-500 transition-all flex items-center justify-center"
                title="Decrease step size"
              >
                <Minus size={14} />
              </button>
              <span className="text-white text-xs w-12 text-center font-mono">{getIncrement().toFixed(2)}</span>
              <button
                onClick={() => setIncrement(getIncrement() + 0.05)}
                className="w-8 h-8 rounded-lg bg-purple-600/90 text-white hover:bg-purple-500 transition-all flex items-center justify-center"
                title="Increase step size"
              >
                <Plus size={14} />
              </button>
            </div>
            
            <div className="flex items-center justify-center gap-2 mb-2">
              <span className="text-white/60 text-xs">Char:</span>
              <button
                onClick={() => setGlobalZoogiScale(getGlobalZoogiScale() - 0.01)}
                className="w-8 h-8 rounded-lg bg-green-600/90 text-white hover:bg-green-500 transition-all flex items-center justify-center"
                title="Decrease character scale"
              >
                <Minus size={14} />
              </button>
              <span className="text-white text-xs w-10 text-center font-mono">{getGlobalZoogiScale().toFixed(2)}</span>
              <button
                onClick={() => setGlobalZoogiScale(getGlobalZoogiScale() + 0.01)}
                className="w-8 h-8 rounded-lg bg-green-600/90 text-white hover:bg-green-500 transition-all flex items-center justify-center"
                title="Increase character scale"
              >
                <Plus size={14} />
              </button>
            </div>
            
            <div className="flex items-center justify-center gap-2 mb-2">
              <span className="text-white/60 text-xs">Orb:</span>
              <button
                onClick={() => setGlobalOrbScale(getGlobalOrbScale() - 0.01)}
                className="w-8 h-8 rounded-lg bg-yellow-600/90 text-white hover:bg-yellow-500 transition-all flex items-center justify-center"
                title="Decrease orb scale"
              >
                <Minus size={14} />
              </button>
              <span className="text-white text-xs w-10 text-center font-mono">{getGlobalOrbScale().toFixed(2)}</span>
              <button
                onClick={() => setGlobalOrbScale(getGlobalOrbScale() + 0.01)}
                className="w-8 h-8 rounded-lg bg-yellow-600/90 text-white hover:bg-yellow-500 transition-all flex items-center justify-center"
                title="Increase orb scale"
              >
                <Plus size={14} />
              </button>
            </div>
            
            <LaunchPadTiltControl />
            <LaunchPadPitchControl />
            <LaunchPadHeightControl />
            
            <div className="flex gap-3 items-center">
              <div className="flex flex-col items-center gap-1">
                <button
                  onClick={() => moveSelectedElementByArrows(0, 0, -getIncrement())}
                  className="w-10 h-10 rounded-lg bg-gray-700/90 text-white hover:bg-gray-600 transition-all flex items-center justify-center"
                  title="Move Forward"
                >
                  <ArrowUp size={18} />
                </button>
                <div className="flex gap-1">
                  <button
                    onClick={() => moveSelectedElementByArrows(-getIncrement(), 0, 0)}
                    className="w-10 h-10 rounded-lg bg-gray-700/90 text-white hover:bg-gray-600 transition-all flex items-center justify-center"
                    title="Move Left"
                  >
                    <ArrowLeft size={18} />
                  </button>
                  <button
                    onClick={() => moveSelectedElementByArrows(0, 0, getIncrement())}
                    className="w-10 h-10 rounded-lg bg-gray-700/90 text-white hover:bg-gray-600 transition-all flex items-center justify-center"
                    title="Move Backward"
                  >
                    <ArrowDown size={18} />
                  </button>
                  <button
                    onClick={() => moveSelectedElementByArrows(getIncrement(), 0, 0)}
                    className="w-10 h-10 rounded-lg bg-gray-700/90 text-white hover:bg-gray-600 transition-all flex items-center justify-center"
                    title="Move Right"
                  >
                    <ArrowRight size={18} />
                  </button>
                </div>
              </div>
              <div className="flex flex-col gap-1">
                <button
                  onClick={() => moveSelectedElementByArrows(0, getIncrement(), 0)}
                  className="w-10 h-10 rounded-lg bg-blue-600/90 text-white hover:bg-blue-500 transition-all flex items-center justify-center"
                  title="Raise"
                >
                  <ChevronUp size={18} />
                </button>
                <button
                  onClick={() => moveSelectedElementByArrows(0, -getIncrement(), 0)}
                  className="w-10 h-10 rounded-lg bg-blue-600/90 text-white hover:bg-blue-500 transition-all flex items-center justify-center"
                  title="Lower"
                >
                  <ChevronDown size={18} />
                </button>
              </div>
              <div className="flex flex-col gap-1">
                <button
                  onClick={() => rotateSelectedElement(-0.1)}
                  className="w-10 h-10 rounded-lg bg-orange-600/90 text-white hover:bg-orange-500 transition-all flex items-center justify-center"
                  title="Rotate Left"
                >
                  <RotateCcw size={18} />
                </button>
                <button
                  onClick={() => rotateSelectedElement(0.1)}
                  className="w-10 h-10 rounded-lg bg-orange-600/90 text-white hover:bg-orange-500 transition-all flex items-center justify-center"
                  title="Rotate Right"
                >
                  <RotateCw size={18} />
                </button>
              </div>
            </div>
            
            <div className="flex gap-2 mt-3">
              <button
                onClick={snapToGround}
                className="flex-1 px-3 py-2 rounded-lg bg-cyan-600 text-white font-bold hover:bg-cyan-500 transition-all flex items-center justify-center gap-2"
                title="Snap to ground (Y=0)"
              >
                <Anchor size={16} />
                Snap
              </button>
              <button
                onClick={confirmPlacements}
                className="flex-1 px-3 py-2 rounded-lg bg-green-600 text-white font-bold hover:bg-green-500 transition-all flex items-center justify-center gap-2"
                title="Confirm all placements"
              >
                <Check size={16} />
                Confirm
              </button>
              <button
                onClick={() => setShowExportMenu(!showExportMenu)}
                className={`flex-1 px-3 py-2 rounded-lg ${showExportMenu ? 'bg-purple-400' : 'bg-purple-600'} text-white font-bold hover:bg-purple-500 transition-all flex items-center justify-center gap-2`}
                title="Export options"
              >
                <Download size={16} />
                Export
              </button>
            </div>
            
            {showExportMenu && (
              <div className="flex flex-col gap-2 mt-2 p-2 bg-gray-900/90 rounded-lg border border-purple-500/50">
                <button
                  onClick={() => {
                    const data = exportAllOffsets();
                    navigator.clipboard.writeText(data).then(() => {
                      alert("All element positions copied to clipboard!\n\nCheck console for full output.");
                    }).catch(() => {
                      prompt("Copy these positions:", data);
                    });
                    console.log("EXPORTED ELEMENT POSITIONS:\n" + data);
                    setShowExportMenu(false);
                  }}
                  className="px-3 py-2 rounded-lg bg-purple-600 text-white font-bold hover:bg-purple-500 transition-all flex items-center justify-center gap-2"
                  title="Export all element positions"
                >
                  <Download size={16} />
                  Export Elements
                </button>
                <button
                  onClick={() => {
                    const camera = (window as any).__ZOOGI_CAMERA__;
                    const player = useZoogiGame.getState().playerEntity;
                    const orbitAngle = (window as any).__ZOOGI_ORBIT_ANGLE__ || 0;
                    if (camera && player) {
                      const pos = camera.position;
                      const target = (window as any).__ZOOGI_CAMERA_TARGET__ || { x: 0, y: 0, z: 0 };
                      const playerPos = player.position;
                      const worldDx = pos.x - playerPos[0];
                      const worldDz = pos.z - playerPos[2];
                      const localForward = worldDx * Math.sin(orbitAngle) + worldDz * Math.cos(orbitAngle);
                      const localRight = worldDx * Math.cos(orbitAngle) - worldDz * Math.sin(orbitAngle);
                      const targetDx = target.x - playerPos[0];
                      const targetDz = target.z - playerPos[2];
                      const targetLocalForward = targetDx * Math.sin(orbitAngle) + targetDz * Math.cos(orbitAngle);
                      const targetLocalRight = targetDx * Math.cos(orbitAngle) - targetDz * Math.sin(orbitAngle);
                      const offsetPos: [number, number, number] = [
                        parseFloat(localForward.toFixed(2)),
                        parseFloat(pos.y.toFixed(2)),
                        parseFloat(localRight.toFixed(2))
                      ];
                      const offsetLookAt: [number, number, number] = [
                        parseFloat(targetLocalForward.toFixed(2)),
                        parseFloat(target.y.toFixed(2)),
                        parseFloat(targetLocalRight.toFixed(2))
                      ];
                      const cameraData = {
                        position: offsetPos,
                        lookAt: offsetLookAt
                      };
                      const dataStr = JSON.stringify(cameraData, null, 2);
                      navigator.clipboard.writeText(dataStr).then(() => {
                        alert(`Camera Position Saved (rotation-aware)!\n\nForward/Height/Right: [${offsetPos.join(', ')}]\nLookAt: [${offsetLookAt.join(', ')}]\n\nCamera will follow player facing direction!`);
                      }).catch(() => {
                        prompt("Copy camera position:", dataStr);
                      });
                      console.log("CAMERA POSITION (rotation-aware, forward/height/right):", dataStr);
                      
                      useZoogiGame.getState().setSavedCameraPosition({
                        position: offsetPos,
                        lookAt: offsetLookAt
                      });
                    } else if (!camera) {
                      alert("Enable Developer Camera first, then position it where you want!");
                    } else {
                      alert("No player entity found!");
                    }
                    setShowExportMenu(false);
                  }}
                  className="px-3 py-2 rounded-lg bg-cyan-600 text-white font-bold hover:bg-cyan-500 transition-all flex items-center justify-center gap-2"
                  title="Save current camera position for over-shoulder view"
                >
                  <Camera size={16} />
                  Save Camera Position
                </button>
                <button
                  onClick={() => {
                    const state = useZoogiGame.getState();
                    const tilt = state.launchPadTilt;
                    const pitch = state.launchPadPitch;
                    const height = state.launchPadHeight;
                    
                    const codeSnippet = `// Launch Pad Camera Settings - paste in client/src/lib/stores/useZoogiGame.tsx (initial state)
launchPadTilt: ${tilt},
launchPadPitch: ${pitch},
launchPadHeight: ${height},`;
                    
                    navigator.clipboard.writeText(codeSnippet).then(() => {
                      alert(`Launch Pad Camera Code Copied!\n\nPaste these values in useZoogiGame.tsx initial state:\n\nlaunchPadTilt: ${tilt}\nlaunchPadPitch: ${pitch}\nlaunchPadHeight: ${height}`);
                    }).catch(() => {
                      prompt("Copy Launch Pad camera code:", codeSnippet);
                    });
                    console.log("=== LAUNCH PAD CAMERA CODE ===");
                    console.log("Paste in: client/src/lib/stores/useZoogiGame.tsx (initial state section)");
                    console.log(codeSnippet);
                    console.log("==============================");
                    setShowExportMenu(false);
                  }}
                  className="px-3 py-2 rounded-lg bg-green-600 text-white font-bold hover:bg-green-500 transition-all flex items-center justify-center gap-2"
                  title="Save Launch Pad camera tilt, pitch, and height as code"
                >
                  <Camera size={16} />
                  Save Launch Pad Camera
                </button>
              </div>
            )}
          </div>
        </div>
      )}
      
      <AnimatePresence>
        {lockOnEnabled && lockOnTargetId && (
          <motion.div 
            initial={{ opacity: 0, x: -50 }}
            animate={{ opacity: 1, x: 0 }}
            exit={{ opacity: 0, x: -50 }}
            className="absolute bottom-32 left-4 flex flex-col gap-2 pointer-events-auto"
          >
            <div className="flex items-center gap-2">
              <button
                onClick={() => setArcType(arcType === "left" ? null : "left")}
                className={`w-12 h-12 rounded-xl font-bold text-lg transition-all ${
                  arcType === "left"
                    ? "bg-purple-500 text-white ring-2 ring-white shadow-lg scale-110"
                    : "bg-black/70 text-white/80 hover:bg-black/90"
                }`}
              >
                ←
              </button>
              <button
                onClick={toggleStraightMode}
                className={`w-12 h-12 rounded-xl font-bold text-lg transition-all ${
                  straightMode
                    ? "bg-green-500 text-white ring-2 ring-white shadow-lg scale-110"
                    : arcType === "over"
                    ? "bg-purple-500 text-white ring-2 ring-white shadow-lg scale-110"
                    : "bg-black/70 text-white/80 hover:bg-black/90"
                }`}
                title={straightMode ? "Straight Shot" : arcType === "over" ? "Over Arc" : "Tap: Straight → Over → None"}
              >
                ↑
              </button>
              <button
                onClick={() => setArcType(arcType === "right" ? null : "right")}
                className={`w-12 h-12 rounded-xl font-bold text-lg transition-all ${
                  arcType === "right"
                    ? "bg-purple-500 text-white ring-2 ring-white shadow-lg scale-110"
                    : "bg-black/70 text-white/80 hover:bg-black/90"
                }`}
              >
                →
              </button>
            </div>
            {/* Tangent offset buttons - appear when straight mode is active */}
            {straightMode && (
              <div className="flex items-center gap-2 justify-center">
                <button
                  onClick={() => setTangentOffset(tangentOffset === "left" ? "none" : "left")}
                  className={`w-10 h-10 rounded-xl font-bold text-sm transition-all ${
                    tangentOffset === "left"
                      ? "bg-orange-500 text-white ring-2 ring-white shadow-lg scale-110"
                      : "bg-black/70 text-white/80 hover:bg-black/90"
                  }`}
                  title="Glance Left Edge"
                >
                  ↖
                </button>
                <span className="text-white/60 text-xs">ANGLE</span>
                <button
                  onClick={() => setTangentOffset(tangentOffset === "right" ? "none" : "right")}
                  className={`w-10 h-10 rounded-xl font-bold text-sm transition-all ${
                    tangentOffset === "right"
                      ? "bg-orange-500 text-white ring-2 ring-white shadow-lg scale-110"
                      : "bg-black/70 text-white/80 hover:bg-black/90"
                  }`}
                  title="Glance Right Edge"
                >
                  ↗
                </button>
              </div>
            )}
            <button
              onClick={() => (arcType || straightMode) && triggerArcLaunch()}
              disabled={!arcType && !straightMode}
              className={`w-full h-12 rounded-xl font-bold text-sm transition-all ${
                (arcType || straightMode)
                  ? "bg-green-500 text-white hover:bg-green-400 shadow-lg animate-pulse"
                  : "bg-gray-600/50 text-gray-400 cursor-not-allowed"
              }`}
            >
              LAUNCH
            </button>
          </motion.div>
        )}
      </AnimatePresence>
      
      {isMapEditor && (
        <>
          <motion.button
            initial={{ x: 100, opacity: 0 }}
            animate={{ x: 0, opacity: 1 }}
            onClick={() => setShowMapEditorPanel(true)}
            className="absolute right-4 top-20 px-4 py-3 rounded-xl bg-gradient-to-r from-violet-600 to-purple-600 text-white font-bold shadow-lg pointer-events-auto flex items-center gap-2"
          >
            <Package size={18} />
            My Arenas
          </motion.button>
          
          <MapEditorPanel
            isOpen={showMapEditorPanel}
            onClose={() => setShowMapEditorPanel(false)}
          />
          
          <BackgroundControlPanel
            settings={backgroundSettings}
            onSettingsChange={setBackgroundSettings}
            onImageUpload={(imageDataUrl) => {
              setBackgroundSettings({
                ...backgroundSettings,
                customImage: imageDataUrl
              });
            }}
            wallSettings={wallSettings}
            onWallSettingsChange={setWallSettings}
            zoneSettings={zoneSettings}
            onZoneSettingsChange={setZoneSettings}
            isOpen={showBackgroundPanel}
            onOpenChange={setShowBackgroundPanel}
          />
          
          <ZoneEditorPanel />
        </>
      )}
      
      {/* Panels available during gameplay */}
      <CollisionTuningPanel />
      <AIControlsPanel />
    </div>
  );
}

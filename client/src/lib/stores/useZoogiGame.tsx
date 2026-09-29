import { create } from "zustand";
import { useProgression, XP_VALUES } from "./useProgression";
import { subscribeWithSelector } from "zustand/middleware";
import { 
  ARENA_RADIUS, 
  WALL_THICKNESS, 
  isInGap, 
  getSnowmanPositions,
  WALL_OWNERSHIP_GREEN_RESPAWN_RADIUS,
  WALL_OWNERSHIP_GAP_ANGLES,
  WALL_OWNERSHIP_OUTER_RADIUS,
  WALL_OWNERSHIP_CONTROL_ZONE_RADIUS,
  KNOCKOUT_RADIUS,
  KNOCKOUT_SCORE_ORB,
  KNOCKOUT_SCORE_PLAYER,
  KNOCKOUT_PENALTY,
  BUMPER_SCORE,
  KNOCKOUT_RESPAWN_BEAT,
  ZONE_SCORE_ORB,
  SCORE_ZONE_RADIUS,
  RESTRICTION_PHASE_DURATION,
  INVULNERABILITY_DURATION,
  RESPAWN_DELAY,
  isBeyondArenaBoundary,
  getGreenPadRespawnPosition,
  getSpawnPointPosition,
  setActiveMapTheme,
  isInsideRing
} from "../arenaConstants";
import { setCurrentMap } from "@/lib/treeOffsets";
import { triggerKnockoffFeel, triggerCollisionFeel, triggerCollectFeel, triggerAbilityFeel, triggerWallHitFeel, useGameFeel } from "./useGameFeel";
import { useAudio } from "./useAudio";
import { triggerAbilityCameraEffect, triggerKnockoffCameraEffect, triggerCollisionCameraEffect, triggerTargetFocusCameraEffect } from "./useCameraEffects";
import { getDeviceId } from "@/lib/deviceId";
import { COSMOS_STAGE, GRASS_STAGE, arabianPlayTransform, collectMatchSolids, getIcePatches, getMapLayout, knockoffOffsetForMap, resolveSolidCollision } from "../arenaColliders";
import { isOutsideNeonCourt, resolveNeonRails } from "../neonCourt";
import { ICE_LINEAR_DAMPING, ICE_ROLLING_DRAG, LINEAR_DAMPING, LOCKON_LAUNCH_SPEED, MARBLE_RESTITUTION, MAX_PLANAR_SPEED, REST_SPEED, ROLLING_DRAG } from "../simFeel";

export const DEFAULT_BACKGROUND_SETTINGS = {
  distance: 200,
  yPos: 70,
  width: 500,
  height: 250,
  rotation: 0,
  mirrorBack: false,
  mirrorFront: true,
  mirrorLeft: false,
  mirrorRight: true,
  opacity: 0.9,
  visible: true,
  customImage: null as string | null,
  groundScale: 3,
  modelPositionX: 0,
  modelPositionY: -0.5,
  modelPositionZ: 0,
  modelScale: 3,
};

export const DEFAULT_WALL_SETTINGS = {
  outerWallEnabled: true,
  outerWallRadiusOffset: 25,
  outerWallRows: 3,
  outerWallBlocksPerRow: 64,
  outerWallGapWidth: Math.PI / 10,
  middleWallEnabled: true,
  middleWallRadiusOffset: 2,
  middleWallRows: 4,
  middleWallBlocksPerRow: 48,
  middleWallGapWidth: Math.PI / 10,
  innerWallEnabled: true,
  innerWallRadiusPercent: 70,
  innerWallRows: 2,
  innerWallBlocksPerRow: 36,
  knockoffBoundaryRadius: 50,
  knockoffBoundaryWidth: 0.5,
  // New PBR/Video outer walls
  pbrInnerWallEnabled: true,
  pbrInnerWallRadiusOffset: 18,
  pbrMiddleWallEnabled: true,
  pbrMiddleWallRadiusOffset: 20,
  videoWallEnabled: true,
  videoWallRadiusOffset: 22,
  videoWallHeight: 3,
};

export const DEFAULT_COLLISION_TUNING = {
  collisionBuffer: 0.8,
  bounceStrength: 0.75,
  breakThreshold: 1.2,
  scatterForce: 1.5,
  particleCount: 25,
  orbBreakMultiplier: 0.8,
  minBounceVelocity: 0.3,
};

export const DEFAULT_AI_CONTROLS = {
  powerMultiplier: 1.0,
  targetPriority: 'closest' as 'closest' | 'weakest' | 'random',
  edgeAwareness: 0.7,
  selfPreservation: 0.5,
  reactionDelay: 1.0,
  accuracy: 0.8,
};

export interface WallSegmentConfig {
  id: string;
  segmentIndex: number;
  visible: boolean;
  positionOffset: { x: number; y: number; z: number };
  startAngle: number;
  endAngle: number;
}

export const DEFAULT_WALL_SEGMENT_EDITOR = {
  selectedWallSegmentId: null as string | null,
  wallSegmentConfigs: [] as WallSegmentConfig[],
  wallSegmentVersion: 0,
  innerWallSegmentConfigs: [] as WallSegmentConfig[],
  selectedInnerWallSegmentId: null as string | null,
};

export const DEFAULT_ZONE_SETTINGS = {
  globalRotationOffset: 0,
  globalDistanceOffset: 17,
  globalScale: 1,
  zonesVisible: true,
};

export type TransformElementType = "arena_model" | "outer_wall" | "middle_wall" | "inner_wall" | "zones" | "knockoff_boundary" | null;
export type TransformMode = "translate" | "rotate" | "scale";

export const DEFAULT_ELEMENT_TRANSFORMS = {
  arenaModelOffset: { x: 0, y: 0, z: 0 },
  arenaModelRotation: { x: 0, y: 0, z: 0 },
  arenaModelScale: 1,
  outerWallOffset: { x: 0.7, y: 0, z: 0 },
  outerWallRotation: { x: 0, y: 0, z: 0 },
  middleWallOffset: { x: 0.8, y: 0, z: 0 },
  middleWallRotation: { x: 0, y: 0, z: 0 },
  innerWallOffset: { x: 0.8, y: 0, z: 0 },
  innerWallRotation: { x: 0, y: 0, z: 0 },
  zonesOffset: { x: 0.7, y: 0, z: 0 },
  zonesRotation: { x: 0, y: 0, z: 0 },
  knockoffBoundaryOffset: { x: 0, y: 0, z: 0 },
  knockoffBoundaryRotation: { x: 0, y: 0, z: 0 },
};

export type GamePhase = "menu" | "shop" | "zoogipedia" | "arena_editor" | "character_selection" | "local_setup" | "map_selection" | "playing" | "round_end" | "game_over" | "feature_hub" | "music_visualizer" | "ringer_creator" | "ringer_trials_loading" | "ringer_trials";
export type GameMode = "classic" | "ringer_royale" | "local_multiplayer" | "practice" | "map_editor";
export type MapTheme = "grass" | "ice" | "lava" | "space" | "saturn" | "tomb" | "neon";

export interface ZoogiStats {
  speed: number;
  power: number;
  defense: number;
  control: number;
}

export interface Zoogi {
  id: string;
  name: string;
  type: string;
  color: string;
  secondaryColor: string;
  ability: string;
  abilityDescription: string;
  stats: ZoogiStats;
}

export const ZOOGI_ROSTER: Zoogi[] = [
  {
    id: "wolfgang",
    name: "Wolfgang",
    type: "Wolf",
    color: "#6B7280",
    secondaryColor: "#9CA3AF",
    ability: "Wolf Pack",
    abilityDescription: "While moving, send three clones that chase the nearest orb or opponent",
    stats: { speed: 85, power: 60, defense: 50, control: 75 }
  },
  {
    id: "hotstreak",
    name: "Hotstreak",
    type: "Lava",
    color: "#EF4444",
    secondaryColor: "#F97316",
    ability: "Instant Explosion",
    abilityDescription: "Create a fiery explosion around you",
    stats: { speed: 55, power: 90, defense: 70, control: 45 }
  },
  {
    id: "lars",
    name: "Lars",
    type: "Bouncer",
    color: "#3B82F6",
    secondaryColor: "#93C5FD",
    ability: "Ricochet",
    abilityDescription: "Ricochets toward the nearest target after a hit",
    stats: { speed: 70, power: 75, defense: 60, control: 80 }
  },
  {
    id: "pinpoint",
    name: "Pinpoint",
    type: "Precision",
    color: "#8B5CF6",
    secondaryColor: "#A78BFA",
    ability: "Laser Trajectory",
    abilityDescription: "Tap Lock-On button, tap any orb or enemy to target it, then drag to set power - shot auto-aims",
    stats: { speed: 60, power: 65, defense: 45, control: 95 }
  },
  {
    id: "bolt",
    name: "Bolt",
    type: "Electric",
    color: "#FBBF24",
    secondaryColor: "#FDE047",
    ability: "Static Shock",
    abilityDescription: "Phase through enemies and stun them",
    stats: { speed: 80, power: 55, defense: 40, control: 70 }
  },
  {
    id: "wraps",
    name: "Wraps",
    type: "Mummy",
    color: "#8B7355",
    secondaryColor: "#D4C4B0",
    ability: "Bandage Bind",
    abilityDescription: "Slow enemies on contact with ancient curses",
    stats: { speed: 50, power: 70, defense: 85, control: 65 }
  },
  {
    id: "nightshade",
    name: "Nightshade",
    type: "Shadow",
    color: "#6B46C1",
    secondaryColor: "#9F7AEA",
    ability: "Shadow Stun",
    abilityDescription: "A short shadow pulse that freezes nearby opponents and skips their next turn",
    stats: { speed: 62, power: 68, defense: 64, control: 84 }
  }
];

export const MAP_OPTIONS: { id: MapTheme; name: string; description: string; color: string }[] = [
  { id: "grass", name: "Meadow Arena", description: "Classic grassy battlefield", color: "#4CAF50" },
  { id: "ice", name: "Frozen Ring", description: "Slippery ice platform", color: "#81D4FA" },
  { id: "lava", name: "Volcanic Pit", description: "Fiery lava arena", color: "#FF5722" },
  { id: "space", name: "Cosmic Platform", description: "Floating in the void", color: "#7C4DFF" },
  { id: "saturn", name: "Arabian Nights", description: "Magical palace arena", color: "#FFA726" },
  { id: "tomb", name: "Pharaoh's Tomb", description: "Sandstone hieroglyph court", color: "#E0B88A" },
  { id: "neon", name: "Night Circuit", description: "Neon rails over a night city", color: "#d946ef" }
];

interface Orb {
  id: string;
  position: [number, number, number];
  velocity: [number, number, number];
  color: string;
  points: number;
  isActive: boolean;
  lastHitBy: "player" | "enemy" | null;
  lastHitByEnemyId: string | null;
  lastHitByLocalPlayerIndex: number | null;
  lastHitTimestamp: number | null;
  isFalling?: boolean;
  fallVelocityY?: number;
  isStarOrb?: boolean;
  starOrbType?: "wolfgang" | "hotstreak" | "bolt" | null;
  isOutOfRing?: boolean;
  scoredInZones?: string[]; // Track which zones this orb has already scored in
  capturedInZone?: string | null; // Track if orb has been captured (stopped) in a zone
}

// Orb capture effect interface
export interface OrbCaptureEffect {
  id: string;
  position: [number, number, number];
  color: string;
  orbId: string;
  createdAt: number;
  duration: number;
}

interface FallingEntity {
  id: string;
  entityType: "player" | "enemy" | "orb";
  entityIndex?: number;
  position: [number, number, number];
  velocity: [number, number, number];
  fallVelocityY: number;
  color: string;
  zoogiId?: string;
  hasLanded: boolean;
  landedAt?: number;
  createdAt: number;
}

interface WolfClone {
  id: string;
  position: [number, number, number];
  velocity: [number, number, number];
  isActive: boolean;
  sectorAngle: number;
  spawnedByPlayerId: string;
}

function steerWolfClone(
  clone: WolfClone,
  orbs: { isActive: boolean; isOutOfRing?: boolean; position: [number, number, number] }[],
  enemies: { id: string; position: [number, number, number] }[],
  stepScale = 1,
): [number, number, number] {
  let vx = clone.velocity[0];
  let vz = clone.velocity[2];
  let bestDx = 0;
  let bestDz = 0;
  let bestDist = Infinity;
  const consider = (x: number, z: number) => {
    const dx = x - clone.position[0];
    const dz = z - clone.position[2];
    const dist = Math.hypot(dx, dz);
    if (dist < bestDist) {
      bestDist = dist;
      bestDx = dx;
      bestDz = dz;
    }
  };
  for (const orb of orbs) {
    if (!orb.isActive || orb.isOutOfRing) continue;
    consider(orb.position[0], orb.position[2]);
  }
  for (const enemy of enemies) {
    if (enemy.id === clone.spawnedByPlayerId) continue;
    consider(enemy.position[0], enemy.position[2]);
  }
  if (bestDist < Infinity && bestDist > 0.3) {
    const homingStrength = 0.12 * stepScale;
    vx += (bestDx / bestDist) * homingStrength;
    vz += (bestDz / bestDist) * homingStrength;
  }
  const speed = Math.hypot(vx, vz);
  const maxSpeed = 1.8;
  if (speed > maxSpeed) {
    vx = (vx / speed) * maxSpeed;
    vz = (vz / speed) * maxSpeed;
  }
  return [vx, 0, vz];
}

interface GroundCrack {
  id: string;
  position: [number, number, number];
  createdAt: number;
}

interface Mushroom {
  id: string;
  position: [number, number, number];
  color: "red" | "white";
  repelForce: number;
  lastHitTime?: number;
}

interface PinballBumper {
  id: string;
  position: [number, number, number];
  repelForce: number;
  lastHitTime?: number;
  pointValue: number;
}

interface CollisionEvent {
  attackerId: string;
  attackerType: string;
  timestamp: number;
}

interface LandedRock {
  id: string;
  position: [number, number, number];
  radius: number;
  repelForce: number;
}

export interface LocalPlayer {
  id: number;
  name: string;
  zoogi: Zoogi | null;
  isEliminated: boolean;
  isRespawning: boolean;
  respawnTime: number | null;
  respawnPadIndex: number | null;
  lastHitByPlayerIndex: number | null;
}

interface GameEntity {
  id: string;
  zoogi: Zoogi;
  position: [number, number, number];
  velocity: [number, number, number];
  health: number;
  maxHealth: number;
  isPlayer: boolean;
  hasShield: boolean;
  shieldTimer: number;
  speedBoost: number;
  speedBoostTimer: number;
  abilityCooldown: number;
  lastHitByPlayer: boolean;
  lastHitByEnemyId: string | null;
  lastHitByLocalPlayerIndex: number | null;
  isStunned: boolean;
  stunTimer: number;
  larsRicochetBoost: number;
  score: number;
  wolfgangAbilityCooldown: number;
  wolfgangAbilityUsedThisTurn: boolean;
  wolfgangAbilityUnlocked: boolean;
  hotstreakAbilityCooldown: number;
  hotstreakAbilityUsedThisTurn: boolean;
  hotstreakAbilityUnlocked: boolean;
  hotstreakGrenadeTimer: number;
  hotstreakGrenadeArmed: boolean;
  boltAbilityCooldown: number;
  boltAbilityUsedThisTurn: boolean;
  boltAbilityUnlocked: boolean;
  larsAbilityUnlocked: boolean;
  wrapsAbilityUnlocked: boolean;
  wrapsBindUntil: number;
  nightshadeAbilityUnlocked: boolean;
  boltPhasingUntil: number;
  slowUntil: number;
  arcMovement: {
    type: "over" | "left" | "right" | null;
    initialDirection: [number, number];
    progress: number;
    maxHeight: number;
    waypoints?: [number, number, number][];
    currentWaypointIndex?: number;
  } | null;
  customModelUrl?: string;
  customThumbnailUrl?: string;
  isCustomZoogi?: boolean;
  invulnerableUntil: number | null;
  isRespawning: boolean;
  respawnAt: number | null;
  respawnPadIndex: number | null;
  isKnockedOut: boolean;
  spawnImmunity: boolean; // Immune to score penalties until entering the ring
  spawnPointIndex: number; // Which spawn point this player uses
}

interface PendingGrenade {
  id: string;
  position: [number, number, number];
  timer: number;
  ownerId: string;
}

interface WallSegment {
  index: number;
  health: number;
  maxHealth: number;
  isDestroyed: boolean;
  angle: number;
}

export interface OuterWallBlock {
  id: string;
  ownerId: string | null;
  ownerColor: string;
  isScattered: boolean;
  scatteredAt: number | null;
  rebuildProgress: number;
}

export interface ControlZone {
  id: string;
  angle: number;
  ownerId: string | null;
  ownerColor: string;
  captureProgress: number;
  isContested: boolean;
}

export interface ZoneEditorConfig {
  id: string;
  angle: number;
  distance: number;
  visible: boolean;
  isSpawn: boolean;
}

export interface CustomArenaDecoration {
  id: string;
  type: string;
  position: [number, number, number];
  rotation: number;
  scale: number;
}

export interface EditorPlacedModel {
  id: string;
  modelUrl: string;
  name: string;
  position: [number, number, number];
  rotation: [number, number, number];
  scale: [number, number, number];
}

export interface EditorWallBlock {
  id: string;
  position: [number, number, number];
  rotation: number;
  dimensions: { length: number; width: number; height: number };
}

export interface EditorScoringZone {
  id: string;
  position: [number, number, number];
  rotation: number;
  radius: number;
}

export interface WallOwnershipConfig {
  enabled: boolean;
  wallBlocks: EditorWallBlock[];
  scoringZones: EditorScoringZone[];
}

export interface CustomZoogiData {
  id: string;
  name: string;
  modelUrl: string;
  thumbnailUrl?: string;
  stats: {
    speed: number;
    power: number;
    control: number;
    ability: number;
  };
  createdAt: number;
}

interface ZoogiGameState {
  phase: GamePhase;
  gameMode: GameMode;
  selectedZoogi: Zoogi | null;
  selectedCustomZoogi: CustomZoogiData | null;
  selectedMap: MapTheme | null;
  customArenaId: string | null;
  customArenaDecorations: CustomArenaDecoration[];
  meshyArenaId: number | null;
  meshyArenaModelUrl: string | null;
  backgroundSettings: {
    distance: number;
    yPos: number;
    width: number;
    height: number;
    rotation: number;
    mirrorBack: boolean;
    mirrorFront: boolean;
    mirrorLeft: boolean;
    mirrorRight: boolean;
    opacity: number;
    visible: boolean;
    customImage: string | null;
    groundScale: number;
    modelPositionX: number;
    modelPositionY: number;
    modelPositionZ: number;
    modelScale: number;
  };
  setBackgroundSettings: (settings: ZoogiGameState['backgroundSettings']) => void;
  wallSettings: {
    outerWallEnabled: boolean;
    outerWallRadiusOffset: number;
    outerWallRows: number;
    outerWallBlocksPerRow: number;
    outerWallGapWidth: number;
    middleWallEnabled: boolean;
    middleWallRadiusOffset: number;
    middleWallRows: number;
    middleWallBlocksPerRow: number;
    middleWallGapWidth: number;
    innerWallEnabled: boolean;
    innerWallRadiusPercent: number;
    innerWallRows: number;
    innerWallBlocksPerRow: number;
    knockoffBoundaryRadius: number;
    knockoffBoundaryWidth: number;
    // New PBR/Video outer walls
    pbrInnerWallEnabled: boolean;
    pbrInnerWallRadiusOffset: number;
    pbrMiddleWallEnabled: boolean;
    pbrMiddleWallRadiusOffset: number;
    videoWallEnabled: boolean;
    videoWallRadiusOffset: number;
    videoWallHeight: number;
  };
  setWallSettings: (settings: Partial<ZoogiGameState['wallSettings']>) => void;
  
  selectedWallSegmentId: string | null;
  wallSegmentConfigs: WallSegmentConfig[];
  wallSegmentVersion: number;
  selectWallSegment: (id: string | null) => void;
  updateWallSegmentConfig: (id: string, updates: Partial<Omit<WallSegmentConfig, 'id'>>) => void;
  setWallSegmentConfigs: (configs: WallSegmentConfig[]) => void;
  toggleWallSegmentVisibility: (id: string) => void;
  incrementWallSegmentVersion: () => void;
  
  selectedInnerWallSegmentId: string | null;
  selectInnerWallSegment: (id: string | null) => void;
  updateInnerWallSegmentConfig: (id: string, updates: Partial<Omit<WallSegmentConfig, 'id'>>) => void;
  
  innerWallSegmentConfigs: WallSegmentConfig[];
  setInnerWallSegmentConfigs: (configs: WallSegmentConfig[]) => void;
  toggleInnerWallSegmentVisibility: (id: string) => void;
  zoneSettings: {
    globalRotationOffset: number;
    globalDistanceOffset: number;
    globalScale: number;
    zonesVisible: boolean;
  };
  setZoneSettings: (settings: ZoogiGameState['zoneSettings']) => void;
  
  collisionTuning: typeof DEFAULT_COLLISION_TUNING;
  setCollisionTuning: (settings: Partial<typeof DEFAULT_COLLISION_TUNING>) => void;
  showCollisionTuningPanel: boolean;
  setShowCollisionTuningPanel: (show: boolean) => void;
  
  aiControls: typeof DEFAULT_AI_CONTROLS;
  setAiControls: (settings: Partial<typeof DEFAULT_AI_CONTROLS>) => void;
  showAiControlsPanel: boolean;
  setShowAiControlsPanel: (show: boolean) => void;
  
  selectedTransformElement: TransformElementType;
  transformMode: TransformMode;
  showTransformGizmo: boolean;
  elementTransforms: typeof DEFAULT_ELEMENT_TRANSFORMS;
  setSelectedTransformElement: (element: TransformElementType) => void;
  setTransformMode: (mode: TransformMode) => void;
  setShowTransformGizmo: (show: boolean) => void;
  setElementTransforms: (transforms: Partial<typeof DEFAULT_ELEMENT_TRANSFORMS>) => void;
  updateElementTransform: (element: TransformElementType, offset: { x: number; y: number; z: number }) => void;
  updateElementRotation: (element: TransformElementType, rotation: { x: number; y: number; z: number }) => void;
  
  editorPlacedModels: EditorPlacedModel[];
  selectedEditorModelId: string | null;
  editorSnapEnabled: boolean;
  
  wallOwnershipConfig: WallOwnershipConfig;
  selectedEditorWallBlockId: string | null;
  selectedEditorScoringZoneId: string | null;
  toggleWallOwnershipEditor: () => void;
  addEditorWallBlock: (block: Omit<EditorWallBlock, 'id'>) => string;
  updateEditorWallBlock: (id: string, updates: Partial<Omit<EditorWallBlock, 'id'>>) => void;
  removeEditorWallBlock: (id: string) => void;
  selectEditorWallBlock: (id: string | null) => void;
  addEditorScoringZone: (zone: Omit<EditorScoringZone, 'id'>) => string;
  updateEditorScoringZone: (id: string, updates: Partial<Omit<EditorScoringZone, 'id'>>) => void;
  removeEditorScoringZone: (id: string) => void;
  selectEditorScoringZone: (id: string | null) => void;
  loadWallOwnershipConfig: (config: WallOwnershipConfig) => void;
  
  zoneEditorConfigs: ZoneEditorConfig[];
  selectedZoneId: string | null;
  selectZone: (id: string | null) => void;
  updateZoneConfig: (id: string, updates: Partial<Omit<ZoneEditorConfig, 'id'>>) => void;
  initializeZoneConfigs: () => void;
  
  playerEntity: GameEntity | null;
  enemies: GameEntity[];
  orbs: Orb[];
  score: number;
  isVictory: boolean;
  
  currentRound: number;
  maxRounds: number;
  playerRoundWins: number;
  enemyRoundWins: Map<string, number>;
  isPlayerTurn: boolean;
  // Set when the marble whose turn it is is actually launched. The turn ends
  // in the physics step once that marble stops, so a missed React frame cannot
  // leave the round stuck on "Your Turn".
  turnHasLaunched: boolean;
  turnIndex: number;
  allMovementStopped: boolean;
  
  localPlayers: LocalPlayer[];
  localPlayerCount: number;
  currentLocalPlayerIndex: number;
  
  gameTimer: number;
  sessionId: string;
  
  birdsEyeView: boolean;
  firstPersonView: boolean;
  overShoulderView: boolean;
  launchPadView: boolean;
  launchPadTilt: number;
  launchPadPitch: number;
  launchPadHeight: number;
  savedCameraPosition: { position: [number, number, number]; lookAt: [number, number, number] } | null;
  developerCamera: boolean;
  developerMoveMode: boolean;
  developerDragActive: boolean;
  isAiming: boolean;
  devToolsVisible: boolean;
  colliderDebug: boolean;
  ioControlsVisible: boolean;
  selectedMoveElement: { type: string; index: number } | null;
  moveUpdateCounter: number;
  showTutorial: boolean;
  tutorialStep: number;
  lastCollisionTime: number;
  lastScoreTime: number;
  lastLaunchTime: number;
  
  wolfClones: WolfClone[];
  groundCracks: GroundCrack[];
  mushrooms: Mushroom[];
  pinballBumpers: PinballBumper[];
  landedRocks: LandedRock[];
  fallingEntities: FallingEntity[];
  lastCollisionEvent: CollisionEvent | null;
  showExplosion: { position: [number, number, number]; timestamp: number; color?: string; radius?: number } | null;
  powerUnlocks: { id: string; position: [number, number, number]; startTime: number; color: string; anchorId: string }[];
  pendingGrenades: PendingGrenade[];
  
  orbCaptureEffects: OrbCaptureEffect[];
  triggerOrbCaptureEffect: (orbId: string, position: [number, number, number], color: string) => void;
  cleanupOrbCaptureEffects: () => void;
  
  knockoffBoundaryFlash: { color: string; timestamp: number; flashCount: number } | null;
  triggerKnockoffBoundaryFlash: (color: string, flashCount?: number) => void;
  
  // Scoring zones - zone control system
  activeScoringZones: Set<string>;
  lastScoringZoneUpdate: number;
  randomizeScoringZones: () => void;
  zoneOwnership: Map<string, string>;
  zoneControlActive: boolean;
  zoneControlActivationTime: number | null;
  allPlayersLaunchedOnce: boolean;
  scoredZonesThisTurn: Set<string>;
  updateZoneOwnership: (zoneId: string, playerColor: string) => void;
  checkZoneControlActivation: () => void;
  clearScoredZones: () => void;
  markZoneScored: (zoneId: string) => void;
  isZoneScoredThisTurn: (zoneId: string) => boolean;
  
  lockOnEnabled: boolean;
  lockOnTargetId: string | null;
  lockOnTargetType: "orb" | "enemy" | null;
  arcType: "over" | "left" | "right" | null;
  straightMode: boolean;
  tangentOffset: "none" | "left" | "right";
  orbMultiplier: 1 | 2 | 3;
  aiPlayerCount: 0 | 1 | 2 | 3;
  playerFacingRotation: { y: number; x: number } | null;
  cinematicArcMode: boolean;
  slowMotionFactor: number;
  
  wallSegments: WallSegment[];
  damageWallSegment: (segmentIndex: number, damage: number) => void;
  initializeWalls: () => void;
  
  restrictionPhaseActive: boolean;
  restrictionPhaseStartTime: number | null;
  nextRespawnPadIndex: number;
  firstTickProcessed: boolean;
  
  wallOwnershipMode: boolean;
  wallOwnershipStartTime: number | null;
  outerWallBlocks: OuterWallBlock[];
  controlZones: ControlZone[];
  ownershipScores: Record<string, number>;
  activateWallOwnershipMode: () => void;
  updateBlockOwnership: (blockId: string, ownerId: string, ownerColor: string) => void;
  scatterBlock: (blockId: string) => void;
  updateBlockRebuild: (blockId: string, progress: number) => void;
  updateControlZone: (zoneId: string, ownerId: string | null, progress: number, contested: boolean) => void;
  addOwnershipScore: (playerId: string, points: number) => void;
  
  setPhase: (phase: GamePhase) => void;
  setGameMode: (mode: GameMode) => void;
  toggleBirdsEyeView: () => void;
  toggleFirstPersonView: () => void;
  toggleOverShoulderView: () => void;
  toggleLaunchPadView: () => void;
  setLaunchPadTilt: (tilt: number) => void;
  setLaunchPadPitch: (pitch: number) => void;
  setLaunchPadHeight: (height: number) => void;
  setSavedCameraPosition: (pos: { position: [number, number, number]; lookAt: [number, number, number] } | null) => void;
  toggleDeveloperCamera: () => void;
  toggleDeveloperMoveMode: () => void;
  setDeveloperMoveMode: (enabled: boolean) => void;
  toggleDevToolsVisible: () => void;
  toggleColliderDebug: () => void;
  toggleIoControlsVisible: () => void;
  setMapForEditing: (map: MapTheme) => void;
  setSelectedMoveElement: (element: { type: string; index: number } | null) => void;
  moveSelectedElement: (deltaX: number, deltaY: number, deltaZ: number) => void;
  incrementMoveCounter: () => void;
  setDeveloperDragActive: (active: boolean) => void;
  setIsAiming: (aiming: boolean) => void;
  openTutorial: () => void;
  closeTutorial: () => void;
  nextTutorialStep: () => void;
  triggerCollision: (attackerType?: string) => void;
  triggerScore: () => void;
  
  spawnWolfClones: (position: [number, number, number], velocity: [number, number, number], ownerId?: string) => void;
  updateWolfClones: () => void;
  clearWolfClones: () => void;
  activateWolfgangAbility: (casterId: string) => void;
  canUseWolfgangAbility: () => boolean;
  activateHotstreakAbility: (casterId: string) => void;
  canUseHotstreakAbility: () => boolean;
  activateBoltAbility: (casterId: string) => void;
  canUseBoltAbility: () => boolean;
  activateLarsAbility: (casterId: string) => void;
  canUseLarsAbility: () => boolean;
  activateWrapsAbility: (casterId: string) => void;
  canUseWrapsAbility: () => boolean;
  activateNightshadeAbility: (casterId: string) => void;
  canUseNightshadeAbility: () => boolean;
  aiPowerOpportunity: (casterId: string) => AiPowerName | null;
  useAiPower: (casterId: string) => AiPowerName | null;
  debugUnlockPower: (entityId: string) => void;
  abilityNotice: { text: string; until: number } | null;
  showAbilityNotice: (text: string) => void;
  
  toggleLockOn: () => void;
  setLockOnTarget: (targetId: string | null, targetType: "orb" | "enemy" | null) => void;
  clearLockOn: () => void;
  setArcType: (arcType: "over" | "left" | "right" | null) => void;
  toggleStraightMode: () => void;
  setTangentOffset: (offset: "none" | "left" | "right") => void;
  setOrbMultiplier: (multiplier: 1 | 2 | 3) => void;
  incrementOrbMultiplier: () => void;
  decrementOrbMultiplier: () => void;
  setAiPlayerCount: (count: 0 | 1 | 2 | 3) => void;
  incrementAiPlayerCount: () => void;
  decrementAiPlayerCount: () => void;
  setPlayerFacingRotation: (rotation: { y: number; x: number } | null) => void;
  triggerArcLaunch: () => void;
  arcWaypoints: [number, number, number][] | null;
  setCinematicArcMode: (enabled: boolean) => void;
  arcPeakEffectActive: boolean;
  arcPeakTriggeredThisLaunch: boolean;
  arcPeakTargetPosition: [number, number, number] | null;
  setArcPeakEffectActive: (active: boolean) => void;
  clearArcPeakEffect: () => void;
  
  addGroundCrack: (position: [number, number, number]) => void;
  triggerExplosion: (position: [number, number, number], color?: string, radius?: number) => void;
  addLandedRock: (rock: LandedRock) => void;
  armHotstreakGrenade: (position: [number, number, number], ownerId: string) => void;
  
  applyStun: (entityId: string) => void;
  applyLarsBoost: () => void;
  selectZoogi: (zoogi: Zoogi) => void;
  selectCustomZoogi: (customZoogi: CustomZoogiData | null) => void;
  selectMap: (map: MapTheme, customArenaId?: string, customDecorations?: CustomArenaDecoration[], meshyArenaId?: number, meshyArenaModelUrl?: string) => void;
  startGame: () => void;
  startPracticeGame: () => void;
  startNextRound: () => void;
  restartWithSameZoogi: () => void;
  endGame: (victory: boolean) => void;
  returnToMenu: () => void;
  
  updatePlayerPosition: (position: [number, number, number]) => void;
  updatePlayerVelocity: (velocity: [number, number, number]) => void;
  getCurrentControlledEntity: () => GameEntity | null;
  updateLocalPlayerVelocity: (playerIndex: number, velocity: [number, number, number]) => void;
  respawnPlayer: () => void;
  
  updateEnemy: (id: string, updates: Partial<GameEntity>) => void;
  respawnEnemy: (id: string) => void;
  
  updateOrb: (id: string, updates: Partial<Orb>) => void;
  removeOrb: (id: string) => void;
  
  addScore: (points: number) => void;
  scoreKnockOff: (type: "enemy" | "orb", points?: number) => void;
  
  endTurn: () => void;
  setMovementStopped: (stopped: boolean) => void;
  resetTurnState: () => void;
  freezeAllEntities: () => void;
  
  setLocalPlayerCount: (count: number) => void;
  setLocalPlayerZoogi: (playerIndex: number, zoogi: Zoogi) => void;
  setLocalPlayerName: (playerIndex: number, name: string) => void;
  startLocalGame: () => void;
  startMapEditor: () => void;
  eliminateLocalPlayer: (playerIndex: number) => void;
  
  addEditorPlacedModel: (model: Omit<EditorPlacedModel, "id">) => string;
  removeEditorPlacedModel: (id: string) => void;
  updateEditorPlacedModel: (id: string, updates: Partial<EditorPlacedModel>) => void;
  clearEditorPlacedModels: () => void;
  loadEditorPlacedModels: (models: EditorPlacedModel[]) => void;
  selectEditorModel: (id: string | null) => void;
  toggleEditorSnap: () => void;
  moveSelectedEditorModel: (dx: number, dy: number, dz: number) => void;
  rotateSelectedEditorModel: (axis: "y" | "x" | "z", angleDegrees: number) => void;
  scaleSelectedEditorModel: (scaleDelta: number) => void;
  
  saveMapDecorations: () => Promise<boolean>;
  loadMapDecorations: (mapId: string) => Promise<void>;
  exportBackgroundSettings: () => void;
  
  tickTimers: (delta: number) => void;
  physicsTick: (delta: number) => void;
}


const getRandomSpawnPosition = (): [number, number, number] => {
  const angle = Math.random() * Math.PI * 2;
  const radius = Math.random() * (ARENA_RADIUS - 3) * 0.6;
  return [Math.cos(angle) * radius, 0.5, Math.sin(angle) * radius];
};

const RESPAWN_PAD_DISTANCE = ARENA_RADIUS + 4;
const RESPAWN_PAD_ANGLES = [0, Math.PI / 2, Math.PI, Math.PI * 1.5];

export const RESPAWN_PADS: { position: [number, number, number]; angle: number; facingAngle: number }[] = RESPAWN_PAD_ANGLES.map(angle => ({
  position: [
    Math.cos(angle) * RESPAWN_PAD_DISTANCE,
    0.5,
    Math.sin(angle) * RESPAWN_PAD_DISTANCE
  ],
  angle,
  facingAngle: angle + Math.PI
}));

export const GREEN_RESPAWN_PADS: { position: [number, number, number]; angle: number }[] = WALL_OWNERSHIP_GAP_ANGLES.map(angle => ({
  position: [
    Math.cos(angle) * WALL_OWNERSHIP_GREEN_RESPAWN_RADIUS,
    0.5,
    Math.sin(angle) * WALL_OWNERSHIP_GREEN_RESPAWN_RADIUS
  ],
  angle
}));

let nextRespawnPadIndex = 0;
let nextGreenRespawnIndex = 0;

const getNextRespawnPadPosition = (): [number, number, number] => {
  const pad = RESPAWN_PADS[nextRespawnPadIndex];
  nextRespawnPadIndex = (nextRespawnPadIndex + 1) % RESPAWN_PADS.length;
  return pad.position;
};

const getNextGreenRespawnPosition = (): [number, number, number] => {
  const pad = GREEN_RESPAWN_PADS[nextGreenRespawnIndex];
  nextGreenRespawnIndex = (nextGreenRespawnIndex + 1) % GREEN_RESPAWN_PADS.length;
  return pad.position;
};

const resetRespawnIndices = () => {
  nextRespawnPadIndex = 0;
  nextGreenRespawnIndex = 0;
};

const createEnemy = (zoogi: Zoogi, position: [number, number, number], spawnPointIndex: number = 0, hasSpawnImmunity: boolean = false): GameEntity => ({
  id: `enemy-${zoogi.id}-${Math.random().toString(36).substr(2, 9)}`,
  zoogi,
  position,
  velocity: [0, 0, 0],
  health: 100,
  maxHealth: 100,
  isPlayer: false,
  hasShield: false,
  shieldTimer: 0,
  speedBoost: 1,
  speedBoostTimer: 0,
  abilityCooldown: 0,
  lastHitByPlayer: false,
  lastHitByEnemyId: null,
  lastHitByLocalPlayerIndex: null,
  isStunned: false,
  stunTimer: 0,
  larsRicochetBoost: 1,
  score: 0,
  wolfgangAbilityCooldown: 0,
  wolfgangAbilityUsedThisTurn: false,
  wolfgangAbilityUnlocked: false,
  hotstreakAbilityCooldown: 0,
  hotstreakAbilityUsedThisTurn: false,
  hotstreakAbilityUnlocked: false,
  hotstreakGrenadeTimer: 0,
  hotstreakGrenadeArmed: false,
  boltAbilityCooldown: 0,
  boltAbilityUsedThisTurn: false,
  boltAbilityUnlocked: false,
  larsAbilityUnlocked: false,
  wrapsAbilityUnlocked: false,
  wrapsBindUntil: 0,
  nightshadeAbilityUnlocked: false,
  boltPhasingUntil: 0,
  slowUntil: 0,
  arcMovement: null,
  invulnerableUntil: null,
  isRespawning: false,
  respawnAt: null,
  respawnPadIndex: null,
  isKnockedOut: false,
  spawnImmunity: hasSpawnImmunity,
  spawnPointIndex
});

const createOrbs = (ringRadius = 5): Orb[] => {
  const orbs: Orb[] = [];
  const orbColor = "#87CEEB"; // Light blue for all orbs
  
  // Spawn 15 orbs in a circular pattern
  const orbCount = 15;
  const radius = ringRadius;
  
  // Randomly select 3 orbs to be star orbs (unlock different abilities)
  const indices = Array.from({ length: orbCount }, (_, i) => i);
  const shuffled = indices.sort(() => Math.random() - 0.5);
  const wolfgangStarIndex = shuffled[0];
  const hotstreakStarIndex = shuffled[1];
  const boltStarIndex = shuffled[2];
  
  for (let i = 0; i < orbCount; i++) {
    const angle = (i / orbCount) * Math.PI * 2;
    const x = Math.cos(angle) * radius;
    const z = Math.sin(angle) * radius;
    
    let starOrbType: "wolfgang" | "hotstreak" | "bolt" | null = null;
    if (i === wolfgangStarIndex) starOrbType = "wolfgang";
    else if (i === hotstreakStarIndex) starOrbType = "hotstreak";
    else if (i === boltStarIndex) starOrbType = "bolt";
    
    orbs.push({
      id: `orb-${i}-${Math.random().toString(36).substr(2, 9)}`,
      position: [x, 0.5, z],
      velocity: [0, 0, 0],
      color: orbColor,
      points: 50,
      isActive: true,
      lastHitBy: null,
      lastHitByEnemyId: null,
      lastHitByLocalPlayerIndex: null,
      lastHitTimestamp: null,
      isStarOrb: starOrbType !== null,
      starOrbType
    });
  }
  
  return orbs;
};

const createMushrooms = (): Mushroom[] => {
  // Mushrooms are not drawn and are not solids. Keep the array empty so no
  // leftover positions can be mistaken for colliders.
  return [];
};

const LEGACY_BUMPER_POSITIONS: [number, number, number][] = [
  [10, 0, 0],
  [-10, 0, 0],
  [0, 0, 10],
  [0, 0, -10],
  [7, 0, 7],
  [-7, 0, 7],
  [7, 0, -7],
  [-7, 0, -7],
];

const createPinballBumpers = (map?: string | null): PinballBumper[] => {
  const layout = map ? getMapLayout(map) : null;
  const positions: [number, number, number][] = layout
    ? layout.bumpers.map((bumper) => [bumper.x, 0, bumper.z])
    : LEGACY_BUMPER_POSITIONS;
  return positions.map((pos, i) => ({
    id: `bumper-${i}`,
    position: pos,
    repelForce: 0.5,
    lastHitTime: undefined,
    pointValue: 50,
  }));
};

const createPracticeOrbs = (multiplier: 1 | 2 | 3 = 1): Orb[] => {
  const orbs: Orb[] = [];
  const colors = ["#FFD700", "#FF69B4", "#00CED1", "#FF6347", "#7CFC00", "#FF00FF", "#00FF00", "#FF8C00"];
  let total = 0;
  for (let ring = 1; ring <= multiplier; ring++) {
    total += 4 + (ring - 1) * 4;
  }
  const starSlots = new Map<number, "wolfgang" | "hotstreak" | "bolt">();
  const bag = Array.from({ length: total }, (_, index) => index);
  for (let i = bag.length - 1; i > 0; i--) {
    const j = Math.floor(Math.random() * (i + 1));
    const swap = bag[i];
    bag[i] = bag[j];
    bag[j] = swap;
  }
  const starTypes = ["wolfgang", "hotstreak", "bolt"] as const;
  for (let slot = 0; slot < Math.min(3, total); slot++) {
    starSlots.set(bag[slot], starTypes[slot]);
  }

  let orbIndex = 0;
  
  // Spawn orbs in expanding rings based on multiplier
  for (let ring = 1; ring <= multiplier; ring++) {
    const radius = ring * 1.8; // Stay inside the bumper ring on the scaled playfield
    const orbsInRing = 4 + (ring - 1) * 4; // 4, 8, 12 orbs per ring
    
    for (let i = 0; i < orbsInRing; i++) {
      const angle = (i / orbsInRing) * Math.PI * 2;
      const x = Math.cos(angle) * radius;
      const z = Math.sin(angle) * radius;
      const starOrbType = starSlots.get(orbIndex) ?? null;
      
      orbs.push({
        id: `orb-r${ring}-i${i}`,
        position: [x, 0.5, z],
        velocity: [0, 0, 0],
        color: colors[orbIndex % colors.length],
        points: 50,
        isActive: true,
        lastHitBy: null,
        lastHitByEnemyId: null,
        lastHitByLocalPlayerIndex: null,
        lastHitTimestamp: null,
        isStarOrb: starOrbType !== null,
        starOrbType
      });
      orbIndex++;
    }
  }
  
  return orbs;
};

const initializePracticeGame = (selectedZoogi: Zoogi, orbMultiplier: 1 | 2 | 3 = 1) => {
  // Practice mode starts player in center (no spawn immunity)
  const playerEntity: GameEntity = {
    id: "player",
    zoogi: selectedZoogi,
    position: [0, 0.5, 0],
    velocity: [0, 0, 0],
    health: 100,
    maxHealth: 100,
    isPlayer: true,
    hasShield: false,
    shieldTimer: 0,
    speedBoost: 1,
    speedBoostTimer: 0,
    abilityCooldown: 0,
    lastHitByPlayer: false,
    lastHitByEnemyId: null,
    lastHitByLocalPlayerIndex: null,
    isStunned: false,
    stunTimer: 0,
    larsRicochetBoost: 1,
    score: 0,
    wolfgangAbilityCooldown: 0,
    wolfgangAbilityUsedThisTurn: false,
    wolfgangAbilityUnlocked: false,
    hotstreakAbilityCooldown: 0,
    hotstreakAbilityUsedThisTurn: false,
    hotstreakAbilityUnlocked: false,
    hotstreakGrenadeTimer: 0,
    hotstreakGrenadeArmed: false,
    boltAbilityCooldown: 0,
    boltAbilityUsedThisTurn: false,
    boltAbilityUnlocked: false,
    larsAbilityUnlocked: false,
    wrapsAbilityUnlocked: false,
    wrapsBindUntil: 0,
    nightshadeAbilityUnlocked: false,
    boltPhasingUntil: 0,
    slowUntil: 0,
    arcMovement: null,
    invulnerableUntil: null,
    isRespawning: false,
    respawnAt: null,
    respawnPadIndex: null,
    isKnockedOut: false,
    spawnImmunity: false,
    spawnPointIndex: 0
  };
  
  const orbs = createPracticeOrbs(orbMultiplier);
  
  return { playerEntity, enemies: [], orbs, mushrooms: [], pinballBumpers: createPinballBumpers("grass") };
};

function zonesFromLayout(map: string | null | undefined): ZoneEditorConfig[] | null {
  const layout = getMapLayout(map);
  if (!layout) return null;
  return layout.zones.map((zone) => ({
    id: zone.id,
    angle: zone.angle,
    distance: zone.distance,
    visible: zone.visible,
    isSpawn: zone.isSpawn,
  }));
}

/** Knockoff ring, scoring zones, and grass stage scale for a match. Null map keeps the caller's settings. */
function playfieldSettings(
  map: string | null | undefined,
  wallSettings: ZoogiGameState["wallSettings"],
  backgroundSettings: ZoogiGameState["backgroundSettings"],
  elementTransforms: ZoogiGameState["elementTransforms"],
) {
  const layout = getMapLayout(map);
  const zones = zonesFromLayout(map);
  if (!layout || !zones) return {};
  const patch: Partial<ZoogiGameState> = {
    zoneEditorConfigs: zones,
    wallSettings: { ...wallSettings, knockoffBoundaryRadius: layout.knockoffRadius },
    elementTransforms: {
      ...elementTransforms,
      knockoffBoundaryOffset: knockoffOffsetForMap(map),
    },
  };
  const stage =
    map === "grass"
      ? { x: 0, y: GRASS_STAGE.modelOffsetY, z: 0, scale: GRASS_STAGE.modelScale }
      : map === "space"
        ? { x: 0, y: COSMOS_STAGE.modelOffsetY, z: 0, scale: COSMOS_STAGE.modelScale }
        : map === "saturn"
          ? arabianPlayTransform()
          : null;
  if (stage) {
    patch.backgroundSettings = {
      ...backgroundSettings,
      modelScale: stage.scale,
      modelPositionX: stage.x,
      modelPositionY: stage.y,
      modelPositionZ: stage.z,
    };
  }
  return patch;
}

const initializeGame = (
  selectedZoogi: Zoogi, 
  gameMode: GameMode = "classic", 
  aiPlayerCount: number = 3,
  zoneEditorConfigs?: { angle: number; distance: number; isSpawn: boolean }[],
  mapTheme?: string
) => {
  // Set active map theme so all getSpawnPointPosition calls use the right radius
  if (mapTheme) setActiveMapTheme(mapTheme);

  // A selected map owns its spawns. Saved or leftover zone configs are ignored
  // so a marble does not start outside the playfield. A null map (tests) keeps
  // the configs the caller passed in.
  const layout = getMapLayout(mapTheme);
  const spawnZones = layout ? zonesFromLayout(mapTheme) ?? undefined : zoneEditorConfigs;
  
  // Player spawns at spawn point 0 (using zone editor spawn points if available)
  const playerSpawnIndex = 0;
  const playerSpawnPos = getSpawnPointPosition(playerSpawnIndex, spawnZones, mapTheme);
  console.log("Player spawn position:", playerSpawnPos[0].toFixed(2), playerSpawnPos[2].toFixed(2), "from layout:", layout?.id ?? "none");
  
  const playerEntity: GameEntity = {
    id: "player",
    zoogi: selectedZoogi,
    position: playerSpawnPos,
    velocity: [0, 0, 0],
    health: 100,
    maxHealth: 100,
    isPlayer: true,
    hasShield: false,
    shieldTimer: 0,
    speedBoost: 1,
    speedBoostTimer: 0,
    abilityCooldown: 0,
    lastHitByPlayer: false,
    lastHitByEnemyId: null,
    lastHitByLocalPlayerIndex: null,
    isStunned: false,
    stunTimer: 0,
    larsRicochetBoost: 1,
    score: 0,
    wolfgangAbilityCooldown: 0,
    wolfgangAbilityUsedThisTurn: false,
    wolfgangAbilityUnlocked: false,
    hotstreakAbilityCooldown: 0,
    hotstreakAbilityUsedThisTurn: false,
    hotstreakAbilityUnlocked: false,
    hotstreakGrenadeTimer: 0,
    hotstreakGrenadeArmed: false,
    boltAbilityCooldown: 0,
    boltAbilityUsedThisTurn: false,
    boltAbilityUnlocked: false,
    larsAbilityUnlocked: false,
    wrapsAbilityUnlocked: false,
    wrapsBindUntil: 0,
    nightshadeAbilityUnlocked: false,
    boltPhasingUntil: 0,
    slowUntil: 0,
    arcMovement: null,
    invulnerableUntil: null,
    isRespawning: false,
    respawnAt: null,
    respawnPadIndex: null,
    isKnockedOut: false,
    spawnImmunity: false,
    spawnPointIndex: playerSpawnIndex
  };
  
  const availableEnemies = ZOOGI_ROSTER.filter(z => z.id !== selectedZoogi.id);
  const shuffled = [...availableEnemies].sort(() => Math.random() - 0.5);
  
  const isRingerRoyale = gameMode === "ringer_royale";
  const enemyCount = isRingerRoyale ? availableEnemies.length : Math.min(aiPlayerCount, 3);
  const selectedEnemies = isRingerRoyale ? availableEnemies : shuffled.slice(0, enemyCount);
  
  // Each enemy spawns at their own spawn point (using zone editor spawn points if available)
  const enemies = selectedEnemies.map((zoogi, i) => {
    const spawnIndex = i + 1; // Player is at 0, enemies at 1, 2, 3, ...
    const spawnPos = getSpawnPointPosition(spawnIndex, spawnZones, mapTheme);
    return createEnemy(zoogi, spawnPos, spawnIndex, false);
  });
  
  const orbs = createOrbs(layout?.orbRingRadius ?? 5);
  const mushrooms = createMushrooms();
  const pinballBumpers = createPinballBumpers(mapTheme);
  
  return { playerEntity, enemies, orbs, mushrooms, pinballBumpers };
};

let lastTurnEndedAt = 0;
let turnWatchKey = "";
let turnReadyForLaunch = false;
let turnSlowFrames = 0;

/** Fold this round's score into the round-win counters. A tied round goes to the player. */
function awardRoundWin(
  score: number,
  enemies: { id: string; score: number; zoogi?: { name?: string } }[],
  playerRoundWins: number,
  enemyRoundWins: Map<string, number>,
) {
  const maxEnemyScore = Math.max(...enemies.map((enemy) => enemy.score), 0);
  const winningEnemy = enemies.find((enemy) => enemy.score === maxEnemyScore && maxEnemyScore > 0);
  let nextPlayerWins = playerRoundWins;
  const nextEnemyWins = new Map(enemyRoundWins);
  if (score > maxEnemyScore) {
    nextPlayerWins++;
    console.log(`Round winner: Player! (${score} vs ${maxEnemyScore})`);
  } else if (winningEnemy && maxEnemyScore > score) {
    nextEnemyWins.set(winningEnemy.id, (nextEnemyWins.get(winningEnemy.id) || 0) + 1);
    console.log(`Round winner: ${winningEnemy.zoogi?.name || "Enemy"}! (${maxEnemyScore} vs ${score})`);
  } else {
    nextPlayerWins++;
    console.log(`Round tie - Player wins the round (${score} vs ${maxEnemyScore})`);
  }
  const bestEnemyWins = Math.max(...Array.from(nextEnemyWins.values()), 0);
  return { nextPlayerWins, nextEnemyWins, bestEnemyWins };
}

/**
 * A marble may end the turn only while the live store still has that marble up
 * and the launch flag is still set. Physics clears the flag as it hands off,
 * so a later stop-frame must not call endTurn again.
 */
export function stopFrameMayEndTurn(
  state: {
    gameMode: string;
    isPlayerTurn: boolean;
    turnIndex: number;
    currentLocalPlayerIndex: number;
    turnHasLaunched: boolean;
  },
  actor: "player" | "enemy" | "local",
  index = 0,
): boolean {
  if (!state.turnHasLaunched || state.gameMode === "ringer_royale") return false;
  if (actor === "player") return state.isPlayerTurn;
  if (actor === "enemy") return !state.isPlayerTurn && state.turnIndex === index;
  return state.gameMode === "local_multiplayer" && state.currentLocalPlayerIndex === index;
}

function actorTakingTurn(state: {
  gameMode: string;
  isPlayerTurn: boolean;
  turnIndex: number;
  currentLocalPlayerIndex: number;
  playerEntity: { velocity: [number, number, number]; isKnockedOut?: boolean } | null;
  enemies: { velocity: [number, number, number]; isKnockedOut?: boolean }[];
}) {
  if (state.gameMode === "local_multiplayer") {
    if (state.currentLocalPlayerIndex <= 0) return state.playerEntity;
    return state.enemies[state.currentLocalPlayerIndex - 1] ?? null;
  }
  if (state.isPlayerTurn) return state.playerEntity;
  return state.enemies[state.turnIndex] ?? state.enemies[0] ?? null;
}

function unlockPatchForZoogi(zoogiId: string): Partial<GameEntity> {
  switch (zoogiId) {
    case "wolfgang": return { wolfgangAbilityUnlocked: true };
    case "hotstreak": return { hotstreakAbilityUnlocked: true };
    case "bolt": return { boltAbilityUnlocked: true };
    case "lars": return { larsAbilityUnlocked: true };
    case "wraps": return { wrapsAbilityUnlocked: true };
    case "nightshade": return { nightshadeAbilityUnlocked: true };
    default: return {};
  }
}

export type AiPowerName = "hotstreak" | "bolt" | "wolfgang" | "lars" | "wraps" | "nightshade";

const BLAST_RADIUS = 8;
const BIND_RADIUS = 7;
/** Shorter than Bolt's shock (8) and Wraps' bind (7). Close enough to catch a marble you are beside, not the whole ring. */
export const SHADOW_STUN_RADIUS = 4.5;

const flashedStarOrbIds = new Set<string>();

function distanceXZ(a: [number, number, number], b: [number, number, number]): number {
  return Math.hypot(a[0] - b[0], a[2] - b[2]);
}

function readEntity(state: { playerEntity: GameEntity | null; enemies: GameEntity[] }, id: string): GameEntity | null {
  if (state.playerEntity?.id === id) return state.playerEntity;
  return state.enemies.find((enemy) => enemy.id === id) ?? null;
}

function writeEntity(
  set: (fn: (state: ZoogiGameState) => Partial<ZoogiGameState>) => void,
  id: string,
  patch: Partial<GameEntity>,
) {
  set((state) => ({
    playerEntity: state.playerEntity && state.playerEntity.id === id
      ? { ...state.playerEntity, ...patch }
      : state.playerEntity,
    enemies: state.enemies.map((enemy) => (enemy.id === id ? { ...enemy, ...patch } : enemy)),
  }));
}

function starFlashColor(starType: string | null | undefined): string {
  if (starType === "hotstreak") return "#ff4444";
  if (starType === "bolt") return "#4488ff";
  return "#ffd700";
}

/** Gold burst sits on the star's exit when that point is in frame, otherwise on the marble that unlocked it. */
export function resolveUnlockSpot(
  exit: [number, number, number],
  anchor: [number, number, number] | null,
  exitOnScreen: boolean,
): [number, number, number] {
  const source = exitOnScreen || !anchor ? exit : anchor;
  return [source[0], source[1], source[2]];
}

function flashStarUnlock(
  set: (fn: (state: ZoogiGameState) => Partial<ZoogiGameState>) => void,
  orb: { id: string; position: [number, number, number]; starOrbType?: string | null },
  anchorId: string,
) {
  if (flashedStarOrbIds.has(orb.id)) return;
  flashedStarOrbIds.add(orb.id);
  useAudio.getState().playPowerUnlock();
  const now = Date.now();
  set((state) => ({
    powerUnlocks: [
      ...state.powerUnlocks.filter((flash) => now - flash.startTime < 4000),
      {
        id: orb.id,
        position: [orb.position[0], orb.position[1], orb.position[2]],
        startTime: now,
        color: starFlashColor(orb.starOrbType),
        anchorId,
      },
    ],
  }));
}

function someoneInside(
  origin: [number, number, number],
  points: [number, number, number][],
  radius: number,
): boolean {
  return points.some((point) => {
    const dist = distanceXZ(origin, point);
    return dist > 0.1 && dist < radius;
  });
}

/** Which one-shot an AI marble may cast right now. Locked powers return null. */
export function chooseAiPower(
  caster: GameEntity,
  others: GameEntity[],
  orbs: { isActive: boolean; isOutOfRing?: boolean; position: [number, number, number] }[],
): AiPowerName | null {
  const foes = others.filter((entity) => entity.id !== caster.id && !entity.isKnockedOut);
  const foePoints = foes.map((entity) => entity.position);
  const orbPoints = orbs.filter((orb) => orb.isActive && !orb.isOutOfRing).map((orb) => orb.position);
  const moving = Math.hypot(caster.velocity[0], caster.velocity[2]) >= 0.05;
  switch (caster.zoogi.id) {
    case "hotstreak":
      if (!caster.hotstreakAbilityUnlocked) return null;
      return someoneInside(caster.position, [...foePoints, ...orbPoints], BLAST_RADIUS) ? "hotstreak" : null;
    case "bolt":
      if (!caster.boltAbilityUnlocked) return null;
      return someoneInside(caster.position, foePoints, BLAST_RADIUS) ? "bolt" : null;
    case "wolfgang":
      if (!caster.wolfgangAbilityUnlocked || !moving) return null;
      return "wolfgang";
    case "lars":
      return caster.larsAbilityUnlocked ? "lars" : null;
    case "wraps":
      if (!caster.wrapsAbilityUnlocked) return null;
      return someoneInside(caster.position, foePoints, BIND_RADIUS) ? "wraps" : null;
    case "nightshade":
      if (!caster.nightshadeAbilityUnlocked) return null;
      return someoneInside(caster.position, foePoints, SHADOW_STUN_RADIUS) ? "nightshade" : null;
    default:
      return null;
  }
}

function respawnInsidePlayfield(
  spawnIndex: number,
  zones: { angle: number; distance: number; isSpawn: boolean }[] | undefined,
  map: string | null | undefined,
  knockoffRadius: number,
): [number, number, number] {
  const pos = getSpawnPointPosition(spawnIndex, zones, map ?? undefined);
  const dist = Math.hypot(pos[0], pos[2]);
  const limit = Math.max(3, knockoffRadius - 2.5);
  if (dist > limit && dist > 0) {
    const scale = limit / dist;
    return [pos[0] * scale, 0.5, pos[2] * scale];
  }
  return [pos[0], 0.5, pos[2]];
}

function readyToPlaceBack(entity: { isKnockedOut?: boolean; isRespawning?: boolean; respawnAt?: number | null } | null | undefined): boolean {
  if (!entity || !(entity.isKnockedOut || entity.isRespawning)) return false;
  if (entity.respawnAt != null && Date.now() < entity.respawnAt) return false;
  return true;
}

export const useZoogiGame = create<ZoogiGameState>()(
  subscribeWithSelector((set, get) => ({
    phase: "menu",
    gameMode: "classic",
    selectedZoogi: null,
    selectedCustomZoogi: null,
    selectedMap: null,
    customArenaId: null,
    customArenaDecorations: [],
    meshyArenaId: null,
    meshyArenaModelUrl: null,
    backgroundSettings: { ...DEFAULT_BACKGROUND_SETTINGS },
    setBackgroundSettings: (settings) => set({ backgroundSettings: { ...DEFAULT_BACKGROUND_SETTINGS, ...settings } }),
    wallSettings: { ...DEFAULT_WALL_SETTINGS },
    setWallSettings: (settings) => {
      set((state) => ({ 
        wallSettings: { ...state.wallSettings, ...settings },
        wallSegmentVersion: state.wallSegmentVersion + 1
      }));
    },
    
    selectedWallSegmentId: null,
    wallSegmentConfigs: [],
    wallSegmentVersion: 0,
    innerWallSegmentConfigs: [],
    selectedInnerWallSegmentId: null,
    
    selectWallSegment: (id) => set({ selectedWallSegmentId: id, selectedInnerWallSegmentId: null }),
    
    selectInnerWallSegment: (id) => set({ selectedInnerWallSegmentId: id, selectedWallSegmentId: null }),
    
    updateInnerWallSegmentConfig: (id, updates) => set((state) => ({
      innerWallSegmentConfigs: state.innerWallSegmentConfigs.map(config =>
        config.id === id ? { ...config, ...updates } : config
      )
    })),
    
    updateWallSegmentConfig: (id, updates) => set((state) => ({
      wallSegmentConfigs: state.wallSegmentConfigs.map(config =>
        config.id === id ? { ...config, ...updates } : config
      )
    })),
    
    setWallSegmentConfigs: (configs) => set({ wallSegmentConfigs: configs }),
    
    toggleWallSegmentVisibility: (id) => set((state) => ({
      wallSegmentConfigs: state.wallSegmentConfigs.map(config =>
        config.id === id ? { ...config, visible: !config.visible } : config
      )
    })),
    
    incrementWallSegmentVersion: () => set((state) => ({ 
      wallSegmentVersion: state.wallSegmentVersion + 1 
    })),
    
    setInnerWallSegmentConfigs: (configs) => set({ innerWallSegmentConfigs: configs }),
    
    toggleInnerWallSegmentVisibility: (id) => set((state) => ({
      innerWallSegmentConfigs: state.innerWallSegmentConfigs.map(config =>
        config.id === id ? { ...config, visible: !config.visible } : config
      )
    })),
    
    zoneSettings: { ...DEFAULT_ZONE_SETTINGS },
    setZoneSettings: (settings) => set((state) => ({ zoneSettings: { ...state.zoneSettings, ...settings } })),
    
    collisionTuning: { ...DEFAULT_COLLISION_TUNING },
    setCollisionTuning: (settings) => set((state) => ({ collisionTuning: { ...state.collisionTuning, ...settings } })),
    showCollisionTuningPanel: false,
    setShowCollisionTuningPanel: (show) => set({ showCollisionTuningPanel: show }),
    
    aiControls: { ...DEFAULT_AI_CONTROLS },
    setAiControls: (settings) => set((state) => ({ aiControls: { ...state.aiControls, ...settings } })),
    showAiControlsPanel: false,
    setShowAiControlsPanel: (show) => set({ showAiControlsPanel: show }),
    
    selectedTransformElement: null,
    transformMode: "translate",
    showTransformGizmo: true,
    elementTransforms: { ...DEFAULT_ELEMENT_TRANSFORMS },
    setSelectedTransformElement: (element) => set({ selectedTransformElement: element }),
    setTransformMode: (mode) => set({ transformMode: mode }),
    setShowTransformGizmo: (show) => set({ showTransformGizmo: show }),
    setElementTransforms: (transforms) => set((state) => ({ 
      elementTransforms: { ...state.elementTransforms, ...transforms } 
    })),
    updateElementTransform: (element, offset) => set((state) => {
      const newTransforms = { ...state.elementTransforms };
      switch (element) {
        case "arena_model":
          newTransforms.arenaModelOffset = offset;
          break;
        case "outer_wall":
          newTransforms.outerWallOffset = offset;
          break;
        case "middle_wall":
          newTransforms.middleWallOffset = offset;
          break;
        case "inner_wall":
          newTransforms.innerWallOffset = offset;
          break;
        case "zones":
          newTransforms.zonesOffset = offset;
          break;
        case "knockoff_boundary":
          newTransforms.knockoffBoundaryOffset = offset;
          break;
      }
      return { elementTransforms: newTransforms };
    }),
    updateElementRotation: (element, rotation) => set((state) => {
      const newTransforms = { ...state.elementTransforms };
      switch (element) {
        case "arena_model":
          newTransforms.arenaModelRotation = rotation;
          break;
        case "outer_wall":
          newTransforms.outerWallRotation = rotation;
          break;
        case "middle_wall":
          newTransforms.middleWallRotation = rotation;
          break;
        case "inner_wall":
          newTransforms.innerWallRotation = rotation;
          break;
        case "zones":
          newTransforms.zonesRotation = rotation;
          break;
        case "knockoff_boundary":
          newTransforms.knockoffBoundaryRotation = rotation;
          break;
      }
      return { elementTransforms: newTransforms };
    }),
    
    editorPlacedModels: [],
    selectedEditorModelId: null,
    editorSnapEnabled: true,
    
    wallOwnershipConfig: { enabled: false, wallBlocks: [], scoringZones: [] },
    selectedEditorWallBlockId: null,
    selectedEditorScoringZoneId: null,
    
    toggleWallOwnershipEditor: () => set((state) => ({
      wallOwnershipConfig: {
        ...state.wallOwnershipConfig,
        enabled: !state.wallOwnershipConfig.enabled
      }
    })),
    
    addEditorWallBlock: (block) => {
      const id = `wall-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
      set((state) => ({
        wallOwnershipConfig: {
          ...state.wallOwnershipConfig,
          wallBlocks: [...state.wallOwnershipConfig.wallBlocks, { ...block, id }]
        },
        selectedEditorWallBlockId: id
      }));
      return id;
    },
    
    updateEditorWallBlock: (id, updates) => set((state) => ({
      wallOwnershipConfig: {
        ...state.wallOwnershipConfig,
        wallBlocks: state.wallOwnershipConfig.wallBlocks.map(block =>
          block.id === id ? { ...block, ...updates } : block
        )
      }
    })),
    
    removeEditorWallBlock: (id) => set((state) => ({
      wallOwnershipConfig: {
        ...state.wallOwnershipConfig,
        wallBlocks: state.wallOwnershipConfig.wallBlocks.filter(block => block.id !== id)
      },
      selectedEditorWallBlockId: state.selectedEditorWallBlockId === id ? null : state.selectedEditorWallBlockId
    })),
    
    selectEditorWallBlock: (id) => set({ selectedEditorWallBlockId: id, selectedEditorScoringZoneId: null }),
    
    addEditorScoringZone: (zone) => {
      const id = `zone-${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
      set((state) => ({
        wallOwnershipConfig: {
          ...state.wallOwnershipConfig,
          scoringZones: [...state.wallOwnershipConfig.scoringZones, { ...zone, id }]
        },
        selectedEditorScoringZoneId: id
      }));
      return id;
    },
    
    updateEditorScoringZone: (id, updates) => set((state) => ({
      wallOwnershipConfig: {
        ...state.wallOwnershipConfig,
        scoringZones: state.wallOwnershipConfig.scoringZones.map(zone =>
          zone.id === id ? { ...zone, ...updates } : zone
        )
      }
    })),
    
    removeEditorScoringZone: (id) => set((state) => ({
      wallOwnershipConfig: {
        ...state.wallOwnershipConfig,
        scoringZones: state.wallOwnershipConfig.scoringZones.filter(zone => zone.id !== id)
      },
      selectedEditorScoringZoneId: state.selectedEditorScoringZoneId === id ? null : state.selectedEditorScoringZoneId
    })),
    
    selectEditorScoringZone: (id) => set({ selectedEditorScoringZoneId: id, selectedEditorWallBlockId: null }),
    
    loadWallOwnershipConfig: (config) => set({ wallOwnershipConfig: config }),
    
    zoneEditorConfigs: [],
    selectedZoneId: null,
    selectZone: (id) => set({ selectedZoneId: id }),
    updateZoneConfig: (id, updates) => set((state) => ({
      zoneEditorConfigs: state.zoneEditorConfigs.map(zone =>
        zone.id === id ? { ...zone, ...updates } : zone
      )
    })),
    initializeZoneConfigs: () => set((state) => {
      if (state.zoneEditorConfigs.length > 0) return state;
      const configs: ZoneEditorConfig[] = WALL_OWNERSHIP_GAP_ANGLES.map((angle, i) => ({
        id: `control-zone-${i}`,
        angle,
        distance: WALL_OWNERSHIP_CONTROL_ZONE_RADIUS,
        visible: true,
        isSpawn: i === 0 || i === 3,
      }));
      return { zoneEditorConfigs: configs };
    }),
    
    playerEntity: null,
    enemies: [],
    orbs: [],
    score: 0,
    isVictory: false,
    
    currentRound: 1,
    maxRounds: 3,
    playerRoundWins: 0,
    enemyRoundWins: new Map<string, number>(),
    isPlayerTurn: true,
        turnHasLaunched: false,
    turnIndex: 0,
    allMovementStopped: true,
    
    localPlayers: [],
    localPlayerCount: 2,
    currentLocalPlayerIndex: 0,
    
    gameTimer: 300,
    sessionId: "",
    
    birdsEyeView: false,
    firstPersonView: false,
    overShoulderView: false,
    launchPadView: false,
    launchPadTilt: 39,
    launchPadPitch: 0,
    launchPadHeight: 20,
    savedCameraPosition: null,
    developerCamera: false,
    developerMoveMode: false,
    developerDragActive: false,
    isAiming: false,
    devToolsVisible: true,
    colliderDebug: false,
    ioControlsVisible: true,
    selectedMoveElement: null,
    moveUpdateCounter: 0,
    showTutorial: false,
    tutorialStep: 0,
    lastCollisionTime: 0,
    lastScoreTime: 0,
    lastLaunchTime: 0,
    
    wolfClones: [],
    groundCracks: [],
    mushrooms: [],
    pinballBumpers: [],
    landedRocks: [],
    fallingEntities: [],
    lastCollisionEvent: null,
    showExplosion: null, powerUnlocks: [],
    pendingGrenades: [],
    
    orbCaptureEffects: [],
    triggerOrbCaptureEffect: (orbId: string, position: [number, number, number], color: string) => {
      const effect: OrbCaptureEffect = {
        id: `capture-${orbId}-${Date.now()}`,
        position: [...position],
        color,
        orbId,
        createdAt: Date.now(),
        duration: 1500
      };
      set((state) => ({
        orbCaptureEffects: [...state.orbCaptureEffects.slice(-5), effect]
      }));
    },
    cleanupOrbCaptureEffects: () => {
      const now = Date.now();
      set((state) => ({
        orbCaptureEffects: state.orbCaptureEffects.filter(e => now - e.createdAt < e.duration)
      }));
    },
    
    knockoffBoundaryFlash: null,
    triggerKnockoffBoundaryFlash: (color: string, flashCount: number = 2) => {
      set({ knockoffBoundaryFlash: { color, timestamp: Date.now(), flashCount } });
    },
    
    // Scoring zones - zone control system
    activeScoringZones: new Set<string>(),
    lastScoringZoneUpdate: 0,
    zoneOwnership: new Map<string, string>(),
    zoneControlActive: false,
    zoneControlActivationTime: null,
    allPlayersLaunchedOnce: false,
    scoredZonesThisTurn: new Set<string>(),
    
    clearScoredZones: () => {
      set({ scoredZonesThisTurn: new Set<string>() });
    },
    
    markZoneScored: (zoneId: string) => {
      const { scoredZonesThisTurn } = get();
      const newSet = new Set(scoredZonesThisTurn);
      newSet.add(zoneId);
      set({ scoredZonesThisTurn: newSet });
    },
    
    isZoneScoredThisTurn: (zoneId: string) => {
      return get().scoredZonesThisTurn.has(zoneId);
    },
    
    randomizeScoringZones: () => {
      // No longer randomly activating zones - zone control activates after all players launch once + 1 minute
    },
    
    updateZoneOwnership: (zoneId: string, playerColor: string) => {
      const { zoneOwnership, controlZones } = get();
      
      const newOwnership = new Map(zoneOwnership);
      newOwnership.set(zoneId, playerColor);
      
      // Also update controlZones to change the visual ring color (ownerColor drives the visual)
      if (controlZones && controlZones.length > 0) {
        const updatedControlZones = controlZones.map(zone => 
          zone.id === zoneId 
            ? { ...zone, ownerId: zoneId, ownerColor: playerColor, captureProgress: 1 }
            : zone
        );
        set({ zoneOwnership: newOwnership, controlZones: updatedControlZones });
      } else {
        set({ zoneOwnership: newOwnership });
      }
      console.log(`Zone ${zoneId} claimed by player with color ${playerColor}`);
    },
    
    checkZoneControlActivation: () => {
      const { allPlayersLaunchedOnce, zoneControlActivationTime, zoneControlActive } = get();
      
      if (zoneControlActive) return;
      
      if (allPlayersLaunchedOnce && zoneControlActivationTime) {
        const elapsed = Date.now() - zoneControlActivationTime;
        if (elapsed >= 60000) { // 1 minute
          set({ zoneControlActive: true });
          console.log("Zone control activated! Players can now claim zones.");
        }
      }
    },
    
    lockOnEnabled: true,
    lockOnTargetId: null,
    lockOnTargetType: null,
    arcType: null,
    straightMode: false,
    tangentOffset: "none",
    orbMultiplier: 1 as 1 | 2 | 3,
    aiPlayerCount: 3 as 0 | 1 | 2 | 3,
    playerFacingRotation: null,
    arcWaypoints: null,
    cinematicArcMode: false,
    arcPeakEffectActive: false,
    arcPeakTriggeredThisLaunch: false,
    arcPeakTargetPosition: null as [number, number, number] | null,
    slowMotionFactor: 1,
    abilityNotice: null,
    
    wallSegments: [],
    restrictionPhaseActive: false,
    restrictionPhaseStartTime: null,
    nextRespawnPadIndex: 0,
    firstTickProcessed: false,
    wallOwnershipMode: false,
    wallOwnershipStartTime: null,
    outerWallBlocks: [],
    controlZones: [],
    ownershipScores: {},
    
    initializeWalls: () => {
      const SEGMENT_COUNT = 48;
      const GAP_COUNT = 5;
      const GAP_SIZE = 3;
      const segmentsPerSection = SEGMENT_COUNT / GAP_COUNT;
      
      const segments: WallSegment[] = [];
      for (let i = 0; i < SEGMENT_COUNT; i++) {
        const posInSection = i % segmentsPerSection;
        if (posInSection < GAP_SIZE) continue;
        
        const angle = (i / SEGMENT_COUNT) * Math.PI * 2;
        segments.push({
          index: i,
          health: 3,
          maxHealth: 3,
          isDestroyed: false,
          angle
        });
      }
      set({ wallSegments: segments });
    },
    
    damageWallSegment: (segmentIndex: number, damage: number) => {
      set((state) => ({
        wallSegments: state.wallSegments.map(seg => {
          if (seg.index === segmentIndex && !seg.isDestroyed) {
            const newHealth = seg.health - damage;
            return {
              ...seg,
              health: newHealth,
              isDestroyed: newHealth <= 0
            };
          }
          return seg;
        })
      }));
    },
    
    activateWallOwnershipMode: () => {
      const OUTER_BLOCKS_COUNT = 200;
      const CONTROL_ZONES_COUNT = 6;
      const GAP_ANGLES = [0, Math.PI/3, 2*Math.PI/3, Math.PI, 4*Math.PI/3, 5*Math.PI/3];
      
      const outerBlocks: OuterWallBlock[] = [];
      for (let i = 0; i < OUTER_BLOCKS_COUNT; i++) {
        outerBlocks.push({
          id: `outer-block-${i}`,
          ownerId: null,
          ownerColor: "#1e3a5f",
          isScattered: false,
          scatteredAt: null,
          rebuildProgress: 1,
        });
      }
      
      const zones: ControlZone[] = GAP_ANGLES.map((angle, i) => ({
        id: `control-zone-${i}`,
        angle,
        ownerId: null,
        ownerColor: "#ffffff",
        captureProgress: 0,
        isContested: false,
      }));
      
      set({
        wallOwnershipMode: true,
        wallOwnershipStartTime: Date.now(),
        outerWallBlocks: outerBlocks,
        controlZones: zones,
        ownershipScores: {},
      });
    },
    
    updateBlockOwnership: (blockId: string, ownerId: string, ownerColor: string) => {
      set((state) => ({
        outerWallBlocks: state.outerWallBlocks.map(block =>
          block.id === blockId ? { ...block, ownerId, ownerColor } : block
        ),
      }));
    },
    
    scatterBlock: (blockId: string) => {
      set((state) => ({
        outerWallBlocks: state.outerWallBlocks.map(block =>
          block.id === blockId ? { ...block, isScattered: true, scatteredAt: Date.now(), rebuildProgress: 0 } : block
        ),
      }));
    },
    
    updateBlockRebuild: (blockId: string, progress: number) => {
      set((state) => ({
        outerWallBlocks: state.outerWallBlocks.map(block =>
          block.id === blockId ? { 
            ...block, 
            rebuildProgress: progress,
            isScattered: progress < 1,
            scatteredAt: progress >= 1 ? null : block.scatteredAt
          } : block
        ),
      }));
    },
    
    updateControlZone: (zoneId: string, ownerId: string | null, progress: number, contested: boolean) => {
      set((state) => {
        const ownerIndex = ownerId ? parseInt(ownerId.replace('player-', '')) : -1;
        const ownerPlayer = ownerIndex >= 0 ? state.localPlayers[ownerIndex] : null;
        const ownerEnemy = ownerIndex > 0 ? state.enemies[ownerIndex - 1] : null;
        const ownerColor = ownerPlayer?.zoogi?.color || ownerEnemy?.zoogi?.color || "#ffffff";
        return {
          controlZones: state.controlZones.map(zone =>
            zone.id === zoneId ? { 
              ...zone, 
              ownerId,
              captureProgress: progress,
              isContested: contested,
              ownerColor
            } : zone
          ),
        };
      });
    },
    
    addOwnershipScore: (playerId: string, points: number) => {
      set((state) => ({
        ownershipScores: {
          ...state.ownershipScores,
          [playerId]: (state.ownershipScores[playerId] || 0) + points,
        },
      }));
    },
    
    setPhase: (phase) => set({ phase }),
    setGameMode: (mode) => set({ gameMode: mode }),
    
    toggleBirdsEyeView: () => set((state) => ({ birdsEyeView: !state.birdsEyeView, firstPersonView: false, overShoulderView: false, launchPadView: false, developerCamera: false })),
    toggleFirstPersonView: () => set((state) => ({ firstPersonView: !state.firstPersonView, birdsEyeView: false, overShoulderView: false, launchPadView: false, developerCamera: false })),
    toggleOverShoulderView: () => set((state) => ({ overShoulderView: !state.overShoulderView, birdsEyeView: false, firstPersonView: false, launchPadView: false, developerCamera: false })),
    toggleLaunchPadView: () => set((state) => ({ launchPadView: !state.launchPadView, birdsEyeView: false, firstPersonView: false, overShoulderView: false, developerCamera: false })),
    setLaunchPadTilt: (tilt) => set({ launchPadTilt: Math.max(-30, Math.min(60, tilt)) }),
    setLaunchPadPitch: (pitch) => set({ launchPadPitch: Math.max(-20, Math.min(20, pitch)) }),
    setLaunchPadHeight: (height) => set({ launchPadHeight: Math.max(20, Math.min(100, height)) }),
    setSavedCameraPosition: (pos) => set({ savedCameraPosition: pos }),
    toggleDeveloperCamera: () => set((state) => ({ developerCamera: !state.developerCamera, birdsEyeView: false, firstPersonView: false, overShoulderView: false, launchPadView: false })),
    toggleDeveloperMoveMode: () => set((state) => ({ 
      developerMoveMode: !state.developerMoveMode, 
      selectedMoveElement: null,
      developerCamera: !state.developerMoveMode ? true : state.developerCamera,
      ioControlsVisible: !state.developerMoveMode ? true : state.ioControlsVisible
    })),
    setDeveloperMoveMode: (enabled) => set((state) => ({ 
      developerMoveMode: enabled, 
      selectedMoveElement: null,
      developerCamera: enabled ? true : state.developerCamera,
      ioControlsVisible: enabled ? true : state.ioControlsVisible
    })),
    toggleDevToolsVisible: () => set((state) => ({ devToolsVisible: !state.devToolsVisible })),
    toggleColliderDebug: () => set((state) => ({ colliderDebug: !state.colliderDebug })),
    toggleIoControlsVisible: () => set((state) => ({ ioControlsVisible: !state.ioControlsVisible })),
    setMapForEditing: (map) => {
      setCurrentMap(map);
      set((state) => ({ 
        selectedMap: map,
        moveUpdateCounter: state.moveUpdateCounter + 1
      }));
    },
    setSelectedMoveElement: (element) => set({ selectedMoveElement: element }),
    moveSelectedElement: (deltaX, deltaY, deltaZ) => set((state) => {
      if (!state.selectedMoveElement) return state;
      console.log(`Moving ${state.selectedMoveElement.type} #${state.selectedMoveElement.index} by (${deltaX.toFixed(2)}, ${deltaY.toFixed(2)}, ${deltaZ.toFixed(2)})`);
      return state;
    }),
    incrementMoveCounter: () => set((state) => ({ moveUpdateCounter: state.moveUpdateCounter + 1 })),
    setDeveloperDragActive: (active) => set({ developerDragActive: active }),
    setIsAiming: (aiming) => set({ isAiming: aiming }),
    
    openTutorial: () => set({ showTutorial: true, tutorialStep: 0 }),
    closeTutorial: () => set({ showTutorial: false, tutorialStep: 0 }),
    nextTutorialStep: () => set((state) => ({ tutorialStep: state.tutorialStep + 1 })),
    
    triggerCollision: (attackerType?: string) => {
      triggerCollisionFeel(0.5);
      triggerCollisionCameraEffect(0.7);
      set({ 
        lastCollisionTime: Date.now(),
        lastCollisionEvent: attackerType ? {
          attackerId: "player",
          attackerType,
          timestamp: Date.now()
        } : null
      });
    },
    
    triggerScore: () => set({ lastScoreTime: Date.now() }),
    
    spawnWolfClones: (position, velocity, ownerId?: string) => {
      const CLONE_SPEED = 1.0;
      const baseAngle = Math.atan2(velocity[2], velocity[0]);
      
      const sectorOffsets = [-Math.PI / 3, 0, Math.PI / 3];
      const spawnDistance = 0.8;
      
      const clones: WolfClone[] = sectorOffsets.map((offset, i) => {
        const angle = baseAngle + offset;
        
        const offsetX = Math.cos(angle) * spawnDistance;
        const offsetZ = Math.sin(angle) * spawnDistance;
        
        const clonePos: [number, number, number] = [
          position[0] + offsetX,
          position[1],
          position[2] + offsetZ
        ];
        const cloneVel: [number, number, number] = [
          Math.cos(angle) * CLONE_SPEED,
          0,
          Math.sin(angle) * CLONE_SPEED
        ];
        
        return {
          id: `wolf-clone-${i + 1}-${Date.now()}`,
          position: clonePos,
          velocity: cloneVel,
          isActive: true,
          sectorAngle: angle,
          spawnedByPlayerId: ownerId || get().playerEntity?.id || ""
        };
      });
      
      set({ wolfClones: clones });
      console.log("Wolfgang spawned 3 wolf clones in fan pattern!");
    },
    
    updateWolfClones: () => set((state) => {
      const chase = state.playerEntity
        ? [...state.enemies, { id: state.playerEntity.id, position: state.playerEntity.position }]
        : state.enemies;
      return {
        wolfClones: state.wolfClones.map((clone) => {
          if (!clone.isActive) return clone;
          return {
            ...clone,
            velocity: steerWolfClone(clone, state.orbs, chase, 1),
          };
        }),
      };
    }),
    
    clearWolfClones: () => set({ wolfClones: [] }),
    
    showAbilityNotice: (text) => set({ abilityNotice: { text, until: Date.now() + 2200 } }),

    activateWolfgangAbility: (casterId: string) => {
      const controlledEntity = readEntity(get(), casterId);
      if (!controlledEntity || !controlledEntity.wolfgangAbilityUnlocked) return;

      const velocity = controlledEntity.velocity;
      const speed = Math.hypot(velocity[0], velocity[2]);
      if (speed < 0.05) {
        get().showAbilityNotice("Launch first, then tap Pack!");
        return;
      }

      get().spawnWolfClones(controlledEntity.position, velocity, controlledEntity.id);
      triggerAbilityFeel(controlledEntity.position, 0.9);
      triggerAbilityCameraEffect("Dash Attack");
      useGameFeel.getState().triggerCartoonStarburst(controlledEntity.position, "#d1d5db");
      useAudio.getState().playWolfDash();
      get().showAbilityNotice("Wolf pack!");
      writeEntity(set, casterId, {
        wolfgangAbilityUnlocked: false,
        wolfgangAbilityUsedThisTurn: true,
      });
    },

    canUseWolfgangAbility: () => {
      const controlledEntity = get().getCurrentControlledEntity();
      if (!controlledEntity?.wolfgangAbilityUnlocked) return false;
      return Math.hypot(controlledEntity.velocity[0], controlledEntity.velocity[2]) >= 0.05;
    },
    
    activateHotstreakAbility: (casterId: string) => {
      const controlledEntity = readEntity(get(), casterId);
      if (!controlledEntity) return;
      
      // Must be unlocked by collecting the red star orb
      if (!controlledEntity.hotstreakAbilityUnlocked) {
        console.log("Explosion ability not unlocked - collect the red star orb first!");
        return;
      }
      
      // One frozen point: the caster where they are right now, including while rolling.
      const castAt: [number, number, number] = [
        controlledEntity.position[0],
        controlledEntity.position[1],
        controlledEntity.position[2],
      ];
      triggerAbilityFeel(castAt, 1.0);
      triggerAbilityCameraEffect("Instant Explosion");
      useGameFeel.getState().triggerCartoonExplosion(castAt, true);
      useAudio.getState().playExplosion();
      get().triggerExplosion(castAt, undefined, BLAST_RADIUS);
      get().showAbilityNotice("Explosion!");
      
      const EXPLOSION_RADIUS = BLAST_RADIUS;
      const EXPLOSION_FORCE = 1.0;
      
      const currentEnemies = get().enemies;
      const controlledEntityId = controlledEntity.id;
      
      currentEnemies.forEach((enemy) => {
        if (enemy.id === controlledEntityId) return;
        
        const eDx = enemy.position[0] - castAt[0];
        const eDz = enemy.position[2] - castAt[2];
        const eDist = Math.sqrt(eDx * eDx + eDz * eDz);
        if (eDist < EXPLOSION_RADIUS && eDist > 0.1) {
          const force = (1 - eDist / EXPLOSION_RADIUS) * EXPLOSION_FORCE;
          get().updateEnemy(enemy.id, {
            velocity: [
              enemy.velocity[0] + (eDx / eDist) * force,
              0,
              enemy.velocity[2] + (eDz / eDist) * force
            ]
          });
        }
      });

      const blastPlayer = get().playerEntity;
      if (blastPlayer && blastPlayer.id !== controlledEntityId) {
        const pDx = blastPlayer.position[0] - castAt[0];
        const pDz = blastPlayer.position[2] - castAt[2];
        const pDist = Math.sqrt(pDx * pDx + pDz * pDz);
        if (pDist < EXPLOSION_RADIUS && pDist > 0.1) {
          const force = (1 - pDist / EXPLOSION_RADIUS) * EXPLOSION_FORCE;
          set((s) => ({
            playerEntity: s.playerEntity ? {
              ...s.playerEntity,
              velocity: [
                s.playerEntity.velocity[0] + (pDx / pDist) * force,
                0,
                s.playerEntity.velocity[2] + (pDz / pDist) * force
              ]
            } : null
          }));
        }
      }
      
      const currentOrbs = get().orbs;
      currentOrbs.forEach((orb) => {
        if (!orb.isActive) return;
        const oDx = orb.position[0] - castAt[0];
        const oDz = orb.position[2] - castAt[2];
        const oDist = Math.sqrt(oDx * oDx + oDz * oDz);
        if (oDist < EXPLOSION_RADIUS && oDist > 0.1) {
          const force = (1 - oDist / EXPLOSION_RADIUS) * EXPLOSION_FORCE;
          get().updateOrb(orb.id, {
            velocity: [
              orb.velocity[0] + (oDx / oDist) * force,
              0,
              orb.velocity[2] + (oDz / oDist) * force
            ]
          });
        }
      });
      
      writeEntity(set, casterId, {
        hotstreakAbilityUnlocked: false,
        hotstreakAbilityUsedThisTurn: true,
      });
      
      console.log("Explosion ability activated and consumed!");
    },
    
    canUseHotstreakAbility: () => {
      // Get the currently controlled entity (handles local multiplayer)
      const controlledEntity = get().getCurrentControlledEntity();
      if (!controlledEntity) return false;
      
      // Only show if unlocked (one-time use)
      return controlledEntity.hotstreakAbilityUnlocked;
    },
    
    activateBoltAbility: (casterId: string) => {
      const state = get();
      const { enemies, orbs } = state;
      
      const controlledEntity = readEntity(state, casterId);
      if (!controlledEntity) return;
      
      // Must be unlocked by collecting the blue star orb
      if (!controlledEntity.boltAbilityUnlocked) {
        console.log("Stun ability not unlocked - collect the blue star orb first!");
        return;
      }
      
      const explosionPos: [number, number, number] = [...controlledEntity.position];
      triggerAbilityFeel(explosionPos, 0.8);
      triggerAbilityCameraEffect("Static Shock");
      useAudio.getState().playStunZap();
      get().triggerExplosion(explosionPos, "yellow", BLAST_RADIUS);
      useGameFeel.getState().triggerCartoonStarburst(explosionPos, "#FDE047");
      get().showAbilityNotice("Static Shock! Phasing through enemies.");
      const phaseUntil = Date.now() + 4000;
      
      const EXPLOSION_RADIUS = BLAST_RADIUS;
      const EXPLOSION_FORCE = 1.0;
      
      // Get controlled entity ID to exclude self from stun effects
      const controlledEntityId = controlledEntity.id;
      
      enemies.forEach((enemy) => {
        // Don't affect the current player's entity
        if (enemy.id === controlledEntityId) return;
        
        const eDx = enemy.position[0] - explosionPos[0];
        const eDz = enemy.position[2] - explosionPos[2];
        const eDist = Math.sqrt(eDx * eDx + eDz * eDz);
        if (eDist < EXPLOSION_RADIUS && eDist > 0.1) {
          const force = (1 - eDist / EXPLOSION_RADIUS) * EXPLOSION_FORCE;
          get().updateEnemy(enemy.id, {
            velocity: [
              enemy.velocity[0] + (eDx / eDist) * force,
              0,
              enemy.velocity[2] + (eDz / eDist) * force
            ],
            isStunned: true,
            stunTimer: 2
          });
        }
      });
      
      // Also affect playerEntity if it's not the controlled entity (for local multiplayer)
      const { playerEntity } = get();
      if (playerEntity && playerEntity.id !== controlledEntityId) {
        const pDx = playerEntity.position[0] - explosionPos[0];
        const pDz = playerEntity.position[2] - explosionPos[2];
        const pDist = Math.sqrt(pDx * pDx + pDz * pDz);
        if (pDist < EXPLOSION_RADIUS && pDist > 0.1) {
          const force = (1 - pDist / EXPLOSION_RADIUS) * EXPLOSION_FORCE;
          set((s) => ({
            playerEntity: s.playerEntity ? {
              ...s.playerEntity,
              velocity: [
                s.playerEntity.velocity[0] + (pDx / pDist) * force,
                0,
                s.playerEntity.velocity[2] + (pDz / pDist) * force
              ],
              isStunned: true,
              stunTimer: 2
            } : null
          }));
        }
      }
      
      orbs.forEach((orb) => {
        if (!orb.isActive) return;
        const oDx = orb.position[0] - explosionPos[0];
        const oDz = orb.position[2] - explosionPos[2];
        const oDist = Math.sqrt(oDx * oDx + oDz * oDz);
        if (oDist < EXPLOSION_RADIUS && oDist > 0.1) {
          const force = (1 - oDist / EXPLOSION_RADIUS) * EXPLOSION_FORCE;
          get().updateOrb(orb.id, {
            velocity: [
              orb.velocity[0] + (oDx / oDist) * force,
              0,
              orb.velocity[2] + (oDz / oDist) * force
            ]
          });
        }
      });
      
      writeEntity(set, casterId, {
        boltAbilityUnlocked: false,
        boltAbilityUsedThisTurn: true,
        boltPhasingUntil: phaseUntil,
      });
    },
    
    canUseBoltAbility: () => {
      const controlledEntity = get().getCurrentControlledEntity();
      return !!controlledEntity?.boltAbilityUnlocked;
    },

    activateLarsAbility: (casterId: string) => {
      const controlledEntity = readEntity(get(), casterId);
      if (!controlledEntity?.larsAbilityUnlocked) return;
      const state = get();
      let nearest: [number, number, number] | null = null;
      let nearestDist = Infinity;
      const consider = (pos: [number, number, number], id?: string) => {
        if (id && id === controlledEntity.id) return;
        const dist = Math.hypot(pos[0] - controlledEntity.position[0], pos[2] - controlledEntity.position[2]);
        if (dist > 0.4 && dist < nearestDist) {
          nearestDist = dist;
          nearest = pos;
        }
      };
      if (state.playerEntity) consider(state.playerEntity.position, state.playerEntity.id);
      state.enemies.forEach((enemy) => consider(enemy.position, enemy.id));
      state.orbs.forEach((orb) => { if (orb.isActive) consider(orb.position); });

      let velocity = controlledEntity.velocity;
      const speed = Math.hypot(velocity[0], velocity[2]);
      if (nearest && speed > 0.04) {
        const dx = nearest[0] - controlledEntity.position[0];
        const dz = nearest[2] - controlledEntity.position[2];
        const dist = Math.hypot(dx, dz) || 1;
        const redirected = Math.max(speed, 0.7);
        velocity = [(dx / dist) * redirected, 0, (dz / dist) * redirected];
      }
      triggerAbilityFeel(controlledEntity.position, 0.7);
      triggerAbilityCameraEffect("Bone Bounce");
      useGameFeel.getState().triggerCartoonStarburst(controlledEntity.position, "#3B82F6");
      useAudio.getState().playRicochetPing("arm");
      get().showAbilityNotice("Ricochet armed!");
      writeEntity(set, casterId, {
        larsRicochetBoost: 1.45,
        larsAbilityUnlocked: false,
        velocity,
      });
    },

    canUseLarsAbility: () => !!get().getCurrentControlledEntity()?.larsAbilityUnlocked,

    activateWrapsAbility: (casterId: string) => {
      const controlledEntity = readEntity(get(), casterId);
      if (!controlledEntity?.wrapsAbilityUnlocked) return;
      const until = Date.now() + 8000;
      const radius = BIND_RADIUS;
      triggerAbilityFeel(controlledEntity.position, 0.75);
      triggerAbilityCameraEffect("Bandage Bind");
      useGameFeel.getState().triggerCartoonStarburst(controlledEntity.position, "#D4C4B0");
      useAudio.getState().playBindWrap();
      get().showAbilityNotice("Bandage Bind! Enemies are slowed.");

      const slowOne = (pos: [number, number, number], vel: [number, number, number]) => {
        const dist = Math.hypot(pos[0] - controlledEntity.position[0], pos[2] - controlledEntity.position[2]);
        if (dist > radius) return null;
        return {
          velocity: [vel[0] * 0.35, 0, vel[2] * 0.35] as [number, number, number],
          slowUntil: Date.now() + 4000,
        };
      };

      set((s) => {
        const asCaster = <T extends { id: string; position: [number, number, number]; velocity: [number, number, number] }>(entity: T) => {
          if (entity.id === controlledEntity.id) {
            return { ...entity, wrapsBindUntil: until, wrapsAbilityUnlocked: false };
          }
          const slowed = slowOne(entity.position, entity.velocity);
          return slowed ? { ...entity, ...slowed } : entity;
        };
        return {
          playerEntity: s.playerEntity ? asCaster(s.playerEntity) : null,
          enemies: s.enemies.map((enemy) => asCaster(enemy)),
        };
      });
    },

    canUseWrapsAbility: () => !!get().getCurrentControlledEntity()?.wrapsAbilityUnlocked,

    activateNightshadeAbility: (casterId: string) => {
      const controlledEntity = readEntity(get(), casterId);
      if (!controlledEntity?.nightshadeAbilityUnlocked) return;
      const radius = SHADOW_STUN_RADIUS;
      const origin = controlledEntity.position;
      triggerAbilityFeel(origin, 0.8);
      triggerAbilityCameraEffect("Shadow Stun");
      useGameFeel.getState().triggerCartoonStarburst(origin, "#b69cff");
      useAudio.getState().playShadowPulse();
      get().triggerExplosion([...origin], "shadow", radius);
      get().showAbilityNotice("Shadow Stun!");

      set((s) => {
        const apply = <T extends { id: string; position: [number, number, number] }>(entity: T) => {
          if (entity.id === controlledEntity.id) {
            return { ...entity, nightshadeAbilityUnlocked: false };
          }
          const dist = Math.hypot(entity.position[0] - origin[0], entity.position[2] - origin[2]);
          if (dist >= radius || dist <= 0.1) return entity;
          return {
            ...entity,
            velocity: [0, 0, 0] as [number, number, number],
            isStunned: true,
            stunTimer: 2,
          };
        };
        return {
          playerEntity: s.playerEntity ? apply(s.playerEntity) : null,
          enemies: s.enemies.map((enemy) => apply(enemy)),
        };
      });
    },

    canUseNightshadeAbility: () => !!get().getCurrentControlledEntity()?.nightshadeAbilityUnlocked,

    aiPowerOpportunity: (casterId: string) => {
      const state = get();
      const caster = readEntity(state, casterId);
      if (!caster) return null;
      const others = [state.playerEntity, ...state.enemies].filter((entity): entity is GameEntity => !!entity);
      return chooseAiPower(caster, others, state.orbs);
    },

    useAiPower: (casterId: string) => {
      const choice = get().aiPowerOpportunity(casterId);
      if (!choice) return null;
      if (choice === "hotstreak") get().activateHotstreakAbility(casterId);
      else if (choice === "bolt") get().activateBoltAbility(casterId);
      else if (choice === "wolfgang") get().activateWolfgangAbility(casterId);
      else if (choice === "lars") get().activateLarsAbility(casterId);
      else if (choice === "nightshade") get().activateNightshadeAbility(casterId);
      else get().activateWrapsAbility(casterId);
      return get().aiPowerOpportunity(casterId) === null ? choice : null;
    },

    debugUnlockPower: (entityId: string) => {
      const entity = readEntity(get(), entityId);
      if (!entity) return;
      const patch = unlockPatchForZoogi(entity.zoogi.id);
      if (Object.keys(patch).length === 0) return;
      writeEntity(set, entityId, patch);
    },
    
    toggleLockOn: () => set((state) => ({
      lockOnEnabled: !state.lockOnEnabled,
      lockOnTargetId: !state.lockOnEnabled ? state.lockOnTargetId : null,
      lockOnTargetType: !state.lockOnEnabled ? state.lockOnTargetType : null,
      playerFacingRotation: !state.lockOnEnabled ? state.playerFacingRotation : null
    })),
    
    setLockOnTarget: (targetId, targetType) => set({
      lockOnTargetId: targetId,
      lockOnTargetType: targetType
    }),
    
    clearLockOn: () => set({
      lockOnEnabled: false,
      lockOnTargetId: null,
      lockOnTargetType: null,
      arcType: null,
      straightMode: false,
      tangentOffset: "none",
      playerFacingRotation: null
    }),
    
    toggleStraightMode: () => {
      const state = get();
      if (!state.straightMode && state.arcType !== "over") {
        set({ straightMode: true, arcType: null, playerFacingRotation: null });
      } else if (state.straightMode) {
        set({ straightMode: false });
        get().setArcType("over");
      } else if (state.arcType === "over") {
        get().setArcType(null);
      }
    },
    
    setTangentOffset: (offset) => set({ tangentOffset: offset }),
    
    setOrbMultiplier: (multiplier) => set({ orbMultiplier: multiplier }),
    
    incrementOrbMultiplier: () => set((state) => ({
      orbMultiplier: (state.orbMultiplier < 3 ? state.orbMultiplier + 1 : 3) as 1 | 2 | 3
    })),
    
    decrementOrbMultiplier: () => set((state) => ({
      orbMultiplier: (state.orbMultiplier > 1 ? state.orbMultiplier - 1 : 1) as 1 | 2 | 3
    })),
    
    setAiPlayerCount: (count: 0 | 1 | 2 | 3) => set({ aiPlayerCount: count }),
    
    incrementAiPlayerCount: () => set((state) => ({
      aiPlayerCount: (state.aiPlayerCount < 3 ? state.aiPlayerCount + 1 : 3) as 0 | 1 | 2 | 3
    })),
    
    decrementAiPlayerCount: () => set((state) => ({
      aiPlayerCount: (state.aiPlayerCount > 0 ? state.aiPlayerCount - 1 : 0) as 0 | 1 | 2 | 3
    })),
    
    setArcType: (arcType) => {
      const state = get();
      const { lockOnTargetId, lockOnTargetType, playerEntity, enemies, orbs, gameMode, currentLocalPlayerIndex } = state;
      
      if (!arcType) {
        set({ arcType: null, playerFacingRotation: null });
        return;
      }
      
      if (!lockOnTargetId) {
        set({ arcType: null, playerFacingRotation: null });
        return;
      }
      
      const currentEntity = gameMode === "local_multiplayer" 
        ? (currentLocalPlayerIndex === 0 ? playerEntity : enemies[currentLocalPlayerIndex - 1])
        : playerEntity;
      
      if (!currentEntity) {
        set({ arcType: null, playerFacingRotation: null });
        return;
      }
      
      let targetPos: [number, number, number] | null = null;
      if (lockOnTargetType === "orb") {
        const targetOrb = orbs.find(o => o.id === lockOnTargetId && o.isActive);
        if (targetOrb) targetPos = targetOrb.position;
      } else if (lockOnTargetType === "enemy") {
        const targetEnemy = enemies.find(e => e.id === lockOnTargetId);
        if (targetEnemy) targetPos = targetEnemy.position;
      }
      
      if (!targetPos) {
        set({ arcType: null, playerFacingRotation: null });
        return;
      }
      
      const dx = targetPos[0] - currentEntity.position[0];
      const dz = targetPos[2] - currentEntity.position[2];
      const dist = Math.sqrt(dx * dx + dz * dz);
      const forwardX = dx / dist;
      const forwardZ = dz / dist;
      
      const forwardYaw = Math.atan2(forwardX, forwardZ) + Math.PI;
      
      const leftYaw = forwardYaw + Math.PI / 2;
      const rightYaw = forwardYaw - Math.PI / 2;
      
      let yRotation = forwardYaw;
      let xRotation = 0;
      
      if (arcType === "over") {
        xRotation = 0;
      } else if (arcType === "left") {
        yRotation = leftYaw;
      } else if (arcType === "right") {
        yRotation = rightYaw;
      }
      
      set({ arcType, straightMode: false, playerFacingRotation: { y: yRotation, x: xRotation } });
    },
    setPlayerFacingRotation: (rotation) => set({ playerFacingRotation: rotation }),
    setCinematicArcMode: (enabled) => set({ 
      cinematicArcMode: enabled, 
      slowMotionFactor: enabled ? 0.6 : 1,
      arcPeakTriggeredThisLaunch: enabled ? false : get().arcPeakTriggeredThisLaunch
    }),
    
    setArcPeakEffectActive: (active) => set({ 
      arcPeakEffectActive: active,
      arcPeakTriggeredThisLaunch: active ? true : get().arcPeakTriggeredThisLaunch
    }),
    
    clearArcPeakEffect: () => set({
      arcPeakEffectActive: false,
      arcPeakTriggeredThisLaunch: false,
      arcPeakTargetPosition: null
    }),
    
    triggerArcLaunch: () => {
      const state = get();
      const { arcType, straightMode, lockOnTargetId, lockOnTargetType, playerEntity, enemies, orbs, gameMode, currentLocalPlayerIndex, overShoulderView } = state;
      
      const shouldEnableCinematicMode = arcType === "over" && overShoulderView;
      
      if ((!arcType && !straightMode) || !lockOnTargetId) return;
      
      const currentEntity = gameMode === "local_multiplayer" 
        ? (currentLocalPlayerIndex === 0 ? playerEntity : enemies[currentLocalPlayerIndex - 1])
        : playerEntity;
      
      if (!currentEntity) return;
      
      let targetPos: [number, number, number] | null = null;
      if (lockOnTargetType === "orb") {
        const targetOrb = orbs.find(o => o.id === lockOnTargetId && o.isActive);
        if (targetOrb) targetPos = targetOrb.position;
      } else if (lockOnTargetType === "enemy") {
        const targetEnemy = enemies.find(e => e.id === lockOnTargetId);
        if (targetEnemy) targetPos = targetEnemy.position;
      }
      
      if (!targetPos) return;
      
      const startX = currentEntity.position[0];
      const startZ = currentEntity.position[2];
      const endX = targetPos[0];
      const endZ = targetPos[2];
      
      const dx = endX - startX;
      const dz = endZ - startZ;
      const dist = Math.sqrt(dx * dx + dz * dz);
      
      if (dist < 0.5) return;
      
      const LAUNCH_SPEED = LOCKON_LAUNCH_SPEED;
      
      if (straightMode) {
        const { tangentOffset } = state;
        
        // Calculate tangent angle for off-center hits
        let launchDx = dx;
        let launchDz = dz;
        
        if (tangentOffset !== "none") {
          // Calculate tangent to touch edge of target (orb radius 0.4)
          const targetRadius = lockOnTargetType === "orb" ? 0.4 : 0.5;
          
          if (dist > targetRadius) {
            // Calculate tangent angle: sin(angle) = radius / distance
            const tangentAngle = Math.asin(targetRadius / dist);
            
            // Get base angle to target
            const baseAngle = Math.atan2(dz, dx);
            
            // Apply tangent offset (left = -angle, right = +angle)
            const offsetAngle = tangentOffset === "left" ? -tangentAngle : tangentAngle;
            const finalAngle = baseAngle + offsetAngle;
            
            // Calculate new direction
            launchDx = Math.cos(finalAngle) * dist;
            launchDz = Math.sin(finalAngle) * dist;
          }
        }
        
        const launchDist = Math.sqrt(launchDx * launchDx + launchDz * launchDz);
        const straightVelocity: [number, number, number] = [
          (launchDx / launchDist) * LAUNCH_SPEED,
          0,
          (launchDz / launchDist) * LAUNCH_SPEED
        ];
        
        if (gameMode === "local_multiplayer") {
          if (currentLocalPlayerIndex === 0 && playerEntity) {
            set({
              playerEntity: {
                ...playerEntity,
                velocity: straightVelocity,
                arcMovement: null
              },
              lockOnTargetId: null,
              lockOnTargetType: null,
              straightMode: false,
              tangentOffset: "none",
              allMovementStopped: false,
              lastLaunchTime: Date.now()
            });
          } else {
            const updatedEnemies = enemies.map((e, idx) => {
              if (idx === currentLocalPlayerIndex - 1) {
                return { ...e, velocity: straightVelocity, arcMovement: null };
              }
              return e;
            });
            set({
              enemies: updatedEnemies,
              lockOnTargetId: null,
              lockOnTargetType: null,
              straightMode: false,
              tangentOffset: "none",
              allMovementStopped: false,
              lastLaunchTime: Date.now()
            });
          }
        } else {
          if (playerEntity) {
            set({
              playerEntity: {
                ...playerEntity,
                velocity: straightVelocity,
                arcMovement: null
              },
              lockOnTargetId: null,
              lockOnTargetType: null,
              straightMode: false,
              tangentOffset: "none",
              allMovementStopped: false,
              lastLaunchTime: Date.now()
            });
          }
        }
        return;
      }
      
      // FLICK PHYSICS: Apply velocity directly toward target, no waypoint following
      // Calculate velocity toward the lock-on target
      const flickVelocity: [number, number, number] = [
        (dx / dist) * LAUNCH_SPEED,
        0,
        (dz / dist) * LAUNCH_SPEED
      ];
      
      if (gameMode === "local_multiplayer") {
        if (currentLocalPlayerIndex === 0 && playerEntity) {
          set({
            playerEntity: {
              ...playerEntity,
              velocity: flickVelocity,
              arcMovement: null // No arc movement - pure flick physics
            },
            lockOnTargetId: null,
            lockOnTargetType: null,
            arcType: null,
            allMovementStopped: false,
            lastLaunchTime: Date.now()
          });
        } else {
          const updatedEnemies = enemies.map((e, idx) => {
            if (idx === currentLocalPlayerIndex - 1) {
              return {
                ...e,
                velocity: flickVelocity,
                arcMovement: null
              };
            }
            return e;
          });
          set({
            enemies: updatedEnemies,
            lockOnTargetId: null,
            lockOnTargetType: null,
            arcType: null,
            allMovementStopped: false,
            lastLaunchTime: Date.now()
          });
        }
      } else if (playerEntity) {
        set({
          playerEntity: {
            ...playerEntity,
            velocity: flickVelocity,
            arcMovement: null
          },
          lockOnTargetId: null,
          lockOnTargetType: null,
          arcType: null,
          allMovementStopped: false,
          lastLaunchTime: Date.now()
        });
      }
      
      console.log("Flick launch triggered toward target:", arcType);
    },
    
    addGroundCrack: (position) => set((state) => ({
      groundCracks: [...state.groundCracks, {
        id: `crack-${Date.now()}`,
        position,
        createdAt: Date.now()
      }]
    })),
    
    triggerExplosion: (position, color?: string, radius = BLAST_RADIUS) => {
      const frozen: [number, number, number] = [position[0], position[1], position[2]];
      set({ showExplosion: { position: frozen, timestamp: Date.now(), color, radius } });
      console.log("Explosion triggered at", frozen, "color:", color || "orange", "radius:", radius);
    },
    
    addLandedRock: (rock) => set((state) => ({
      landedRocks: [...state.landedRocks, rock]
    })),
    
    armHotstreakGrenade: (position, ownerId) => {
      set((state) => ({
        pendingGrenades: [
          ...state.pendingGrenades,
          {
            id: `grenade-${Date.now()}`,
            position: [...position] as [number, number, number],
            timer: 5,
            ownerId
          }
        ]
      }));
      console.log("Hotstreak grenade armed at", position, "by", ownerId);
    },
    
    applyStun: (entityId) => set((state) => ({
      enemies: state.enemies.map(e => 
        e.id === entityId ? { ...e, isStunned: true, stunTimer: 4, velocity: [0, 0, 0] as [number, number, number] } : e
      )
    })),
    
    applyLarsBoost: () => set((state) => ({
      playerEntity: state.playerEntity ? {
        ...state.playerEntity,
        larsRicochetBoost: state.playerEntity.larsRicochetBoost * 1.15,
        velocity: [
          state.playerEntity.velocity[0] * 1.15,
          0,
          state.playerEntity.velocity[2] * 1.15
        ]
      } : null
    })),
    
    selectZoogi: (zoogi) => set({ selectedZoogi: zoogi, selectedCustomZoogi: null }),
    
    selectCustomZoogi: (customZoogi) => set({ selectedCustomZoogi: customZoogi, selectedZoogi: null }),
    
    selectMap: (map, customArenaId, customDecorations, meshyArenaId, meshyArenaModelUrl) => {
      setCurrentMap(map);
      const currentGameMode = get().gameMode;
      const isMapEditor = currentGameMode === "map_editor";
      set((state) => ({ 
        selectedMap: map,
        customArenaId: customArenaId || null,
        customArenaDecorations: customDecorations || [],
        meshyArenaId: meshyArenaId || null,
        meshyArenaModelUrl: meshyArenaModelUrl || null,
        developerMoveMode: isMapEditor ? true : false,
        developerCamera: isMapEditor ? true : false,
        selectedMoveElement: null,
        moveUpdateCounter: state.moveUpdateCounter + 1
      }));
      // Load saved decorations for this map
      get().loadMapDecorations(map);
    },
    
    startGame: () => {
      flashedStarOrbIds.clear();
      const { selectedZoogi, selectedCustomZoogi, gameMode, customArenaId, customArenaDecorations } = get();
      
      let zoogiToUse: Zoogi;
      let customModelUrl: string | undefined;
      let customThumbnailUrl: string | undefined;
      let isCustomZoogi = false;
      
      if (selectedCustomZoogi) {
        zoogiToUse = {
          id: `custom_${selectedCustomZoogi.id}`,
          name: selectedCustomZoogi.name,
          type: "Custom",
          color: "#8B5CF6",
          secondaryColor: "#A78BFA",
          ability: "Custom Power",
          abilityDescription: "A unique custom ability",
          stats: {
            speed: (selectedCustomZoogi.stats?.speed || 5) * 10,
            power: (selectedCustomZoogi.stats?.power || 5) * 10,
            defense: 50,
            control: (selectedCustomZoogi.stats?.control || 5) * 10
          }
        };
        customModelUrl = selectedCustomZoogi.modelUrl;
        customThumbnailUrl = selectedCustomZoogi.thumbnailUrl;
        isCustomZoogi = true;
      } else if (selectedZoogi) {
        zoogiToUse = selectedZoogi;
      } else {
        return;
      }
      
      const selectedMap = get().selectedMap;
      const { playerEntity, enemies, orbs, mushrooms, pinballBumpers } = initializeGame(zoogiToUse, gameMode, get().aiPlayerCount, get().zoneEditorConfigs, selectedMap ?? undefined);
      
      if (isCustomZoogi) {
        playerEntity.customModelUrl = customModelUrl;
        playerEntity.customThumbnailUrl = customThumbnailUrl;
        playerEntity.isCustomZoogi = true;
      }
      
      const sessionId = `session_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
      
      get().initializeWalls();
      
      
      
      set({
        phase: "playing",
        playerEntity,
        enemies,
        orbs,
        mushrooms,
        pinballBumpers,
        score: 0,
        isVictory: false,
        currentRound: 1,
        playerRoundWins: 0,
        enemyRoundWins: new Map<string, number>(),
        isPlayerTurn: true,
        turnHasLaunched: false,
        turnIndex: 0,
        allMovementStopped: true,
        gameTimer: 300,
        sessionId,
        lastCollisionTime: 0,
        lastScoreTime: 0,
        lastLaunchTime: 0,
        birdsEyeView: false,
        firstPersonView: false,
        overShoulderView: false,
        launchPadView: false,
        showTutorial: false,
        tutorialStep: 0,
        wolfClones: [],
        groundCracks: [],
        landedRocks: [],
        lastCollisionEvent: null,
        showExplosion: null, powerUnlocks: [],
        customArenaId,
        customArenaDecorations,
        developerMoveMode: false,
        developerCamera: false,
        selectedMoveElement: null,
        restrictionPhaseActive: false,
        restrictionPhaseStartTime: Date.now(),
        nextRespawnPadIndex: 0,
        firstTickProcessed: false,
        wallOwnershipMode: false,
        wallOwnershipStartTime: null,
        outerWallBlocks: [],
        editorPlacedModels: get().editorPlacedModels.filter(m => 
          !m.modelUrl.includes('zoogi_town') && !m.modelUrl.includes('workshop')
        ),
        ...playfieldSettings(selectedMap, get().wallSettings, get().backgroundSettings, get().elementTransforms),
      });
      
      // Classic mode now uses same free roam mechanics as local_multiplayer
      // Wall ownership (Ringer Royale) is NOT auto-activated
    },
    
    startPracticeGame: () => {
      flashedStarOrbIds.clear();
      const { selectedZoogi, selectedCustomZoogi } = get();
      
      let zoogiToUse: Zoogi;
      let customModelUrl: string | undefined;
      let customThumbnailUrl: string | undefined;
      let isCustomZoogi = false;
      
      if (selectedCustomZoogi) {
        zoogiToUse = {
          id: `custom_${selectedCustomZoogi.id}`,
          name: selectedCustomZoogi.name,
          type: "Custom",
          color: "#8B5CF6",
          secondaryColor: "#A78BFA",
          ability: "Custom Power",
          abilityDescription: "A unique custom ability",
          stats: {
            speed: (selectedCustomZoogi.stats?.speed || 5) * 10,
            power: (selectedCustomZoogi.stats?.power || 5) * 10,
            defense: 50,
            control: (selectedCustomZoogi.stats?.control || 5) * 10
          }
        };
        customModelUrl = selectedCustomZoogi.modelUrl;
        customThumbnailUrl = selectedCustomZoogi.thumbnailUrl;
        isCustomZoogi = true;
      } else if (selectedZoogi) {
        zoogiToUse = selectedZoogi;
      } else {
        return;
      }
      
      const { playerEntity, enemies, orbs, mushrooms, pinballBumpers } = initializePracticeGame(zoogiToUse, get().orbMultiplier);
      
      if (isCustomZoogi) {
        playerEntity.customModelUrl = customModelUrl;
        playerEntity.customThumbnailUrl = customThumbnailUrl;
        playerEntity.isCustomZoogi = true;
      }
      
      const sessionId = `practice_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
      
      get().initializeWalls();
      
      set({
        phase: "playing",
        gameMode: "practice",
        playerEntity,
        enemies,
        orbs,
        mushrooms,
        pinballBumpers,
        score: 0,
        isVictory: false,
        currentRound: 1,
        playerRoundWins: 0,
        enemyRoundWins: new Map<string, number>(),
        isPlayerTurn: true,
        turnHasLaunched: false,
        turnIndex: 0,
        allMovementStopped: true,
        gameTimer: 9999,
        sessionId,
        lastCollisionTime: 0,
        lastScoreTime: 0,
        lastLaunchTime: 0,
        birdsEyeView: false,
        firstPersonView: false,
        overShoulderView: false,
        launchPadView: false,
        showTutorial: false,
        tutorialStep: 0,
        wolfClones: [],
        groundCracks: [],
        landedRocks: [],
        lastCollisionEvent: null,
        showExplosion: null, powerUnlocks: [],
        customArenaId: null,
        customArenaDecorations: [],
        developerMoveMode: false,
        developerCamera: false,
        selectedMoveElement: null,
        selectedMap: "grass",
        ...playfieldSettings("grass", get().wallSettings, get().backgroundSettings, get().elementTransforms),
        orbMultiplier: 1 as 1 | 2 | 3,
        restrictionPhaseActive: false,
        restrictionPhaseStartTime: Date.now(),
        nextRespawnPadIndex: 0,
        firstTickProcessed: false,
        editorPlacedModels: get().editorPlacedModels.filter(m => 
          !m.modelUrl.includes('zoogi_town') && !m.modelUrl.includes('workshop')
        )
      });
      
      // Activate wall ownership mode immediately at game start
      get().activateWallOwnershipMode();
    },
    
    startNextRound: () => {
      flashedStarOrbIds.clear();
      const { currentRound, maxRounds, playerEntity, enemies, score, playerRoundWins, enemyRoundWins } = get();
      const { nextPlayerWins, nextEnemyWins, bestEnemyWins } = awardRoundWin(score, enemies, playerRoundWins, enemyRoundWins);
      
      if (currentRound >= maxRounds) {
        // The match follows rounds won. Equal round wins is a tie, not a defeat,
        // and a tied last round does not erase rounds the player already won.
        const playerWins = nextPlayerWins >= bestEnemyWins;
        console.log(`Game over after ${maxRounds} rounds! Player wins: ${nextPlayerWins}, Best enemy: ${bestEnemyWins}`);
        set({ playerRoundWins: nextPlayerWins, enemyRoundWins: nextEnemyWins });
        get().endGame(playerWins);
        return;
      }
      
      const roundMap = get().selectedMap;
      const roundLayout = getMapLayout(roundMap);
      const roundZones = zonesFromLayout(roundMap) ?? get().zoneEditorConfigs;
      const newOrbs = createOrbs(roundLayout?.orbRingRadius ?? 5);
      const newMushrooms = createMushrooms();
      const newPinballBumpers = createPinballBumpers(roundMap);
      
      set({
        phase: "playing",
        orbs: newOrbs,
        mushrooms: newMushrooms,
        pinballBumpers: newPinballBumpers,
        landedRocks: [],
        currentRound: currentRound + 1,
        playerRoundWins: nextPlayerWins,
        enemyRoundWins: nextEnemyWins,
        score: 0,
        gameTimer: 300,
        isPlayerTurn: true,
        turnHasLaunched: false,
        turnIndex: 0,
        allMovementStopped: true,
        wolfClones: [],
        ...playfieldSettings(roundMap, get().wallSettings, get().backgroundSettings, get().elementTransforms),
        playerEntity: playerEntity ? {
          ...playerEntity,
          position: getSpawnPointPosition(playerEntity.spawnPointIndex ?? 0, roundZones, roundMap ?? undefined),
          velocity: [0, 0, 0],
          score: 0,
          isKnockedOut: false,
          isRespawning: false
        } : null,
        enemies: enemies.map((e, i) => ({
          ...e,
          position: getSpawnPointPosition(e.spawnPointIndex ?? i + 1, roundZones, roundMap ?? undefined),
          velocity: [0, 0, 0],
          score: 0,
          isKnockedOut: false,
          isRespawning: false
        }))
      });
      
      console.log(`Starting Round ${currentRound + 1}/${maxRounds} - Timer reset to 5:00`);
    },
    
    restartWithSameZoogi: () => {
      flashedStarOrbIds.clear();
      const { selectedZoogi, playerEntity, selectedMap, gameMode } = get();
      const zoogiToUse = selectedZoogi || playerEntity?.zoogi;
      
      if (!zoogiToUse) return;
      
      const gameState = initializeGame(zoogiToUse, gameMode, get().aiPlayerCount, get().zoneEditorConfigs, selectedMap ?? undefined);
      
      set({
        phase: "playing",
        selectedZoogi: zoogiToUse,
        selectedMap: selectedMap,
        ...playfieldSettings(selectedMap, get().wallSettings, get().backgroundSettings, get().elementTransforms),
        playerEntity: gameState.playerEntity,
        enemies: gameState.enemies,
        orbs: gameState.orbs,
        mushrooms: gameState.mushrooms,
        pinballBumpers: gameState.pinballBumpers,
        landedRocks: [],
        score: 0,
        isVictory: false,
        currentRound: 1,
        playerRoundWins: 0,
        enemyRoundWins: new Map<string, number>(),
        isPlayerTurn: true,
        turnHasLaunched: false,
        turnIndex: 0,
        allMovementStopped: true,
        gameTimer: 300,
        wolfClones: [],
        showTutorial: false,
        tutorialStep: 0,
        developerMoveMode: false,
        developerCamera: false,
        selectedMoveElement: null,
        restrictionPhaseActive: false,
        restrictionPhaseStartTime: Date.now(),
        nextRespawnPadIndex: 0,
        firstTickProcessed: false
      });
    },
    
    endGame: (victory) => {
      const progression = useProgression.getState();
      progression.addXp(XP_VALUES.GAME_PLAYED, "Game Played");
      progression.updateChallengeProgress("games", 1);
      progression.addCoins(10);
      
      if (victory) {
        progression.addXp(XP_VALUES.GAME_WIN, "Victory!");
        progression.updateChallengeProgress("wins", 1);
        progression.addCoins(25);
      }
      
      set({ phase: "game_over", isVictory: victory });
    },
    
    returnToMenu: () => {
      resetRespawnIndices();
      set({
        phase: "menu",
        gameMode: "classic",
        selectedZoogi: null,
        selectedCustomZoogi: null,
        selectedMap: null,
        customArenaId: null,
        customArenaDecorations: [],
        meshyArenaId: null,
        meshyArenaModelUrl: null,
        backgroundSettings: { ...DEFAULT_BACKGROUND_SETTINGS },
        playerEntity: null,
        enemies: [],
        orbs: [],
        mushrooms: [],
        pinballBumpers: [],
        landedRocks: [],
        score: 0,
        isVictory: false,
        currentRound: 1,
        playerRoundWins: 0,
        enemyRoundWins: new Map<string, number>(),
        isPlayerTurn: true,
        turnHasLaunched: false,
        turnIndex: 0,
        birdsEyeView: false,
        firstPersonView: false,
        overShoulderView: false,
        launchPadView: false,
        showTutorial: false,
        tutorialStep: 0,
        lastCollisionTime: 0,
        lastScoreTime: 0,
        lastLaunchTime: 0,
        wolfClones: [],
        groundCracks: [],
        lastCollisionEvent: null,
        showExplosion: null, powerUnlocks: [],
        developerMoveMode: false,
        developerCamera: false,
        restrictionPhaseActive: false,
        restrictionPhaseStartTime: Date.now(),
        nextRespawnPadIndex: 0,
        firstTickProcessed: false,
        wallOwnershipMode: false,
        wallOwnershipStartTime: null,
        outerWallBlocks: [],
        controlZones: [],
        ownershipScores: {},
        gameTimer: 300,
        fallingEntities: [],
        localPlayers: [],
        currentLocalPlayerIndex: 0,
        allMovementStopped: true
      });
    },
    
    updatePlayerPosition: (position) => set((state) => ({
      playerEntity: state.playerEntity 
        ? { ...state.playerEntity, position } 
        : null
    })),
    
    updatePlayerVelocity: (velocity) => set((state) => {
      if (!state.playerEntity) return {};
      
      const MAX_VELOCITY = MAX_PLANAR_SPEED;
      const isPinpoint = state.playerEntity.zoogi.id === "pinpoint";
      
      let cappedVelocity = velocity;
      if (!isPinpoint) {
        const speed = Math.sqrt(velocity[0] ** 2 + velocity[2] ** 2);
        if (speed > MAX_VELOCITY) {
          const scale = MAX_VELOCITY / speed;
          cappedVelocity = [velocity[0] * scale, 0, velocity[2] * scale];
          console.log(`Velocity capped from ${speed.toFixed(3)} to ${MAX_VELOCITY}`);
        }
      }
      
      const launchSpeed = Math.sqrt(cappedVelocity[0] ** 2 + cappedVelocity[2] ** 2);
      let arcMovement = null;
      if (state.arcType && !state.straightMode && launchSpeed > 0.1) {
        arcMovement = {
          type: state.arcType,
          initialDirection: [cappedVelocity[0], cappedVelocity[2]] as [number, number],
          progress: 0,
          maxHeight: launchSpeed * 2
        };
      }
      
      const playersLaunch = launchSpeed > 0.05
        && state.gameMode !== "ringer_royale"
        && (state.gameMode === "local_multiplayer" ? state.currentLocalPlayerIndex === 0 : state.isPlayerTurn);
      return { 
        playerEntity: { ...state.playerEntity, velocity: cappedVelocity, arcMovement, lastHitByEnemyId: null },
        arcType: null,
        ...(playersLaunch ? { turnHasLaunched: true } : {}),
      };
    }),
    
    getCurrentControlledEntity: () => {
      const { gameMode, currentLocalPlayerIndex, playerEntity, enemies } = get();
      if (gameMode !== "local_multiplayer") {
        return playerEntity;
      }
      if (currentLocalPlayerIndex === 0) {
        return playerEntity;
      }
      return enemies[currentLocalPlayerIndex - 1] || null;
    },
    
    updateLocalPlayerVelocity: (playerIndex, velocity) => {
      const state = get();
      const MAX_VELOCITY = 2.5;
      const speed = Math.sqrt(velocity[0] ** 2 + velocity[2] ** 2);
      let cappedVelocity = velocity;
      if (speed > MAX_VELOCITY) {
        const scale = MAX_VELOCITY / speed;
        cappedVelocity = [velocity[0] * scale, 0, velocity[2] * scale];
      }
      
      const launchSpeed = Math.sqrt(cappedVelocity[0] ** 2 + cappedVelocity[2] ** 2);
      let arcMovement = null;
      if (state.arcType && !state.straightMode && launchSpeed > 0.1) {
        arcMovement = {
          type: state.arcType,
          initialDirection: [cappedVelocity[0], cappedVelocity[2]] as [number, number],
          progress: 0,
          maxHeight: launchSpeed * 2
        };
      }
      
      const localLaunch = launchSpeed > 0.05
        && state.gameMode === "local_multiplayer"
        && state.currentLocalPlayerIndex === playerIndex;
      if (playerIndex === 0) {
        set((state) => ({
          playerEntity: state.playerEntity 
            ? { ...state.playerEntity, velocity: cappedVelocity, arcMovement } 
            : null,
          arcType: null,
          ...(localLaunch ? { turnHasLaunched: true } : {}),
        }));
      } else {
        set((state) => ({
          enemies: state.enemies.map((e, i) => 
            i === playerIndex - 1 ? { ...e, velocity: cappedVelocity, arcMovement } : e
          ),
          arcType: null,
          ...(localLaunch ? { turnHasLaunched: true } : {}),
        }));
      }
    },
    
    respawnPlayer: () => {
      const { playerEntity, zoneEditorConfigs } = get();
      // Respawn at player's assigned spawn point (using zone editor configs if available)
      const spawnPos = getSpawnPointPosition(playerEntity?.spawnPointIndex ?? 0, zoneEditorConfigs, get().selectedMap ?? undefined);
      set((state) => ({
        playerEntity: state.playerEntity 
          ? { ...state.playerEntity, position: spawnPos, velocity: [0, 0, 0], spawnImmunity: false, isKnockedOut: false, isRespawning: false } 
          : null,
        pendingGrenades: state.pendingGrenades.filter(g => g.ownerId !== "player")
      }));
    },
    
    updateEnemy: (id, updates) => set((state) => {
      const speed = updates.velocity ? Math.hypot(updates.velocity[0], updates.velocity[2]) : 0;
      const index = state.enemies.findIndex(e => e.id === id);
      const theirTurn = state.gameMode === "local_multiplayer"
        ? state.currentLocalPlayerIndex === index + 1
        : !state.isPlayerTurn && state.turnIndex === index;
      const launched = speed > 0.05 && theirTurn && state.gameMode !== "ringer_royale";
      return {
        enemies: state.enemies.map(e => 
          e.id === id ? { ...e, ...updates } : e
        ),
        ...(launched ? { turnHasLaunched: true } : {}),
      };
    }),
    
    respawnEnemy: (id) => {
      set((state) => {
        const enemy = state.enemies.find(e => e.id === id);
        // Respawn at enemy's assigned spawn point (using zone editor configs if available)
        const spawnPos = getSpawnPointPosition(enemy?.spawnPointIndex ?? 0, state.zoneEditorConfigs);
        return {
          enemies: state.enemies.map(e => 
            e.id === id ? { ...e, position: spawnPos, velocity: [0, 0, 0], lastHitByPlayer: false, spawnImmunity: false, isKnockedOut: false, isRespawning: false } : e
          ),
          pendingGrenades: state.pendingGrenades.filter(g => g.ownerId !== id)
        };
      });
    },
    
    updateOrb: (id, updates) => set((state) => ({
      orbs: state.orbs.map(o => 
        o.id === id ? { ...o, ...updates } : o
      )
    })),
    
    removeOrb: (id) => {
      const { orbs, playerEntity, enemies, gameMode } = get();
      const orb = orbs.find(o => o.id === id);
      
      if (!orb || !orb.isActive) return;
      
      // Helper to unlock ability based on star orb type
      const getAbilityUnlock = (starType: "wolfgang" | "hotstreak" | "bolt" | null | undefined) => {
        switch (starType) {
          case "wolfgang": return { wolfgangAbilityUnlocked: true };
          case "hotstreak": return { hotstreakAbilityUnlocked: true };
          case "bolt": return { boltAbilityUnlocked: true };
          default: return {};
        }
      };
      
      if (orb.lastHitBy === "player") {
        // Note: Scoring is now handled in tick() when orb crosses knockoff boundary
        // Only handle ability unlocks here
        
        // Check if this is a star orb - unlock the corresponding ability!
        if (orb.isStarOrb && orb.starOrbType && playerEntity) {
          const abilityName = orb.starOrbType === "wolfgang" ? "Clone" : orb.starOrbType === "hotstreak" ? "Explosion" : "Stun";
          console.log(`⭐ ${orb.starOrbType.toUpperCase()} star orb collected by player! ${abilityName} ability unlocked!`);
          set((state) => ({
            playerEntity: state.playerEntity ? {
              ...state.playerEntity,
              ...getAbilityUnlock(orb.starOrbType)
            } : null
          }));
          flashStarUnlock(set, orb, playerEntity.id);
        }
      } else if (orb.lastHitBy === "enemy" && orb.lastHitByEnemyId) {
        // In local multiplayer, enemies are actually other local players
        if (gameMode === "local_multiplayer" && orb.isStarOrb && orb.starOrbType) {
          const enemyIndex = enemies.findIndex(e => e.id === orb.lastHitByEnemyId);
          if (enemyIndex !== -1) {
            const abilityName = orb.starOrbType === "wolfgang" ? "Clone" : orb.starOrbType === "hotstreak" ? "Explosion" : "Stun";
            console.log(`⭐ ${orb.starOrbType.toUpperCase()} star orb collected by local player ${enemyIndex + 2}! ${abilityName} ability unlocked!`);
            set((state) => ({
              enemies: state.enemies.map((e, idx) => 
                idx === enemyIndex ? { ...e, ...getAbilityUnlock(orb.starOrbType) } : e
              )
            }));
            flashStarUnlock(set, orb, enemies[enemyIndex].id);
          }
        }
      }
      
      set((state) => ({
        orbs: state.orbs.map(o => 
          o.id === id ? { ...o, isActive: false } : o
        )
      }));
      
      const remainingOrbs = get().orbs.filter(o => o.isActive);
      console.log(`Orbs remaining: ${remainingOrbs.length}`);
      
      if (remainingOrbs.length === 0) {
        const { currentRound, maxRounds } = get();
        console.log(`All orbs cleared! Round ${currentRound}/${maxRounds}`);
        set({ phase: "round_end" });
      }
    },
    
    addScore: (points) => set((state) => ({
      score: Math.max(0, state.score + points)
    })),
    
    scoreKnockOff: (type, points = 0) => {
      const basePoints = type === "enemy" ? 100 : points;
      get().addScore(basePoints);
      get().triggerScore();
      console.log(`Scored ${basePoints} points for knocking off ${type}!`);
      
      if (type === "enemy") {
        triggerKnockoffFeel([0, 0.5, 0]);
        triggerKnockoffCameraEffect();
      } else if (type === "orb") {
        triggerCollectFeel([0, 0.5, 0]);
      }
      
      const progression = useProgression.getState();
      if (type === "enemy") {
        progression.addXp(XP_VALUES.ENEMY_KNOCKOFF, "Enemy Knocked Off");
        progression.updateChallengeProgress("knockoffs", 1);
        progression.addCoins(5);
      } else if (type === "orb") {
        progression.addXp(XP_VALUES.ORB_COLLECT, "Orb Collected");
        progression.updateChallengeProgress("orbs", 1);
        progression.addCoins(1);
      }
    },
    
    endTurn: () => {
      const turnNow = Date.now();
      if (turnNow - lastTurnEndedAt < 450) return;
      lastTurnEndedAt = turnNow;
      // Drop the launch flag only after the turn really advances. A debounced
      // call leaves it set so the next physics step can try again.
      set({ turnHasLaunched: false });

      const { enemies, turnIndex, isPlayerTurn, gameMode, localPlayers, currentLocalPlayerIndex, playerEntity, zoneEditorConfigs, zoneControlActive, updateZoneOwnership, checkZoneControlActivation, clearScoredZones } = get();
      
      // Clear scored zones at end of turn for next turn
      clearScoredZones();
      
      // Check zone control activation timing
      checkZoneControlActivation();
      
      // Check if current player stopped in a zone - claim it with player's color and award 15 points
      const checkZoneOwnership = (entity: GameEntity | null, playerIndex: number) => {
        if (!entity) return;
        const ZONE_RADIUS = 4;
        const px = entity.position[0];
        const pz = entity.position[2];
        
        // Get player color based on index
        const playerColors = ["#FF4444", "#4444FF", "#44FF44", "#FFFF44", "#FF44FF", "#44FFFF", "#FF8800", "#8800FF"];
        const playerColor = playerColors[playerIndex % playerColors.length];
        
        for (const zone of zoneEditorConfigs) {
          if (!zone.visible) continue;
          // Skip zones already scored during roll-through this turn
          if (get().isZoneScoredThisTurn(zone.id)) continue;
          
          const zx = Math.cos(zone.angle) * zone.distance;
          const zz = Math.sin(zone.angle) * zone.distance;
          const dist = Math.sqrt((px - zx) ** 2 + (pz - zz) ** 2);
          
          if (dist < ZONE_RADIUS) {
            updateZoneOwnership(zone.id, playerColor);
            get().addScore(ZONE_SCORE_ORB);
            get().triggerKnockoffBoundaryFlash(playerColor, 1);
            console.log(`Player ${playerIndex + 1} claimed zone ${zone.id}! +${ZONE_SCORE_ORB} points`);
            return;
          }
        }
      };
      
      if (gameMode === "local_multiplayer") {
        const activePlayers = localPlayers.filter(p => !p.isEliminated);
        
        // Check zone ownership for current player before switching
        const currentEntity = currentLocalPlayerIndex === 0 ? playerEntity : enemies[currentLocalPlayerIndex - 1];
        checkZoneOwnership(currentEntity, currentLocalPlayerIndex);
        
        if (activePlayers.length <= 1) {
          const winnerIndex = localPlayers.findIndex(p => !p.isEliminated);
          get().endGame(winnerIndex === 0);
          return;
        }
        
        const getEntityForLocalPlayer = (index: number): GameEntity | null => {
          if (index === 0) return playerEntity;
          return enemies[index - 1] || null;
        };
        
        let nextIndex = currentLocalPlayerIndex;
        let attempts = 0;
        const skippedStun = new Set<number>();
        do {
          nextIndex = (nextIndex + 1) % localPlayers.length;
          attempts++;
          const entity = getEntityForLocalPlayer(nextIndex);
          if (entity?.isStunned) {
            console.log(`Local player ${localPlayers[nextIndex]?.name} is stunned, skipping turn`);
            skippedStun.add(nextIndex);
          }
        } while ((localPlayers[nextIndex].isEliminated || getEntityForLocalPlayer(nextIndex)?.isStunned) && attempts < localPlayers.length);
        const clearSkippedStun = (index: number, entity: GameEntity): GameEntity => {
          if (!skippedStun.has(index)) return entity;
          if (entity.isStunned) useAudio.getState().playStunEnd();
          return { ...entity, isStunned: false, stunTimer: 0 };
        };
        
        // Switch to next player - respawn at spawn point if they were knocked out
        const { zoneEditorConfigs } = get();
        
        if (nextIndex === 0) {
          const currentPlayer = get().playerEntity;
          const needsRespawn = readyToPlaceBack(currentPlayer);
          const spawnPos = needsRespawn ? getSpawnPointPosition(currentPlayer?.spawnPointIndex ?? 0, zoneEditorConfigs) : null;
          
          set((s) => ({
            isPlayerTurn: true,
        turnHasLaunched: false,
            currentLocalPlayerIndex: nextIndex,
            playerEntity: s.playerEntity ? {
              ...clearSkippedStun(0, s.playerEntity),
              ...(spawnPos ? { position: spawnPos, velocity: [0, 0, 0] as [number, number, number], isKnockedOut: false, isRespawning: false, respawnAt: null } : {}),
              hotstreakAbilityUsedThisTurn: false,
              wolfgangAbilityUsedThisTurn: false,
              boltAbilityUsedThisTurn: false
            } : null,
            enemies: s.enemies.map((enemy, idx) => clearSkippedStun(idx + 1, enemy)),
          }));
          if (needsRespawn) console.log(`Player 1 respawned at spawn point for their turn`);
        } else {
          const currentEnemy = get().enemies[nextIndex - 1];
          const needsRespawn = readyToPlaceBack(currentEnemy);
          const spawnPos = needsRespawn ? getSpawnPointPosition(currentEnemy?.spawnPointIndex ?? nextIndex, zoneEditorConfigs) : null;
          
          set((s) => ({
            isPlayerTurn: true,
        turnHasLaunched: false,
            currentLocalPlayerIndex: nextIndex,
            playerEntity: s.playerEntity ? clearSkippedStun(0, s.playerEntity) : null,
            enemies: s.enemies.map((e, idx) => 
              idx === nextIndex - 1 ? {
                ...clearSkippedStun(idx + 1, e),
                ...(spawnPos ? { position: spawnPos, velocity: [0, 0, 0] as [number, number, number], isKnockedOut: false, isRespawning: false, respawnAt: null } : {}),
                hotstreakAbilityUsedThisTurn: false,
                wolfgangAbilityUsedThisTurn: false,
                boltAbilityUsedThisTurn: false
              } : clearSkippedStun(idx + 1, e)
            )
          }));
          if (needsRespawn) console.log(`Player ${nextIndex + 1} respawned at spawn point for their turn`);
        }
        console.log(`Local multiplayer: Now ${localPlayers[nextIndex]?.name}'s turn (index ${nextIndex})`);
        
        get().resetTurnState();
        return;
      }
      
      // Classic mode: Check zone ownership for player before turn switch
      if (isPlayerTurn && playerEntity) {
        checkZoneOwnership(playerEntity, 0);
      }
      
      const releaseStun = (entity: GameEntity): GameEntity => {
        if (entity.isStunned) useAudio.getState().playStunEnd();
        return {
          ...entity,
          isStunned: false,
          stunTimer: 0,
        };
      };
      const skipStunnedEnemies = (start: number, list: GameEntity[]) => {
        const next = list.map((enemy) => ({ ...enemy }));
        let index = start;
        while (index < next.length && next[index].isStunned) {
          console.log(`Enemy ${next[index].zoogi.name} is stunned, skipping turn`);
          next[index] = releaseStun(next[index]);
          index++;
        }
        return { index, enemies: next };
      };
      const handPlayerTurn = (list: GameEntity[], clearPlayerStun: boolean) => {
        const currentPlayer = get().playerEntity;
        const needsRespawn = readyToPlaceBack(currentPlayer);
        const { zoneEditorConfigs } = get();
        const spawnPos = needsRespawn ? respawnInsidePlayfield(currentPlayer?.spawnPointIndex ?? 0, zoneEditorConfigs, get().selectedMap, get().wallSettings.knockoffBoundaryRadius) : null;
        set((s) => ({
          isPlayerTurn: true,
        turnHasLaunched: false,
          turnIndex: 0,
          enemies: list,
          playerEntity: s.playerEntity ? {
            ...(clearPlayerStun ? releaseStun(s.playerEntity) : s.playerEntity),
            ...(spawnPos ? { position: spawnPos, velocity: [0, 0, 0] as [number, number, number], isKnockedOut: false, isRespawning: false, respawnAt: null } : {}),
            hotstreakAbilityUsedThisTurn: false,
            wolfgangAbilityUsedThisTurn: false,
            boltAbilityUsedThisTurn: false,
          } : null,
        }));
        if (needsRespawn) console.log(`Player respawned at spawn point for their turn`);
      };

      const advanced = skipStunnedEnemies(isPlayerTurn ? 0 : turnIndex + 1, enemies);
      if (advanced.index < advanced.enemies.length) {
        set({ isPlayerTurn: false, turnIndex: advanced.index, enemies: advanced.enemies });
      } else if (get().playerEntity?.isStunned) {
        console.log("Player is stunned, skipping turn");
        const afterPlayer = skipStunnedEnemies(0, advanced.enemies);
        if (afterPlayer.index < afterPlayer.enemies.length) {
          set((s) => ({
            isPlayerTurn: false,
            turnIndex: afterPlayer.index,
            enemies: afterPlayer.enemies,
            playerEntity: s.playerEntity ? releaseStun(s.playerEntity) : null,
          }));
        } else {
          handPlayerTurn(afterPlayer.enemies, true);
        }
      } else {
        handPlayerTurn(advanced.enemies, false);
      }
      
      get().resetTurnState();
    },
    
    setMovementStopped: (stopped) => set({ 
      allMovementStopped: stopped,
      ...(stopped ? { cinematicArcMode: false, slowMotionFactor: 1 } : {})
    }),
    
    resetTurnState: () => {
      set({
        cinematicArcMode: false,
        slowMotionFactor: 1
      });
    },
    
    freezeAllEntities: () => {
      set((state) => ({
        playerEntity: state.playerEntity ? {
          ...state.playerEntity,
          velocity: [0, 0, 0]
        } : null,
        enemies: state.enemies.map(e => ({
          ...e,
          velocity: [0, 0, 0]
        })),
        orbs: state.orbs.map(o => ({
          ...o,
          velocity: [0, 0, 0]
        })),
        allMovementStopped: true,
        cinematicArcMode: false,
        slowMotionFactor: 1
      }));
    },
    
    setLocalPlayerCount: (count) => {
      const players: LocalPlayer[] = [];
      for (let i = 0; i < count; i++) {
        players.push({
          id: i,
          name: `Player ${i + 1}`,
          zoogi: null,
          isEliminated: false,
          isRespawning: false,
          respawnTime: null,
          respawnPadIndex: null,
          lastHitByPlayerIndex: null
        });
      }
      const sessionId = `session_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
      set({ localPlayerCount: count, localPlayers: players, sessionId });
    },
    
    setLocalPlayerZoogi: (playerIndex, zoogi) => set((state) => ({
      localPlayers: state.localPlayers.map((p, i) => 
        i === playerIndex ? { ...p, zoogi } : p
      )
    })),
    
    setLocalPlayerName: (playerIndex, name) => set((state) => ({
      localPlayers: state.localPlayers.map((p, i) => 
        i === playerIndex ? { ...p, name } : p
      )
    })),
    
    startLocalGame: () => {
      const { localPlayers, selectedMap, customArenaId, customArenaDecorations } = get();
      if (localPlayers.some(p => !p.zoogi)) return;
      
      const localLayout = getMapLayout(selectedMap);
      const localZones = zonesFromLayout(selectedMap) ?? get().zoneEditorConfigs;
      // Each local player spawns at their own spawn point on the playfield
      const entities: GameEntity[] = localPlayers.map((player, i) => {
        const spawnPos = getSpawnPointPosition(i, localZones, selectedMap ?? undefined);
        return {
          id: `local-player-${player.id}`,
          zoogi: player.zoogi!,
          position: spawnPos,
          velocity: [0, 0, 0] as [number, number, number],
          health: 100,
          maxHealth: 100,
          isPlayer: true,
          hasShield: false,
          shieldTimer: 0,
          speedBoost: 1,
          speedBoostTimer: 0,
          abilityCooldown: 0,
          lastHitByPlayer: false,
          lastHitByEnemyId: null,
          lastHitByLocalPlayerIndex: null,
          isStunned: false,
          stunTimer: 0,
          larsRicochetBoost: 1,
          score: 0,
          wolfgangAbilityCooldown: 0,
          wolfgangAbilityUsedThisTurn: false,
          wolfgangAbilityUnlocked: false,
          hotstreakAbilityCooldown: 0,
          hotstreakAbilityUsedThisTurn: false,
          hotstreakAbilityUnlocked: false,
          hotstreakGrenadeTimer: 0,
          hotstreakGrenadeArmed: false,
          boltAbilityCooldown: 0,
          boltAbilityUsedThisTurn: false,
          boltAbilityUnlocked: false,
          larsAbilityUnlocked: false,
          wrapsAbilityUnlocked: false,
          wrapsBindUntil: 0,
          nightshadeAbilityUnlocked: false,
          boltPhasingUntil: 0,
          slowUntil: 0,
          arcMovement: null,
          invulnerableUntil: null,
          isRespawning: false,
          respawnAt: null,
          respawnPadIndex: null,
          isKnockedOut: false,
          spawnImmunity: false,
          spawnPointIndex: i
        };
      });
      
      const orbs = createOrbs(localLayout?.orbRingRadius ?? 5);
      const mushrooms = createMushrooms();
      
      const sessionId = `session_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
      
      get().initializeWalls();
      
      set({
        phase: "playing",
        playerEntity: entities[0],
        enemies: entities.slice(1),
        orbs,
        mushrooms,
        pinballBumpers: createPinballBumpers(selectedMap),
        ...playfieldSettings(selectedMap, get().wallSettings, get().backgroundSettings, get().elementTransforms),
        score: 0,
        isVictory: false,
        currentRound: 1,
        playerRoundWins: 0,
        enemyRoundWins: new Map<string, number>(),
        isPlayerTurn: true,
        turnHasLaunched: false,
        turnIndex: 0,
        currentLocalPlayerIndex: 0,
        allMovementStopped: true,
        gameTimer: 300,
        sessionId,
        wolfClones: [],
        groundCracks: [],
        landedRocks: [],
        showExplosion: null, powerUnlocks: [],
        customArenaId,
        customArenaDecorations,
        developerMoveMode: false,
        developerCamera: false,
        selectedMoveElement: null,
        restrictionPhaseActive: false,
        restrictionPhaseStartTime: Date.now(),
        nextRespawnPadIndex: 0,
        firstTickProcessed: false,
        wallOwnershipMode: false,
        wallOwnershipStartTime: null,
        outerWallBlocks: []
      });
    },
    
    startMapEditor: () => {
      const { selectedMap, customArenaId, customArenaDecorations, meshyArenaId, meshyArenaModelUrl } = get();
      
      const sessionId = `editor_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
      
      get().initializeWalls();
      
      set({
        phase: "playing",
        gameMode: "map_editor",
        playerEntity: null,
        enemies: [],
        orbs: [],
        mushrooms: [],
        pinballBumpers: [],
        score: 0,
        isVictory: false,
        currentRound: 1,
        playerRoundWins: 0,
        enemyRoundWins: new Map<string, number>(),
        isPlayerTurn: true,
        turnHasLaunched: false,
        turnIndex: 0,
        allMovementStopped: true,
        gameTimer: 0,
        sessionId,
        wolfClones: [],
        groundCracks: [],
        landedRocks: [],
        showExplosion: null, powerUnlocks: [],
        customArenaId,
        customArenaDecorations,
        meshyArenaId,
        meshyArenaModelUrl,
        developerMoveMode: true,
        developerCamera: true,
        selectedMoveElement: null
      });
    },
    
    eliminateLocalPlayer: (playerIndex) => {
      set((state) => {
        const localPlayer = state.localPlayers[playerIndex];
        const entityId = playerIndex === 0 ? "player" : (state.enemies[playerIndex - 1]?.id || `local-player-${localPlayer?.id || playerIndex}`);
        return {
          localPlayers: state.localPlayers.map((p, i) => 
            i === playerIndex ? { ...p, isEliminated: true } : p
          ),
          pendingGrenades: state.pendingGrenades.filter(g => g.ownerId !== entityId)
        };
      });
    },
    
    addEditorPlacedModel: (model) => {
      const id = `placed_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
      const newModel: EditorPlacedModel = { id, ...model };
      set((state) => ({
        editorPlacedModels: [...state.editorPlacedModels, newModel]
      }));
      return id;
    },
    
    removeEditorPlacedModel: (id) => {
      set((state) => ({
        editorPlacedModels: state.editorPlacedModels.filter(m => m.id !== id)
      }));
    },
    
    updateEditorPlacedModel: (id, updates) => {
      set((state) => ({
        editorPlacedModels: state.editorPlacedModels.map(m => 
          m.id === id ? { ...m, ...updates } : m
        )
      }));
    },
    
    clearEditorPlacedModels: () => {
      set({ editorPlacedModels: [] });
    },
    
    loadEditorPlacedModels: (models) => {
      set({ editorPlacedModels: models });
    },
    
    selectEditorModel: (id) => {
      set({ selectedEditorModelId: id });
    },
    
    toggleEditorSnap: () => {
      set((state) => ({ editorSnapEnabled: !state.editorSnapEnabled }));
    },
    
    moveSelectedEditorModel: (dx, dy, dz) => {
      const state = get();
      if (!state.selectedEditorModelId) return;
      
      const snap = state.editorSnapEnabled ? 0.5 : 0.1;
      const snappedDx = Math.round(dx / snap) * snap;
      const snappedDy = Math.round(dy / snap) * snap;
      const snappedDz = Math.round(dz / snap) * snap;
      
      set((state) => ({
        editorPlacedModels: state.editorPlacedModels.map(m => {
          if (m.id !== state.selectedEditorModelId) return m;
          return {
            ...m,
            position: [
              m.position[0] + snappedDx,
              m.position[1] + snappedDy,
              m.position[2] + snappedDz
            ] as [number, number, number]
          };
        })
      }));
    },
    
    rotateSelectedEditorModel: (axis, angleDegrees) => {
      const state = get();
      if (!state.selectedEditorModelId) return;
      
      const snap = state.editorSnapEnabled ? 15 : 5;
      const snappedAngle = (Math.round(angleDegrees / snap) * snap) * (Math.PI / 180);
      
      set((state) => ({
        editorPlacedModels: state.editorPlacedModels.map(m => {
          if (m.id !== state.selectedEditorModelId) return m;
          const newRotation = [...m.rotation] as [number, number, number];
          if (axis === "y") newRotation[1] += snappedAngle;
          else if (axis === "x") newRotation[0] += snappedAngle;
          else if (axis === "z") newRotation[2] += snappedAngle;
          return { ...m, rotation: newRotation };
        })
      }));
    },
    
    scaleSelectedEditorModel: (scaleDelta) => {
      const state = get();
      if (!state.selectedEditorModelId) return;
      
      set((state) => ({
        editorPlacedModels: state.editorPlacedModels.map(m => {
          if (m.id !== state.selectedEditorModelId) return m;
          const newScale = Math.max(0.1, m.scale[0] + scaleDelta);
          return { ...m, scale: [newScale, newScale, newScale] as [number, number, number] };
        })
      }));
    },
    
    saveMapDecorations: async () => {
      const state = get();
      const mapId = state.selectedMap || "grass";
      const deviceId = getDeviceId();
      
      console.log("Saving map decorations:", { mapId, deviceId, modelsCount: state.editorPlacedModels.length });
      console.log("Current URL:", window.location.href);
      console.log("API endpoint:", `/api/map-decorations/${mapId}`);
      
      try {
        const requestBody = {
          deviceId,
          placedModels: state.editorPlacedModels,
          backgroundSettings: state.backgroundSettings,
          wallSettings: state.wallSettings,
          wallSegmentConfigs: state.wallSegmentConfigs,
          innerWallSegmentConfigs: state.innerWallSegmentConfigs,
          zoneSettings: state.zoneSettings,
          zoneEditorConfigs: state.zoneEditorConfigs,
          elementTransforms: state.elementTransforms,
          collisionTuning: state.collisionTuning
        };
        console.log("Request body size:", JSON.stringify(requestBody).length);
        
        console.log("About to fetch...");
        const response = await fetch(`/api/map-decorations/${mapId}`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(requestBody)
        });
        
        console.log("Fetch completed, response status:", response.status, response.statusText);
        
        if (response.ok) {
          const result = await response.json();
          console.log("Map decorations saved successfully:", result);
          return true;
        }
        const errorText = await response.text().catch(() => "No response body");
        console.error("Failed to save map decorations:", response.status, response.statusText, errorText);
        return false;
      } catch (error) {
        console.error("Error saving map decorations - exception:", error);
        console.error("Error type:", error instanceof Error ? error.constructor.name : typeof error);
        console.error("Error message:", error instanceof Error ? error.message : String(error));
        throw error;
      }
    },
    
    loadMapDecorations: async (mapId: string) => {
      const deviceId = getDeviceId();
      console.log("Loading map decorations:", { mapId, deviceId });
      
      // Import map default configs
      const { getMapDefaultConfig } = await import("../mapDefaultConfigs");
      const defaultConfig = getMapDefaultConfig(mapId);
      
      try {
        const response = await fetch(`/api/map-decorations/${mapId}?deviceId=${deviceId}`);
        console.log("Load response status:", response.status);
        if (response.ok) {
          const data = await response.json();
          console.log("Load response data:", { hasDecoration: !!data.decoration, modelsCount: data.decoration?.placedModels?.length });
          if (data.decoration) {
            if (data.decoration.placedModels) {
              set({ editorPlacedModels: data.decoration.placedModels });
            }
            if (data.decoration.backgroundSettings) {
              set({ backgroundSettings: { ...DEFAULT_BACKGROUND_SETTINGS, ...data.decoration.backgroundSettings } });
            }
            if (data.decoration.wallSettings) {
              set({ wallSettings: { ...DEFAULT_WALL_SETTINGS, ...data.decoration.wallSettings } });
            }
            if (data.decoration.wallSegmentConfigs && Array.isArray(data.decoration.wallSegmentConfigs)) {
              set({ wallSegmentConfigs: data.decoration.wallSegmentConfigs });
              console.log("Loaded wall segment configs:", data.decoration.wallSegmentConfigs.length);
            }
            if (data.decoration.innerWallSegmentConfigs && Array.isArray(data.decoration.innerWallSegmentConfigs)) {
              set({ innerWallSegmentConfigs: data.decoration.innerWallSegmentConfigs });
              console.log("Loaded inner wall segment configs:", data.decoration.innerWallSegmentConfigs.length);
            }
            if (data.decoration.zoneSettings) {
              set({ zoneSettings: { ...DEFAULT_ZONE_SETTINGS, ...data.decoration.zoneSettings } });
            }
            if (data.decoration.zoneEditorConfigs && Array.isArray(data.decoration.zoneEditorConfigs)) {
              set({ zoneEditorConfigs: data.decoration.zoneEditorConfigs });
            }
            if (data.decoration.elementTransforms) {
              set({ elementTransforms: { ...DEFAULT_ELEMENT_TRANSFORMS, ...data.decoration.elementTransforms } });
            }
            if (data.decoration.collisionTuning) {
              set({ collisionTuning: { ...DEFAULT_COLLISION_TUNING, ...data.decoration.collisionTuning } });
              console.log("Loaded collision tuning settings");
            }
            console.log("Map decorations loaded successfully:", data.decoration.placedModels?.length || 0, "models");
          } else if (defaultConfig) {
            // Apply hardcoded default config for this map
            console.log("Applying default config for map:", mapId);
            if (defaultConfig.backgroundSettings) {
              set({ backgroundSettings: { ...DEFAULT_BACKGROUND_SETTINGS, ...defaultConfig.backgroundSettings } });
            }
            if (defaultConfig.wallSettings) {
              set({ wallSettings: { ...DEFAULT_WALL_SETTINGS, ...defaultConfig.wallSettings } });
            }
            if (defaultConfig.wallSegmentConfigs) {
              set({ wallSegmentConfigs: defaultConfig.wallSegmentConfigs });
            }
            if (defaultConfig.innerWallSegmentConfigs) {
              set({ innerWallSegmentConfigs: defaultConfig.innerWallSegmentConfigs });
            }
            if (defaultConfig.elementTransforms) {
              set({ elementTransforms: { ...DEFAULT_ELEMENT_TRANSFORMS, ...defaultConfig.elementTransforms } });
            }
            if (defaultConfig.zoneSettings) {
              set({ zoneSettings: { ...DEFAULT_ZONE_SETTINGS, ...defaultConfig.zoneSettings } });
            }
            if (defaultConfig.zoneEditorConfigs) {
              set({ zoneEditorConfigs: defaultConfig.zoneEditorConfigs });
            }
          } else {
            console.log("No saved decorations found for this map/device");
          }
        } else if (defaultConfig) {
          // Apply hardcoded default config as fallback
          console.log("Applying default config for map (response not ok):", mapId);
          if (defaultConfig.backgroundSettings) {
            set({ backgroundSettings: { ...DEFAULT_BACKGROUND_SETTINGS, ...defaultConfig.backgroundSettings } });
          }
          if (defaultConfig.wallSettings) {
            set({ wallSettings: { ...DEFAULT_WALL_SETTINGS, ...defaultConfig.wallSettings } });
          }
          if (defaultConfig.wallSegmentConfigs) {
            set({ wallSegmentConfigs: defaultConfig.wallSegmentConfigs });
          }
          if (defaultConfig.innerWallSegmentConfigs) {
            set({ innerWallSegmentConfigs: defaultConfig.innerWallSegmentConfigs });
          }
          if (defaultConfig.elementTransforms) {
            set({ elementTransforms: { ...DEFAULT_ELEMENT_TRANSFORMS, ...defaultConfig.elementTransforms } });
          }
          if (defaultConfig.zoneSettings) {
            set({ zoneSettings: { ...DEFAULT_ZONE_SETTINGS, ...defaultConfig.zoneSettings } });
          }
          if (defaultConfig.zoneEditorConfigs) {
            set({ zoneEditorConfigs: defaultConfig.zoneEditorConfigs });
          }
        } else {
          console.error("Failed to load map decorations:", response.status);
        }
      } catch (error) {
        console.error("Error loading map decorations:", error);
        // Apply default config on error
        if (defaultConfig) {
          console.log("Applying default config for map (on error):", mapId);
          if (defaultConfig.backgroundSettings) {
            set({ backgroundSettings: { ...DEFAULT_BACKGROUND_SETTINGS, ...defaultConfig.backgroundSettings } });
          }
          if (defaultConfig.wallSettings) {
            set({ wallSettings: { ...DEFAULT_WALL_SETTINGS, ...defaultConfig.wallSettings } });
          }
          if (defaultConfig.wallSegmentConfigs) {
            set({ wallSegmentConfigs: defaultConfig.wallSegmentConfigs });
          }
          if (defaultConfig.innerWallSegmentConfigs) {
            set({ innerWallSegmentConfigs: defaultConfig.innerWallSegmentConfigs });
          }
          if (defaultConfig.elementTransforms) {
            set({ elementTransforms: { ...DEFAULT_ELEMENT_TRANSFORMS, ...defaultConfig.elementTransforms } });
          }
          if (defaultConfig.zoneSettings) {
            set({ zoneSettings: { ...DEFAULT_ZONE_SETTINGS, ...defaultConfig.zoneSettings } });
          }
          if (defaultConfig.zoneEditorConfigs) {
            set({ zoneEditorConfigs: defaultConfig.zoneEditorConfigs });
          }
        }
      }

      // Saved decorations can arrive after the match has already placed spawns.
      // Put the playfield (knockoff, zones, grass scale) back so those saves
      // cannot leave zones outside the drop or shrink the island under the marbles.
      const afterLoad = get();
      if (
        afterLoad.phase === "playing" &&
        afterLoad.gameMode !== "map_editor" &&
        afterLoad.selectedMap === mapId
      ) {
        set(playfieldSettings(mapId, afterLoad.wallSettings, afterLoad.backgroundSettings, afterLoad.elementTransforms));
      }
    },
    
    exportBackgroundSettings: () => {
      const state = get();
      const exportData = {
        backgroundSettings: state.backgroundSettings,
        wallSettings: state.wallSettings,
        wallSegmentConfigs: state.wallSegmentConfigs,
        innerWallSegmentConfigs: state.innerWallSegmentConfigs,
        zoneSettings: state.zoneSettings,
        zoneEditorConfigs: state.zoneEditorConfigs,
        elementTransforms: state.elementTransforms,
        placedModels: state.editorPlacedModels,
        mapId: state.selectedMap || "grass",
        exportedAt: new Date().toISOString()
      };
      
      const jsonString = JSON.stringify(exportData, null, 2);
      
      // Copy to clipboard
      navigator.clipboard.writeText(jsonString).then(() => {
        console.log("Background settings copied to clipboard");
      }).catch(err => {
        console.error("Failed to copy to clipboard:", err);
      });
      
      // Trigger download
      const blob = new Blob([jsonString], { type: "application/json" });
      const url = URL.createObjectURL(blob);
      const link = document.createElement("a");
      link.href = url;
      link.download = `map-config-${state.selectedMap || "grass"}-${Date.now()}.json`;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      URL.revokeObjectURL(url);
    },
    
    tickTimers: (delta) => {
      const state = get();
      if (!state.playerEntity) return;
      
      // Timer runs continuously during gameplay phase
      // Only pause if game is not in playing phase
      if (state.phase !== "playing") return;
      
      const newTimer = Math.max(0, state.gameTimer - delta);
      
      if (newTimer <= 0 && state.phase === "playing") {
        // Multi-round support: show round end screen before transitioning
        const { currentRound, maxRounds } = state;
        if (currentRound < maxRounds) {
          console.log(`Round ${currentRound} time up! Showing round end screen`);
          set({ phase: "round_end" });
          return;
        }
        // Final round complete. The match is rounds won, not this round's score.
        // A 0–0 last round still goes to the player, then the higher round total wins.
        const { nextPlayerWins, nextEnemyWins, bestEnemyWins } = awardRoundWin(
          state.score,
          state.enemies,
          state.playerRoundWins,
          state.enemyRoundWins,
        );
        const playerWins = nextPlayerWins >= bestEnemyWins;
        console.log(`Final round over: player rounds ${nextPlayerWins} vs ${bestEnemyWins}, win=${playerWins}, tied=${nextPlayerWins === bestEnemyWins}`);
        set({ playerRoundWins: nextPlayerWins, enemyRoundWins: nextEnemyWins, gameTimer: 0 });
        get().endGame(playerWins);
        return;
      }
      
      const WALL_OWNERSHIP_START_TIME = 300;
      if (!state.wallOwnershipMode && state.gameTimer > WALL_OWNERSHIP_START_TIME && newTimer <= WALL_OWNERSHIP_START_TIME) {
        get().activateWallOwnershipMode();
        console.log("Ringer Royale Mode activated at 5 min mark!");
      }
      
      const player = state.playerEntity;
      let hasShield = player.hasShield;
      let shieldTimer = player.shieldTimer - delta;
      let speedBoost = player.speedBoost;
      let speedBoostTimer = player.speedBoostTimer - delta;
      let abilityCooldown = Math.max(0, player.abilityCooldown - delta);
      let isStunned = player.isStunned;
      let stunTimer = player.stunTimer;
      
      if (shieldTimer <= 0) {
        hasShield = false;
        shieldTimer = 0;
      }
      
      if (speedBoostTimer <= 0) {
        speedBoost = 1;
        speedBoostTimer = 0;
      }
      
      let wolfgangAbilityCooldown = Math.max(0, player.wolfgangAbilityCooldown - delta);
      
      const updatedEnemies = state.enemies.map(e => {
        let updated = { ...e };
        if (e.wolfgangAbilityCooldown > 0) {
          updated = { ...updated, wolfgangAbilityCooldown: Math.max(0, e.wolfgangAbilityCooldown - delta) };
        }
        return updated;
      });
      
      let updatedGrenades = state.pendingGrenades.map(g => ({
        ...g,
        timer: g.timer - delta
      }));
      
      const explodingGrenades = updatedGrenades.filter(g => g.timer <= 0);
      updatedGrenades = updatedGrenades.filter(g => g.timer > 0);
      
      for (const grenade of explodingGrenades) {
        console.log("Grenade exploding at", grenade.position);
        get().triggerExplosion(grenade.position, undefined, 4);
        
        const GRENADE_RADIUS = 4;
        const GRENADE_FORCE = 0.5;
        
        const explosionPos = grenade.position;
        
        const currentPlayer = get().playerEntity;
        if (currentPlayer) {
          const plDx = currentPlayer.position[0] - explosionPos[0];
          const plDz = currentPlayer.position[2] - explosionPos[2];
          const plDist = Math.sqrt(plDx * plDx + plDz * plDz);
          if (plDist < GRENADE_RADIUS && plDist > 0.1) {
            const force = (1 - plDist / GRENADE_RADIUS) * GRENADE_FORCE;
            set((s) => ({
              playerEntity: s.playerEntity ? {
                ...s.playerEntity,
                velocity: [
                  s.playerEntity.velocity[0] + (plDx / plDist) * force,
                  0,
                  s.playerEntity.velocity[2] + (plDz / plDist) * force
                ]
              } : null
            }));
          }
        }
        
        const currentEnemies = get().enemies;
        currentEnemies.forEach((enemy, i) => {
          const eDx = enemy.position[0] - explosionPos[0];
          const eDz = enemy.position[2] - explosionPos[2];
          const eDist = Math.sqrt(eDx * eDx + eDz * eDz);
          if (eDist < GRENADE_RADIUS && eDist > 0.1) {
            const force = (1 - eDist / GRENADE_RADIUS) * GRENADE_FORCE;
            get().updateEnemy(enemy.id, {
              velocity: [
                enemy.velocity[0] + (eDx / eDist) * force,
                0,
                enemy.velocity[2] + (eDz / eDist) * force
              ]
            });
          }
        });
        
        const currentOrbs = get().orbs;
        currentOrbs.forEach((orb) => {
          if (!orb.isActive) return;
          const oDx = orb.position[0] - explosionPos[0];
          const oDz = orb.position[2] - explosionPos[2];
          const oDist = Math.sqrt(oDx * oDx + oDz * oDz);
          if (oDist < GRENADE_RADIUS && oDist > 0.1) {
            const force = (1 - oDist / GRENADE_RADIUS) * GRENADE_FORCE;
            get().updateOrb(orb.id, {
              velocity: [
                orb.velocity[0] + (oDx / oDist) * force,
                0,
                orb.velocity[2] + (oDz / oDist) * force
              ]
            });
          }
        });
      }
      
      set({
        gameTimer: newTimer,
        playerEntity: {
          ...player,
          hasShield,
          shieldTimer,
          speedBoost,
          speedBoostTimer,
          abilityCooldown,
          isStunned,
          stunTimer,
          wolfgangAbilityCooldown
        },
        enemies: updatedEnemies,
        pendingGrenades: updatedGrenades
      });
    },
    
    physicsTick: (_delta) => {
      const state = get();
      if (!state.playerEntity || state.phase !== "playing") return;
      
      const { gameMode, currentLocalPlayerIndex } = state;
      
      const COLLISION_RADIUS = 0.5;
      
      // Collision profiles for different entity type pairs
      // Equal-mass marbles. restitution is the bounce coefficient (0–1).
      // friction is Coulomb mu for the tangent, not a post-hit speed scale.
      // minImpulse stays 0: a floor on the impulse adds kinetic energy.
      const COLLISION_PROFILES = {
        playerPlayer: { restitution: MARBLE_RESTITUTION, friction: 0.35, minImpulse: 0, maxVelocity: MAX_PLANAR_SPEED },
        playerOrb: { restitution: 0.68, friction: 0.4, minImpulse: 0, maxVelocity: MAX_PLANAR_SPEED },
        orbOrb: { restitution: 0.55, friction: 0.45, minImpulse: 0, maxVelocity: MAX_PLANAR_SPEED },
      };

      // One physicsTick is one fixed 1/60 s step. Velocity is distance per step.
      // Damping, drag, and the Frozen Ring patch live in simFeel.ts.
      
      let player = { ...state.playerEntity };
      let enemies = state.enemies.map(e => ({ ...e }));
      let orbs = state.orbs.map(o => ({ ...o }));
      let orbsToRemove: string[] = [];
      
      const applyFriction = (vel: [number, number, number], onIce = false): [number, number, number] => {
        const damping = onIce ? ICE_LINEAR_DAMPING : LINEAR_DAMPING;
        const drag = onIce ? ICE_ROLLING_DRAG : ROLLING_DRAG;
        let vx = vel[0] * damping;
        let vz = vel[2] * damping;
        const speed = Math.hypot(vx, vz);
        if (speed < REST_SPEED) return [0, 0, 0];
        const slowed = speed - drag;
        if (slowed < REST_SPEED) return [0, 0, 0];
        const scale = slowed / speed;
        return [vx * scale, 0, vz * scale];
      };
      
      const resolveCollision = (
        pos1: [number, number, number],
        vel1: [number, number, number],
        pos2: [number, number, number],
        vel2: [number, number, number],
        profile: { restitution: number; friction: number; minImpulse: number; maxVelocity: number },
        mass1: number = 1,
        mass2: number = 1
      ): { vel1: [number, number, number]; vel2: [number, number, number] } => {
        const dx = pos2[0] - pos1[0];
        const dz = pos2[2] - pos1[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        
        if (dist === 0) return { vel1, vel2 };
        
        const nx = dx / dist;
        const nz = dz / dist;
        
        const dvx = vel1[0] - vel2[0];
        const dvz = vel1[2] - vel2[2];
        const dvn = dvx * nx + dvz * nz;
        
        // Already separating or resting. Do not add a shove.
        if (dvn <= 0) return { vel1, vel2 };

        const inv1 = mass1 > 0 ? 1 / mass1 : 0;
        const inv2 = mass2 > 0 ? 1 / mass2 : 0;
        const invSum = inv1 + inv2;
        if (invSum <= 0) return { vel1, vel2 };

        const restitution = Math.min(1, Math.max(0, profile.restitution));
        const impulse = ((1 + restitution) * dvn) / invSum;

        const tx = dvx - dvn * nx;
        const tz = dvz - dvn * nz;
        const tangentSpeed = Math.hypot(tx, tz);
        let jtx = 0;
        let jtz = 0;
        if (tangentSpeed > 1e-8) {
          const mu = Math.min(1, Math.max(0, profile.friction));
          const jt = Math.min(mu * impulse, tangentSpeed / invSum);
          jtx = (tx / tangentSpeed) * jt;
          jtz = (tz / tangentSpeed) * jt;
        }

        let newVel1: [number, number, number] = [
          vel1[0] - (impulse * nx + jtx) * inv1,
          0,
          vel1[2] - (impulse * nz + jtz) * inv1
        ];
        
        let newVel2: [number, number, number] = [
          vel2[0] + (impulse * nx + jtx) * inv2,
          0,
          vel2[2] + (impulse * nz + jtz) * inv2
        ];

        const keBefore = 0.5 * mass1 * (vel1[0] ** 2 + vel1[2] ** 2) + 0.5 * mass2 * (vel2[0] ** 2 + vel2[2] ** 2);
        const keAfter = 0.5 * mass1 * (newVel1[0] ** 2 + newVel1[2] ** 2) + 0.5 * mass2 * (newVel2[0] ** 2 + newVel2[2] ** 2);
        if (keAfter > keBefore && keAfter > 1e-12) {
          const scale = Math.sqrt(keBefore / keAfter);
          newVel1 = [newVel1[0] * scale, 0, newVel1[2] * scale];
          newVel2 = [newVel2[0] * scale, 0, newVel2[2] * scale];
        }
        
        const speed1 = Math.hypot(newVel1[0], newVel1[2]);
        if (speed1 > profile.maxVelocity) {
          const scale = profile.maxVelocity / speed1;
          newVel1 = [newVel1[0] * scale, 0, newVel1[2] * scale];
        } else if (speed1 < REST_SPEED) {
          newVel1 = [0, 0, 0];
        }
        const speed2 = Math.hypot(newVel2[0], newVel2[2]);
        if (speed2 > profile.maxVelocity) {
          const scale = profile.maxVelocity / speed2;
          newVel2 = [newVel2[0] * scale, 0, newVel2[2] * scale];
        } else if (speed2 < REST_SPEED) {
          newVel2 = [0, 0, 0];
        }
        
        return { vel1: newVel1, vel2: newVel2 };
      };
      
      const checkCollision = (pos1: [number, number, number], pos2: [number, number, number], r1: number, r2: number): boolean => {
        const dx = pos2[0] - pos1[0];
        const dz = pos2[2] - pos1[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        return dist < (r1 + r2);
      };
      
      const separateEntities = (
        pos1: [number, number, number],
        pos2: [number, number, number],
        r1: number,
        r2: number
      ): { pos1: [number, number, number]; pos2: [number, number, number] } => {
        const dx = pos2[0] - pos1[0];
        const dz = pos2[2] - pos1[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        
        if (dist === 0) {
          return {
            pos1: [pos1[0] - 0.1, pos1[1], pos1[2]],
            pos2: [pos2[0] + 0.1, pos2[1], pos2[2]]
          };
        }
        
        const overlap = (r1 + r2) - dist;
        if (overlap <= 0) return { pos1, pos2 };
        
        const nx = dx / dist;
        const nz = dz / dist;
        const separation = overlap / 2 + 0.01;
        
        return {
          pos1: [pos1[0] - nx * separation, pos1[1], pos1[2] - nz * separation],
          pos2: [pos2[0] + nx * separation, pos2[1], pos2[2] + nz * separation]
        };
      };
      
      const MAX_VELOCITY = MAX_PLANAR_SPEED;
      const capVelocity = (vel: [number, number, number], zoogiId?: string): [number, number, number] => {
        const speed = Math.sqrt(vel[0] ** 2 + vel[2] ** 2);
        if (speed > MAX_VELOCITY) {
          const scale = MAX_VELOCITY / speed;
          return [vel[0] * scale, 0, vel[2] * scale];
        }
        return vel;
      };
      
      const icePatchPositions = state.selectedMap === "ice" ? getIcePatches() : [];
      
      const isOnIce = (pos: [number, number, number]): boolean => {
        for (const patch of icePatchPositions) {
          const dx = pos[0] - patch.x;
          const dz = pos[2] - patch.z;
          const dist = Math.sqrt(dx * dx + dz * dz);
          if (dist < patch.radius) return true;
        }
        return false;
      };
      
      const prevPlayerPos: [number, number, number] = [player.position[0], player.position[1], player.position[2]];
      
      if (!player.isStunned) {
        player.velocity = capVelocity(applyFriction(player.velocity, isOnIce(player.position)), player.zoogi.id);
        
        if (player.arcMovement && player.arcMovement.waypoints && player.arcMovement.currentWaypointIndex !== undefined) {
          const waypoints = player.arcMovement.waypoints;
          const wpIdx = player.arcMovement.currentWaypointIndex;
          
          if (wpIdx < waypoints.length) {
            const targetWp = waypoints[wpIdx];
            const toWpX = targetWp[0] - player.position[0];
            const toWpZ = targetWp[2] - player.position[2];
            const toWpDist = Math.sqrt(toWpX * toWpX + toWpZ * toWpZ);
            
            const ARC_SPEED = 0.35;
            if (toWpDist > 0.3) {
              player.velocity = [
                (toWpX / toWpDist) * ARC_SPEED,
                0,
                (toWpZ / toWpDist) * ARC_SPEED
              ];
              player.position = [
                player.position[0] + player.velocity[0],
                targetWp[1],
                player.position[2] + player.velocity[2]
              ];
            } else {
              player.arcMovement.currentWaypointIndex = wpIdx + 1;
              player.position = [targetWp[0], targetWp[1], targetWp[2]];
              
              // Detect arc peak for "over" type arcs
              const peakIndex = Math.floor(waypoints.length / 2);
              const { arcPeakTriggeredThisLaunch } = get();
              if (player.arcMovement.type === "over" && wpIdx + 1 === peakIndex && !arcPeakTriggeredThisLaunch) {
                console.log("Arc peak reached at waypoint", wpIdx + 1, "of", waypoints.length);
                // Get target position from lockOn
                const { lockOnTargetId, lockOnTargetType, orbs, enemies } = get();
                let targetPos: [number, number, number] | null = null;
                if (lockOnTargetId) {
                  if (lockOnTargetType === "orb") {
                    const targetOrb = orbs.find(o => o.id === lockOnTargetId);
                    if (targetOrb) targetPos = targetOrb.position;
                  } else if (lockOnTargetType === "enemy") {
                    const targetEnemy = enemies.find(e => e.id === lockOnTargetId);
                    if (targetEnemy) targetPos = targetEnemy.position;
                  }
                }
                set({ 
                  arcPeakEffectActive: true, 
                  arcPeakTriggeredThisLaunch: true,
                  arcPeakTargetPosition: targetPos
                });
              }
            }
          } else {
            // Arc movement complete - trigger camera focus on final position
            const lastWaypoint = waypoints[waypoints.length - 1];
            if (lastWaypoint) {
              triggerTargetFocusCameraEffect(lastWaypoint);
            }
            player.arcMovement = null;
            player.velocity = [0, 0, 0];
            player.position = [player.position[0], 0.5, player.position[2]];
          }
        } else if (player.arcMovement) {
          const speed = Math.sqrt(player.velocity[0] ** 2 + player.velocity[2] ** 2);
          if (speed > 0.02) {
            player.arcMovement.progress += 0.05;
            
            if (player.arcMovement.type === "over") {
              const arcHeight = Math.sin(player.arcMovement.progress * Math.PI) * player.arcMovement.maxHeight;
              player.position = [
                player.position[0] + player.velocity[0],
                Math.max(0.5, arcHeight),
                player.position[2] + player.velocity[2]
              ];
            } else if (player.arcMovement.type === "left" || player.arcMovement.type === "right") {
              const perpX = -player.velocity[2];
              const perpZ = player.velocity[0];
              const perpMag = Math.sqrt(perpX * perpX + perpZ * perpZ);
              if (perpMag > 0.001) {
                const normPerpX = perpX / perpMag;
                const normPerpZ = perpZ / perpMag;
                const curveForce = player.arcMovement.type === "left" ? -0.008 : 0.008;
                player.velocity = [
                  player.velocity[0] + normPerpX * curveForce,
                  0,
                  player.velocity[2] + normPerpZ * curveForce
                ];
              }
              player.position = [
                player.position[0] + player.velocity[0],
                player.position[1],
                player.position[2] + player.velocity[2]
              ];
            }
            
            if (player.arcMovement.progress >= 1) {
              player.arcMovement = null;
              player.position = [player.position[0], 0.5, player.position[2]];
            }
          } else {
            player.arcMovement = null;
            player.position = [
              player.position[0] + player.velocity[0],
              0.5,
              player.position[2] + player.velocity[2]
            ];
          }
        } else {
          player.position = [
            player.position[0] + player.velocity[0],
            player.position[1],
            player.position[2] + player.velocity[2]
          ];
        }
      } else {
        player.velocity = [0, 0, 0];
      }
      
      const prevEnemyPositions = enemies.map(e => [...e.position] as [number, number, number]);
      enemies = enemies.map(e => {
        if (e.isStunned) {
          return { ...e, velocity: [0, 0, 0] as [number, number, number] };
        }
        let newVel = capVelocity(applyFriction(e.velocity, isOnIce(e.position)), e.zoogi.id);
        
        let newPos: [number, number, number];
        let newArcMovement = e.arcMovement;
        
        if (e.arcMovement && e.arcMovement.waypoints && e.arcMovement.currentWaypointIndex !== undefined) {
          const waypoints = e.arcMovement.waypoints;
          const wpIdx = e.arcMovement.currentWaypointIndex;
          
          if (wpIdx < waypoints.length) {
            const targetWp = waypoints[wpIdx];
            const toWpX = targetWp[0] - e.position[0];
            const toWpZ = targetWp[2] - e.position[2];
            const toWpDist = Math.sqrt(toWpX * toWpX + toWpZ * toWpZ);
            
            const ARC_SPEED = 0.35;
            if (toWpDist > 0.3) {
              newVel = [
                (toWpX / toWpDist) * ARC_SPEED,
                0,
                (toWpZ / toWpDist) * ARC_SPEED
              ];
              newPos = [
                e.position[0] + newVel[0],
                targetWp[1],
                e.position[2] + newVel[2]
              ];
            } else {
              newArcMovement = { ...e.arcMovement, currentWaypointIndex: wpIdx + 1 };
              newPos = [targetWp[0], targetWp[1], targetWp[2]];
            }
          } else {
            newArcMovement = null;
            newVel = [0, 0, 0];
            newPos = [e.position[0], 0.5, e.position[2]];
          }
        } else if (e.arcMovement) {
          const speed = Math.sqrt(newVel[0] ** 2 + newVel[2] ** 2);
          if (speed > 0.02) {
            newArcMovement = { ...e.arcMovement, progress: e.arcMovement.progress + 0.05 };
            
            if (e.arcMovement.type === "over") {
              const arcHeight = Math.sin(newArcMovement.progress * Math.PI) * e.arcMovement.maxHeight;
              newPos = [e.position[0] + newVel[0], Math.max(0.5, arcHeight), e.position[2] + newVel[2]];
            } else if (e.arcMovement.type === "left" || e.arcMovement.type === "right") {
              const perpX = -newVel[2];
              const perpZ = newVel[0];
              const perpMag = Math.sqrt(perpX * perpX + perpZ * perpZ);
              if (perpMag > 0.001) {
                const normPerpX = perpX / perpMag;
                const normPerpZ = perpZ / perpMag;
                const curveForce = e.arcMovement.type === "left" ? -0.008 : 0.008;
                newVel = [newVel[0] + normPerpX * curveForce, 0, newVel[2] + normPerpZ * curveForce];
              }
              newPos = [e.position[0] + newVel[0], e.position[1], e.position[2] + newVel[2]];
            } else {
              newPos = [e.position[0] + newVel[0], e.position[1], e.position[2] + newVel[2]];
            }
            
            if (newArcMovement.progress >= 1) {
              newArcMovement = null;
              newPos = [newPos[0], 0.5, newPos[2]];
            }
          } else {
            newArcMovement = null;
            newPos = [e.position[0] + newVel[0], 0.5, e.position[2] + newVel[2]];
          }
        } else {
          newPos = [e.position[0] + newVel[0], e.position[1], e.position[2] + newVel[2]];
        }
        
        return { ...e, velocity: newVel, position: newPos, arcMovement: newArcMovement };
      });
      
      // Store previous orb positions for swept collision detection
      const prevOrbPositions = orbs.map(o => [...o.position] as [number, number, number]);
      
      orbs = orbs.map(o => {
        if (!o.isActive) return o;
        const newVel = applyFriction(o.velocity);
        const newPos: [number, number, number] = [o.position[0] + newVel[0], o.position[1], o.position[2] + newVel[2]];
        return { ...o, velocity: newVel, position: newPos };
      });
      
      let hadCollision = false;
      
      const checkSweptCollisionBothMoving = (
        entity1Start: [number, number, number],
        entity1End: [number, number, number],
        entity2Start: [number, number, number],
        entity2End: [number, number, number],
        radius1: number,
        radius2: number
      ): boolean => {
        if (checkCollision(entity1End, entity2End, radius1, radius2)) return true;
        const combinedRadius = radius1 + radius2;
        const dx1 = entity1End[0] - entity1Start[0];
        const dz1 = entity1End[2] - entity1Start[2];
        const dx2 = entity2End[0] - entity2Start[0];
        const dz2 = entity2End[2] - entity2Start[2];
        const maxDist = Math.max(
          Math.sqrt(dx1 * dx1 + dz1 * dz1),
          Math.sqrt(dx2 * dx2 + dz2 * dz2)
        );
        if (maxDist < 0.01) return false;
        const steps = Math.max(Math.ceil(maxDist / (combinedRadius * 0.3)), 4);
        for (let s = 0; s <= steps; s++) {
          const t = s / steps;
          const pos1X = entity1Start[0] + dx1 * t;
          const pos1Z = entity1Start[2] + dz1 * t;
          const pos2X = entity2Start[0] + dx2 * t;
          const pos2Z = entity2Start[2] + dz2 * t;
          const dist = Math.sqrt((pos1X - pos2X) ** 2 + (pos1Z - pos2Z) ** 2);
          if (dist < combinedRadius) return true;
        }
        return false;
      };

      orbs.forEach((orb, i) => {
        if (!orb.isActive) return;
        
        const MIN_AIRBORNE_HEIGHT = 0.7;
        const isPlayerAirborne = player.arcMovement && 
          player.arcMovement.type === "over" && 
          player.position[1] > MIN_AIRBORNE_HEIGHT;
        if (isPlayerAirborne) return;
        
        const prevOrbPos = prevOrbPositions[i];
        const didCollide = checkSweptCollisionBothMoving(
          prevPlayerPos, player.position,
          prevOrbPos, orb.position,
          COLLISION_RADIUS, 0.4
        );
        
        if (didCollide) {
          const { pos1, pos2 } = separateEntities(player.position, orb.position, COLLISION_RADIUS, 0.4);
          player.position = pos1;
          orb.position = pos2;
          
          const result = resolveCollision(player.position, player.velocity, orb.position, orb.velocity, COLLISION_PROFILES.playerOrb, 1, 1);
          player.velocity = capVelocity(result.vel1, player.zoogi.id);
          orb.velocity = result.vel2;
          
          orb.lastHitBy = "player";
          orb.lastHitByLocalPlayerIndex = gameMode === "local_multiplayer" ? 0 : null;
          orb.lastHitTimestamp = Date.now();
          orbs[i] = orb;
          hadCollision = true;
        }
      });
      
      let hotstreakExplosions: [number, number, number][] = [];
      let triggeredLarsBoost = false;
      let enemyLarsRicochets: { enemyIndex: number; targetPos: [number, number, number] }[] = [];
      let playerTriggeredHotstreak = false;
      let enemyHotstreakIndices: number[] = [];
      
      enemies.forEach((enemy, i) => {
        const MIN_AIRBORNE_HEIGHT = 0.7;
        const isPlayerAirborne = player.arcMovement && 
          player.arcMovement.type === "over" && 
          player.position[1] > MIN_AIRBORNE_HEIGHT;
        const isEnemyAirborne = enemy.arcMovement && 
          enemy.arcMovement.type === "over" && 
          enemy.position[1] > MIN_AIRBORNE_HEIGHT;
        if (isPlayerAirborne || isEnemyAirborne) return;
        
        if (checkCollision(player.position, enemy.position, COLLISION_RADIUS, COLLISION_RADIUS)) {
          const now = Date.now();
          const playerPhasing = player.zoogi.id === "bolt" && (player.boltPhasingUntil || 0) > now;
          const enemyPhasing = enemy.zoogi.id === "bolt" && (enemy.boltPhasingUntil || 0) > now;
          if (playerPhasing || enemyPhasing) {
            if (playerPhasing) {
              enemy.isStunned = true;
              enemy.stunTimer = Math.max(enemy.stunTimer, 2);
              useGameFeel.getState().triggerCartoonStarburst(enemy.position, "#FDE047");
            }
            if (enemyPhasing) {
              player.isStunned = true;
              player.stunTimer = Math.max(player.stunTimer, 2);
            }
            enemies[i] = enemy;
            return;
          }
          const playerInvulnerable = player.invulnerableUntil !== null && now < player.invulnerableUntil;
          const enemyInvulnerable = enemy.invulnerableUntil !== null && now < enemy.invulnerableUntil;
          
          const collisionMidpoint: [number, number, number] = [
            (player.position[0] + enemy.position[0]) / 2,
            0.5,
            (player.position[2] + enemy.position[2]) / 2
          ];
          
          const prePlayerVel = [...player.velocity] as [number, number, number];
          const preEnemyVel = [...enemy.velocity] as [number, number, number];
          
          const dx = enemy.position[0] - player.position[0];
          const dz = enemy.position[2] - player.position[2];
          const dist = Math.sqrt(dx * dx + dz * dz) || 1;
          const nx = dx / dist;
          const nz = dz / dist;
          
          const { pos1, pos2 } = separateEntities(player.position, enemy.position, COLLISION_RADIUS, COLLISION_RADIUS);
          player.position = pos1;
          enemy.position = pos2;
          
          {
            
            const result = resolveCollision(player.position, player.velocity, enemy.position, enemy.velocity, COLLISION_PROFILES.playerPlayer, 1, 1);
            player.velocity = capVelocity(result.vel1, player.zoogi.id);
            enemy.velocity = capVelocity(result.vel2, enemy.zoogi.id);
            
            // Only attribute hits if neither entity is invulnerable
            if (!playerInvulnerable && !enemyInvulnerable) {
              enemy.lastHitByPlayer = true;
              player.lastHitByEnemyId = enemy.id;
              
              if (gameMode === "local_multiplayer") {
                const playerApproachSpeed = prePlayerVel[0] * nx + prePlayerVel[2] * nz;
                const enemyApproachSpeed = -(preEnemyVel[0] * nx + preEnemyVel[2] * nz);
                
                if (playerApproachSpeed > 0 && playerApproachSpeed >= enemyApproachSpeed) {
                  enemy.lastHitByLocalPlayerIndex = 0;
                  console.log(`Local: Player 0 (approach ${playerApproachSpeed.toFixed(2)}) hit player ${i+1}`);
                }
                if (enemyApproachSpeed > 0 && enemyApproachSpeed > playerApproachSpeed) {
                  player.lastHitByLocalPlayerIndex = i + 1;
                  console.log(`Local: Player ${i+1} (approach ${enemyApproachSpeed.toFixed(2)}) hit player 0`);
                }
              }
            } else if (playerInvulnerable || enemyInvulnerable) {
              console.log(`Collision ignored for attribution - invulnerable entity`);
            }
            
            enemies[i] = enemy;
            hadCollision = true;
            
            // Player Hotstreak uses button-activated explosion, not auto on collision
            // Ricochet is a deliberate Lars boost (larsRicochetBoost > 1), not an always-on pull.
            if (player.zoogi.id === "lars" && player.larsRicochetBoost > 1) {
              triggeredLarsBoost = true;
            }
            if (player.zoogi.id === "wraps" && (player.wrapsBindUntil || 0) > now) {
              enemy.velocity = [enemy.velocity[0] * 0.35, 0, enemy.velocity[2] * 0.35];
              enemy.slowUntil = now + 3500;
              useGameFeel.getState().triggerCartoonStarburst(collisionMidpoint, "#D4C4B0");
            }
            if (enemy.zoogi.id === "wraps" && (enemy.wrapsBindUntil || 0) > now) {
              player.velocity = [player.velocity[0] * 0.35, 0, player.velocity[2] * 0.35];
              player.slowUntil = now + 3500;
            }
            
            // Enemy Hotstreak no longer triggers explosions on contact
            // Only player Hotstreak can use button-activated explosion
            if (enemy.zoogi.id === "lars" && enemy.larsRicochetBoost > 1) {
              let nearestPos: [number, number, number] | null = null;
              let nearestD = Infinity;
              orbs.forEach(o => {
                if (!o.isActive) return;
                const d = Math.sqrt((o.position[0] - enemy.position[0]) ** 2 + (o.position[2] - enemy.position[2]) ** 2);
                if (d > 1 && d < nearestD) { nearestD = d; nearestPos = [...o.position] as [number, number, number]; }
              });
              if (nearestPos) enemyLarsRicochets.push({ enemyIndex: i, targetPos: nearestPos });
            }
          }
        }
      });
      
      if (triggeredLarsBoost && player.zoogi.id === "lars") {
        let nearestTargetPos: [number, number, number] | null = null;
        let nearestDist = Infinity;
        
        for (const orb of orbs) {
          if (!orb.isActive) continue;
          const dx = orb.position[0] - player.position[0];
          const dz = orb.position[2] - player.position[2];
          const dist = Math.sqrt(dx * dx + dz * dz);
          if (dist > 1 && dist < nearestDist) {
            nearestDist = dist;
            nearestTargetPos = [...orb.position] as [number, number, number];
          }
        }
        
        if (nearestTargetPos) {
          const currentSpeed = Math.sqrt(player.velocity[0] ** 2 + player.velocity[2] ** 2);
          const dx = nearestTargetPos[0] - player.position[0];
          const dz = nearestTargetPos[2] - player.position[2];
          const dist = Math.sqrt(dx * dx + dz * dz);
          
          const redirectSpeed = currentSpeed * 0.85;
          player.velocity = capVelocity([
            (dx / dist) * redirectSpeed,
            0,
            (dz / dist) * redirectSpeed
          ], player.zoogi.id);
          player.larsRicochetBoost = 1;
          useGameFeel.getState().triggerCartoonStarburst(player.position, "#3B82F6");
          useAudio.getState().playRicochetPing("hit");
          get().showAbilityNotice("Ricochet!");
          console.log(`Lars auto-ricochet toward nearest target!`);
        }
      }
      
      let newExplosion: { position: [number, number, number]; timestamp: number } | null = null;
      
      if (hadCollision) {
        if (hotstreakExplosions.length > 0) {
          const firstExplosion = hotstreakExplosions[0];
          newExplosion = { position: firstExplosion, timestamp: Date.now() };
          
          const EXPLOSION_RADIUS = 5;
          const EXPLOSION_FORCE_OPPONENTS = 0.85;
          const EXPLOSION_FORCE_SELF = 0.18;
          
          for (const explosionPos of hotstreakExplosions) {
            console.log("Hotstreak explosion at", explosionPos);
            
            enemies.forEach((enemy, i) => {
              const dx = enemy.position[0] - explosionPos[0];
              const dz = enemy.position[2] - explosionPos[2];
              const dist = Math.sqrt(dx * dx + dz * dz);
              if (dist < EXPLOSION_RADIUS && dist > 0.1) {
                const force = (1 - dist / EXPLOSION_RADIUS) * EXPLOSION_FORCE_OPPONENTS;
                enemies[i] = {
                  ...enemy,
                  velocity: capVelocity([
                    enemy.velocity[0] + (dx / dist) * force,
                    0,
                    enemy.velocity[2] + (dz / dist) * force
                  ], enemy.zoogi.id)
                };
              }
            });
            
            orbs.forEach((orb, i) => {
              if (!orb.isActive) return;
              const dx = orb.position[0] - explosionPos[0];
              const dz = orb.position[2] - explosionPos[2];
              const dist = Math.sqrt(dx * dx + dz * dz);
              if (dist < EXPLOSION_RADIUS && dist > 0.1) {
                const force = (1 - dist / EXPLOSION_RADIUS) * EXPLOSION_FORCE_OPPONENTS;
                orbs[i] = {
                  ...orb,
                  velocity: [
                    orb.velocity[0] + (dx / dist) * force,
                    0,
                    orb.velocity[2] + (dz / dist) * force
                  ]
                };
              }
            });
            
            const plDx = player.position[0] - explosionPos[0];
            const plDz = player.position[2] - explosionPos[2];
            const plDist = Math.sqrt(plDx * plDx + plDz * plDz);
            if (plDist < EXPLOSION_RADIUS && plDist > 0.1) {
              const force = (1 - plDist / EXPLOSION_RADIUS) * EXPLOSION_FORCE_SELF;
              player.velocity = capVelocity([
                player.velocity[0] + (plDx / plDist) * force,
                0,
                player.velocity[2] + (plDz / plDist) * force
              ], player.zoogi.id);
            }
          }
          
          const HOTSTREAK_SLOWDOWN = 0.4;
          if (playerTriggeredHotstreak) {
            player.velocity = [
              player.velocity[0] * HOTSTREAK_SLOWDOWN,
              0,
              player.velocity[2] * HOTSTREAK_SLOWDOWN
            ];
            console.log("Hotstreak slowed after explosion!");
          }
          
          enemyHotstreakIndices.forEach(idx => {
            const enemy = enemies[idx];
            enemy.velocity = capVelocity([
              enemy.velocity[0] * HOTSTREAK_SLOWDOWN,
              0,
              enemy.velocity[2] * HOTSTREAK_SLOWDOWN
            ], enemy.zoogi.id);
            enemies[idx] = enemy;
            console.log(`Enemy Hotstreak ${enemy.zoogi.name} slowed after explosion!`);
          });
        }
        
        enemyLarsRicochets.forEach(({ enemyIndex, targetPos }) => {
          const enemy = enemies[enemyIndex];
          const currentSpeed = Math.sqrt(enemy.velocity[0] ** 2 + enemy.velocity[2] ** 2);
          const dx = targetPos[0] - enemy.position[0];
          const dz = targetPos[2] - enemy.position[2];
          const dist = Math.sqrt(dx * dx + dz * dz);
          if (dist > 0.1) {
            const redirectSpeed = currentSpeed * 0.85;
            enemies[enemyIndex] = {
              ...enemy,
              larsRicochetBoost: 1,
              velocity: capVelocity([(dx / dist) * redirectSpeed, 0, (dz / dist) * redirectSpeed], enemy.zoogi.id)
            };
            useAudio.getState().playRicochetPing("hit");
            get().showAbilityNotice("Ricochet!");
            console.log(`Enemy Lars auto-ricochet toward target!`);
          }
        });
      }
      
      enemies.forEach((enemy, ei) => {
        orbs.forEach((orb, oi) => {
          if (!orb.isActive) return;
          
          const MIN_AIRBORNE_HEIGHT = 0.7;
          const isEnemyAirborne = enemy.arcMovement && 
            enemy.arcMovement.type === "over" && 
            enemy.position[1] > MIN_AIRBORNE_HEIGHT;
          if (isEnemyAirborne) return;
          
          if (checkCollision(enemy.position, orb.position, COLLISION_RADIUS, 0.4)) {
            const collisionMid: [number, number, number] = [
              (enemy.position[0] + orb.position[0]) / 2,
              0.5,
              (enemy.position[2] + orb.position[2]) / 2
            ];
            
            const { pos1, pos2 } = separateEntities(enemy.position, orb.position, COLLISION_RADIUS, 0.4);
            enemy.position = pos1;
            orb.position = pos2;
            
            const result = resolveCollision(enemy.position, enemy.velocity, orb.position, orb.velocity, COLLISION_PROFILES.playerOrb, 1, 1);
            enemy.velocity = capVelocity(result.vel1, enemy.zoogi.id);
            orb.velocity = result.vel2;
            
            // Transfer player credit: if player's orb hits an enemy, credit player for knockoff
            if (orb.lastHitBy === "player") {
              enemy.lastHitByPlayer = true;
              console.log(`Player's orb hit ${enemy.zoogi.name} - player gets credit for knockoff!`);
            }
            
            orb.lastHitBy = "enemy";
            orb.lastHitByEnemyId = enemy.id;
            orb.lastHitByLocalPlayerIndex = gameMode === "local_multiplayer" ? ei + 1 : null;
            orb.lastHitTimestamp = Date.now();
            
            if (enemy.zoogi.id === "lars" && enemy.larsRicochetBoost > 1) {
              let nearPos: [number, number, number] | null = null;
              let nearD = Infinity;
              orbs.forEach(o => {
                if (!o.isActive || o.id === orb.id) return;
                const d = Math.sqrt((o.position[0]-enemy.position[0])**2 + (o.position[2]-enemy.position[2])**2);
                if (d > 1 && d < nearD) { nearD = d; nearPos = [...o.position] as [number, number, number]; }
              });
              if (nearPos) {
                const spd = Math.sqrt(enemy.velocity[0]**2 + enemy.velocity[2]**2);
                const ddx = nearPos[0] - enemy.position[0];
                const ddz = nearPos[2] - enemy.position[2];
                const dd = Math.sqrt(ddx*ddx + ddz*ddz);
                enemy.velocity = capVelocity([(ddx/dd)*spd*0.85, 0, (ddz/dd)*spd*0.85], enemy.zoogi.id);
                enemy.larsRicochetBoost = 1;
                useAudio.getState().playRicochetPing("hit");
                get().showAbilityNotice("Ricochet!");
              }
            }
            
            enemies[ei] = enemy;
            orbs[oi] = orb;
          }
        });
      });
      
      for (let i = 0; i < orbs.length; i++) {
        if (!orbs[i].isActive) continue;
        for (let j = i + 1; j < orbs.length; j++) {
          if (!orbs[j].isActive) continue;
          if (checkCollision(orbs[i].position, orbs[j].position, 0.4, 0.4)) {
            const { pos1, pos2 } = separateEntities(orbs[i].position, orbs[j].position, 0.4, 0.4);
            orbs[i].position = pos1;
            orbs[j].position = pos2;
            
            const result = resolveCollision(orbs[i].position, orbs[i].velocity, orbs[j].position, orbs[j].velocity, COLLISION_PROFILES.orbOrb, 1, 1);
            orbs[i].velocity = result.vel1;
            orbs[j].velocity = result.vel2;
            
            if (orbs[i].lastHitBy && !orbs[j].lastHitBy) {
              orbs[j].lastHitBy = orbs[i].lastHitBy;
              orbs[j].lastHitByEnemyId = orbs[i].lastHitByEnemyId;
              orbs[j].lastHitByLocalPlayerIndex = orbs[i].lastHitByLocalPlayerIndex;
              orbs[j].lastHitTimestamp = orbs[i].lastHitTimestamp;
            } else if (orbs[j].lastHitBy && !orbs[i].lastHitBy) {
              orbs[i].lastHitBy = orbs[j].lastHitBy;
              orbs[i].lastHitByEnemyId = orbs[j].lastHitByEnemyId;
              orbs[i].lastHitByLocalPlayerIndex = orbs[j].lastHitByLocalPlayerIndex;
              orbs[i].lastHitTimestamp = orbs[j].lastHitTimestamp;
            }
          }
        }
      }
      
      for (let i = 0; i < enemies.length; i++) {
        for (let j = i + 1; j < enemies.length; j++) {
          if (checkCollision(enemies[i].position, enemies[j].position, COLLISION_RADIUS, COLLISION_RADIUS)) {
            const now = Date.now();
            const enemyIInvulnerable = enemies[i].invulnerableUntil !== null && now < (enemies[i].invulnerableUntil ?? 0);
            const enemyJInvulnerable = enemies[j].invulnerableUntil !== null && now < (enemies[j].invulnerableUntil ?? 0);
            
            const { pos1, pos2 } = separateEntities(enemies[i].position, enemies[j].position, COLLISION_RADIUS, COLLISION_RADIUS);
            enemies[i].position = pos1;
            enemies[j].position = pos2;
            
            const preVelI = [...enemies[i].velocity] as [number, number, number];
            const preVelJ = [...enemies[j].velocity] as [number, number, number];
            
            const dx = enemies[j].position[0] - enemies[i].position[0];
            const dz = enemies[j].position[2] - enemies[i].position[2];
            const dist = Math.sqrt(dx * dx + dz * dz) || 1;
            const nx = dx / dist;
            const nz = dz / dist;
            
            const result = resolveCollision(enemies[i].position, enemies[i].velocity, enemies[j].position, enemies[j].velocity, COLLISION_PROFILES.playerPlayer, 1, 1);
            enemies[i].velocity = capVelocity(result.vel1, enemies[i].zoogi.id);
            enemies[j].velocity = capVelocity(result.vel2, enemies[j].zoogi.id);
            
            // Only attribute hits if neither entity is invulnerable
            if (gameMode === "local_multiplayer" && !enemyIInvulnerable && !enemyJInvulnerable) {
              const approachI = preVelI[0] * nx + preVelI[2] * nz;
              const approachJ = -(preVelJ[0] * nx + preVelJ[2] * nz);
              
              if (approachI > 0 && approachI >= approachJ) {
                enemies[j].lastHitByLocalPlayerIndex = i + 1;
                console.log(`Local: Player ${i+1} (approach ${approachI.toFixed(2)}) hit player ${j+1}`);
              }
              if (approachJ > 0 && approachJ > approachI) {
                enemies[i].lastHitByLocalPlayerIndex = j + 1;
                console.log(`Local: Player ${j+1} (approach ${approachJ.toFixed(2)}) hit player ${i+1}`);
              }
            }
            
          }
        }
      }
      
      const matchSolids = collectMatchSolids({
        map: state.selectedMap,
        bumpers: state.pinballBumpers,
        landedRocks: state.landedRocks,
        editorModels: state.editorPlacedModels,
      });

      let wolfClonesUpdated = state.wolfClones.map(c => ({ ...c }));
      const frameScale = Math.min(3, _delta > 0 ? _delta * 60 : 1);
      
      wolfClonesUpdated.forEach((clone, ci) => {
        if (!clone.isActive) return;
        
        const CLONE_RADIUS = 0.5;
        const CLONE_BOUNCE = 0.75;
        
        const prevX = clone.position[0];
        const prevZ = clone.position[2];
        
        const chaseTargets = [...enemies, { id: player.id, position: player.position }];
        clone.velocity = steerWolfClone(clone, orbs, chaseTargets, frameScale);
        const dampedClone = applyFriction(clone.velocity);
        let vx = dampedClone[0];
        let vz = dampedClone[2];

        const clonePrev: [number, number, number] = [prevX, clone.position[1], prevZ];
        const stepVx = vx * frameScale;
        const stepVz = vz * frameScale;
        const cloneNext: [number, number, number] = [prevX + stepVx, clone.position[1], prevZ + stepVz];
        const cloneHit = resolveSolidCollision(clonePrev, cloneNext, [vx, 0, vz], CLONE_RADIUS, matchSolids, CLONE_BOUNCE);
        vx = cloneHit.vel[0];
        vz = cloneHit.vel[2];
        
        clone.velocity = [vx, 0, vz];
        clone.position = cloneHit.pos;
        
        enemies.forEach((enemy, ei) => {
          if (enemy.id === clone.spawnedByPlayerId) return;
          if (checkCollision(clone.position, enemy.position, CLONE_RADIUS, COLLISION_RADIUS)) {
            const result = resolveCollision(clone.position, clone.velocity, enemy.position, enemy.velocity, COLLISION_PROFILES.playerPlayer, 0.6, 1);
            clone.velocity = result.vel1;
            enemy.velocity = capVelocity(result.vel2, enemy.zoogi.id);
            enemy.lastHitByPlayer = true;
            enemies[ei] = enemy;
            console.log("Wolf clone hit enemy!");
          }
        });
        
        orbs.forEach((orb, oi) => {
          if (!orb.isActive) return;
          if (checkCollision(clone.position, orb.position, CLONE_RADIUS, 0.4)) {
            const result = resolveCollision(clone.position, clone.velocity, orb.position, orb.velocity, COLLISION_PROFILES.playerOrb, 0.6, 0.8);
            clone.velocity = result.vel1;
            orb.velocity = result.vel2;
            orb.lastHitBy = "player";
            orb.lastHitByLocalPlayerIndex = gameMode === "local_multiplayer" ? 0 : null;
            orb.lastHitTimestamp = Date.now();
            orbs[oi] = orb;
            console.log("Wolf clone hit orb!");
          }
        });
        
        const finalDist = Math.sqrt(clone.position[0] ** 2 + clone.position[2] ** 2);
        const speed = Math.sqrt(clone.velocity[0] ** 2 + clone.velocity[2] ** 2);
        const cloneKnockoff = state.wallSettings.knockoffBoundaryRadius ?? 50;
        clone.isActive = speed > 0.015 && finalDist <= cloneKnockoff + 0.5;
        
        wolfClonesUpdated[ci] = clone;
      });
      
      wolfClonesUpdated = wolfClonesUpdated.filter(c => c.isActive);
      
      const mushroomsUpdated = state.mushrooms;
      
      const bumperHits: { bumperId: string; hitTime: number }[] = [];

      const applySolidHits = (
        prev: [number, number, number],
        pos: [number, number, number],
        vel: [number, number, number],
        entityRadius: number,
        onBumper: (() => void) | null,
      ): { pos: [number, number, number]; vel: [number, number, number] } => {
        const resolved = resolveSolidCollision(prev, pos, vel, entityRadius, matchSolids);
        const seen = new Set<string>();
        for (const id of resolved.hits) {
          if (seen.has(id)) continue;
          seen.add(id);
          const solid = matchSolids.find((item) => item.id === id);
          if (solid?.kind !== "bumper") continue;
          bumperHits.push({ bumperId: id, hitTime: Date.now() });
          onBumper?.();
        }
        return { pos: resolved.pos, vel: resolved.vel };
      };

      const playerSolid = applySolidHits(prevPlayerPos, player.position, player.velocity, COLLISION_RADIUS, () => {
        get().addScore(BUMPER_SCORE);
        player.score += BUMPER_SCORE;
        get().showAbilityNotice(`Bumper! +${BUMPER_SCORE}`);
      });
      player.position = playerSolid.pos;
      player.velocity = playerSolid.vel;

      enemies = enemies.map((enemy, ei) => {
        const result = applySolidHits(prevEnemyPositions[ei], enemy.position, enemy.velocity, COLLISION_RADIUS, () => {
          enemy.score += BUMPER_SCORE;
        });
        return { ...enemy, position: result.pos, velocity: result.vel, score: enemy.score };
      });

      orbs = orbs.map((orb, oi) => {
        if (!orb.isActive) return orb;
        const result = applySolidHits(prevOrbPositions[oi], orb.position, orb.velocity, 0.4, null);
        return { ...orb, position: result.pos, velocity: result.vel };
      });

      if (state.selectedMap === "neon") {
        const playerRail = resolveNeonRails(prevPlayerPos, player.position, player.velocity, COLLISION_RADIUS);
        player.position = playerRail.pos;
        player.velocity = playerRail.vel;
        enemies = enemies.map((enemy, ei) => {
          const rail = resolveNeonRails(prevEnemyPositions[ei], enemy.position, enemy.velocity, COLLISION_RADIUS);
          return { ...enemy, position: rail.pos, velocity: rail.vel };
        });
        orbs = orbs.map((orb, oi) => {
          if (!orb.isActive) return orb;
          const rail = resolveNeonRails(prevOrbPositions[oi], orb.position, orb.velocity, 0.4);
          return { ...orb, position: rail.pos, velocity: rail.vel };
        });
      }

      const BOUNCE_FACTOR = 0.7;
      const WALL_COLLISION_DIST = ARENA_RADIUS;
      
      const findBoundaryIntersection = (
        startX: number, startZ: number,
        endX: number, endZ: number,
        boundaryRadius: number
      ): { x: number; z: number } | null => {
        const dx = endX - startX;
        const dz = endZ - startZ;
        const a = dx * dx + dz * dz;
        const b = 2 * (startX * dx + startZ * dz);
        const c = startX * startX + startZ * startZ - boundaryRadius * boundaryRadius;
        const discriminant = b * b - 4 * a * c;
        
        if (discriminant < 0 || a === 0) return null;
        
        const t = (-b + Math.sqrt(discriminant)) / (2 * a);
        if (t >= 0 && t <= 1) {
          return { x: startX + dx * t, z: startZ + dz * t };
        }
        return null;
      };
      
      // Gap angles matching DestructibleRingWall.tsx - 4 gaps at cardinal directions
      const GAP_ANGLES = [0, Math.PI / 2, Math.PI, Math.PI * 1.5];
      const GAP_WIDTH = Math.PI / 10; // ~18 degrees on each side
      
      const isAtCardinalGap = (x: number, z: number, entityRadius: number = 0): boolean => {
        const angle = Math.atan2(z, x);
        const normalizedAngle = angle < 0 ? angle + Math.PI * 2 : angle;
        const dist = Math.sqrt(x * x + z * z);
        // Account for entity radius at the boundary - smaller margin for larger entities
        const angularMargin = dist > 0 ? (entityRadius * 0.3) / dist : 0;
        
        for (const gapAngle of GAP_ANGLES) {
          let diff = Math.abs(normalizedAngle - gapAngle);
          if (diff > Math.PI) diff = Math.PI * 2 - diff;
          // Entity is in gap if within gap width minus angular margin
          if (diff < GAP_WIDTH - angularMargin) return true;
        }
        return false;
      };
      
      const isAtDestroyedSegment = (x: number, z: number, entityRadius: number = 0): boolean => {
        // First check if in a cardinal gap - not a destroyed segment
        if (isAtCardinalGap(x, z, entityRadius)) return false;
        
        const angle = Math.atan2(z, x);
        const normalizedAngle = angle < 0 ? angle + Math.PI * 2 : angle;
        const SEGMENT_COUNT = 48;
        
        const segmentIndex = Math.floor((normalizedAngle / (Math.PI * 2)) * SEGMENT_COUNT);
        const wallSeg = state.wallSegments.find(s => s.index === segmentIndex);
        return wallSeg ? wallSeg.isDestroyed : false;
      };
      
      const getWallSegmentAtAngle = (x: number, z: number, entityRadius: number = 0): number | null => {
        // First check if in a cardinal gap - no wall segment there
        if (isAtCardinalGap(x, z, entityRadius)) return null;
        
        const angle = Math.atan2(z, x);
        const normalizedAngle = angle < 0 ? angle + Math.PI * 2 : angle;
        const SEGMENT_COUNT = 48;
        
        const segmentIndex = Math.floor((normalizedAngle / (Math.PI * 2)) * SEGMENT_COUNT);
        const wallSeg = state.wallSegments.find(s => s.index === segmentIndex);
        if (wallSeg && !wallSeg.isDestroyed) {
          return segmentIndex;
        }
        return null;
      };
      
      const bounceOffCircularWall = (
        prevPos: [number, number, number],
        pos: [number, number, number],
        vel: [number, number, number],
        radius: number,
        zoogiId?: string
      ): { pos: [number, number, number]; vel: [number, number, number]; hitSegment: number | null } => {
        const dist = Math.sqrt(pos[0] ** 2 + pos[2] ** 2);
        const maxDist = WALL_COLLISION_DIST - radius;
        const prevDist = Math.sqrt(prevPos[0] ** 2 + prevPos[2] ** 2);
        
        if (dist <= maxDist) {
          return { pos, vel, hitSegment: null };
        }
        
        if (isInGap(pos[0], pos[2], radius) || isAtDestroyedSegment(pos[0], pos[2], radius)) {
          return { pos, vel, hitSegment: null };
        }
        
        if (prevDist < maxDist) {
          const intersection = findBoundaryIntersection(prevPos[0], prevPos[2], pos[0], pos[2], maxDist);
          if (intersection && (isInGap(intersection.x, intersection.z, radius) || isAtDestroyedSegment(intersection.x, intersection.z, radius))) {
            return { pos, vel, hitSegment: null };
          }
        }
        
        const hitSegment = getWallSegmentAtAngle(pos[0], pos[2], radius);
        
        if (hitSegment === null) {
          return { pos, vel, hitSegment: null };
        }
        
        const nx = pos[0] / dist;
        const nz = pos[2] / dist;
        
        const dot = vel[0] * nx + vel[2] * nz;
        let newVel: [number, number, number];
        if (dot > 0) {
          newVel = [
            (vel[0] - 2 * dot * nx) * BOUNCE_FACTOR,
            0,
            (vel[2] - 2 * dot * nz) * BOUNCE_FACTOR
          ];
        } else {
          newVel = [...vel] as [number, number, number];
        }
        
        if (zoogiId) {
          newVel = capVelocity(newVel, zoogiId);
        }
        
        const newPos: [number, number, number] = [
          nx * maxDist,
          pos[1],
          nz * maxDist
        ];
        
        return { pos: newPos, vel: newVel, hitSegment };
      };
      
      const wallHitSegments: number[] = [];
      
      // Circular ring bounce stays off. Inner and outer walls bounce entities
      // by writing back into this same store, so there is only one simulation.
      const { wallOwnershipMode } = state;
      
      // Always skip JS wall collision - walls are now visual only with no collision
      if (false && !wallOwnershipMode) {
        const preBounceSpeed = Math.sqrt(player.velocity[0] ** 2 + player.velocity[2] ** 2);
        const playerBounce = bounceOffCircularWall(prevPlayerPos, player.position, player.velocity, COLLISION_RADIUS, player.zoogi.id);
        player.position = playerBounce.pos;
        player.velocity = playerBounce.vel;
        if (playerBounce.hitSegment !== null && typeof playerBounce.hitSegment === 'number') {
          wallHitSegments.push(playerBounce.hitSegment as number);
          triggerWallHitFeel(Math.max(preBounceSpeed, 0.4));
          const wallNormal: [number, number, number] = [
            -player.position[0] / Math.sqrt(player.position[0] ** 2 + player.position[2] ** 2),
            0,
            -player.position[2] / Math.sqrt(player.position[0] ** 2 + player.position[2] ** 2)
          ];
          useGameFeel.getState().triggerWallSpark(player.position, wallNormal, Math.max(preBounceSpeed, 0.5));
          if (preBounceSpeed > 0.3) {
            useGameFeel.getState().triggerCartoonStarburst(player.position, "#ffff00");
            useGameFeel.getState().triggerCartoonSparks(player.position, wallNormal, "#ffaa00");
          }
        }
        
        enemies = enemies.map((enemy, ei) => {
          const enemyPreBounceSpeed = Math.sqrt(enemy.velocity[0] ** 2 + enemy.velocity[2] ** 2);
          const bounce = bounceOffCircularWall(prevEnemyPositions[ei], enemy.position, enemy.velocity, COLLISION_RADIUS, enemy.zoogi.id);
          if (bounce.hitSegment !== null) {
            wallHitSegments.push(bounce.hitSegment);
            if (enemyPreBounceSpeed > 0.3) {
              const enemyWallNormal: [number, number, number] = [
                -bounce.pos[0] / Math.sqrt(bounce.pos[0] ** 2 + bounce.pos[2] ** 2),
                0,
                -bounce.pos[2] / Math.sqrt(bounce.pos[0] ** 2 + bounce.pos[2] ** 2)
              ];
              useGameFeel.getState().triggerCartoonStarburst(bounce.pos, "#ffff00");
              useGameFeel.getState().triggerCartoonSparks(bounce.pos, enemyWallNormal, "#ffaa00");
            }
          }
          return { ...enemy, position: bounce.pos, velocity: bounce.vel };
        });
        
        orbs = orbs.map((orb, oi) => {
          if (!orb.isActive) return orb;
          const orbPreBounceSpeed = Math.sqrt(orb.velocity[0] ** 2 + orb.velocity[2] ** 2);
          const bounce = bounceOffCircularWall(prevOrbPositions[oi], orb.position, orb.velocity, 0.4);
          if (bounce.hitSegment !== null) {
            wallHitSegments.push(bounce.hitSegment);
            if (orbPreBounceSpeed > 0.2) {
              const orbWallNormal: [number, number, number] = [
                -bounce.pos[0] / Math.sqrt(bounce.pos[0] ** 2 + bounce.pos[2] ** 2),
                0,
                -bounce.pos[2] / Math.sqrt(bounce.pos[0] ** 2 + bounce.pos[2] ** 2)
              ];
              useGameFeel.getState().triggerCartoonStarburst(bounce.pos, "#ffff00");
              useGameFeel.getState().triggerCartoonSparks(bounce.pos, orbWallNormal, "#ffaa00");
            }
          }
          return { ...orb, position: bounce.pos, velocity: bounce.vel };
        });
      }
      
      const uniqueHitSegments = Array.from(new Set(wallHitSegments));
      uniqueHitSegments.forEach(segmentIndex => {
        get().damageWallSegment(segmentIndex, 1);
      });
      
      // Edge ring repel stays off. Wall response is applied by the wall
      // components into this same store, not a second physics world.
      if (false && !wallOwnershipMode) {
        const EDGE_REPEL_START = ARENA_RADIUS - 2; // Start repelling 2 units before edge
        const EDGE_REPEL_FORCE = 0.08; // Repel force strength
        
        // Repel player from edge
        const playerEdgeDist = Math.sqrt(player.position[0] ** 2 + player.position[2] ** 2);
        if (playerEdgeDist > EDGE_REPEL_START && playerEdgeDist < ARENA_RADIUS) {
          const repelStrength = ((playerEdgeDist - EDGE_REPEL_START) / (ARENA_RADIUS - EDGE_REPEL_START)) * EDGE_REPEL_FORCE;
          const nx = player.position[0] / playerEdgeDist;
          const nz = player.position[2] / playerEdgeDist;
          player.velocity = [
            player.velocity[0] - nx * repelStrength,
            player.velocity[1],
            player.velocity[2] - nz * repelStrength
          ];
        }
        
        // Repel enemies from edge
        enemies = enemies.map(enemy => {
          const enemyEdgeDist = Math.sqrt(enemy.position[0] ** 2 + enemy.position[2] ** 2);
          if (enemyEdgeDist > EDGE_REPEL_START && enemyEdgeDist < ARENA_RADIUS) {
            const repelStrength = ((enemyEdgeDist - EDGE_REPEL_START) / (ARENA_RADIUS - EDGE_REPEL_START)) * EDGE_REPEL_FORCE;
            const nx = enemy.position[0] / enemyEdgeDist;
            const nz = enemy.position[2] / enemyEdgeDist;
            return {
              ...enemy,
              velocity: [
                enemy.velocity[0] - nx * repelStrength,
                enemy.velocity[1],
                enemy.velocity[2] - nz * repelStrength
              ] as [number, number, number]
            };
          }
          return enemy;
        });
        
        // Repel orbs from edge
        orbs = orbs.map(orb => {
          if (!orb.isActive || orb.isOutOfRing) return orb;
          const orbEdgeDist = Math.sqrt(orb.position[0] ** 2 + orb.position[2] ** 2);
          if (orbEdgeDist > EDGE_REPEL_START && orbEdgeDist < ARENA_RADIUS) {
            const repelStrength = ((orbEdgeDist - EDGE_REPEL_START) / (ARENA_RADIUS - EDGE_REPEL_START)) * EDGE_REPEL_FORCE;
            const nx = orb.position[0] / orbEdgeDist;
            const nz = orb.position[2] / orbEdgeDist;
            return {
              ...orb,
              velocity: [
                orb.velocity[0] - nx * repelStrength,
                orb.velocity[1],
                orb.velocity[2] - nz * repelStrength
              ] as [number, number, number]
            };
          }
          return orb;
        });
      }
      
      // Knockout system - players respawn at green pads with penalty during restriction phase
      // Orbs stay where they land when knocked out of ring
      const { fallingEntities, restrictionPhaseActive, restrictionPhaseStartTime, nextRespawnPadIndex } = get();
      const now = Date.now();
      
      // Check if restriction phase should end (5 minutes elapsed OR all orbs out of ring)
      let shouldEndRestriction = false;
      if (restrictionPhaseActive && restrictionPhaseStartTime) {
        const elapsed = now - restrictionPhaseStartTime;
        if (elapsed >= RESTRICTION_PHASE_DURATION) {
          shouldEndRestriction = true;
          console.log("Restriction phase ended: 5 minutes elapsed");
        }
      }
      
      // Check if all orbs are out of ring
      const allOrbsOut = orbs.length > 0 && orbs.every(orb => orb.isOutOfRing);
      if (restrictionPhaseActive && allOrbsOut) {
        shouldEndRestriction = true;
        console.log("Restriction phase ended: all orbs knocked out of ring");
      }
      
      if (shouldEndRestriction) {
        set({ restrictionPhaseActive: false });
      }

      // Knockouts used to wait on restrictionPhaseActive, but that flag was
      // only ever stored as false, so falling off the island did nothing.
      // Scoring, the penalty, and the respawn stay on for the whole round.
      const isRestricted = true;
      let endTurnAfterTick = false;
      let currentRespawnPadIndex = nextRespawnPadIndex;
      
      const GROUND_LEVEL = 0;
      const GRAVITY = 0.02;
      let newFallingEntities = [...fallingEntities];
      
      // Update existing falling entities with gravity
      const MAX_FALLING_TIME = 5000;
      newFallingEntities = newFallingEntities.map(fe => {
        const createdAt = fe.createdAt || now;
        if (now - createdAt > MAX_FALLING_TIME) {
          return null;
        }
        
        if (fe.hasLanded) {
          if (fe.landedAt && now - fe.landedAt > 1000) {
            return null;
          }
          return fe;
        }
        
        const newFallVelY = fe.fallVelocityY - GRAVITY;
        const newY = fe.position[1] + newFallVelY;
        const newX = fe.position[0] + fe.velocity[0] * 0.5;
        const newZ = fe.position[2] + fe.velocity[2] * 0.5;
        
        if (newY <= GROUND_LEVEL) {
          return {
            ...fe,
            position: [newX, GROUND_LEVEL, newZ] as [number, number, number],
            fallVelocityY: 0,
            velocity: [0, 0, 0] as [number, number, number],
            hasLanded: true,
            landedAt: now
          };
        }
        
        return {
          ...fe,
          position: [newX, newY, newZ] as [number, number, number],
          fallVelocityY: newFallVelY,
          velocity: [fe.velocity[0] * 0.98, 0, fe.velocity[2] * 0.98] as [number, number, number]
        };
      }).filter((fe): fe is FallingEntity => fe !== null);
      
      // Get configured knockoff boundary for player/enemy knockout checks
      // Use exactly what the editor specifies - fallback matches visual ring default
      const knockoffRadiusForPlayers = state.wallSettings.knockoffBoundaryRadius ?? 21;
      const knockoffOffsetForPlayers = state.elementTransforms.knockoffBoundaryOffset ?? { x: 0, y: 0, z: 0 };
      
      // Mark first tick as processed (skip spawn immunity checks on first frame to allow position sync)
      if (!state.firstTickProcessed) {
        set({ firstTickProcessed: true });
        console.log("First tick processed - spawn immunity checks now active");
      }
      
      // Check if player has entered the ring - remove spawn immunity (skip on first tick)
      if (state.firstTickProcessed && player.spawnImmunity && isInsideRing(player.position[0], player.position[2])) {
        player.spawnImmunity = false;
        console.log("Player entered the ring - spawn immunity removed");
      }
      
      // Player knockout check - using configured knockoff boundary
      // Skip if player has spawn immunity (just spawned, not yet entered ring)
      const playerAdjustedX = player.position[0] - knockoffOffsetForPlayers.x;
      const playerAdjustedZ = player.position[2] - knockoffOffsetForPlayers.z;
      const playerDist = Math.sqrt(playerAdjustedX ** 2 + playerAdjustedZ ** 2);
      const playerIsInvulnerable = player.invulnerableUntil !== null && now < player.invulnerableUntil;
      const playerIsRespawning = player.isRespawning;
      const playerIsKnockedOut = player.isKnockedOut;
      const playerHasSpawnImmunity = player.spawnImmunity;
      const playerOut = state.selectedMap === "neon"
        ? isOutsideNeonCourt(playerAdjustedX, playerAdjustedZ)
        : playerDist > knockoffRadiusForPlayers;
      // Night Circuit knocks a marble out as soon as it leaves the floor.
      // The other maps still wait for the restriction phase.
      const knockoutLive = isRestricted || state.selectedMap === "neon";
      if (playerOut && !playerIsInvulnerable && !playerIsRespawning && !playerIsKnockedOut && !playerHasSpawnImmunity) {
        if (knockoutLive) {
          // Add player to falling entities for visual effect
          const playerFalling: FallingEntity = {
            id: `fall-player-${Date.now()}`,
            entityType: "player",
            position: [...player.position] as [number, number, number],
            velocity: [...player.velocity] as [number, number, number],
            fallVelocityY: 0,
            color: player.zoogi.color,
            zoogiId: player.zoogi.id,
            hasLanded: false,
            createdAt: Date.now()
          };
          newFallingEntities.push(playerFalling);
          
          if (gameMode === "local_multiplayer") {
            // Apply penalty to knocked out player (never go below 0)
            player.score = Math.max(0, player.score - KNOCKOUT_PENALTY);
            get().triggerKnockoffBoundaryFlash("#FF0000", 3); // Red flash for player knockout
            console.log(`Local player 0 knocked out! -${KNOCKOUT_PENALTY} points`);
            
            // Award points to attacker
            if (player.lastHitByLocalPlayerIndex !== null) {
              const attackerLocalIndex = player.lastHitByLocalPlayerIndex;
              if (attackerLocalIndex === 0) {
                console.log(`Player 0 knocked themselves out - no bonus awarded`);
              } else {
                const attackerEnemyIndex = attackerLocalIndex - 1;
                if (attackerEnemyIndex >= 0 && attackerEnemyIndex < enemies.length) {
                  enemies[attackerEnemyIndex] = {
                    ...enemies[attackerEnemyIndex],
                    score: enemies[attackerEnemyIndex].score + KNOCKOUT_SCORE_PLAYER
                  };
                  console.log(`Local player ${attackerLocalIndex} scored +${KNOCKOUT_SCORE_PLAYER} for knockout!`);
                }
              }
            }
            player.lastHitByLocalPlayerIndex = null;
          } else {
            get().addScore(-KNOCKOUT_PENALTY);
            get().triggerKnockoffBoundaryFlash("#FF0000", 3); // Red flash for player knockout
            console.log(`Player knocked out! -${KNOCKOUT_PENALTY} points`);
            if (player.lastHitByEnemyId && enemies.length > 0) {
              const attackerIndex = enemies.findIndex(e => e.id === player.lastHitByEnemyId);
              if (attackerIndex >= 0) {
                enemies[attackerIndex] = {
                  ...enemies[attackerIndex],
                  score: enemies[attackerIndex].score + KNOCKOUT_SCORE_PLAYER
                };
                console.log(`Enemy ${enemies[attackerIndex].zoogi.name} scored +${KNOCKOUT_SCORE_PLAYER} for knockout!`);
              }
            }
            player.lastHitByEnemyId = null;
          }
          
          // Schedule delayed respawn at green pad
          const targetPadIndex = currentRespawnPadIndex;
          currentRespawnPadIndex = (currentRespawnPadIndex + 1) % WALL_OWNERSHIP_GAP_ANGLES.length;
          
          // Stop the marble now. Waiting for it to roll to a halt off the
          // island left the turn stuck until the round timer ran out.
          player.velocity = [0, 0, 0];
          player.isKnockedOut = true;
          player.isRespawning = true;
          player.respawnAt = now + KNOCKOUT_RESPAWN_BEAT;
          player.respawnPadIndex = targetPadIndex;
          const playersTurn = gameMode === "local_multiplayer" ? currentLocalPlayerIndex === 0 : state.isPlayerTurn;
          if (gameMode !== "ringer_royale" && playersTurn) endTurnAfterTick = true;
          get().showAbilityNotice(`Fell off! -${KNOCKOUT_PENALTY}`);
          console.log(`Player knocked out! Respawning in ${KNOCKOUT_RESPAWN_BEAT}ms`);
        }
      }
      
      // Process pending player respawn - wait for momentum to stop
      if (player.isKnockedOut && !player.isRespawning) {
        const playerSpeed = Math.sqrt(player.velocity[0] ** 2 + player.velocity[2] ** 2);
        if (playerSpeed < 0.02) {
          // Momentum stopped after knockout - mark for respawn and end turn
          // Player will respawn at spawn point when their next turn starts
          player.velocity = [0, 0, 0];
          player.isRespawning = true; // Mark as waiting for respawn on next turn
          player.isKnockedOut = false; // Clear knocked out state so turn can advance
          console.log(`Player knocked out - will respawn at spawn point on next turn`);
          
          // Only if this marble is still the one up. A handoff that already
          // happened must not be ended again when the timer fires.
          setTimeout(() => {
            const live = get();
            if (live.phase !== "playing" || live.gameMode === "ringer_royale") return;
            const stillUp = live.gameMode === "local_multiplayer" ? live.currentLocalPlayerIndex === 0 : live.isPlayerTurn;
            if (stillUp) live.endTurn();
          }, 100);
        }
      }
      
      // Legacy respawn check (for backward compatibility)
      if (player.isRespawning && player.respawnAt !== null && now >= player.respawnAt) {
        const respawnPos = respawnInsidePlayfield(player.spawnPointIndex, state.zoneEditorConfigs, state.selectedMap, knockoffRadiusForPlayers);
        player.position = respawnPos;
        player.velocity = [0, 0, 0];
        player.isRespawning = false;
        player.isKnockedOut = false;
        player.respawnAt = null;
        player.respawnPadIndex = null;
        player.spawnImmunity = false;
        console.log(`Player respawned at spawn point ${player.spawnPointIndex}`);
      }
      
      // Enemy knockout check - mark for delayed respawn
      // Track bonus points to award AFTER the map completes (to avoid mutation issues)
      const bonusPointsToAward: Array<{attackerIndex: number, points: number}> = [];
      
      enemies = enemies.map((enemy, enemyIndex) => {
        // Skip if already respawning or knocked out
        if (enemy.isRespawning || enemy.isKnockedOut) return enemy;
        
        // Use configured knockoff boundary
        const enemyAdjustedX = enemy.position[0] - knockoffOffsetForPlayers.x;
        const enemyAdjustedZ = enemy.position[2] - knockoffOffsetForPlayers.z;
        const dist = Math.sqrt(enemyAdjustedX ** 2 + enemyAdjustedZ ** 2);
        const enemyIsInvulnerable = enemy.invulnerableUntil !== null && now < enemy.invulnerableUntil;
        const enemyOut = state.selectedMap === "neon"
          ? isOutsideNeonCourt(enemyAdjustedX, enemyAdjustedZ)
          : dist > knockoffRadiusForPlayers;
        if (enemyOut && knockoutLive && !enemyIsInvulnerable) {
          const localPlayerIndex = enemyIndex + 1;
          
          // Add enemy to falling entities for visual effect
          const enemyFalling: FallingEntity = {
            id: `fall-enemy-${enemyIndex}-${Date.now()}`,
            entityType: "enemy",
            entityIndex: enemyIndex,
            position: [...enemy.position] as [number, number, number],
            velocity: [...enemy.velocity] as [number, number, number],
            fallVelocityY: 0,
            color: enemy.zoogi.color,
            zoogiId: enemy.zoogi.id,
            hasLanded: false,
            createdAt: Date.now()
          };
          newFallingEntities.push(enemyFalling);
          
          if (gameMode === "local_multiplayer") {
            // Award points to attacker
            if (enemy.lastHitByLocalPlayerIndex !== null) {
              const attackerLocalIndex = enemy.lastHitByLocalPlayerIndex;
              if (attackerLocalIndex !== localPlayerIndex) {
                if (attackerLocalIndex === 0) {
                  player.score += KNOCKOUT_SCORE_PLAYER;
                } else {
                  // Queue bonus to be applied AFTER map completes (avoid mutation issues)
                  const attackerEnemyIndex = attackerLocalIndex - 1;
                  if (attackerEnemyIndex >= 0 && attackerEnemyIndex < enemies.length && attackerEnemyIndex !== enemyIndex) {
                    bonusPointsToAward.push({ attackerIndex: attackerEnemyIndex, points: KNOCKOUT_SCORE_PLAYER });
                  }
                }
              }
            }
          } else {
            if (enemy.lastHitByPlayer) {
              get().scoreKnockOff("enemy", KNOCKOUT_SCORE_PLAYER);
              get().showAbilityNotice(`Knockout! +${KNOCKOUT_SCORE_PLAYER}`);
            }
            get().triggerKnockoffBoundaryFlash("#FF0000", 3); // Red flash for any player knockout
            console.log(`Enemy ${enemy.zoogi.name} knocked out! -${KNOCKOUT_PENALTY} points`);
          }
          
          // Schedule delayed respawn at green pad
          const targetPadIndex = currentRespawnPadIndex;
          currentRespawnPadIndex = (currentRespawnPadIndex + 1) % WALL_OWNERSHIP_GAP_ANGLES.length;
          
          const enemyTurn = gameMode === "local_multiplayer"
            ? currentLocalPlayerIndex === enemyIndex + 1
            : !state.isPlayerTurn && state.turnIndex === enemyIndex;
          if (gameMode !== "ringer_royale" && enemyTurn) endTurnAfterTick = true;
          return {
            ...enemy,
            velocity: [0, 0, 0] as [number, number, number],
            lastHitByPlayer: false,
            lastHitByLocalPlayerIndex: null,
            score: Math.max(0, enemy.score - KNOCKOUT_PENALTY),
            isKnockedOut: true,
            isRespawning: true,
            respawnAt: now + KNOCKOUT_RESPAWN_BEAT,
            respawnPadIndex: targetPadIndex
          };
        }
        return enemy;
      });
      
      // Process knocked out enemies - mark for respawn on next turn when momentum stops
      enemies = enemies.map((enemy, idx) => {
        if (enemy.isKnockedOut && !enemy.isRespawning) {
          const enemySpeed = Math.sqrt(enemy.velocity[0] ** 2 + enemy.velocity[2] ** 2);
          if (enemySpeed < 0.02) {
            // Momentum stopped - mark for respawn on their next turn
            console.log(`Enemy ${enemy.zoogi.name} knocked out - will respawn at spawn point on next turn`);
            
            // For local multiplayer, trigger turn end for this player
            if (gameMode === "local_multiplayer" && currentLocalPlayerIndex === idx + 1) {
              const knockedIndex = idx + 1;
              setTimeout(() => {
                const live = get();
                if (live.phase === "playing" && live.gameMode === "local_multiplayer" && live.currentLocalPlayerIndex === knockedIndex) {
                  live.endTurn();
                }
              }, 100);
            }
            
            return {
              ...enemy,
              velocity: [0, 0, 0] as [number, number, number],
              isKnockedOut: false,
              isRespawning: true // Mark as waiting for respawn on next turn
            };
          }
        }
        return enemy;
      });
      
      // Apply queued bonus points after map completes (to avoid mutation issues during map iteration)
      for (const bonus of bonusPointsToAward) {
        enemies[bonus.attackerIndex] = {
          ...enemies[bonus.attackerIndex],
          score: enemies[bonus.attackerIndex].score + bonus.points
        };
      }
      
      // Process pending enemy respawns (legacy respawn timer)
      enemies = enemies.map((enemy) => {
        if (enemy.isRespawning && enemy.respawnAt !== null && now >= enemy.respawnAt) {
          const respawnPos = respawnInsidePlayfield(enemy.spawnPointIndex, state.zoneEditorConfigs, state.selectedMap, knockoffRadiusForPlayers);
          console.log(`Enemy ${enemy.zoogi.name} respawned at spawn point ${enemy.spawnPointIndex}`);
          return {
            ...enemy,
            position: respawnPos,
            velocity: [0, 0, 0] as [number, number, number],
            isRespawning: false,
            isKnockedOut: false,
            respawnAt: null,
            respawnPadIndex: null,
            spawnImmunity: false
          };
        }
        return enemy;
      });
      
      // Get configured knockoff boundary settings
      const { wallSettings, elementTransforms } = get();
      const knockoffRadius = wallSettings.knockoffBoundaryRadius ?? 21;
      const knockoffOffset = elementTransforms.knockoffBoundaryOffset ?? { x: 0, y: 0, z: 0 };
      
      // Orb knockout check - orbs continue momentum outside boundary, disappear when stopped
      orbs = orbs.map((orb) => {
        if (!orb.isActive) return orb;
        
        // Check distance from knockoff boundary center (with offset applied)
        const adjustedX = orb.position[0] - knockoffOffset.x;
        const adjustedZ = orb.position[2] - knockoffOffset.z;
        const dist = Math.sqrt(adjustedX ** 2 + adjustedZ ** 2);
        const orbSpeed = Math.sqrt(orb.velocity[0] ** 2 + orb.velocity[2] ** 2);
        
        // If already marked as out of ring, check if momentum has stopped
        if (orb.isOutOfRing) {
          if (orbSpeed < 0.02) {
            // Orb has stopped moving - deactivate it
            return {
              ...orb,
              isActive: false
            };
          }
          // Continue with momentum
          return orb;
        }
        
        // Check if orb STOPPED inside a score zone (non-spawn zones award 15 points when orb stops)
        const { zoneEditorConfigs, triggerOrbCaptureEffect } = get();
        let updatedOrb = { ...orb };
        
        // Only process if zoneEditorConfigs is valid and has entries
        if (zoneEditorConfigs && zoneEditorConfigs.length > 0) {
          const scoreZones = zoneEditorConfigs.filter(z => z && z.visible && !z.isSpawn && z.id);
          
          for (const zone of scoreZones) {
            const zoneX = Math.cos(zone.angle) * zone.distance;
            const zoneZ = Math.sin(zone.angle) * zone.distance;
            const dxZone = orb.position[0] - zoneX;
            const dzZone = orb.position[2] - zoneZ;
            const distToZone = Math.sqrt(dxZone * dxZone + dzZone * dzZone);
            
            // Check if orb is inside the score zone AND has stopped moving (orbSpeed < 0.02)
            if (distToZone < SCORE_ZONE_RADIUS && orbSpeed < 0.02) {
              // Check if orb hasn't already been captured in this zone
              if (updatedOrb.capturedInZone !== zone.id) {
                // Award points to whoever last hit the orb
                let attackerColor = "#00FFFF"; // Default cyan for zone capture
                
                if (updatedOrb.lastHitByLocalPlayerIndex !== null) {
                  const attackerLocalIndex = updatedOrb.lastHitByLocalPlayerIndex;
                  if (attackerLocalIndex === 0) {
                    player.score += ZONE_SCORE_ORB;
                    set((s) => ({ score: s.score + ZONE_SCORE_ORB }));
                    attackerColor = player.zoogi.color;
                    console.log(`🎯 Orb CAPTURED in zone ${zone.id}! Local player 0 scored +${ZONE_SCORE_ORB}!`);
                  } else {
                    const attackerEnemyIndex = attackerLocalIndex - 1;
                    if (attackerEnemyIndex >= 0 && attackerEnemyIndex < enemies.length) {
                      attackerColor = enemies[attackerEnemyIndex].zoogi.color;
                      enemies[attackerEnemyIndex] = {
                        ...enemies[attackerEnemyIndex],
                        score: enemies[attackerEnemyIndex].score + ZONE_SCORE_ORB
                      };
                      console.log(`🎯 Orb CAPTURED in zone ${zone.id}! Local player ${attackerLocalIndex} scored +${ZONE_SCORE_ORB}!`);
                    }
                  }
                } else if (updatedOrb.lastHitBy === "player") {
                  player.score += ZONE_SCORE_ORB;
                  set((s) => ({ score: s.score + ZONE_SCORE_ORB }));
                  attackerColor = player.zoogi.color;
                  console.log(`🎯 Orb CAPTURED in zone ${zone.id}! Player scored +${ZONE_SCORE_ORB}!`);
                } else if (updatedOrb.lastHitBy === "enemy" && updatedOrb.lastHitByEnemyId) {
                  const attackerIndex = enemies.findIndex(e => e.id === updatedOrb.lastHitByEnemyId);
                  if (attackerIndex >= 0) {
                    attackerColor = enemies[attackerIndex].zoogi.color;
                    enemies[attackerIndex] = {
                      ...enemies[attackerIndex],
                      score: enemies[attackerIndex].score + ZONE_SCORE_ORB
                    };
                    console.log(`🎯 Orb CAPTURED in zone ${zone.id}! Enemy ${enemies[attackerIndex].zoogi.name} scored +${ZONE_SCORE_ORB}!`);
                  }
                }
                
                // Trigger sci-fi capture effect
                triggerOrbCaptureEffect(orb.id, orb.position, attackerColor);
                
                // Trigger visual feedback (flash for zone capture)
                get().triggerKnockoffBoundaryFlash(attackerColor, 1);
                
                // Mark this orb as captured in this zone (can only score once per zone per orb)
                updatedOrb = {
                  ...updatedOrb,
                  capturedInZone: zone.id
                };
              }
            }
          }
        }
        
        // Apply zone scoring updates to orb
        orb = updatedOrb;
        
        const orbOut = state.selectedMap === "neon"
          ? isOutsideNeonCourt(adjustedX, adjustedZ)
          : dist > knockoffRadius;
        // Check if orb just crossed the knockoff boundary
        if (orbOut) {
          // Award points to whoever knocked the orb out
          let attackerColor = "#FFFFFF"; // Default white
          
          if (orb.lastHitByLocalPlayerIndex !== null) {
            const attackerLocalIndex = orb.lastHitByLocalPlayerIndex;
            if (attackerLocalIndex === 0) {
              player.score += KNOCKOUT_SCORE_ORB;
              // Also update global score state for UI display
              set((s) => ({ score: s.score + KNOCKOUT_SCORE_ORB }));
              attackerColor = player.zoogi.color;
              console.log(`Local player 0 scored +${KNOCKOUT_SCORE_ORB} for knocking orb out!`);
            } else {
              const attackerEnemyIndex = attackerLocalIndex - 1;
              if (attackerEnemyIndex >= 0 && attackerEnemyIndex < enemies.length) {
                attackerColor = enemies[attackerEnemyIndex].zoogi.color;
                enemies[attackerEnemyIndex] = {
                  ...enemies[attackerEnemyIndex],
                  score: enemies[attackerEnemyIndex].score + KNOCKOUT_SCORE_ORB
                };
                console.log(`Local player ${attackerLocalIndex} scored +${KNOCKOUT_SCORE_ORB} for knocking orb out!`);
              }
            }
          } else if (orb.lastHitBy === "player") {
            player.score += KNOCKOUT_SCORE_ORB;
            // Also update global score state for UI display
            set((s) => ({ score: s.score + KNOCKOUT_SCORE_ORB }));
            attackerColor = player.zoogi.color;
            console.log(`Player scored +${KNOCKOUT_SCORE_ORB} for knocking orb out!`);
            get().showAbilityNotice(`Orb off! +${KNOCKOUT_SCORE_ORB}`);
          } else if (orb.lastHitBy === "enemy" && orb.lastHitByEnemyId) {
            const attackerIndex = enemies.findIndex(e => e.id === orb.lastHitByEnemyId);
            if (attackerIndex >= 0) {
              attackerColor = enemies[attackerIndex].zoogi.color;
              enemies[attackerIndex] = {
                ...enemies[attackerIndex],
                score: enemies[attackerIndex].score + KNOCKOUT_SCORE_ORB
              };
              console.log(`Enemy ${enemies[attackerIndex].zoogi.name} scored +${KNOCKOUT_SCORE_ORB} for knocking orb out!`);
              get().showAbilityNotice(`${enemies[attackerIndex].zoogi.name} scored +${KNOCKOUT_SCORE_ORB}`);
            }
          }
          
          // Trigger knockoff boundary flash with player's color (2 flashes)
          get().triggerKnockoffBoundaryFlash(attackerColor, 2);

          if (orb.isStarOrb && orb.starOrbType) {
            const grant = (zoogiId: string, name: string) => {
              get().showAbilityNotice(`${name} unlocked!`);
              return unlockPatchForZoogi(zoogiId);
            };
            let anchorId: string | null = null;
            if (orb.lastHitBy === "player" || orb.lastHitByLocalPlayerIndex === 0) {
              player = { ...player, ...grant(player.zoogi.id, player.zoogi.ability) };
              anchorId = player.id;
            } else if (orb.lastHitByLocalPlayerIndex !== null && orb.lastHitByLocalPlayerIndex > 0) {
              const attackerIndex = orb.lastHitByLocalPlayerIndex - 1;
              if (enemies[attackerIndex]) {
                const attacker = enemies[attackerIndex];
                enemies[attackerIndex] = { ...attacker, ...grant(attacker.zoogi.id, attacker.zoogi.ability) };
                anchorId = attacker.id;
              }
            } else if (orb.lastHitBy === "enemy" && orb.lastHitByEnemyId) {
              const attackerIndex = enemies.findIndex(e => e.id === orb.lastHitByEnemyId);
              if (attackerIndex >= 0) {
                const attacker = enemies[attackerIndex];
                enemies[attackerIndex] = { ...attacker, ...grant(attacker.zoogi.id, attacker.zoogi.ability) };
                anchorId = attacker.id;
              }
            }
            if (anchorId) flashStarUnlock(set, orb, anchorId);
          }
          
          // Mark orb as out of ring but keep momentum going
          return {
            ...orb,
            isOutOfRing: true,
            lastHitBy: null,
            lastHitByEnemyId: null,
            lastHitByLocalPlayerIndex: null
          };
        }
        return orb;
      });
      
      
      const slowedNow = Date.now();
      if ((player.slowUntil || 0) > slowedNow) {
        player.velocity = [player.velocity[0] * 0.8, 0, player.velocity[2] * 0.8];
      }
      enemies = enemies.map((enemy) => (
        (enemy.slowUntil || 0) > slowedNow
          ? { ...enemy, velocity: [enemy.velocity[0] * 0.8, 0, enemy.velocity[2] * 0.8] as [number, number, number] }
          : enemy
      ));

      const stateUpdates: Partial<ZoogiGameState> = {
        playerEntity: player,
        enemies,
        orbs,
        wolfClones: wolfClonesUpdated,
        mushrooms: mushroomsUpdated,
        pinballBumpers: bumperHits.length > 0 ? state.pinballBumpers.map(b => { const hit = bumperHits.find(h => h.bumperId === b.id); return hit ? { ...b, lastHitTime: hit.hitTime } : b; }) : state.pinballBumpers,
        fallingEntities: newFallingEntities,
        nextRespawnPadIndex: currentRespawnPadIndex
      };
      
      if (hadCollision) {
        stateUpdates.lastCollisionTime = Date.now();
        stateUpdates.lastCollisionEvent = {
          attackerId: "player",
          attackerType: player.zoogi.type,
          timestamp: Date.now()
        };
      }
      
      if (newExplosion) {
        stateUpdates.showExplosion = newExplosion;
      }

      const hadActiveOrbs = state.orbs.some(o => o.isActive);
      const stillActiveOrbs = orbs.some(o => o.isActive);
      if (hadActiveOrbs && !stillActiveOrbs) {
        stateUpdates.phase = "round_end";
      }
      
      set(stateUpdates as ZoogiGameState);
      
      orbsToRemove.forEach(id => get().removeOrb(id));
      if (endTurnAfterTick) get().endTurn();

      // End the turn from the simulation, not from a React render. The marble
      // components only noticed a flick was over when one frame saw the speed
      // fall through 0.02. If physics finished the whole roll before that
      // render committed, the shot lock stayed on and the computer never moved.
      // A launch is real once this marble has been still on this turn and then
      // rolled. Coasting in from the previous hit does not count, so the
      // computer still gets to take its own shot.
      const settled = get();
      if (settled.phase === "playing" && settled.gameMode !== "ringer_royale") {
        const watchKey = settled.gameMode === "local_multiplayer"
          ? `L${settled.currentRound}:${settled.currentLocalPlayerIndex}`
          : `C${settled.currentRound}:${settled.isPlayerTurn ? "p" : "e"}${settled.turnIndex}`;
        if (watchKey !== turnWatchKey) {
          turnWatchKey = watchKey;
          turnReadyForLaunch = false;
          turnSlowFrames = 0;
        }
        const actor = actorTakingTurn(settled);
        if (actor && !actor.isKnockedOut) {
          const actorSpeed = Math.hypot(actor.velocity[0], actor.velocity[2]);
          if (!turnReadyForLaunch) {
            if (actorSpeed < 0.02) turnReadyForLaunch = true;
          } else if (actorSpeed > 0.08 && !settled.turnHasLaunched) {
            set({ turnHasLaunched: true });
            turnSlowFrames = 0;
          } else if (get().turnHasLaunched) {
            if (actorSpeed < 0.02) {
              get().endTurn();
            } else if (actorSpeed < 0.12) {
              turnSlowFrames += 1;
              if (turnSlowFrames > 40) get().endTurn();
            } else {
              turnSlowFrames = 0;
            }
          }
        }
      }
    }
  }))
);

import * as THREE from "three";
import { useRef, useState, useEffect, useMemo, Suspense, Component, type ReactNode } from "react";
import { useFrame, useThree } from "@react-three/fiber";
import { Line, Html, useGLTF } from "@react-three/drei";
import { stopFrameMayEndTurn, useZoogiGame } from "@/lib/stores/useZoogiGame";
import { marbleUniformScale, resolveZoogiModel, rollMarble, zoogiModelPreloadUrls, type ZoogiModelSettings } from "@/lib/zoogiModels";
import { useAudio } from "@/lib/stores/useAudio";
import { useProgression } from "@/lib/stores/useProgression";
import { triggerLaunchFeel } from "@/lib/stores/useGameFeel";
import { triggerLaunchCameraEffect, clearAimCameraEffect } from "@/lib/stores/useCameraEffects";
import { getSkinEffect, getRainbowColor } from "@/lib/skinEffects";
import { StunnedIndicator } from "./PowerEffects";
import { AimBeam, AimPath, BindRibbons, Glint, RicochetShell, type Vec3 } from "@/vfx/powerLooks";

// Global scale control - adjust this to resize ALL Zoogis uniformly
let globalZoogiScale = 0.5;

export function getGlobalZoogiScale(): number {
  return globalZoogiScale;
}

export function setGlobalZoogiScale(value: number) {
  globalZoogiScale = Math.max(0.1, Math.min(2.0, value));
  useZoogiGame.getState().incrementMoveCounter();
}

// Per-character material styles for polished marble look
const ZOOGI_MATERIAL_STYLES: Record<string, { 
  clearcoat: number; 
  clearcoatRoughness: number; 
  metalness: number; 
  roughness: number;
  reflectivity: number;
  sheen: number;
  sheenRoughness: number;
  sheenColor?: string;
}> = {
  wolfgang: { clearcoat: 1.0, clearcoatRoughness: 0.05, metalness: 0.95, roughness: 0.15, reflectivity: 1.0, sheen: 0.3, sheenRoughness: 0.2, sheenColor: "#888888" },
  hotstreak: { clearcoat: 1.0, clearcoatRoughness: 0.08, metalness: 0.85, roughness: 0.2, reflectivity: 0.9, sheen: 0.5, sheenRoughness: 0.15, sheenColor: "#FF6600" },
  lars: { clearcoat: 1.0, clearcoatRoughness: 0.03, metalness: 0.7, roughness: 0.1, reflectivity: 1.0, sheen: 0.2, sheenRoughness: 0.1, sheenColor: "#4488FF" },
  wraps: { clearcoat: 0.35, clearcoatRoughness: 0.45, metalness: 0.05, roughness: 0.62, reflectivity: 0.3, sheen: 0.85, sheenRoughness: 0.4, sheenColor: "#D4C4B0" },
  pinpoint: { clearcoat: 1.0, clearcoatRoughness: 0.1, metalness: 0.9, roughness: 0.25, reflectivity: 0.85, sheen: 0.4, sheenRoughness: 0.2, sheenColor: "#AA66FF" },
  bolt: { clearcoat: 1.0, clearcoatRoughness: 0.02, metalness: 0.8, roughness: 0.08, reflectivity: 1.0, sheen: 0.6, sheenRoughness: 0.1, sheenColor: "#FFDD00" },
  nightshade: { clearcoat: 1.0, clearcoatRoughness: 0.03, metalness: 0.9, roughness: 0.08, reflectivity: 1.0, sheen: 0.55, sheenRoughness: 0.12, sheenColor: "#b69cff" },
};

const DEFAULT_MATERIAL_STYLE = { clearcoat: 1.0, clearcoatRoughness: 0.1, metalness: 0.8, roughness: 0.2, reflectivity: 0.9, sheen: 0.3, sheenRoughness: 0.2 };

class ZoogiModelErrorBoundary extends Component<{ children: ReactNode; fallback: ReactNode }, { failed: boolean }> {
  state = { failed: false };

  static getDerivedStateFromError() {
    return { failed: true };
  }

  componentDidCatch(error: Error) {
    console.warn("Zoogi model failed to load, showing the colored marble instead.", error.message);
  }

  render() {
    return this.state.failed ? this.props.fallback : this.props.children;
  }
}

function ColoredMarble({
  radius,
  color,
  glowColor,
  glowIntensity,
  materialStyle,
}: {
  radius: number;
  color: string;
  glowColor: string;
  glowIntensity: number;
  materialStyle: typeof DEFAULT_MATERIAL_STYLE & { sheenColor?: string };
}) {
  return (
    <mesh castShadow scale={[radius, radius, radius]}>
      <sphereGeometry args={[1, 64, 32]} />
      <meshPhysicalMaterial
        color={color}
        emissive={glowColor}
        emissiveIntensity={glowIntensity}
        metalness={materialStyle.metalness}
        roughness={materialStyle.roughness}
        clearcoat={materialStyle.clearcoat}
        clearcoatRoughness={materialStyle.clearcoatRoughness}
        reflectivity={materialStyle.reflectivity}
        sheen={materialStyle.sheen}
        sheenRoughness={materialStyle.sheenRoughness}
        sheenColor={materialStyle.sheenColor || color}
        envMapIntensity={1.2}
      />
    </mesh>
  );
}

// Real character model, fitted to the marble and rested on the floor.
// The colored ball stays up until this finishes loading.
function FittedZoogiModel({ settings, marbleRadius }: { settings: ZoogiModelSettings; marbleRadius: number }) {
  const { scene } = useGLTF(settings.url);
  const cloned = useMemo(() => {
    const copy = scene.clone(true);
    copy.traverse((child) => {
      const mesh = child as THREE.Mesh;
      if (mesh.isMesh) {
        mesh.castShadow = true;
        mesh.receiveShadow = true;
      }
    });
    return copy;
  }, [scene]);

  const bounds = useMemo(() => {
    const box = new THREE.Box3().setFromObject(cloned);
    const size = new THREE.Vector3();
    const center = new THREE.Vector3();
    box.getSize(size);
    box.getCenter(center);
    return { size, center };
  }, [cloned]);

  const maxDimension = Math.max(bounds.size.x, bounds.size.y, bounds.size.z, 1e-4);
  const uniform = ((marbleRadius * 2) / maxDimension) * settings.scale;
  // After the fit, shift so the lowest point sits on the floor like the ball.
  const bottom = -(bounds.size.y / 2) * uniform;
  const restOnFloor = -marbleRadius - bottom;

  // Horns (and anything else past the ball) must not drive the fit. The file
  // origin is already the ball centre, so scale it like the roster and leave Y alone.
  if (settings.fit === "pivot") {
    const pivotScale = marbleUniformScale(marbleRadius, settings);
    return (
      <group
        position={[settings.offset[0], settings.offset[1], settings.offset[2]]}
        rotation={settings.rotation}
        scale={pivotScale}
      >
        <primitive object={cloned} />
      </group>
    );
  }

  return (
    <group
      position={[settings.offset[0], settings.offset[1] + restOnFloor, settings.offset[2]]}
      rotation={settings.rotation}
    >
      <group scale={uniform}>
        <group position={[-bounds.center.x, -bounds.center.y, -bounds.center.z]}>
          <primitive object={cloned} />
        </group>
      </group>
    </group>
  );
}

// Character model for every Zoogi in a match. Falls back to the colored marble
// while the file loads, and if the file is missing.
function ZoogiModelSwitch({ zoogiId, hasShield = false, hasSpawnImmunity = false, color = "#888888", customModelUrl, isPlayer = false }: { zoogiId: string; hasShield?: boolean; hasSpawnImmunity?: boolean; color?: string; customModelUrl?: string; isPlayer?: boolean }) {
  const marbleRadius = getGlobalZoogiScale();
  const { equippedSkin } = useProgression();
  const skinEffect = isPlayer ? getSkinEffect(equippedSkin) : getSkinEffect(null);
  const timeRef = useRef(0);
  const [skinGlowColor, setSkinGlowColor] = useState(skinEffect.glowColor || color);
  
  useFrame((_, delta) => {
    if (isPlayer && skinEffect.id === "skin_rainbow") {
      timeRef.current += delta;
      setSkinGlowColor(getRainbowColor(timeRef.current));
    }
  });
  
  // Spawn immunity gives a green glow, shield gives cyan glow, then skin effects
  const baseGlowColor = hasSpawnImmunity ? "#00FF88" : hasShield ? "#00FFFF" : (isPlayer && skinEffect.glowColor ? skinEffect.glowColor : color);
  const glowIntensity = hasSpawnImmunity ? 0.5 : hasShield ? 0.4 : (isPlayer && skinEffect.glowIntensity > 0 ? skinEffect.glowIntensity : 0.15);
  
  // Apply skin color override if available
  const displayColor = isPlayer && skinEffect.colorOverride ? skinEffect.colorOverride : color;
  
  // Get per-character material style or use default
  const materialStyle = ZOOGI_MATERIAL_STYLES[zoogiId] || DEFAULT_MATERIAL_STYLE;
  const model = resolveZoogiModel(zoogiId, customModelUrl);
  const coloredMarble = (
    <ColoredMarble
      radius={marbleRadius}
      color={displayColor}
      glowColor={skinEffect.id === "skin_rainbow" ? skinGlowColor : baseGlowColor}
      glowIntensity={glowIntensity}
      materialStyle={materialStyle}
    />
  );
  
  return (
    <>
      {model ? (
        <ZoogiModelErrorBoundary key={model.url} fallback={coloredMarble}>
          <Suspense fallback={coloredMarble}>
            <FittedZoogiModel settings={model} marbleRadius={marbleRadius} />
          </Suspense>
        </ZoogiModelErrorBoundary>
      ) : (
        coloredMarble
      )}
      {/* Skin glow effect */}
      {isPlayer && skinEffect.glowIntensity > 0.3 && (
        <mesh scale={[marbleRadius * 1.3, marbleRadius * 1.3, marbleRadius * 1.3]}>
          <sphereGeometry args={[1, 16, 16]} />
          <meshPhysicalMaterial
            color={skinEffect.id === "skin_rainbow" ? skinGlowColor : (skinEffect.glowColor || color)}
            transparent
            opacity={0.2}
            side={THREE.DoubleSide}
            emissive={skinEffect.id === "skin_rainbow" ? skinGlowColor : (skinEffect.glowColor || color)}
            emissiveIntensity={0.3}
            clearcoat={0.5}
            roughness={0.3}
          />
        </mesh>
      )}
      {/* Spawn immunity visual - green pulsing ring */}
      {hasSpawnImmunity && (
        <mesh scale={[marbleRadius * 1.5, marbleRadius * 1.5, marbleRadius * 1.5]}>
          <sphereGeometry args={[1, 16, 16]} />
          <meshPhysicalMaterial
            color="#00FF88"
            transparent
            opacity={0.25}
            side={THREE.DoubleSide}
            emissive="#00FF88"
            emissiveIntensity={0.4}
            clearcoat={0.5}
            roughness={0.3}
          />
        </mesh>
      )}
      {hasShield && (
        <mesh scale={[marbleRadius * 1.4, marbleRadius * 1.4, marbleRadius * 1.4]}>
          <sphereGeometry args={[1, 16, 16]} />
          <meshPhysicalMaterial
            color="#00FFFF"
            transparent
            opacity={0.3}
            side={THREE.DoubleSide}
            clearcoat={0.8}
            roughness={0.2}
            transmission={0.3}
          />
        </mesh>
      )}
    </>
  );
}

const LAUNCH_POWER_MULTIPLIER = 0.22;
const MAX_LAUNCH_SPEED = 1.4;

const ZOOGI_TRAJECTORY_COLORS: Record<string, string> = {
  wolfgang: "#6B7280",   // gray
  hotstreak: "#F97316",  // orange
  lars: "#3B82F6",       // blue
  wraps: "#D4C4B0",
  pinpoint: "#8B5CF6",   // purple
  bolt: "#FBBF24",       // yellow
  nightshade: "#b69cff", // glowing cracks
};

function boundNow(entity: { wrapsBindUntil?: number; slowUntil?: number }): boolean {
  const now = Date.now();
  return (entity.wrapsBindUntil || 0) > now || (entity.slowUntil || 0) > now;
}

function StatusLooks({ ricochet, bound }: { ricochet: boolean; bound: boolean }) {
  return (
    <>
      {ricochet && <RicochetShell />}
      {bound && <BindRibbons />}
    </>
  );
}

function PinpointStroke({
  points,
  color,
  pinpoint,
}: {
  points: Vec3[];
  color: string;
  pinpoint: boolean;
}) {
  if (points.length < 2) return null;
  if (pinpoint) return <AimPath points={points} />;
  return <Line points={points} color={color} lineWidth={6} />;
}

function PinpointDragBeam({
  origin,
  dx,
  dz,
  multiplier,
}: {
  origin: Vec3;
  dx: number;
  dz: number;
  multiplier: number;
}) {
  const span = Math.hypot(dx, dz);
  if (span < 0.05) return null;
  const length = Math.min(span * multiplier, 8);
  const to: Vec3 = [
    origin[0] + (dx / span) * length,
    0.55,
    origin[2] + (dz / span) * length,
  ];
  return <AimBeam from={[origin[0], 0.55, origin[2]]} to={to} />;
}

function TurnStunMarker({ entity }: { entity: { position: [number, number, number]; isStunned: boolean; stunTimer: number } }) {
  if (!entity.isStunned) return null;
  return (
    <StunnedIndicator
      position={entity.position}
      remaining={entity.stunTimer > 0 ? entity.stunTimer : 1}
      duration={0}
      pulse
    />
  );
}

export function PlayerZoogi() {
  const meshRef = useRef<THREE.Group>(null);
  const { 
    playerEntity, 
    updatePlayerVelocity,
    isPlayerTurn,
    currentRound,
    endTurn,
    setMovementStopped,
    armHotstreakGrenade,
    gameMode,
    lockOnEnabled,
    lockOnTargetId,
    lockOnTargetType,
    setLockOnTarget,
    arcType,
    straightMode,
    tangentOffset,
    orbs,
    enemies,
    firstPersonView,
    overShoulderView,
    birdsEyeView,
    setIsAiming
  } = useZoogiGame();
  const { camera, gl } = useThree();
  const { playSound } = useAudio();
  
  const [isDragging, setIsDragging] = useState(false);
  const [dragStart, setDragStart] = useState<[number, number]>([0, 0]);
  const [dragEnd, setDragEnd] = useState<[number, number]>([0, 0]);
  const hasLaunchedRef = useRef(false);
  const prevSpeedRef = useRef(0);
  const launchCooldownRef = useRef(false);
  const lastLaunchTimeRef = useRef(0);
  const prevIsPlayerTurnRef = useRef(isPlayerTurn);
  const lastPullSoundRef = useRef(0);
  
  const [swipeStart, setSwipeStart] = useState<{ x: number; y: number; time: number } | null>(null);
  const lastBoostTimeRef = useRef(0);
  const BOOST_COOLDOWN_MS = 800;
  const BOOST_POWER = 0.4;
  const TAP_THRESHOLD = 50;
  const SWIPE_MIN_DISTANCE = 30;
  
  const isFreeForAll = gameMode === "ringer_royale";
  const LAUNCH_COOLDOWN_MS = 1500;
  
  useEffect(() => {
    if (isPlayerTurn && !prevIsPlayerTurnRef.current) {
      launchCooldownRef.current = false;
      hasLaunchedRef.current = false;
    }
    prevIsPlayerTurnRef.current = isPlayerTurn;
  }, [isPlayerTurn]);

  // A round can end while it is still this marble's turn (the clock hit 0).
  // The turn flag never flips, so the shot lock from the previous round would
  // otherwise stay on and the next round could not be flicked.
  useEffect(() => {
    launchCooldownRef.current = false;
    hasLaunchedRef.current = false;
    prevSpeedRef.current = 0;
  }, [currentRound]);

  useEffect(() => {
    if (playerEntity && isPlayerTurn) {
      const speed = Math.sqrt(playerEntity.velocity[0] ** 2 + playerEntity.velocity[2] ** 2);
      if (speed > 0.1 && prevSpeedRef.current < 0.1 && !hasLaunchedRef.current) {
        hasLaunchedRef.current = true;
      }
    }
  }, [playerEntity?.velocity, isPlayerTurn]);

  useEffect(() => {
    const handlePointerMove = (e: PointerEvent) => {
      if (isDragging && playerEntity) {
        const rect = gl.domElement.getBoundingClientRect();
        const x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
        const y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
        
        const raycaster = new THREE.Raycaster();
        raycaster.setFromCamera(new THREE.Vector2(x, y), camera);
        const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
        const intersection = new THREE.Vector3();
        raycaster.ray.intersectPlane(plane, intersection);
        
        if (intersection) {
          setDragEnd([intersection.x, intersection.z]);
          
          const now = Date.now();
          if (now - lastPullSoundRef.current > 150) {
            const pdx = dragStart[0] - intersection.x;
            const pdz = dragStart[1] - intersection.z;
            const pullDistance = Math.sqrt(pdx * pdx + pdz * pdz);
            const pullIntensity = Math.min(pullDistance / 10, 1);
            if (pullIntensity > 0.1) {
              playSound("pull_back", pullIntensity);
              lastPullSoundRef.current = now;
            }
          }
        }
      }
    };

    const handlePointerUp = () => {
      if (isDragging && playerEntity) {
        const dx = dragStart[0] - dragEnd[0];
        const dz = dragStart[1] - dragEnd[1];
        const rawPower = Math.sqrt(dx * dx + dz * dz);
        
        let adjustedDx = dx;
        let adjustedDz = dz;
        
        if (lockOnEnabled && lockOnTargetId) {
          let targetPos: [number, number, number] | null = null;
          if (lockOnTargetType === "orb") {
            const targetOrb = orbs.find(o => o.id === lockOnTargetId && o.isActive);
            if (targetOrb) targetPos = targetOrb.position;
          } else if (lockOnTargetType === "enemy") {
            const targetEnemy = enemies.find(e => e.id === lockOnTargetId);
            if (targetEnemy) targetPos = targetEnemy.position;
          }
          
          if (targetPos) {
            const toDx = targetPos[0] - playerEntity.position[0];
            const toDz = targetPos[2] - playerEntity.position[2];
            const toDist = Math.sqrt(toDx * toDx + toDz * toDz);
            if (toDist > 0.1) {
              const launchMagnitude = Math.sqrt(dx * dx + dz * dz);
              adjustedDx = (toDx / toDist) * launchMagnitude;
              adjustedDz = (toDz / toDist) * launchMagnitude;
            }
          }
        }
        
        const power = Math.min(Math.sqrt(adjustedDx * adjustedDx + adjustedDz * adjustedDz), 15);
        
        if (power > 1) {
          const speedBoost = playerEntity.speedBoost || 1;
          const larsBoost = playerEntity.zoogi.id === "lars" ? (playerEntity.larsRicochetBoost || 1) : 1;
          const totalBoost = speedBoost * larsBoost;
          
          let vx = adjustedDx * LAUNCH_POWER_MULTIPLIER * totalBoost;
          let vz = adjustedDz * LAUNCH_POWER_MULTIPLIER * totalBoost;
          
          const launchSpeed = Math.sqrt(vx * vx + vz * vz);
          if (launchSpeed > MAX_LAUNCH_SPEED) {
            const scale = MAX_LAUNCH_SPEED / launchSpeed;
            vx *= scale;
            vz *= scale;
          }
          
          const newVelocity: [number, number, number] = [vx, 0, vz];
          updatePlayerVelocity(newVelocity);
          hasLaunchedRef.current = true;
          launchCooldownRef.current = true;
          lastLaunchTimeRef.current = Date.now();
          setMovementStopped(false);
          playSound("launch");
          triggerLaunchFeel();
          triggerLaunchCameraEffect();
          console.log("Player launched with velocity:", newVelocity, "Lars boost:", larsBoost, "Mode:", gameMode);
        }
        
        clearAimCameraEffect();
        setIsDragging(false);
        setIsAiming(false);
      }
    };

    window.addEventListener("pointermove", handlePointerMove);
    window.addEventListener("pointerup", handlePointerUp);
    
    return () => {
      window.removeEventListener("pointermove", handlePointerMove);
      window.removeEventListener("pointerup", handlePointerUp);
    };
  }, [isDragging, dragStart, dragEnd, camera, gl, playerEntity, updatePlayerVelocity, setMovementStopped, lockOnEnabled, lockOnTargetId, lockOnTargetType, orbs, enemies, playSound, setIsAiming]);

  useEffect(() => {
    if (!isFreeForAll) return;
    
    const MAX_BOOSTED_SPEED = 0.8;
    
    const clampVelocity = (vel: [number, number, number], preserveY: number): [number, number, number] => {
      const speed = Math.sqrt(vel[0] ** 2 + vel[2] ** 2);
      if (speed > MAX_BOOSTED_SPEED) {
        const scale = MAX_BOOSTED_SPEED / speed;
        return [vel[0] * scale, preserveY, vel[2] * scale];
      }
      return [vel[0], preserveY, vel[2]];
    };
    
    const handleTouchStart = (e: TouchEvent) => {
      if (!playerEntity) return;
      e.preventDefault();
      const touch = e.touches[0];
      setSwipeStart({ x: touch.clientX, y: touch.clientY, time: Date.now() });
    };
    
    const handleTouchEnd = (e: TouchEvent) => {
      e.preventDefault();
      const currentSwipeStart = swipeStart;
      setSwipeStart(null);
      
      if (!playerEntity || !currentSwipeStart) {
        return;
      }
      
      const now = Date.now();
      if (now - lastBoostTimeRef.current < BOOST_COOLDOWN_MS) {
        return;
      }
      
      const touch = e.changedTouches[0];
      const dx = touch.clientX - currentSwipeStart.x;
      const dy = touch.clientY - currentSwipeStart.y;
      const distance = Math.sqrt(dx * dx + dy * dy);
      const duration = now - currentSwipeStart.time;
      
      const currentY = playerEntity.velocity[1];
      
      if (distance < TAP_THRESHOLD && duration < 300) {
        const speed = Math.sqrt(playerEntity.velocity[0] ** 2 + playerEntity.velocity[2] ** 2);
        let boostVx: number, boostVz: number;
        
        if (speed > 0.05) {
          const vx = playerEntity.velocity[0];
          const vz = playerEntity.velocity[2];
          const mag = Math.sqrt(vx * vx + vz * vz);
          boostVx = (vx / mag) * BOOST_POWER;
          boostVz = (vz / mag) * BOOST_POWER;
        } else {
          const rect = gl.domElement.getBoundingClientRect();
          const tapNdcX = ((touch.clientX - rect.left) / rect.width) * 2 - 1;
          const tapNdcY = -((touch.clientY - rect.top) / rect.height) * 2 + 1;
          
          const raycaster = new THREE.Raycaster();
          const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
          raycaster.setFromCamera(new THREE.Vector2(tapNdcX, tapNdcY), camera);
          const tapWorld = new THREE.Vector3();
          raycaster.ray.intersectPlane(plane, tapWorld);
          
          if (tapWorld) {
            const toDx = tapWorld.x - playerEntity.position[0];
            const toDz = tapWorld.z - playerEntity.position[2];
            const toDist = Math.sqrt(toDx * toDx + toDz * toDz);
            if (toDist > 0.1) {
              boostVx = (toDx / toDist) * BOOST_POWER;
              boostVz = (toDz / toDist) * BOOST_POWER;
            } else {
              boostVx = 0;
              boostVz = BOOST_POWER;
            }
          } else {
            boostVx = 0;
            boostVz = BOOST_POWER;
          }
        }
        
        const newVel = clampVelocity([
          playerEntity.velocity[0] + boostVx,
          currentY,
          playerEntity.velocity[2] + boostVz
        ], currentY);
        updatePlayerVelocity(newVel);
        lastBoostTimeRef.current = now;
        playSound("launch");
        triggerLaunchFeel();
        console.log("Tap boost applied! New speed:", Math.sqrt(newVel[0] ** 2 + newVel[2] ** 2).toFixed(3));
      } else if (distance >= SWIPE_MIN_DISTANCE && duration < 500) {
        const rect = gl.domElement.getBoundingClientRect();
        const startNdcX = ((currentSwipeStart.x - rect.left) / rect.width) * 2 - 1;
        const startNdcY = -((currentSwipeStart.y - rect.top) / rect.height) * 2 + 1;
        const endNdcX = ((touch.clientX - rect.left) / rect.width) * 2 - 1;
        const endNdcY = -((touch.clientY - rect.top) / rect.height) * 2 + 1;
        
        const raycaster = new THREE.Raycaster();
        const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
        
        raycaster.setFromCamera(new THREE.Vector2(startNdcX, startNdcY), camera);
        const startWorld = new THREE.Vector3();
        raycaster.ray.intersectPlane(plane, startWorld);
        
        raycaster.setFromCamera(new THREE.Vector2(endNdcX, endNdcY), camera);
        const endWorld = new THREE.Vector3();
        raycaster.ray.intersectPlane(plane, endWorld);
        
        if (startWorld && endWorld) {
          const swipeDx = endWorld.x - startWorld.x;
          const swipeDz = endWorld.z - startWorld.z;
          const swipeMag = Math.sqrt(swipeDx * swipeDx + swipeDz * swipeDz);
          
          if (swipeMag > 0.1) {
            const boostStrength = Math.min(swipeMag * 0.06, BOOST_POWER * 0.8);
            const boostVx = (swipeDx / swipeMag) * boostStrength;
            const boostVz = (swipeDz / swipeMag) * boostStrength;
            const newVel = clampVelocity([
              playerEntity.velocity[0] + boostVx,
              currentY,
              playerEntity.velocity[2] + boostVz
            ], currentY);
            updatePlayerVelocity(newVel);
            lastBoostTimeRef.current = now;
            playSound("launch");
            triggerLaunchFeel();
            console.log("Swipe boost applied! New speed:", Math.sqrt(newVel[0] ** 2 + newVel[2] ** 2).toFixed(3));
          }
        }
      }
    };
    
    const canvas = gl.domElement;
    canvas.addEventListener("touchstart", handleTouchStart, { passive: false });
    canvas.addEventListener("touchend", handleTouchEnd, { passive: false });
    
    return () => {
      canvas.removeEventListener("touchstart", handleTouchStart);
      canvas.removeEventListener("touchend", handleTouchEnd);
    };
  }, [isFreeForAll, playerEntity, swipeStart, updatePlayerVelocity, camera, gl, playSound]);

  useFrame(() => {
    if (!meshRef.current || !playerEntity) return;
    
    meshRef.current.position.set(
      playerEntity.position[0],
      playerEntity.position[1],
      playerEntity.position[2]
    );
    
    const speed = Math.sqrt(playerEntity.velocity[0] ** 2 + playerEntity.velocity[2] ** 2);
    rollMarble(meshRef.current, playerEntity.position[0], playerEntity.position[2], getGlobalZoogiScale());
    
    meshRef.current.scale.set(1, 1, 1);
    
    meshRef.current.updateMatrixWorld(true);
    
    if (hasLaunchedRef.current && speed < 0.02 && prevSpeedRef.current >= 0.02) {
      hasLaunchedRef.current = false;
      launchCooldownRef.current = false;
      setMovementStopped(true);

      // Physics already ends the turn when the roll stops. A later frame can
      // still see this same slowdown. Ending again would skip whoever is up now.
      if (!isFreeForAll && stopFrameMayEndTurn(useZoogiGame.getState(), "player")) {
        endTurn();
        console.log("Player turn ended");
      }
    }
    prevSpeedRef.current = speed;
  });

  if (!playerEntity) return null;

  const pos = playerEntity.position;
  
  const rawLaunchDx = dragStart[0] - dragEnd[0];
  const rawLaunchDz = dragStart[1] - dragEnd[1];
  const rawDragDist = Math.sqrt(rawLaunchDx * rawLaunchDx + rawLaunchDz * rawLaunchDz);
  
  let effectiveLaunchDx = rawLaunchDx;
  let effectiveLaunchDz = rawLaunchDz;
  
  let lockOnTargetPos: [number, number, number] | null = null;
  if (lockOnEnabled && lockOnTargetId) {
    if (lockOnTargetType === "orb") {
      const targetOrb = orbs.find(o => o.id === lockOnTargetId && o.isActive);
      if (targetOrb) lockOnTargetPos = targetOrb.position;
    } else if (lockOnTargetType === "enemy") {
      const targetEnemy = enemies.find(e => e.id === lockOnTargetId);
      if (targetEnemy) lockOnTargetPos = targetEnemy.position;
    }
    
    if (lockOnTargetPos && rawDragDist > 0.1) {
      const toDx = lockOnTargetPos[0] - pos[0];
      const toDz = lockOnTargetPos[2] - pos[2];
      const toDist = Math.sqrt(toDx * toDx + toDz * toDz);
      if (toDist > 0.1) {
        effectiveLaunchDx = (toDx / toDist) * rawDragDist;
        effectiveLaunchDz = (toDz / toDist) * rawDragDist;
      }
    }
  }
  
  const effectiveDist = Math.sqrt(effectiveLaunchDx * effectiveLaunchDx + effectiveLaunchDz * effectiveLaunchDz);
  const launchDx = effectiveLaunchDx;
  const launchDz = effectiveLaunchDz;
  const launchMultiplier = 2;
  
  const dragPower = Math.min(effectiveDist, 15);
  const powerLevel = dragPower < 2 ? "LOW" : dragPower < 4 ? "MEDIUM" : "HIGH";
  const powerColor = dragPower < 2 ? "#22C55E" : dragPower < 4 ? "#EAB308" : "#EF4444";
  
  const zoogiColor = ZOOGI_TRAJECTORY_COLORS[playerEntity.zoogi.id] || playerEntity.zoogi.color;
  
  const showArcTrajectory = arcType !== null && lockOnEnabled && lockOnTargetId;
  const showStraightTrajectory = straightMode && lockOnEnabled && lockOnTargetId && lockOnTargetPos;
  
  // Calculate tangent trajectory end point for off-center hits
  const straightTrajectoryEndPoint: [number, number, number] | null = useMemo(() => {
    if (!showStraightTrajectory || !lockOnTargetPos) return null;
    
    const dx = lockOnTargetPos[0] - pos[0];
    const dz = lockOnTargetPos[2] - pos[2];
    const dist = Math.sqrt(dx * dx + dz * dz);
    
    if (tangentOffset === "none" || dist < 0.5) {
      return lockOnTargetPos;
    }
    
    // Calculate tangent to touch edge of target
    const targetRadius = lockOnTargetType === "orb" ? 0.4 : 0.5;
    
    if (dist > targetRadius) {
      const tangentAngle = Math.asin(targetRadius / dist);
      const baseAngle = Math.atan2(dz, dx);
      const offsetAngle = tangentOffset === "left" ? -tangentAngle : tangentAngle;
      const finalAngle = baseAngle + offsetAngle;
      
      // Extend trajectory line past the target for visual clarity
      const extendedDist = dist * 1.5;
      return [
        pos[0] + Math.cos(finalAngle) * extendedDist,
        0.6,
        pos[2] + Math.sin(finalAngle) * extendedDist
      ];
    }
    
    return lockOnTargetPos;
  }, [showStraightTrajectory, lockOnTargetPos, pos, tangentOffset, lockOnTargetType]);
  
  const trajectoryPoints: [number, number, number][] = useMemo(() => {
    if (!showArcTrajectory || !lockOnTargetPos || !arcType) return [];
    
    const startX = pos[0];
    const startZ = pos[2];
    const endX = lockOnTargetPos[0];
    const endZ = lockOnTargetPos[2];
    
    const dx = endX - startX;
    const dz = endZ - startZ;
    const dist = Math.sqrt(dx * dx + dz * dz);
    
    if (dist < 0.5) return [];
    
    const points: [number, number, number][] = [];
    const numPoints = 25;
    
    if (arcType === "over") {
      for (let i = 0; i <= numPoints; i++) {
        const t = i / numPoints;
        const x = startX + dx * t;
        const z = startZ + dz * t;
        const y = 0.5 + Math.sin(t * Math.PI) * (dist * 0.4);
        points.push([x, y, z]);
      }
    } else {
      const midX = (startX + endX) / 2;
      const midZ = (startZ + endZ) / 2;
      const radius = dist / 2;
      
      const startAngle = Math.atan2(startZ - midZ, startX - midX);
      const endAngle = Math.atan2(endZ - midZ, endX - midX);
      
      let angleDiff = endAngle - startAngle;
      
      if (arcType === "left") {
        if (angleDiff < 0) angleDiff += Math.PI * 2;
      } else {
        if (angleDiff > 0) angleDiff -= Math.PI * 2;
      }
      
      for (let i = 0; i <= numPoints; i++) {
        const t = i / numPoints;
        const angle = startAngle + angleDiff * t;
        const x = midX + Math.cos(angle) * radius;
        const z = midZ + Math.sin(angle) * radius;
        points.push([x, 0.5, z]);
      }
    }
    
    return points;
  }, [showArcTrajectory, pos, arcType, lockOnTargetPos]);

  const handlePointerDown = (e: any) => {
    if (!playerEntity || launchCooldownRef.current) return;
    if (!isPlayerTurn && !isFreeForAll) return;
    if (firstPersonView || overShoulderView || birdsEyeView) return;
    if (playerEntity.isKnockedOut) return;
    
    if (e.stopPropagation) e.stopPropagation();
    
    const rect = gl.domElement.getBoundingClientRect();
    const clientX = e.clientX ?? e.nativeEvent?.clientX ?? 0;
    const clientY = e.clientY ?? e.nativeEvent?.clientY ?? 0;
    const x = ((clientX - rect.left) / rect.width) * 2 - 1;
    const y = -((clientY - rect.top) / rect.height) * 2 + 1;
    
    const raycaster = new THREE.Raycaster();
    raycaster.setFromCamera(new THREE.Vector2(x, y), camera);
    const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
    const intersection = new THREE.Vector3();
    raycaster.ray.intersectPlane(plane, intersection);
    
    if (intersection) {
      setDragStart([intersection.x, intersection.z]);
      setDragEnd([intersection.x, intersection.z]);
      setIsDragging(true);
      setIsAiming(true);
      console.log("Drag started at:", intersection.x.toFixed(2), intersection.z.toFixed(2));
    }
  };

  return (
    <group>
      <group ref={meshRef} position={pos} visible={!firstPersonView && !playerEntity.isKnockedOut && !playerEntity.isRespawning}>
        <ZoogiModelSwitch zoogiId={playerEntity.zoogi.id} hasShield={playerEntity.hasShield} hasSpawnImmunity={playerEntity.spawnImmunity} color={playerEntity.zoogi.color} customModelUrl={playerEntity.customModelUrl} isPlayer={true} />
        <StatusLooks ricochet={playerEntity.larsRicochetBoost > 1} bound={boundNow(playerEntity)} />
        {(playerEntity.boltPhasingUntil || 0) > Date.now() && (
          <mesh>
            <sphereGeometry args={[1.15, 16, 16]} />
            <meshBasicMaterial color="#FDE047" transparent opacity={0.35} />
          </mesh>
        )}
        {playerEntity.speedBoost > 1 && (
          <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]}>
            <ringGeometry args={[0.9, 1.15, 20]} />
            <meshBasicMaterial color="#e5e7eb" transparent opacity={0.8} />
          </mesh>
        )}
      </group>
      
      {/* Floating player icon */}
      {!firstPersonView && !playerEntity.isKnockedOut && !playerEntity.isRespawning && (
        <Html
          position={[pos[0], pos[1] + 3.2, pos[2]]}
          center
          style={{ pointerEvents: 'none' }}
        >
          <div
            style={{
              width: '32px',
              height: '32px',
              borderRadius: '50%',
              backgroundColor: playerEntity.zoogi.color || '#6366f1',
              border: '2px solid white',
              boxShadow: '0 2px 8px rgba(0,0,0,0.4)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              color: 'white',
              fontWeight: 'bold',
              fontSize: '14px',
              textShadow: '0 1px 2px rgba(0,0,0,0.5)',
            }}
          >
            {playerEntity.zoogi.name?.charAt(0)?.toUpperCase() || 'P'}
          </div>
        </Html>
      )}
      
      {/* Invisible hitbox for drag interaction - disabled in first person, over shoulder, and birds eye views */}
      {(isPlayerTurn || isFreeForAll) && !launchCooldownRef.current && !playerEntity.isKnockedOut && !playerEntity.isRespawning && !firstPersonView && !overShoulderView && !birdsEyeView && (
        <mesh 
          position={pos} 
          onPointerDown={handlePointerDown}
        >
          <sphereGeometry args={[1.2, 16, 16]} />
          <meshBasicMaterial transparent opacity={0} />
        </mesh>
      )}
      
      <TurnStunMarker entity={playerEntity} />

      <PinpointStroke
        points={trajectoryPoints}
        color={zoogiColor}
        pinpoint={playerEntity.zoogi.id === "pinpoint"}
      />

      {showStraightTrajectory && straightTrajectoryEndPoint && (
        <PinpointStroke
          points={[[pos[0], 0.6, pos[2]], straightTrajectoryEndPoint]}
          color={tangentOffset !== "none" ? "#F97316" : zoogiColor}
          pinpoint={playerEntity.zoogi.id === "pinpoint"}
        />
      )}

      {isDragging && playerEntity.zoogi.id === "pinpoint" && (
        <PinpointDragBeam origin={pos} dx={launchDx} dz={launchDz} multiplier={2} />
      )}

      {isDragging && playerEntity.zoogi.id !== "pinpoint" && (
        <>
          {/* Dynamic transparent blue arrow trajectory indicator */}
          {(() => {
            const rawLength = Math.sqrt(launchDx * launchDx + launchDz * launchDz) * launchMultiplier;
            const arrowLength = Math.min(rawLength, 8);
            const arrowRotation = Math.atan2(launchDz, launchDx);
            const shaftLength = arrowLength * 0.7;
            const headSize = Math.min(0.5, arrowLength * 0.15);
            
            return arrowLength > 0.3 ? (
              <group position={[pos[0], 0.15, pos[2]]} rotation={[0, -arrowRotation + Math.PI / 2, 0]}>
                <mesh position={[0, 0, shaftLength / 2]} rotation={[-Math.PI / 2, 0, 0]}>
                  <planeGeometry args={[0.25, shaftLength]} />
                  <meshBasicMaterial color="#3B82F6" transparent opacity={0.4} side={THREE.DoubleSide} />
                </mesh>
                <mesh position={[0, 0, shaftLength + headSize / 2]} rotation={[Math.PI / 2, 0, 0]}>
                  <coneGeometry args={[headSize, headSize * 1.5, 3]} />
                  <meshBasicMaterial color="#3B82F6" transparent opacity={0.5} />
                </mesh>
              </group>
            ) : null;
          })()}
          <mesh position={[pos[0], 0.1, pos[2]]} rotation={[-Math.PI / 2, 0, 0]}>
            <ringGeometry args={[0.6, 0.8, 32]} />
            <meshBasicMaterial color="#3B82F6" transparent opacity={0.5} />
          </mesh>
          {dragPower > 0.1 && (
            <Html
              position={[pos[0] + launchDx * 0.5, 1.5, pos[2] + launchDz * 0.5]}
              center
              style={{ pointerEvents: 'none' }}
            >
              <div
                style={{
                  backgroundColor: powerColor,
                  color: 'white',
                  padding: '4px 12px',
                  borderRadius: '4px',
                  fontWeight: 'bold',
                  fontSize: '14px',
                  textShadow: '0 1px 2px rgba(0,0,0,0.5)',
                  whiteSpace: 'nowrap'
                }}
              >
                {powerLevel}
              </div>
            </Html>
          )}
        </>
      )}

      {(isPlayerTurn || isFreeForAll) && !isDragging && !playerEntity.isKnockedOut && !playerEntity.isRespawning && (
        <mesh position={[pos[0], 0.05, pos[2]]} rotation={[-Math.PI / 2, 0, 0]}>
          <ringGeometry args={[0.7, 0.9, 32]} />
          <meshBasicMaterial color={isFreeForAll ? "#A855F7" : "#00FF00"} transparent opacity={0.5} />
        </mesh>
      )}

      <pointLight
        position={[pos[0], pos[1] + 1, pos[2]]}
        color={playerEntity.zoogi.color}
        intensity={0.5}
        distance={5}
      />
    </group>
  );
}

export function EnemyZoogi({ entityId }: { entityId: string }) {
  const meshRef = useRef<THREE.Group>(null);
  const aiTimerRef = useRef(0);
  const hasLaunchedRef = useRef(false);
  const prevTurnRef = useRef(false);
  const prevRoundRef = useRef(0);
  const prevSpeedRef = useRef(0);
  const lastLaunchTimeRef = useRef(0);
  const ffaCooldownRef = useRef(1.5 + Math.random() * 1.5);
  const prePowerRef = useRef<"pending" | "cast" | "done">("pending");
  const wolfTimerRef = useRef<number | null>(null);
  
  const { enemies, isPlayerTurn, turnIndex, currentRound, playerEntity, orbs, updateEnemy, endTurn, setMovementStopped, spawnWolfClones, gameMode, lockOnEnabled, lockOnTargetId, setLockOnTarget, aiControls } = useZoogiGame();
  
  const enemy = enemies.find(e => e.id === entityId);
  const myIndex = enemies.findIndex(e => e.id === entityId);
  const isFreeForAll = gameMode === "ringer_royale";
  const isMyTurn = isFreeForAll ? true : (!isPlayerTurn && turnIndex === myIndex && !enemy?.isStunned);
  
  useFrame((_, delta) => {
    if (!meshRef.current || !enemy || !playerEntity) return;
    
    meshRef.current.position.set(enemy.position[0], enemy.position[1], enemy.position[2]);
    
    const speed = Math.sqrt(enemy.velocity[0] ** 2 + enemy.velocity[2] ** 2);
    rollMarble(meshRef.current, enemy.position[0], enemy.position[2], getGlobalZoogiScale());
    
    meshRef.current.scale.set(1, 1, 1);
    
    const liveTurn = useZoogiGame.getState();
    const liveOwnsTurn = !isFreeForAll && !liveTurn.isPlayerTurn && liveTurn.turnIndex === myIndex;
    const liveIsMyTurn = liveOwnsTurn && !liveTurn.enemies[myIndex]?.isStunned;

    if (!isFreeForAll) {
      if (liveOwnsTurn && liveTurn.enemies[myIndex]?.isStunned && !prevTurnRef.current) {
        console.log(`Enemy ${enemy.zoogi.name} is stunned, auto-skipping turn`);
        setTimeout(() => {
          const live = useZoogiGame.getState();
          if (!live.isPlayerTurn && live.turnIndex === myIndex && live.enemies[myIndex]?.isStunned) {
            live.endTurn();
          }
        }, 300);
      }
      
      if (liveIsMyTurn && !prevTurnRef.current) {
        hasLaunchedRef.current = false;
        aiTimerRef.current = 0;
        prePowerRef.current = "pending";
        wolfTimerRef.current = null;
        console.log(`Enemy ${enemy.zoogi.name}'s turn starting`);
      }
      prevTurnRef.current = !liveTurn.isPlayerTurn && liveTurn.turnIndex === myIndex;
    }

    if (prevRoundRef.current !== currentRound) {
      prevRoundRef.current = currentRound;
      hasLaunchedRef.current = false;
      prevSpeedRef.current = 0;
      aiTimerRef.current = 0;
    }
    
    const canLaunchFFA = isFreeForAll && speed < 0.1 && !enemy.isStunned;
    const canLaunchTurnBased = liveIsMyTurn && !hasLaunchedRef.current;
    
    if (canLaunchFFA || canLaunchTurnBased) {
      aiTimerRef.current += delta;
      
      const launchDelay = isFreeForAll ? ffaCooldownRef.current * aiControls.reactionDelay : 0.8 * aiControls.reactionDelay;
      if (prePowerRef.current === "pending" && aiTimerRef.current >= 0.4) {
        const powerId = enemy.zoogi.id;
        if (powerId === "pinpoint") {
          useZoogiGame.getState().showAbilityNotice(`${enemy.zoogi.name} locks on!`);
          prePowerRef.current = "done";
        } else if (powerId === "wolfgang") {
          prePowerRef.current = "done";
        } else {
          // Cast on the next pass, after this frame's physics step, so a rolling
          // marble and the blast share the position that actually gets drawn.
          prePowerRef.current = "cast";
        }
      }
      if (aiTimerRef.current > launchDelay) {
        if (enemy.isStunned) {
          console.log(`Enemy ${enemy.zoogi.name} cannot launch - stunned!`);
          const live = useZoogiGame.getState();
          if (!isFreeForAll && !live.isPlayerTurn && live.turnIndex === myIndex) live.endTurn();
          return;
        }
        
        aiTimerRef.current = 0;
        hasLaunchedRef.current = true;
        if (enemy.zoogi.id === "wolfgang" && enemy.wolfgangAbilityUnlocked) {
          wolfTimerRef.current = 0;
        }
        if (isFreeForAll) {
          ffaCooldownRef.current = 1.5 + Math.random() * 1.5;
        }
        
        const ARENA_RADIUS = 18;
        const activeOrbs = orbs.filter(o => o.isActive);
        
        const getDistance = (p1: [number, number, number], p2: [number, number, number]) => 
          Math.sqrt((p1[0] - p2[0]) ** 2 + (p1[2] - p2[2]) ** 2);
        
        const getDistanceFromCenter = (pos: [number, number, number]) => 
          Math.sqrt(pos[0] ** 2 + pos[2] ** 2);
        
        const calculateHitProbability = (targetPos: [number, number, number], targetRadius: number) => {
          const dist = getDistance(enemy.position, targetPos);
          const angularSize = targetRadius / Math.max(dist, 0.5);
          const baseProbability = Math.min(1, angularSize * 3);
          const distancePenalty = Math.max(0, 1 - dist / 25);
          return baseProbability * (0.5 + distancePenalty * 0.5);
        };
        
        const isNearEdge = (pos: [number, number, number]) => 
          getDistanceFromCenter(pos) > ARENA_RADIUS - 3;
        
        const wouldPushSelfOffEdge = (launchDir: [number, number]) => {
          // Skip self-preservation check if selfPreservation is very low
          if (aiControls.selfPreservation < 0.2) return false;
          const selfRecoilDir = [-launchDir[0], -launchDir[1]];
          const projectedPos: [number, number, number] = [
            enemy.position[0] + selfRecoilDir[0] * 2 * aiControls.selfPreservation,
            0,
            enemy.position[2] + selfRecoilDir[1] * 2 * aiControls.selfPreservation
          ];
          return getDistanceFromCenter(projectedPos) > ARENA_RADIUS - (1 + aiControls.selfPreservation);
        };
        
        const playerDist = getDistance(enemy.position, playerEntity.position);
        const playerHitProb = calculateHitProbability(playerEntity.position, 0.5);
        const playerNearEdge = isNearEdge(playerEntity.position);
        // Massively prioritize players near edge for knockout potential - scaled by edge awareness
        const playerEdgeBonus = playerNearEdge ? (1 + 4.0 * aiControls.edgeAwareness) : 1;
        const playerProximityBonus = playerDist < 8 ? 1.6 : 1;
        // Higher base score for targeting players - knockouts are the goal
        const playerScore = 180 * playerHitProb * playerEdgeBonus * playerProximityBonus;
        
        interface ScoredTarget {
          type: 'player' | 'orb';
          position: [number, number, number];
          score: number;
          distance: number;
        }
        
        let targets: ScoredTarget[] = [{
          type: 'player',
          position: playerEntity.position,
          score: playerScore,
          distance: playerDist
        }];
        
        for (const orb of activeOrbs) {
          const orbDist = getDistance(enemy.position, orb.position);
          const orbHitProb = calculateHitProbability(orb.position, 0.4);
          const orbProximityBonus = Math.max(1, 2 - orbDist / 10);
          const orbDistFromCenter = getDistanceFromCenter(orb.position);
          const orbNearEdge = orbDistFromCenter > ARENA_RADIUS - 4;
          const orbEdgeBonus = orbNearEdge ? 2.5 : 1.0;
          const orbPointBonus = orb.points >= 10 ? 1.3 : 1.0;
          const orbBaseScore = 120;
          const orbScore = orbBaseScore * orbHitProb * orbProximityBonus * orbEdgeBonus * orbPointBonus;
          
          targets.push({
            type: 'orb',
            position: orb.position,
            score: orbScore,
            distance: orbDist
          });
        }
        
        // Sort based on AI target priority setting
        if (aiControls.targetPriority === 'closest') {
          // Target the closest entity first
          targets.sort((a, b) => a.distance - b.distance);
        } else if (aiControls.targetPriority === 'weakest') {
          // Prioritize targets near edge (most likely to knock off) - low distance from edge = highest priority
          targets.sort((a, b) => {
            const aDistFromEdge = ARENA_RADIUS - getDistanceFromCenter(a.position);
            const bDistFromEdge = ARENA_RADIUS - getDistanceFromCenter(b.position);
            return aDistFromEdge - bDistFromEdge; // smaller = closer to edge = higher priority
          });
        } else if (aiControls.targetPriority === 'random') {
          // Shuffle targets randomly
          for (let i = targets.length - 1; i > 0; i--) {
            const j = Math.floor(Math.random() * (i + 1));
            [targets[i], targets[j]] = [targets[j], targets[i]];
          }
        } else {
          // Default - use score-based sorting (strategic)
          targets.sort((a, b) => b.score - a.score);
        }
        
        let chosenTarget: ScoredTarget | null = null;
        for (const t of targets) {
          const dx = t.position[0] - enemy.position[0];
          const dz = t.position[2] - enemy.position[2];
          const dist = Math.sqrt(dx * dx + dz * dz);
          if (dist > 0.1) {
            const launchDir: [number, number] = [dx / dist, dz / dist];
            if (!wouldPushSelfOffEdge(launchDir)) {
              chosenTarget = t;
              break;
            }
          }
        }
        
        if (!chosenTarget) {
          if (activeOrbs.length > 0) {
            let nearestOrb = activeOrbs[0];
            let nearestDist = Infinity;
            for (const orb of activeOrbs) {
              const d = getDistance(enemy.position, orb.position);
              if (d < nearestDist) {
                nearestDist = d;
                nearestOrb = orb;
              }
            }
            const dx = nearestOrb.position[0] - enemy.position[0];
            const dz = nearestOrb.position[2] - enemy.position[2];
            const dist = Math.sqrt(dx * dx + dz * dz);
            if (dist > 0.5) {
              const launchSpeed = 0.6 + Math.random() * 0.3;
              const newVel: [number, number, number] = [
                (dx / dist) * launchSpeed,
                0,
                (dz / dist) * launchSpeed
              ];
              updateEnemy(entityId, { velocity: newVel });
              setMovementStopped(false);
              console.log(`Enemy ${enemy.zoogi.name} launching toward nearest orb (fallback)`);
              return;
            }
          }
          
          const centerDir = [-enemy.position[0], -enemy.position[2]];
          const centerDist = Math.sqrt(centerDir[0] ** 2 + centerDir[1] ** 2);
          if (centerDist > 0.1) {
            const safeSpeed = 0.25;
            const newVel: [number, number, number] = [
              (centerDir[0] / centerDist) * safeSpeed,
              0,
              (centerDir[1] / centerDist) * safeSpeed
            ];
            updateEnemy(entityId, { velocity: newVel });
            setMovementStopped(false);
            console.log(`Enemy ${enemy.zoogi.name} moving toward center`);
          } else {
            console.log(`Enemy ${enemy.zoogi.name} at center, looking for next opportunity`);
            const live = useZoogiGame.getState();
            if (!isFreeForAll && !live.isPlayerTurn && live.turnIndex === myIndex) live.endTurn();
          }
          return;
        }
        
        let dx = chosenTarget.position[0] - enemy.position[0];
        let dz = chosenTarget.position[2] - enemy.position[2];
        const distance = Math.sqrt(dx * dx + dz * dz);
        
        if (distance > 0.1) {
          const statMaxSpeed = 0.7 + (enemy.zoogi.stats.power / 100) * 0.4;
          const AI_MAX_LAUNCH_SPEED = Math.min(1.4, statMaxSpeed);
          const targetIsPlayer = chosenTarget.type === 'player';
          const targetNearEdge = isNearEdge(chosenTarget.position);
          
          // SMART TARGETING: When targeting player not near edge, aim to push them toward nearest edge
          if (targetIsPlayer && !targetNearEdge) {
            // Calculate direction from center to player (push direction toward edge)
            const playerFromCenterX = chosenTarget.position[0];
            const playerFromCenterZ = chosenTarget.position[2];
            const playerDistFromCenter = Math.sqrt(playerFromCenterX ** 2 + playerFromCenterZ ** 2);
            
            if (playerDistFromCenter > 0.5) {
              // Aim to hit player from the side that pushes them toward edge
              const pushDirX = playerFromCenterX / playerDistFromCenter;
              const pushDirZ = playerFromCenterZ / playerDistFromCenter;
              
              // Blend direct hit with edge-push angle (40% push bias)
              const directAngle = Math.atan2(dz, dx);
              const pushAngle = Math.atan2(pushDirZ, pushDirX);
              const blendedAngle = directAngle + (pushAngle - directAngle) * 0.4;
              
              dx = Math.cos(blendedAngle) * distance;
              dz = Math.sin(blendedAngle) * distance;
            }
          }
          
          // SMART ORB TARGETING: Aim to push orbs toward the ring edge for scoring
          if (!targetIsPlayer) {
            const orbFromCenterX = chosenTarget.position[0];
            const orbFromCenterZ = chosenTarget.position[2];
            const orbDistFromCenter = Math.sqrt(orbFromCenterX ** 2 + orbFromCenterZ ** 2);
            
            if (orbDistFromCenter > 1) {
              // Calculate push direction (from orb toward edge)
              const pushDirX = orbFromCenterX / orbDistFromCenter;
              const pushDirZ = orbFromCenterZ / orbDistFromCenter;
              
              // Blend direct hit with outward push (50% push bias for orbs)
              const directAngle = Math.atan2(dz, dx);
              const pushAngle = Math.atan2(pushDirZ, pushDirX);
              const blendedAngle = directAngle + (pushAngle - directAngle) * 0.5;
              
              dx = Math.cos(blendedAngle) * distance;
              dz = Math.sin(blendedAngle) * distance;
            }
          }
          
          // AGGRESSIVE AI - use high power for all targets
          let powerLevel: number;
          if (targetIsPlayer && targetNearEdge) {
            // Maximum power to knock player off edge!
            powerLevel = 0.95 + Math.random() * 0.05;
          } else if (targetIsPlayer) {
            // High power to push player toward edge
            powerLevel = 0.85 + Math.random() * 0.15;
          } else if (targetNearEdge) {
            // Maximum power for orbs near edge - knock them off!
            powerLevel = 0.9 + Math.random() * 0.1;
          } else {
            // High power for orbs - push them toward edge
            powerLevel = 0.8 + Math.random() * 0.15;
          }
          
          const distanceBonus = Math.min(0.15, distance / 40);
          // Apply AI power multiplier from controls
          const adjustedPower = powerLevel * aiControls.powerMultiplier;
          const launchSpeed = Math.min(AI_MAX_LAUNCH_SPEED * aiControls.powerMultiplier, AI_MAX_LAUNCH_SPEED * adjustedPower + distanceBonus);
          
          // Accuracy from AI controls - lower accuracy = more random angle offset
          const baseAccuracy = aiControls.accuracy;
          const pinpointAim = enemy.zoogi.id === "pinpoint";
          const accuracy = pinpointAim ? 1 : baseAccuracy + Math.random() * (1 - baseAccuracy) * 0.5;
          // Angle variance inversely proportional to accuracy setting
          const angleVariance = 0.3 * (1 - aiControls.accuracy);
          const angleOffset = pinpointAim ? 0 : (Math.random() - 0.5) * angleVariance;
          const angle = Math.atan2(dz, dx) + angleOffset;
          
          const newVel: [number, number, number] = [
            Math.cos(angle) * launchSpeed * accuracy,
            0,
            Math.sin(angle) * launchSpeed * accuracy
          ];
          updateEnemy(entityId, { velocity: newVel, lastHitByPlayer: false });
          setMovementStopped(false);
          
          
          console.log(`Enemy ${enemy.zoogi.name} targeting ${chosenTarget.type} with power ${(powerLevel * 100).toFixed(0)}% (score: ${chosenTarget.score.toFixed(1)})`);
        }
      }
    }
    
    if (wolfTimerRef.current !== null && speed > 0.05) {
      wolfTimerRef.current += delta;
      if (wolfTimerRef.current >= 0.4) {
        const live = useZoogiGame.getState().enemies.find((entry) => entry.id === entityId);
        if (live && meshRef.current) {
          meshRef.current.position.set(live.position[0], live.position[1], live.position[2]);
        }
        useZoogiGame.getState().useAiPower(entityId);
        wolfTimerRef.current = null;
      }
    }

    if (hasLaunchedRef.current && speed < 0.02 && prevSpeedRef.current >= 0.02) {
      hasLaunchedRef.current = false;
      aiTimerRef.current = 0;
      prePowerRef.current = "pending";
      wolfTimerRef.current = null;
      setMovementStopped(true);
      // A late frame after physics already handed the turn back must not skip
      // the human, and a power cast must not be what ends the turn.
      if (!isFreeForAll && stopFrameMayEndTurn(useZoogiGame.getState(), "enemy", myIndex)) {
        endTurn();
        console.log(`Enemy ${enemy.zoogi.name}'s turn ended`);
      }
    }
    prevSpeedRef.current = speed;
    
  });

  useFrame(() => {
    if (prePowerRef.current !== "cast") return;
    const state = useZoogiGame.getState();
    const live = state.enemies.find((entry) => entry.id === entityId);
    if (live && meshRef.current) {
      meshRef.current.position.set(live.position[0], live.position[1], live.position[2]);
    }
    state.useAiPower(entityId);
    prePowerRef.current = "done";
  }, 0.5);
  
  const isLockedOn = lockOnTargetId === entityId;
  
  const handleClick = (e: any) => {
    if (e.stopPropagation) e.stopPropagation();
    // Don't allow targeting other players in local multiplayer
    if (gameMode === "local_multiplayer") return;
    if (lockOnEnabled && enemy) {
      setLockOnTarget(entityId, "enemy");
      console.log("Locked onto enemy:", enemy.zoogi.name);
    }
  };
  
  if (!enemy) return null;

  return (
    <group>
      <group ref={meshRef} position={enemy.position} onClick={handleClick} visible={!enemy.isKnockedOut && !enemy.isRespawning}>
        <ZoogiModelSwitch zoogiId={enemy.zoogi.id} hasShield={false} hasSpawnImmunity={enemy.spawnImmunity} color={enemy.zoogi.color} />
        <StatusLooks ricochet={enemy.larsRicochetBoost > 1} bound={boundNow(enemy)} />
      </group>

      {/* Floating enemy icon */}
      {!enemy.isKnockedOut && !enemy.isRespawning && (
      <Html
        position={[enemy.position[0], enemy.position[1] + 3.2, enemy.position[2]]}
        center
        style={{ pointerEvents: 'none' }}
      >
        <div
          style={{
            width: '28px',
            height: '28px',
            borderRadius: '50%',
            backgroundColor: enemy.zoogi.color || '#ef4444',
            border: '2px solid #fca5a5',
            boxShadow: '0 2px 8px rgba(0,0,0,0.4)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: 'white',
            fontWeight: 'bold',
            fontSize: '12px',
            textShadow: '0 1px 2px rgba(0,0,0,0.5)',
          }}
        >
          {enemy.zoogi.name?.charAt(0)?.toUpperCase() || 'E'}
        </div>
        {(enemy.slowUntil || 0) > Date.now() && (
          <div style={{ marginTop: 4, background: "#D4C4B0", color: "#3f2e22", fontWeight: 700, fontSize: 11, padding: "2px 6px", borderRadius: 6 }}>
            Slowed
          </div>
        )}
        {enemy.isStunned && (
          <div style={{ marginTop: 4, background: "#FDE047", color: "#3f2e22", fontWeight: 700, fontSize: 11, padding: "2px 6px", borderRadius: 6 }}>
            Stunned
          </div>
        )}
      </Html>
      )}

      {isMyTurn && !isFreeForAll && (
        <mesh position={[enemy.position[0], 0.05, enemy.position[2]]} rotation={[-Math.PI / 2, 0, 0]}>
          <ringGeometry args={[0.7, 0.9, 32]} />
          <meshBasicMaterial color="#FF6600" transparent opacity={0.5} />
        </mesh>
      )}
      
      {isLockedOn && (
        <Glint
          position={[enemy.position[0], enemy.position[1] + 0.2, enemy.position[2]]}
          size={0.7}
          color="#d5e8ff"
        />
      )}
      
      <TurnStunMarker entity={enemy} />

      <pointLight
        position={[enemy.position[0], enemy.position[1] + 0.5, enemy.position[2]]}
        color={enemy.zoogi.color}
        intensity={0.3}
        distance={3}
      />
    </group>
  );
}

export function LocalMultiplayerZoogi({ playerIndex }: { playerIndex: number }) {
  const meshRef = useRef<THREE.Group>(null);
  const { 
    playerEntity, 
    enemies,
    orbs,
    currentLocalPlayerIndex,
    currentRound,
    localPlayers,
    updateLocalPlayerVelocity,
    endTurn,
    setMovementStopped,
    spawnWolfClones,
    gameMode,
    lockOnEnabled,
    lockOnTargetId,
    lockOnTargetType,
    arcType,
    straightMode,
    tangentOffset,
    firstPersonView,
    overShoulderView,
    birdsEyeView,
    setIsAiming
  } = useZoogiGame();
  const { camera, gl } = useThree();
  
  const [isDragging, setIsDragging] = useState(false);
  const [dragStart, setDragStart] = useState<[number, number]>([0, 0]);
  const [dragEnd, setDragEnd] = useState<[number, number]>([0, 0]);
  const hasLaunchedRef = useRef(false);
  const prevSpeedRef = useRef(0);
  const launchCooldownRef = useRef(false);
  const prevTurnRef = useRef(currentLocalPlayerIndex);
  
  const entity = playerIndex === 0 ? playerEntity : enemies[playerIndex - 1];
  const isMyTurn = gameMode === "local_multiplayer" && currentLocalPlayerIndex === playerIndex;
  const localPlayer = localPlayers[playerIndex];
  const playerColors = ["#3B82F6", "#EF4444", "#22C55E", "#A855F7"];
  
  useEffect(() => {
    launchCooldownRef.current = false;
    hasLaunchedRef.current = false;
    prevSpeedRef.current = 0;
  }, [currentRound]);

  useEffect(() => {
    if (currentLocalPlayerIndex === playerIndex && prevTurnRef.current !== playerIndex) {
      launchCooldownRef.current = false;
      hasLaunchedRef.current = false;
    }
    prevTurnRef.current = currentLocalPlayerIndex;
  }, [currentLocalPlayerIndex, playerIndex]);

  useEffect(() => {
    if (entity && isMyTurn) {
      const speed = Math.sqrt(entity.velocity[0] ** 2 + entity.velocity[2] ** 2);
      if (speed > 0.1 && prevSpeedRef.current < 0.1 && !hasLaunchedRef.current) {
        hasLaunchedRef.current = true;
      }
    }
  }, [entity?.velocity, isMyTurn]);

  useEffect(() => {
    const handlePointerMove = (e: PointerEvent) => {
      if (isDragging && entity) {
        const rect = gl.domElement.getBoundingClientRect();
        const x = ((e.clientX - rect.left) / rect.width) * 2 - 1;
        const y = -((e.clientY - rect.top) / rect.height) * 2 + 1;
        
        const raycaster = new THREE.Raycaster();
        raycaster.setFromCamera(new THREE.Vector2(x, y), camera);
        const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
        const intersection = new THREE.Vector3();
        raycaster.ray.intersectPlane(plane, intersection);
        
        if (intersection) {
          setDragEnd([intersection.x, intersection.z]);
        }
      }
    };

    const handlePointerUp = () => {
      if (isDragging && entity) {
        const dx = dragStart[0] - dragEnd[0];
        const dz = dragStart[1] - dragEnd[1];
        const rawPower = Math.sqrt(dx * dx + dz * dz);
        
        const isPinpointChar = entity.zoogi.id === "pinpoint";
        let adjustedDx = dx;
        let adjustedDz = dz;
        
        if (isPinpointChar && rawPower > 0) {
          const controlSensitivity = 0.6;
          const dampedPower = Math.pow(rawPower, 0.8) * controlSensitivity;
          const scaleFactor = dampedPower / rawPower;
          adjustedDx = dx * scaleFactor;
          adjustedDz = dz * scaleFactor;
        }
        
        const power = Math.min(Math.sqrt(adjustedDx * adjustedDx + adjustedDz * adjustedDz), 15);
        
        if (power > 1) {
          let vx = adjustedDx * LAUNCH_POWER_MULTIPLIER;
          let vz = adjustedDz * LAUNCH_POWER_MULTIPLIER;
          
          const launchSpeed = Math.sqrt(vx * vx + vz * vz);
          if (launchSpeed > MAX_LAUNCH_SPEED) {
            const scale = MAX_LAUNCH_SPEED / launchSpeed;
            vx *= scale;
            vz *= scale;
          }
          
          const newVelocity: [number, number, number] = [vx, 0, vz];
          updateLocalPlayerVelocity(playerIndex, newVelocity);
          hasLaunchedRef.current = true;
          launchCooldownRef.current = true;
          setMovementStopped(false);
          triggerLaunchFeel();
          triggerLaunchCameraEffect();
          console.log(`Local player ${playerIndex} launched with velocity:`, newVelocity);
          
        }
        
        clearAimCameraEffect();
        setIsDragging(false);
        setIsAiming(false);
      }
    };

    window.addEventListener("pointermove", handlePointerMove);
    window.addEventListener("pointerup", handlePointerUp);
    
    return () => {
      window.removeEventListener("pointermove", handlePointerMove);
      window.removeEventListener("pointerup", handlePointerUp);
    };
  }, [isDragging, dragStart, dragEnd, camera, gl, entity, updateLocalPlayerVelocity, setMovementStopped, spawnWolfClones, playerIndex, setIsAiming]);

  useFrame(() => {
    if (!meshRef.current || !entity) return;
    
    meshRef.current.position.set(
      entity.position[0],
      entity.position[1],
      entity.position[2]
    );
    
    const speed = Math.sqrt(entity.velocity[0] ** 2 + entity.velocity[2] ** 2);
    rollMarble(meshRef.current, entity.position[0], entity.position[2], getGlobalZoogiScale());
    
    meshRef.current.scale.set(1, 1, 1);
    
    if (hasLaunchedRef.current && speed < 0.02 && prevSpeedRef.current >= 0.02) {
      hasLaunchedRef.current = false;
      launchCooldownRef.current = false;
      setMovementStopped(true);
      if (stopFrameMayEndTurn(useZoogiGame.getState(), "local", playerIndex)) {
        console.log(`Local player ${playerIndex} turn ended`);
        endTurn();
      }
    }
    prevSpeedRef.current = speed;
  });

  if (!entity || localPlayer?.isEliminated) return null;

  const pos = entity.position;
  
  const isPinpoint = entity.zoogi.id === "pinpoint";
  const rawLaunchDx = dragStart[0] - dragEnd[0];
  const rawLaunchDz = dragStart[1] - dragEnd[1];
  const rawDragDist = Math.sqrt(rawLaunchDx * rawLaunchDx + rawLaunchDz * rawLaunchDz);
  
  let effectiveLaunchDx = rawLaunchDx;
  let effectiveLaunchDz = rawLaunchDz;
  if (isPinpoint && rawDragDist > 0) {
    const controlSensitivity = 0.6;
    const dampedPower = Math.pow(rawDragDist, 0.8) * controlSensitivity;
    const scaleFactor = dampedPower / rawDragDist;
    effectiveLaunchDx = rawLaunchDx * scaleFactor;
    effectiveLaunchDz = rawLaunchDz * scaleFactor;
  }
  
  const launchDx = effectiveLaunchDx;
  const launchDz = effectiveLaunchDz;
  const launchMultiplier = isPinpoint ? 4 : 2;
  const effectiveDist = Math.sqrt(launchDx * launchDx + launchDz * launchDz);
  const dragPower = Math.min(effectiveDist, 15);
  const powerColor = dragPower < 2 ? "#22C55E" : dragPower < 4 ? "#EAB308" : "#EF4444";
  const zoogiColor = ZOOGI_TRAJECTORY_COLORS[entity.zoogi.id] || entity.zoogi.color;
  
  const showArcTrajectoryLocal = arcType !== null && lockOnEnabled && lockOnTargetId;
  
  let lockOnTargetPosLocal: [number, number, number] | null = null;
  if (lockOnEnabled && lockOnTargetId) {
    if (lockOnTargetType === "orb") {
      const targetOrb = orbs.find(o => o.id === lockOnTargetId && o.isActive);
      if (targetOrb) lockOnTargetPosLocal = targetOrb.position;
    } else if (lockOnTargetType === "enemy") {
      const targetEnemy = enemies.find(e => e.id === lockOnTargetId);
      if (targetEnemy) lockOnTargetPosLocal = targetEnemy.position;
    }
  }
  
  const showStraightTrajectoryLocal = straightMode && lockOnEnabled && lockOnTargetId && lockOnTargetPosLocal;
  
  // Calculate tangent trajectory end point for off-center hits
  const straightTrajectoryEndPointLocal: [number, number, number] | null = useMemo(() => {
    if (!showStraightTrajectoryLocal || !lockOnTargetPosLocal) return null;
    
    const dx = lockOnTargetPosLocal[0] - pos[0];
    const dz = lockOnTargetPosLocal[2] - pos[2];
    const dist = Math.sqrt(dx * dx + dz * dz);
    
    if (tangentOffset === "none" || dist < 0.5) {
      return lockOnTargetPosLocal;
    }
    
    // Calculate tangent to touch edge of target
    const targetRadius = lockOnTargetType === "orb" ? 0.4 : 0.5;
    
    if (dist > targetRadius) {
      const tangentAngle = Math.asin(targetRadius / dist);
      const baseAngle = Math.atan2(dz, dx);
      const offsetAngle = tangentOffset === "left" ? -tangentAngle : tangentAngle;
      const finalAngle = baseAngle + offsetAngle;
      
      // Extend trajectory line past the target for visual clarity
      const extendedDist = dist * 1.5;
      return [
        pos[0] + Math.cos(finalAngle) * extendedDist,
        0.6,
        pos[2] + Math.sin(finalAngle) * extendedDist
      ];
    }
    
    return lockOnTargetPosLocal;
  }, [showStraightTrajectoryLocal, lockOnTargetPosLocal, pos, tangentOffset, lockOnTargetType]);
  
  const trajectoryPoints: [number, number, number][] = useMemo(() => {
    if (!showArcTrajectoryLocal || !lockOnTargetPosLocal || !arcType) return [];
    
    const startX = pos[0];
    const startZ = pos[2];
    const endX = lockOnTargetPosLocal[0];
    const endZ = lockOnTargetPosLocal[2];
    
    const dx = endX - startX;
    const dz = endZ - startZ;
    const dist = Math.sqrt(dx * dx + dz * dz);
    
    if (dist < 0.5) return [];
    
    const points: [number, number, number][] = [];
    const numPoints = 25;
    
    if (arcType === "over") {
      for (let i = 0; i <= numPoints; i++) {
        const t = i / numPoints;
        const x = startX + dx * t;
        const z = startZ + dz * t;
        const y = 0.5 + Math.sin(t * Math.PI) * (dist * 0.4);
        points.push([x, y, z]);
      }
    } else {
      const midX = (startX + endX) / 2;
      const midZ = (startZ + endZ) / 2;
      const radius = dist / 2;
      
      const startAngle = Math.atan2(startZ - midZ, startX - midX);
      const endAngle = Math.atan2(endZ - midZ, endX - midX);
      
      let angleDiff = endAngle - startAngle;
      
      if (arcType === "left") {
        if (angleDiff < 0) angleDiff += Math.PI * 2;
      } else {
        if (angleDiff > 0) angleDiff -= Math.PI * 2;
      }
      
      for (let i = 0; i <= numPoints; i++) {
        const t = i / numPoints;
        const angle = startAngle + angleDiff * t;
        const x = midX + Math.cos(angle) * radius;
        const z = midZ + Math.sin(angle) * radius;
        points.push([x, 0.5, z]);
      }
    }
    
    return points;
  }, [showArcTrajectoryLocal, pos, arcType, lockOnTargetPosLocal]);

  const handlePointerDown = (e: any) => {
    if (!entity || launchCooldownRef.current) return;
    if (!isMyTurn) return;
    if (firstPersonView || overShoulderView || birdsEyeView) return;
    
    if (e.stopPropagation) e.stopPropagation();
    
    const rect = gl.domElement.getBoundingClientRect();
    const clientX = e.clientX ?? e.nativeEvent?.clientX ?? 0;
    const clientY = e.clientY ?? e.nativeEvent?.clientY ?? 0;
    const x = ((clientX - rect.left) / rect.width) * 2 - 1;
    const y = -((clientY - rect.top) / rect.height) * 2 + 1;
    
    const raycaster = new THREE.Raycaster();
    raycaster.setFromCamera(new THREE.Vector2(x, y), camera);
    const plane = new THREE.Plane(new THREE.Vector3(0, 1, 0), 0);
    const intersection = new THREE.Vector3();
    raycaster.ray.intersectPlane(plane, intersection);
    
    if (intersection) {
      setDragStart([intersection.x, intersection.z]);
      setDragEnd([intersection.x, intersection.z]);
      setIsDragging(true);
      setIsAiming(true);
      console.log(`Local player ${playerIndex} drag started at:`, intersection.x.toFixed(2), intersection.z.toFixed(2));
    }
  };

  // In local multiplayer, only show the player whose turn it is
  // Other players are hidden at their spawn points until their turn
  if (gameMode === "local_multiplayer" && !isMyTurn) {
    return null;
  }

  return (
    <group>
      <group ref={meshRef} position={pos} visible={!entity.isKnockedOut && !entity.isRespawning}>
        <ZoogiModelSwitch zoogiId={entity.zoogi.id} hasShield={false} color={entity.zoogi.color} />
        <StatusLooks ricochet={entity.larsRicochetBoost > 1} bound={boundNow(entity)} />
      </group>
      
      {/* Floating local player icon */}
      <Html
        position={[pos[0], pos[1] + 1.8, pos[2]]}
        center
        style={{ pointerEvents: 'none' }}
      >
        <div
          style={{
            width: '30px',
            height: '30px',
            borderRadius: '50%',
            backgroundColor: playerColors[playerIndex] || '#6366f1',
            border: isMyTurn ? '3px solid #fbbf24' : '2px solid white',
            boxShadow: isMyTurn ? '0 0 12px rgba(251, 191, 36, 0.6)' : '0 2px 8px rgba(0,0,0,0.4)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            color: 'white',
            fontWeight: 'bold',
            fontSize: '13px',
            textShadow: '0 1px 2px rgba(0,0,0,0.5)',
          }}
        >
          P{playerIndex + 1}
        </div>
      </Html>
      
      {/* Invisible hitbox for drag interaction - disabled in first person, over shoulder, and birds eye views */}
      {isMyTurn && !launchCooldownRef.current && !firstPersonView && !overShoulderView && !birdsEyeView && (
        <mesh 
          position={pos} 
          onPointerDown={handlePointerDown}
        >
          <sphereGeometry args={[1.2, 16, 16]} />
          <meshBasicMaterial transparent opacity={0} />
        </mesh>
      )}
      
      {isMyTurn && (
        <mesh position={[pos[0], 0.05, pos[2]]} rotation={[-Math.PI / 2, 0, 0]}>
          <ringGeometry args={[0.7, 0.9, 32]} />
          <meshBasicMaterial color={playerColors[playerIndex]} transparent opacity={0.6} />
        </mesh>
      )}

      {/* Only show trajectory when it's this player's turn */}
      {isMyTurn && (
        <PinpointStroke points={trajectoryPoints} color={zoogiColor} pinpoint={isPinpoint} />
      )}

      {isMyTurn && showStraightTrajectoryLocal && straightTrajectoryEndPointLocal && (
        <PinpointStroke
          points={[[pos[0], 0.6, pos[2]], straightTrajectoryEndPointLocal]}
          color={tangentOffset !== "none" ? "#F97316" : zoogiColor}
          pinpoint={isPinpoint}
        />
      )}

      {isMyTurn && isDragging && isPinpoint && (
        <PinpointDragBeam origin={pos} dx={launchDx} dz={launchDz} multiplier={launchMultiplier} />
      )}

      {isMyTurn && isDragging && !isPinpoint && (
        <>
          {/* Dynamic transparent blue arrow */}
          {(() => {
            const rawLength = Math.sqrt(launchDx * launchDx + launchDz * launchDz) * launchMultiplier;
            const arrowLength = Math.min(rawLength, 8);
            const arrowRotation = Math.atan2(launchDz, launchDx);
            const shaftLength = arrowLength * 0.7;
            const headSize = Math.min(0.5, arrowLength * 0.15);
            
            return arrowLength > 0.3 ? (
              <group position={[pos[0], 0.15, pos[2]]} rotation={[0, -arrowRotation + Math.PI / 2, 0]}>
                <mesh position={[0, 0, shaftLength / 2]} rotation={[-Math.PI / 2, 0, 0]}>
                  <planeGeometry args={[0.25, shaftLength]} />
                  <meshBasicMaterial color="#3B82F6" transparent opacity={0.4} side={THREE.DoubleSide} />
                </mesh>
                <mesh position={[0, 0, shaftLength + headSize / 2]} rotation={[Math.PI / 2, 0, 0]}>
                  <coneGeometry args={[headSize, headSize * 1.5, 3]} />
                  <meshBasicMaterial color="#3B82F6" transparent opacity={0.5} />
                </mesh>
              </group>
            ) : null;
          })()}
          <mesh position={[pos[0], 0.1, pos[2]]} rotation={[-Math.PI / 2, 0, 0]}>
            <ringGeometry args={[0.6, 0.8, 32]} />
            <meshBasicMaterial color="#3B82F6" transparent opacity={0.5} />
          </mesh>
        </>
      )}

      <TurnStunMarker entity={entity} />

      <pointLight
        position={[pos[0], pos[1] + 0.5, pos[2]]}
        color={playerColors[playerIndex]}
        intensity={isMyTurn ? 0.6 : 0.3}
        distance={3}
      />
    </group>
  );
}

for (const modelUrl of zoogiModelPreloadUrls()) {
  useGLTF.preload(modelUrl);
}

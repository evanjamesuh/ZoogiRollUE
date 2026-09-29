import { Canvas, useThree, useFrame } from "@react-three/fiber";
import { Component, ReactNode, Suspense } from "react";
import { Arena } from "./Arena";
import { ColliderDebug } from "./ColliderDebug";
import { PlayerZoogi, EnemyZoogi, LocalMultiplayerZoogi } from "./Zoogi";
import { OrbManager } from "./Orb";
import { PhysicsManager } from "./PhysicsManager";
import { GameUI } from "./GameUI";
import { Lights } from "./Lights";
import { GameCamera } from "./GameCamera";
import { WolfClones, MotionTrails, LaunchBurst, WindParticles, ImpactSparks, CollisionBurstEffects } from "./Effects";
import { ExplosionBlast, PowerUnlockFlash, ShadowPulse, StunBurst } from "./PowerEffects";
import { OrbCaptureEffects } from "./OrbCaptureEffect";
import { FallingEntities } from "./FallingEntities";
import { DeveloperMoveControls } from "./DeveloperMoveControls";
import { Tutorial } from "./Tutorial";
import { ArcPeakOverlay } from "./ArcPeakOverlay";
import { HitEffects, ScreenFlash, FireBursts, WallSparks, CartoonExplosions, CartoonStarbursts, CartoonSparks } from "./GameFeelEffects";
import { ArcSelector } from "./ArcSelector";
import { EditorPlacedModels } from "./EditorPlacedModels";
import { TransformGizmo } from "./TransformGizmo";
import { WallSegmentGizmo } from "./WallSegmentGizmo";
import { InnerWallSegmentGizmo } from "./InnerWallSegmentGizmo";
import { resolveUnlockSpot, useZoogiGame } from "@/lib/stores/useZoogiGame";
import { Sky, Environment } from "@react-three/drei";
import * as THREE from "three";
import { PostFX } from "./PostFX";
import { ImpactFX } from "./ImpactFX";
import { useCallback, useEffect, useRef, useMemo, useState } from "react";
import { triggerArcPeakCameraEffect, triggerArcPeakFreezeOnly, clearArcPeakCameraEffect } from "@/lib/stores/useCameraEffects";
import { useMapDecorations } from "@/hooks/useMapDecorations";
import { canvasPixelRatio, isMobileGraphics } from "@/lib/mobileGraphics";

function GradientSky() {
  const meshRef = useRef<THREE.Mesh>(null);
  const { camera } = useThree();
  const shaderData = useMemo(() => ({
    uniforms: {
      topColor: { value: new THREE.Color('#4DC8FF') },
      bottomColor: { value: new THREE.Color('#B8E8FF') },
    },
    vertexShader: `
      varying vec3 vWorldPosition;
      void main() {
        vec4 worldPosition = modelMatrix * vec4(position, 1.0);
        vWorldPosition = worldPosition.xyz;
        gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
      }
    `,
    fragmentShader: `
      uniform vec3 topColor;
      uniform vec3 bottomColor;
      varying vec3 vWorldPosition;
      void main() {
        float h = normalize(vWorldPosition).y;
        float t = clamp(h * 0.5 + 0.5, 0.0, 1.0);
        gl_FragColor = vec4(mix(bottomColor, topColor, t), 1.0);
      }
    `,
  }), []);

  useFrame(() => {
    if (meshRef.current) {
      meshRef.current.position.copy(camera.position);
    }
  });

  return (
    <mesh ref={meshRef} scale={[500, 500, 500]}>
      <sphereGeometry args={[1, 32, 32]} />
      <shaderMaterial
        args={[shaderData]}
        side={THREE.BackSide}
        depthWrite={false}
      />
    </mesh>
  );
}

function ArcPeakOverlayWrapper() {
  const arcPeakEffectActive = useZoogiGame(state => state.arcPeakEffectActive);
  const clearArcPeakEffect = useZoogiGame(state => state.clearArcPeakEffect);
  const birdsEyeView = useZoogiGame(state => state.birdsEyeView);
  const effectTriggeredRef = useRef(false);
  
  useEffect(() => {
    if (arcPeakEffectActive && !effectTriggeredRef.current) {
      console.log("Arc peak effect activated - switching to target first-person view");
      // In birds eye view: freeze only (no camera zoom/tilt), otherwise full camera effect
      if (birdsEyeView) {
        triggerArcPeakFreezeOnly();
      } else {
        triggerArcPeakCameraEffect();
      }
      effectTriggeredRef.current = true;
    } else if (!arcPeakEffectActive) {
      effectTriggeredRef.current = false;
    }
  }, [arcPeakEffectActive, birdsEyeView]);
  
  const handleComplete = useCallback(() => {
    console.log("Arc peak effect complete - returning to normal camera and resuming physics");
    clearArcPeakEffect();
    clearArcPeakCameraEffect();
  }, [clearArcPeakEffect]);
  
  return (
    <ArcPeakOverlay 
      isActive={arcPeakEffectActive} 
      onComplete={handleComplete}
    />
  );
}

const unlockProbe = new THREE.Vector3();

function exitIsOnScreen(camera: THREE.Camera, position: [number, number, number]): boolean {
  camera.updateWorldMatrix(true, false);
  unlockProbe.set(position[0], Math.max(position[1], 0.5) + 0.9, position[2]);
  const dist = unlockProbe.distanceTo(camera.position);
  if (dist < 1.2 || dist > 36) return false;
  unlockProbe.project(camera);
  return unlockProbe.z >= -1 && unlockProbe.z <= 0.98 && Math.abs(unlockProbe.x) < 0.78 && Math.abs(unlockProbe.y) < 0.7;
}

function MatchUnlockFlash({
  flash,
}: {
  flash: { id: string; position: [number, number, number]; startTime: number; color: string; anchorId: string };
}) {
  const camera = useThree((state) => state.camera);
  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const enemies = useZoogiGame((state) => state.enemies);
  const spot = useRef<[number, number, number] | null>(null);
  const [, setVersion] = useState(0);
  const anchor = playerEntity?.id === flash.anchorId
    ? playerEntity
    : enemies.find((enemy) => enemy.id === flash.anchorId) ?? null;

  useFrame(() => {
    if (spot.current) return;
    const state = useZoogiGame.getState();
    const live = state.playerEntity?.id === flash.anchorId
      ? state.playerEntity
      : state.enemies.find((enemy) => enemy.id === flash.anchorId) ?? null;
    const anchorPos = live ? [live.position[0], live.position[1], live.position[2]] as [number, number, number] : null;
    const onScreen = exitIsOnScreen(camera, flash.position);
    spot.current = resolveUnlockSpot(flash.position, anchorPos, onScreen);
    setVersion((version) => version + 1);
  });

  // Until the follow camera has been sampled, keep the burst on the unlocking marble
  // so an exit point behind the camera cannot be the only thing we draw.
  const position = spot.current ?? resolveUnlockSpot(
    flash.position,
    anchor ? [anchor.position[0], anchor.position[1], anchor.position[2]] : null,
    false,
  );

  return (
    <PowerUnlockFlash
      position={position}
      startTime={flash.startTime}
      radius={5.6}
      color={flash.color}
    />
  );
}

function MatchPowerVisuals() {
  const showExplosion = useZoogiGame((state) => state.showExplosion);
  const powerUnlocks = useZoogiGame((state) => state.powerUnlocks);
  const now = Date.now();

  return (
    <>
      {showExplosion && showExplosion.color === "shadow" && (
        <ShadowPulse
          key={showExplosion.timestamp}
          position={showExplosion.position}
          startTime={showExplosion.timestamp}
          radius={showExplosion.radius ?? 4.5}
        />
      )}
      {showExplosion && showExplosion.color === "yellow" && (
        <StunBurst
          key={showExplosion.timestamp}
          position={showExplosion.position}
          startTime={showExplosion.timestamp}
          radius={showExplosion.radius ?? 8}
        />
      )}
      {showExplosion && showExplosion.color !== "yellow" && showExplosion.color !== "shadow" && (
        <ExplosionBlast
          key={showExplosion.timestamp}
          position={showExplosion.position}
          startTime={showExplosion.timestamp}
          radius={showExplosion.radius ?? 8}
        />
      )}
      {powerUnlocks.filter((flash) => now - flash.startTime < 4000).map((flash) => (
        <MatchUnlockFlash key={flash.id} flash={flash} />
      ))}
    </>
  );
}

class OptionalSceneBoundary extends Component<{ children: ReactNode }, { hasError: boolean }> {
  state = { hasError: false };
  static getDerivedStateFromError() {
    return { hasError: true };
  }
  render() {
    return this.state.hasError ? null : this.props.children;
  }
}

export function Game() {
  const { enemies, selectedMap, gameMode, localPlayers } = useZoogiGame();
  const [graphics] = useState(() => ({
    mobile: isMobileGraphics(),
    dpr: canvasPixelRatio(),
  }));
  
  useMapDecorations();

  const skySettings = {
    grass: { sunPosition: [100, 20, 100] as [number, number, number] },
    ice: { sunPosition: [100, 50, 100] as [number, number, number] },
    lava: { sunPosition: [100, 5, 100] as [number, number, number] },
    space: { sunPosition: [100, 80, 100] as [number, number, number] },
    saturn: { sunPosition: [100, 60, 100] as [number, number, number] },
    neon: { sunPosition: [40, 30, 80] as [number, number, number] }
  };

  const currentSky = skySettings[selectedMap || "grass"] || skySettings.grass;

  return (
    <>
      <Canvas
        shadows={!graphics.mobile}
        dpr={graphics.dpr}
        camera={{
          position: [0, 25, 30],
          fov: 50,
          near: 0.1,
          far: 1000
        }}
        gl={{
          antialias: !graphics.mobile,
          powerPreference: "default",
          toneMapping: THREE.ACESFilmicToneMapping,
          toneMappingExposure: 1.0,
          outputColorSpace: THREE.SRGBColorSpace
        }}
        style={{ position: "absolute", inset: 0 }}
      >
        <color attach="background" args={[selectedMap === "neon" ? "#070814" : (selectedMap === "space" || selectedMap === "saturn") ? "#0a0a1a" : selectedMap === "ice" ? "#87CEEB" : "#1a1a2e"]} />
        
        {selectedMap === "ice" && <fog attach="fog" args={['#c8e6f8', 60, 150]} />}
        
        <OptionalSceneBoundary>
          <Suspense fallback={null}>
            {selectedMap === "ice" && <GradientSky />}
            {selectedMap !== "ice" && selectedMap !== "neon" && <Sky sunPosition={currentSky.sunPosition} />}
            {selectedMap !== "ice" && selectedMap !== "neon" && <Environment preset="sunset" background={false} />}
          </Suspense>
        </OptionalSceneBoundary>

          <Lights />
          
          <Arena theme={selectedMap || "grass"} />
          <ColliderDebug />
          
          <EditorPlacedModels />
          
          {/* Reference character sphere for Map Editor scale reference */}
          {gameMode === "map_editor" && (
            <>
              <mesh position={[0, 1.2, 0]}>
                <sphereGeometry args={[1.2, 32, 32]} />
                <meshStandardMaterial color="#6366f1" metalness={0.3} roughness={0.4} />
              </mesh>
              <TransformGizmo />
              <WallSegmentGizmo />
              <InnerWallSegmentGizmo />
            </>
          )}
          
          {gameMode !== "map_editor" && (
            gameMode === "local_multiplayer" ? (
              <>
                {localPlayers.map((_, index) => (
                  <LocalMultiplayerZoogi key={`local-${index}`} playerIndex={index} />
                ))}
              </>
            ) : (
              <>
                <PlayerZoogi />
                {enemies.map((enemy) => (
                  <EnemyZoogi key={enemy.id} entityId={enemy.id} />
                ))}
              </>
            )
          )}
          
          {gameMode !== "map_editor" && <OrbManager />}
          
          <WolfClones />
          <MatchPowerVisuals />
          <OrbCaptureEffects />
          
          <FallingEntities />
          
          <MotionTrails />
          <LaunchBurst />
          <WindParticles />
          <ImpactSparks />
          <CollisionBurstEffects />
          
          <HitEffects />
          <ScreenFlash />
          <FireBursts />
          <WallSparks />
          <CartoonExplosions />
          <CartoonStarbursts />
          <CartoonSparks />
          
          <ArcSelector />
          
          <ImpactFX />
          <PostFX />
          
          <PhysicsManager />
          
          <DeveloperMoveControls key={`dev-controls-${selectedMap || "grass"}`} />
          
          <GameCamera />
      </Canvas>
      
      <GameUI />
      <Tutorial />
      <ArcPeakOverlayWrapper />
    </>
  );
}

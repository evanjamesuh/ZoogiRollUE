import { Canvas, useThree, useFrame } from "@react-three/fiber";
import { Suspense } from "react";
import { Arena } from "./Arena";
import { PlayerZoogi, EnemyZoogi, LocalMultiplayerZoogi } from "./Zoogi";
import { OrbManager } from "./Orb";
import { PhysicsManager } from "./PhysicsManager";
import { GameUI } from "./GameUI";
import { Lights } from "./Lights";
import { GameCamera } from "./GameCamera";
import { WolfClones, ExplosionEffect, MotionTrails, LaunchBurst, WindParticles, ImpactSparks, CollisionBurstEffects } from "./Effects";
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
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { Sky, Environment } from "@react-three/drei";
import * as THREE from "three";
import { EffectComposer, Bloom } from "@react-three/postprocessing";
import { useCallback, useEffect, useRef, useMemo } from "react";
import { triggerArcPeakCameraEffect, triggerArcPeakFreezeOnly, clearArcPeakCameraEffect } from "@/lib/stores/useCameraEffects";
import { useMapDecorations } from "@/hooks/useMapDecorations";

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

export function Game() {
  const { enemies, selectedMap, gameMode, localPlayers } = useZoogiGame();
  
  useMapDecorations();

  const skySettings = {
    grass: { sunPosition: [100, 20, 100] as [number, number, number] },
    ice: { sunPosition: [100, 50, 100] as [number, number, number] },
    lava: { sunPosition: [100, 5, 100] as [number, number, number] },
    space: { sunPosition: [100, 80, 100] as [number, number, number] },
    saturn: { sunPosition: [100, 60, 100] as [number, number, number] }
  };

  const currentSky = skySettings[selectedMap || "grass"] || skySettings.grass;

  return (
    <>
      <Canvas
        shadows
        camera={{
          position: [0, 25, 30],
          fov: 50,
          near: 0.1,
          far: 1000
        }}
        gl={{
          antialias: true,
          powerPreference: "default",
          toneMapping: THREE.ACESFilmicToneMapping,
          toneMappingExposure: 1.0,
          outputColorSpace: THREE.SRGBColorSpace
        }}
        style={{ position: "absolute", inset: 0 }}
      >
        <color attach="background" args={[(selectedMap === "space" || selectedMap === "saturn") ? "#0a0a1a" : selectedMap === "ice" ? "#87CEEB" : "#1a1a2e"]} />
        
        {selectedMap === "ice" && <fog attach="fog" args={['#c8e6f8', 60, 150]} />}
        
        <Suspense fallback={null}>
          {selectedMap === "ice" && <GradientSky />}
          {selectedMap !== "ice" && <Sky sunPosition={currentSky.sunPosition} />}
          {selectedMap !== "ice" && <Environment preset="sunset" background={false} />}
          
          <Lights />
          
          <Arena theme={selectedMap || "grass"} />
          
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
          <ExplosionEffect />
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
          
          <EffectComposer>
            <Bloom 
              intensity={0.8}
              luminanceThreshold={0.6}
              luminanceSmoothing={0.3}
              mipmapBlur
            />
          </EffectComposer>
          
          <PhysicsManager />
          
          <DeveloperMoveControls key={`dev-controls-${selectedMap || "grass"}`} />
          
          <GameCamera />
        </Suspense>
      </Canvas>
      
      <GameUI />
      <Tutorial />
      <ArcPeakOverlayWrapper />
    </>
  );
}

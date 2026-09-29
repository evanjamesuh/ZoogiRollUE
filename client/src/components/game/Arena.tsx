import * as THREE from "three";
import { useRef, useMemo, Suspense, Component, ReactNode } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF, Center } from "@react-three/drei";
import { useZoogiGame, CustomArenaDecoration, type MapTheme } from "@/lib/stores/useZoogiGame";
import { Trees } from "./Trees";
import { IcePatches } from "./IcePatches";
import { Clouds } from "./Clouds";
import { Snowmen } from "./Snowmen";
import { ArenaWalls } from "./ArenaWalls";
import { OuterRingWall } from "./OuterRingWall";
import { FloatingAsteroids } from "./FloatingAsteroids";
import { FallingRocks } from "./FallingRocks";
import { FlowerPatches } from "./FlowerPatches";
import { SpaceBackground, SpaceWeather } from "./SpaceBackground";
import { MeadowsBackground } from "./MeadowsBackground";
import { SnowPines } from "./SnowPines";
import { WinterAnimals } from "./WinterAnimals";
import { SnowfallEffect } from "./SnowfallEffect";
import { AlienCrystals } from "./AlienCrystals";
import { DesertHoodoos } from "./DesertHoodoos";
import { EditorWallBlocks } from "./EditorWallBlocks";
import { EditorScoringZones } from "./EditorScoringZones";
import { PinballBumpers } from "./PinballBumpers";
import { NeonCourtArena } from "./NeonCourtArena";
import { ARENA_RADIUS } from "@/lib/arenaConstants";
import { COSMOS_STAGE, getMapLayout } from "@/lib/arenaColliders";
import { PharaohTombArena } from "./PharaohTombArena";
import { ArabianNightDressing, CosmicVoidDressing, VolcanicPitDressing } from "./ComicMapDressing";
import { ArabianArena, FrozenArena, MeadowArena } from "./RoundMapArenas";

export { ARENA_RADIUS };

function CosmosArenaModel() {
  const groupRef = useRef<THREE.Group>(null);
  const { scene } = useGLTF("/models/cosmos_arena.glb");
  const backgroundSettings = useZoogiGame((state) => state.backgroundSettings);
  const elementTransforms = useZoogiGame((state) => state.elementTransforms);
  const gameMode = useZoogiGame((state) => state.gameMode);
  const editing = gameMode === "map_editor";

  const arenaOffset = elementTransforms.arenaModelOffset;
  const modelX = editing ? (backgroundSettings.modelPositionX ?? 0) + arenaOffset.x : 0;
  const modelY = editing ? (backgroundSettings.modelPositionY ?? -0.5) + arenaOffset.y : COSMOS_STAGE.modelOffsetY;
  const modelZ = editing ? (backgroundSettings.modelPositionZ ?? 0) + arenaOffset.z : 0;
  const scale = editing ? (backgroundSettings.modelScale ?? COSMOS_STAGE.modelScale) : COSMOS_STAGE.modelScale;
  const modelScale: [number, number, number] = [scale, scale, scale];
  const modelPosition: [number, number, number] = [modelX, modelY, modelZ];
  
  useMemo(() => {
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
        if (child.material) {
          const mat = child.material as THREE.MeshStandardMaterial;
          if (mat.emissive) {
            mat.emissiveIntensity = 1.5;
          }
        }
      }
    });
  }, [scene]);
  
  const arenaRotation = elementTransforms.arenaModelRotation;
  const modelRotation: [number, number, number] = editing
    ? [arenaRotation?.x ?? 0, arenaRotation?.y ?? 0, arenaRotation?.z ?? 0]
    : [0, 0, 0];
  
  return (
    <group ref={groupRef} position={modelPosition} scale={modelScale} rotation={modelRotation}>
      <primitive object={scene} />
    </group>
  );
}

function CustomDecoration({ decoration }: { decoration: CustomArenaDecoration }) {
  const { type, position, rotation, scale } = decoration;
  
  const renderDecoration = () => {
    switch (type) {
      case "oak_tree":
        return (
          <group>
            <mesh position={[0, 0.4, 0]}>
              <cylinderGeometry args={[0.15, 0.2, 0.8, 6]} />
              <meshStandardMaterial color="#5D4037" />
            </mesh>
            <mesh position={[0, 1.2, 0]}>
              <sphereGeometry args={[0.8, 8, 8]} />
              <meshStandardMaterial color="#2E7D32" />
            </mesh>
          </group>
        );
      case "pine_tree":
        return (
          <group>
            <mesh position={[0, 0.3, 0]}>
              <cylinderGeometry args={[0.1, 0.15, 0.6, 6]} />
              <meshStandardMaterial color="#4E342E" />
            </mesh>
            <mesh position={[0, 0.8, 0]}>
              <coneGeometry args={[0.5, 0.8, 6]} />
              <meshStandardMaterial color="#1B5E20" />
            </mesh>
            <mesh position={[0, 1.3, 0]}>
              <coneGeometry args={[0.35, 0.6, 6]} />
              <meshStandardMaterial color="#2E7D32" />
            </mesh>
          </group>
        );
      case "mushroom_red":
      case "mushroom_white":
        return (
          <group>
            <mesh position={[0, 0.2, 0]}>
              <cylinderGeometry args={[0.1, 0.12, 0.35, 8]} />
              <meshStandardMaterial color="#F5DEB3" />
            </mesh>
            <mesh position={[0, 0.45, 0]}>
              <sphereGeometry args={[0.25, 16, 8, 0, Math.PI * 2, 0, Math.PI / 2]} />
              <meshStandardMaterial color={type === "mushroom_red" ? "#EF4444" : "#FFFFFF"} />
            </mesh>
          </group>
        );
      case "flower_pink":
      case "flower_yellow":
        return (
          <group>
            <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
              <circleGeometry args={[0.5, 8]} />
              <meshStandardMaterial color="#7CB342" />
            </mesh>
            <mesh position={[0, 0.15, 0]}>
              <sphereGeometry args={[0.1, 6, 6]} />
              <meshStandardMaterial 
                color={type === "flower_pink" ? "#EC407A" : "#FDD835"} 
                emissive={type === "flower_pink" ? "#FF69B4" : "#FFEB3B"}
                emissiveIntensity={0.2}
              />
            </mesh>
          </group>
        );
      case "snow_pine":
        return (
          <group>
            <mesh position={[0, 0.3, 0]}>
              <cylinderGeometry args={[0.1, 0.12, 0.5, 6]} />
              <meshStandardMaterial color="#5D4037" />
            </mesh>
            <mesh position={[0, 0.7, 0]}>
              <coneGeometry args={[0.45, 0.6, 6]} />
              <meshStandardMaterial color="#E8F5E9" />
            </mesh>
            <mesh position={[0, 1.1, 0]}>
              <coneGeometry args={[0.3, 0.5, 6]} />
              <meshStandardMaterial color="#FFFFFF" />
            </mesh>
          </group>
        );
      case "snowman":
        return (
          <group>
            <mesh position={[0, 0.3, 0]}>
              <sphereGeometry args={[0.3, 16, 16]} />
              <meshStandardMaterial color="white" />
            </mesh>
            <mesh position={[0, 0.65, 0]}>
              <sphereGeometry args={[0.22, 16, 16]} />
              <meshStandardMaterial color="white" />
            </mesh>
            <mesh position={[0, 0.95, 0]}>
              <sphereGeometry args={[0.15, 16, 16]} />
              <meshStandardMaterial color="white" />
            </mesh>
          </group>
        );
      case "ice_patch":
        return (
          <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
            <circleGeometry args={[1, 16]} />
            <meshStandardMaterial color="#E3F2FD" transparent opacity={0.6} />
          </mesh>
        );
      case "hoodoo":
        return (
          <group>
            <mesh position={[0, 0.5, 0]}>
              <cylinderGeometry args={[0.25, 0.35, 0.9, 6]} />
              <meshStandardMaterial color="#D4A574" />
            </mesh>
            <mesh position={[0, 1.1, 0]}>
              <cylinderGeometry args={[0.3, 0.25, 0.5, 6]} />
              <meshStandardMaterial color="#C4956A" />
            </mesh>
            <mesh position={[0, 1.5, 0]}>
              <sphereGeometry args={[0.35, 6, 6]} />
              <meshStandardMaterial color="#B4855A" />
            </mesh>
          </group>
        );
      case "lava_rock":
        return (
          <mesh position={[0, 0.3, 0]}>
            <dodecahedronGeometry args={[0.4, 0]} />
            <meshStandardMaterial color="#4A4A4A" roughness={0.9} />
          </mesh>
        );
      case "lava_glow":
        return (
          <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
            <circleGeometry args={[0.8, 16]} />
            <meshStandardMaterial 
              color="#FF4500" 
              emissive="#FF4500" 
              emissiveIntensity={0.5}
              transparent 
              opacity={0.5} 
            />
          </mesh>
        );
      case "alien_crystal":
        return (
          <group>
            <mesh position={[0, 0.5, 0]}>
              <cylinderGeometry args={[0.12, 0.3, 0.9, 5]} />
              <meshStandardMaterial 
                color="#9C27B0" 
                emissive="#9C27B0" 
                emissiveIntensity={0.4}
                transparent
                opacity={0.9}
              />
            </mesh>
            <mesh position={[0, 1.05, 0]}>
              <octahedronGeometry args={[0.25, 0]} />
              <meshStandardMaterial 
                color="#E040FB" 
                emissive="#E040FB" 
                emissiveIntensity={0.7}
                transparent
                opacity={0.85}
              />
            </mesh>
            <pointLight color="#aa66ff" intensity={1} distance={3} position={[0, 0.8, 0]} />
          </group>
        );
      case "alien":
        return (
          <group>
            <mesh position={[0, 0.35, 0]}>
              <capsuleGeometry args={[0.18, 0.3, 8, 16]} />
              <meshStandardMaterial color="#7CFC00" emissive="#7CFC00" emissiveIntensity={0.3} />
            </mesh>
            <mesh position={[0, 0.75, 0]}>
              <sphereGeometry args={[0.22, 16, 16]} />
              <meshStandardMaterial color="#7CFC00" emissive="#7CFC00" emissiveIntensity={0.3} />
            </mesh>
          </group>
        );
      case "asteroid":
        return (
          <mesh position={[0, 0.4, 0]}>
            <icosahedronGeometry args={[0.45, 0]} />
            <meshStandardMaterial color="#757575" roughness={0.8} />
          </mesh>
        );
      default:
        return (
          <mesh position={[0, 0.3, 0]}>
            <boxGeometry args={[0.5, 0.5, 0.5]} />
            <meshStandardMaterial color="#2196F3" />
          </mesh>
        );
    }
  };
  
  return (
    <group position={position} rotation={[0, rotation, 0]} scale={scale}>
      {renderDecoration()}
    </group>
  );
}

function CustomDecorations() {
  const customArenaDecorations = useZoogiGame(state => state.customArenaDecorations);
  
  if (!customArenaDecorations || customArenaDecorations.length === 0) {
    return null;
  }
  
  return (
    <>
      {customArenaDecorations.map((decoration) => (
        <CustomDecoration key={decoration.id} decoration={decoration} />
      ))}
    </>
  );
}

interface ArenaProps {
  theme?: MapTheme;
}

class MeshyArenaErrorBoundary extends Component<{ children: ReactNode; fallback: ReactNode; onError?: () => void }, { hasError: boolean }> {
  constructor(props: { children: ReactNode; fallback: ReactNode; onError?: () => void }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() { return { hasError: true }; }
  componentDidCatch(error: Error) {
    console.error("Meshy arena loading error:", error);
    this.props.onError?.();
  }
  render() { return this.state.hasError ? this.props.fallback : this.props.children; }
}

function MeshyArenaModelInner({ modelUrl }: { modelUrl: string }) {
  const { scene } = useGLTF(modelUrl);
  const backgroundSettings = useZoogiGame((state) => state.backgroundSettings);
  
  const modelX = backgroundSettings.modelPositionX ?? 0;
  const modelY = backgroundSettings.modelPositionY ?? -0.5;
  const modelZ = backgroundSettings.modelPositionZ ?? 0;
  const modelScale = backgroundSettings.modelScale ?? 8;
  
  useMemo(() => {
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
  }, [scene]);

  return (
    <Center>
      <primitive object={scene} scale={modelScale} position={[modelX, modelY, modelZ]} />
    </Center>
  );
}

function MeshyArenaModel({ modelUrl }: { modelUrl: string }) {
  return (
    <MeshyArenaErrorBoundary fallback={null}>
      <Suspense fallback={null}>
        <MeshyArenaModelInner modelUrl={modelUrl} />
      </Suspense>
    </MeshyArenaErrorBoundary>
  );
}

function KnockoffBoundaryRing({ radius, width, offset = { x: 0, y: 0, z: 0 }, rotation = { x: 0, y: 0, z: 0 }, isGameplay = false }: { radius: number; width: number; offset?: { x: number; y: number; z: number }; rotation?: { x: number; y: number; z: number }; isGameplay?: boolean }) {
  const meshRef = useRef<THREE.Mesh>(null);
  const knockoffBoundaryFlash = useZoogiGame(state => state.knockoffBoundaryFlash);
  const flashStartRef = useRef<number | null>(null);
  const flashColorRef = useRef<string>("#FFFFFF");
  const flashCountRef = useRef<number>(2);
  
  useFrame(() => {
    if (!meshRef.current) return;
    const material = meshRef.current.material as THREE.MeshBasicMaterial;
    
    if (knockoffBoundaryFlash && knockoffBoundaryFlash.timestamp !== flashStartRef.current) {
      flashStartRef.current = knockoffBoundaryFlash.timestamp;
      flashColorRef.current = knockoffBoundaryFlash.color;
      flashCountRef.current = knockoffBoundaryFlash.flashCount;
    }
    
    if (flashStartRef.current) {
      const elapsed = Date.now() - flashStartRef.current;
      const flashDuration = 150;
      const totalFlashTime = flashDuration * flashCountRef.current * 2;
      
      if (elapsed < totalFlashTime) {
        const flashCycle = Math.floor(elapsed / flashDuration);
        const isFlashOn = flashCycle % 2 === 0;
        
        if (isFlashOn) {
          material.color.set(flashColorRef.current);
          material.opacity = 0.9;
        } else {
          material.color.set("#FFFFFF");
          material.opacity = 0.4;
        }
      } else {
        material.color.set("#FFFFFF");
        material.opacity = isGameplay ? 0.4 : 0.5;
        flashStartRef.current = null;
      }
    } else {
      material.color.set(isGameplay ? "#FFFFFF" : "#FF6600");
      material.opacity = isGameplay ? 0.4 : 0.5;
    }
  });
  
  return (
    <mesh ref={meshRef} rotation={[-Math.PI / 2 + rotation.x, rotation.y, rotation.z]} position={[offset.x, 0.05 + offset.y, offset.z]}>
      <ringGeometry args={[radius - width / 2, radius + width / 2, 128]} />
      <meshBasicMaterial 
        color={isGameplay ? "#FFFFFF" : "#FF6600"}
        transparent 
        opacity={isGameplay ? 0.4 : 0.5}
        side={THREE.DoubleSide}
      />
    </mesh>
  );
}

export function Arena({ theme = "grass" }: ArenaProps) {
  const selectedMap = useZoogiGame(state => state.selectedMap);
  const customArenaId = useZoogiGame(state => state.customArenaId);
  const meshyArenaModelUrl = useZoogiGame(state => state.meshyArenaModelUrl);
  const wallSettings = useZoogiGame(state => state.wallSettings);
  const elementTransforms = useZoogiGame(state => state.elementTransforms);
  const gameMode = useZoogiGame(state => state.gameMode);
  const currentTheme = selectedMap || theme;
  
  const themeColors: Record<MapTheme, { platform: string; edge: string; glow: string }> = {
    grass: { platform: "#4CAF50", edge: "#2E7D32", glow: "#81C784" },
    ice: { platform: "#81D4FA", edge: "#0288D1", glow: "#B3E5FC" },
    lava: { platform: "#FF5722", edge: "#BF360C", glow: "#FF8A65" },
    space: { platform: "#7C4DFF", edge: "#311B92", glow: "#B388FF" },
    saturn: { platform: "#3E2723", edge: "#FFA726", glow: "#FFB74D" },
    tomb: { platform: "#E0B88A", edge: "#A87848", glow: "#FFD2A8" },
    neon: { platform: "#14161f", edge: "#ff3ec8", glow: "#22e7ff" },
  };
  
  const colors = themeColors[currentTheme] || themeColors.grass;

  const isIceTheme = currentTheme === "ice";
  const layout = getMapLayout(currentTheme);
  const floorRadius = layout?.floorRadius ?? ARENA_RADIUS;
  // The stand-in disk matches the knockout line. Points come from falling off that edge.
  const standInRadius = layout?.knockoffRadius ?? floorRadius;
  const stageFallback = <PlayfieldDisk radius={standInRadius} color={colors.platform} />;
  
  return (
    <group>
      {/* Lava has no stage model. The disk is the playfield, the same size as the knockoff ring. */}
      {currentTheme === "lava" && (
        <>
          <PlayfieldDisk radius={standInRadius} color={colors.platform} emissive="#ff4a00" emissiveIntensity={0.7} />
          <EdgeRing radius={standInRadius} color={colors.edge} />
          <DangerZone radius={standInRadius} color={colors.edge} />
          <VolcanicPitDressing />
        </>
      )}
      
      {currentTheme === "lava" && <FallingRocks />}
      {currentTheme === "lava" && <DesertHoodoos />}
      {currentTheme === "grass" && <MeadowArena />}
      {isIceTheme && <FrozenArena />}
      {isIceTheme && <IcePatches />}
      {isIceTheme && <Snowmen />}
      {isIceTheme && <SnowfallEffect />}
      {isIceTheme && <WinterAnimals />}
      {currentTheme === "space" && (
        <MeshyArenaErrorBoundary fallback={stageFallback}>
          <Suspense fallback={stageFallback}>
            <CosmosArenaModel />
          </Suspense>
        </MeshyArenaErrorBoundary>
      )}
      {currentTheme === "space" && <SpaceBackground />}
      {currentTheme === "space" && <CosmicVoidDressing />}
      {currentTheme === "saturn" && <ArabianArena />}
      {currentTheme === "saturn" && <ArabianNightDressing />}
      {currentTheme === "tomb" && <PharaohTombArena />}
      {currentTheme === "neon" && <NeonCourtArena />}
      
      {customArenaId && <CustomDecorations />}
      
      {meshyArenaModelUrl && <MeshyArenaModel modelUrl={meshyArenaModelUrl} />}
      
      {/* Hide ArenaWalls when using destructible physics walls */}
      {false && <ArenaWalls theme={currentTheme as "grass" | "ice" | "lava" | "space" | "saturn"} />}
      
      {/* Walls removed - DestructibleRingWall and OuterRingWall disabled */}
      
      <PinballBumpers />
      <EditorWallBlocks />
      <EditorScoringZones />
      
      {currentTheme !== "neon" && gameMode === "map_editor" && (
        <KnockoffBoundaryRing 
          radius={wallSettings.knockoffBoundaryRadius ?? 12} 
          width={wallSettings.knockoffBoundaryWidth ?? 0.3}
          offset={elementTransforms.knockoffBoundaryOffset}
          rotation={elementTransforms.knockoffBoundaryRotation}
          isGameplay={false}
        />
      )}
    </group>
  );
}

function PlayfieldDisk({ radius, color, center = [0, 0, 0], emissive, emissiveIntensity = 0 }: { radius: number; color: string; center?: [number, number, number]; emissive?: string; emissiveIntensity?: number }) {
  const ref = useRef<THREE.Mesh>(null);
  useFrame((state) => {
    if (!emissive || !ref.current) return;
    const material = ref.current.material as THREE.MeshStandardMaterial;
    material.emissiveIntensity = emissiveIntensity + Math.sin(state.clock.elapsedTime * 1.6) * 0.16;
  });
  return (
    <mesh ref={ref} rotation={[-Math.PI / 2, 0, 0]} position={center} receiveShadow>
      <circleGeometry args={[radius, 64]} />
      <meshStandardMaterial color={color} emissive={emissive ?? "#000000"} emissiveIntensity={emissiveIntensity} />
    </mesh>
  );
}

function EdgeRing({ radius, color }: { radius: number; color: string }) {
  const ringRef = useRef<THREE.Mesh>(null);
  
  useFrame((state) => {
    if (ringRef.current) {
      ringRef.current.rotation.z = state.clock.elapsedTime * 0.2;
    }
  });

  return (
    <group>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]}>
        <ringGeometry args={[radius - 1.5, radius - 1, 64]} />
        <meshStandardMaterial 
          color="#ff7a22" 
          transparent 
          opacity={0.55}
          emissive="#ff5a10"
          emissiveIntensity={0.9}
        />
      </mesh>
      
      <mesh ref={ringRef} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.03, 0]}>
        <ringGeometry args={[radius - 2, radius - 1.5, 32]} />
        <meshStandardMaterial 
          color={color} 
          transparent 
          opacity={0.3}
        />
      </mesh>
    </group>
  );
}

function DangerZone({ radius, color }: { radius: number; color: string }) {
  const pulseRef = useRef<THREE.Mesh>(null);
  
  useFrame((state) => {
    if (pulseRef.current) {
      const pulse = Math.sin(state.clock.elapsedTime * 2) * 0.1 + 0.3;
      (pulseRef.current.material as THREE.MeshStandardMaterial).opacity = pulse;
    }
  });

  return (
    <mesh ref={pulseRef} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.01, 0]}>
      <ringGeometry args={[radius - 2, radius, 64]} />
      <meshStandardMaterial 
        color="#ff6a18" 
        emissive="#ff4a00"
        emissiveIntensity={0.45}
        transparent 
        opacity={0.3}
        side={THREE.DoubleSide}
      />
    </mesh>
  );
}

function CenterDecoration({ color }: { color: string }) {
  const ringRef = useRef<THREE.Mesh>(null);
  
  useFrame((state) => {
    if (ringRef.current) {
      ringRef.current.rotation.z = state.clock.elapsedTime * 0.5;
    }
  });

  return (
    <group position={[0, 0.01, 0]}>
      <mesh rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[2.5, 3, 32]} />
        <meshStandardMaterial color="#FFD700" transparent opacity={0.3} />
      </mesh>
      
      <mesh ref={ringRef} rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.01, 0]}>
        <ringGeometry args={[3.5, 4, 6]} />
        <meshStandardMaterial color={color} transparent opacity={0.2} />
      </mesh>
      
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]}>
        <circleGeometry args={[1.5, 32]} />
        <meshStandardMaterial 
          color="#FFD700" 
          transparent 
          opacity={0.2}
          emissive="#FFD700"
          emissiveIntensity={0.3}
        />
      </mesh>
    </group>
  );
}

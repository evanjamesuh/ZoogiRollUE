import * as THREE from "three";
import { useRef, useMemo, Suspense, Component, ReactNode } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF, useAnimations, Center } from "@react-three/drei";
import { useZoogiGame, CustomArenaDecoration } from "@/lib/stores/useZoogiGame";
import { Trees } from "./Trees";
import { IcePatches } from "./IcePatches";
import { Clouds } from "./Clouds";
import { Snowmen } from "./Snowmen";
import { ArenaWalls } from "./ArenaWalls";
import { OuterRingWall } from "./OuterRingWall";
import { ControlPointZones } from "./ControlPointZones";
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
import { ScoringZones } from "./ScoringZones";
import { PinballBumpers } from "./PinballBumpers";
import { ARENA_RADIUS } from "@/lib/arenaConstants";
import { ARABIAN_STAGE, COSMOS_STAGE, GRASS_STAGE, WINTER_STAGE, arabianPlayTransform, getMapLayout } from "@/lib/arenaColliders";

export { ARENA_RADIUS };

function FloatingIslandScene() {
  const groupRef = useRef<THREE.Group>(null);
  const { scene, animations } = useGLTF("/models/floating_island_stage.glb");
  const { actions } = useAnimations(animations, groupRef);
  const backgroundSettings = useZoogiGame((state) => state.backgroundSettings);
  const elementTransforms = useZoogiGame((state) => state.elementTransforms);
  
  const gameMode = useZoogiGame((state) => state.gameMode);
  const editing = gameMode === "map_editor";
  const modelX = editing ? (backgroundSettings.modelPositionX ?? 0) : 0;
  const modelY = editing ? (backgroundSettings.modelPositionY ?? -0.5) : GRASS_STAGE.modelOffsetY;
  const modelZ = editing ? (backgroundSettings.modelPositionZ ?? 0) : 0;
  const modelScale = editing ? (backgroundSettings.modelScale ?? 3) : GRASS_STAGE.modelScale;
  const arenaRotation = elementTransforms.arenaModelRotation;
  const modelRotation: [number, number, number] = [
    arenaRotation?.x ?? 0,
    arenaRotation?.y ?? 0,
    arenaRotation?.z ?? 0
  ];
  
  useMemo(() => {
    if (actions && Object.keys(actions).length > 0) {
      Object.values(actions).forEach(action => {
        if (action) {
          action.play();
        }
      });
    }
  }, [actions]);

  useMemo(() => {
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
  }, [scene]);
  
  return (
    <group ref={groupRef} position={[modelX, modelY, modelZ]} scale={[modelScale, modelScale, modelScale]} rotation={modelRotation}>
      <primitive object={scene} />
    </group>
  );
}

function ArabianNightsScene() {
  const groupRef = useRef<THREE.Group>(null);
  const { scene, animations } = useGLTF("/models/arabian_nights_stage.glb");
  const { actions } = useAnimations(animations, groupRef);
  const backgroundSettings = useZoogiGame((state) => state.backgroundSettings);
  const elementTransforms = useZoogiGame((state) => state.elementTransforms);
  const gameMode = useZoogiGame((state) => state.gameMode);
  const editing = gameMode === "map_editor";
  const placed = arabianPlayTransform();

  const modelX = editing ? (backgroundSettings.modelPositionX ?? 0) : placed.x;
  const modelY = editing ? (backgroundSettings.modelPositionY ?? -0.5) : placed.y;
  const modelZ = editing ? (backgroundSettings.modelPositionZ ?? 0) : placed.z;
  const modelScale = editing ? (backgroundSettings.modelScale ?? ARABIAN_STAGE.modelScale) : placed.scale;
  const arenaRotation = elementTransforms.arenaModelRotation;
  const modelRotation: [number, number, number] = editing
    ? [arenaRotation?.x ?? 0, arenaRotation?.y ?? 0, arenaRotation?.z ?? 0]
    : [0, 0, 0];
  
  useMemo(() => {
    if (actions && Object.keys(actions).length > 0) {
      Object.values(actions).forEach(action => {
        if (action) {
          action.play();
        }
      });
    }
  }, [actions]);

  useMemo(() => {
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
  }, [scene]);
  
  return (
    <group ref={groupRef} position={[modelX, modelY, modelZ]} scale={[modelScale, modelScale, modelScale]} rotation={modelRotation}>
      <primitive object={scene} />
    </group>
  );
}

interface SkatingPenguinData {
  obj: THREE.Object3D;
  baseY: number;
  posX: number;
  posZ: number;
  targetX: number;
  targetZ: number;
  facing: number;
  waddlePhase: number;
  waiting: boolean;
  waitTimer: number;
  jiggle: number;
  lastHitTime: number;
}

const SKATE_RADIUS = 12;
const SKATE_SPEED = 2.0;
const SKATE_ARRIVE_DIST = 1.0;

function getRandomSkateTarget(): [number, number] {
  const angle = Math.random() * Math.PI * 2;
  const dist = 1.0 + Math.random() * (SKATE_RADIUS - 2.0);
  return [Math.cos(angle) * dist, Math.sin(angle) * dist];
}

function clampToIce(x: number, z: number): [number, number] {
  const dist = Math.sqrt(x * x + z * z);
  if (dist > SKATE_RADIUS) {
    const scale = SKATE_RADIUS / dist;
    return [x * scale, z * scale];
  }
  return [x, z];
}

const PENGUIN_STARTS: { x: number; y: number; z: number }[] = [
  { x: 6.36, y: 4.9, z: 6.36 },
  { x: -6.36, y: 4.9, z: 6.36 },
  { x: -6.36, y: 4.9, z: -6.36 },
  { x: 6.36, y: 4.9, z: -6.36 },
];

const _winterPenguinSkaters: SkatingPenguinData[] = [];
let _winterPenguinsReady = false;

function WinterLocationScene() {
  const groupRef = useRef<THREE.Group>(null);
  const { scene } = useGLTF("/models/winter_location.glb");

  const spinObjects = useRef<{ obj: THREE.Object3D; baseY: number; phase: number }[]>([]);
  const swayObjects = useRef<{ obj: THREE.Object3D; baseY: number; baseRotY: number; phase: number }[]>([]);

  useMemo(() => {
    const spins: { obj: THREE.Object3D; baseY: number; phase: number }[] = [];
    const sways: { obj: THREE.Object3D; baseY: number; baseRotY: number; phase: number }[] = [];
    const coinPattern = /^coin_|^ring_001/i;
    const charPattern = /^snowman_|^characters$/i;

    if (!_winterPenguinsReady) {
      const foundPenguins: THREE.Object3D[] = [];
      scene.traverse((child) => {
        if (child.name && /penguin/i.test(child.name)) {
          const distFromCenter = Math.sqrt(child.position.x * child.position.x + child.position.z * child.position.z);
          if (child.position.y < 6 && distFromCenter < 15) {
            foundPenguins.push(child);
          }
        }
      });

      if (foundPenguins.length >= 4) {
        _winterPenguinsReady = true;
        _winterPenguinSkaters.length = 0;
        for (let i = 0; i < 4; i++) {
          const node = foundPenguins[i];
          const start = PENGUIN_STARTS[i];
          node.position.set(start.x, start.y, start.z);
          const [tx, tz] = getRandomSkateTarget();
          _winterPenguinSkaters.push({
            obj: node,
            baseY: start.y,
            posX: start.x,
            posZ: start.z,
            targetX: tx,
            targetZ: tz,
            facing: (Math.PI / 2) * i,
            waddlePhase: (Math.PI / 2) * i,
            waiting: false,
            waitTimer: 0,
            jiggle: 0,
            lastHitTime: 0,
          });
        }
        console.log("[ICE] Initialized 4 skating penguins (module-level)");
      }
    }

    scene.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
        if (child.material) {
          const materials = Array.isArray(child.material) ? child.material : [child.material];
          materials.forEach((mat) => {
            if (mat instanceof THREE.MeshStandardMaterial) {
              mat.roughness = Math.max(mat.roughness, 0.85);
              mat.metalness = 0;
              mat.envMapIntensity = 0;
              mat.emissive = new THREE.Color(0x000000);
              mat.emissiveIntensity = 0;
            }
          });
        }
      }

      if (child.name && coinPattern.test(child.name)) {
        spins.push({
          obj: child,
          baseY: child.position.y,
          phase: Math.random() * Math.PI * 2,
        });
      }

      if (child.name && charPattern.test(child.name)) {
        sways.push({
          obj: child,
          baseY: child.position.y,
          baseRotY: child.rotation.y,
          phase: Math.random() * Math.PI * 2,
        });
      }
    });

    spinObjects.current = spins;
    swayObjects.current = sways;
  }, [scene]);

  useFrame((state, delta) => {
    const t = state.clock.elapsedTime;

    spinObjects.current.forEach(({ obj, baseY, phase }) => {
      obj.rotation.y += 0.03;
      obj.position.y = baseY + Math.sin(t * 2.0 + phase) * 0.3;
    });

    swayObjects.current.forEach(({ obj, baseY, baseRotY, phase }) => {
      obj.position.y = baseY + Math.sin(t * 1.2 + phase) * 0.12;
      obj.rotation.y = baseRotY + Math.sin(t * 0.5 + phase) * 0.15;
    });

    const gameState = useZoogiGame.getState();
    const now = Date.now();

    _winterPenguinSkaters.forEach((p) => {
      if (p.waiting) {
        p.waitTimer -= delta;
        if (p.waitTimer <= 0) {
          p.waiting = false;
          const [tx, tz] = getRandomSkateTarget();
          p.targetX = tx;
          p.targetZ = tz;
        }
      } else {
        const dx = p.targetX - p.posX;
        const dz = p.targetZ - p.posZ;
        const dist = Math.sqrt(dx * dx + dz * dz);

        if (dist < SKATE_ARRIVE_DIST) {
          p.waiting = true;
          p.waitTimer = 1.5 + Math.random() * 3.0;
        } else {
          const nx = dx / dist;
          const nz = dz / dist;
          p.posX += nx * SKATE_SPEED * delta;
          p.posZ += nz * SKATE_SPEED * delta;

          const [cx, cz] = clampToIce(p.posX, p.posZ);
          p.posX = cx;
          p.posZ = cz;

          const targetAngle = Math.atan2(nx, nz);
          let angleDiff = targetAngle - p.facing;
          while (angleDiff > Math.PI) angleDiff -= Math.PI * 2;
          while (angleDiff < -Math.PI) angleDiff += Math.PI * 2;
          p.facing += angleDiff * Math.min(1, delta * 4);
        }
      }

      const HIT_R = 2.2;
      if (now - p.lastHitTime > 500) {
        let hit = false;
        const chk = (px: number, pz: number) => {
          const cdx = px - p.posX;
          const cdz = pz - p.posZ;
          return Math.sqrt(cdx * cdx + cdz * cdz) < HIT_R;
        };
        if (gameState.playerEntity && chk(gameState.playerEntity.position[0], gameState.playerEntity.position[2])) hit = true;
        if (!hit) {
          for (const e of gameState.enemies) {
            if (chk(e.position[0], e.position[2])) { hit = true; break; }
          }
        }
        if (hit) {
          p.jiggle = 1.0;
          p.lastHitTime = now;
          const [tx, tz] = getRandomSkateTarget();
          p.targetX = tx;
          p.targetZ = tz;
          p.waiting = false;
        }
      }

      p.jiggle = Math.max(0, p.jiggle - delta * 3);
      const jAmt = p.jiggle > 0 ? Math.sin(t * 25) * p.jiggle * 0.12 : 0;

      p.obj.position.x = p.posX;
      p.obj.position.z = p.posZ;
      p.obj.position.y = p.baseY;
      p.obj.rotation.set(jAmt * 0.5, p.facing, jAmt);
    });
  });

  const winterScale = WINTER_STAGE.modelScale;
  return (
    <group ref={groupRef} position={[0, WINTER_STAGE.modelOffsetY, 0]} scale={[winterScale, winterScale, winterScale]}>
      <primitive object={scene} />
    </group>
  );
}

useGLTF.preload("/models/winter_location.glb");

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
  theme?: "grass" | "ice" | "lava" | "space" | "saturn";
}

class MeshyArenaErrorBoundary extends Component<{ children: ReactNode; fallback: ReactNode }, { hasError: boolean }> {
  constructor(props: { children: ReactNode; fallback: ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() { return { hasError: true }; }
  componentDidCatch(error: Error) { console.error("Meshy arena loading error:", error); }
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
  
  const themeColors = {
    grass: { platform: "#4CAF50", edge: "#2E7D32", glow: "#81C784" },
    ice: { platform: "#81D4FA", edge: "#0288D1", glow: "#B3E5FC" },
    lava: { platform: "#FF5722", edge: "#BF360C", glow: "#FF8A65" },
    space: { platform: "#7C4DFF", edge: "#311B92", glow: "#B388FF" },
    saturn: { platform: "#3E2723", edge: "#FFA726", glow: "#FFB74D" }
  };
  
  const colors = themeColors[currentTheme as keyof typeof themeColors] || themeColors.grass;

  const isIceTheme = currentTheme === "ice";
  const layout = getMapLayout(currentTheme);
  const floorRadius = layout?.floorRadius ?? ARENA_RADIUS;
  // The stand-in disk matches the knockout line, so rolling off what you
  // see is the same as crossing the scoring ring.
  const standInRadius = layout?.knockoffRadius ?? floorRadius;
  const stageFallback = <PlayfieldDisk radius={standInRadius} color={colors.platform} />;
  
  return (
    <group>
      {/* Lava has no stage model. The disk is the playfield, the same size as the knockoff ring. */}
      {currentTheme === "lava" && (
        <>
          <PlayfieldDisk radius={standInRadius} color={colors.platform} />
          <EdgeRing radius={standInRadius} color={colors.edge} />
          <DangerZone radius={standInRadius} color={colors.edge} />
        </>
      )}
      
      {currentTheme === "lava" && <FallingRocks />}
      {currentTheme === "lava" && <DesertHoodoos />}
      {currentTheme === "grass" && (
        <MeshyArenaErrorBoundary fallback={stageFallback}>
          <Suspense fallback={null}>
            <FloatingIslandScene />
          </Suspense>
        </MeshyArenaErrorBoundary>
      )}
      {isIceTheme && (
        <MeshyArenaErrorBoundary fallback={stageFallback}>
          <Suspense fallback={null}>
            <WinterLocationScene />
          </Suspense>
        </MeshyArenaErrorBoundary>
      )}
      {isIceTheme && <IcePatches />}
      {isIceTheme && <Snowmen />}
      {isIceTheme && <SnowfallEffect />}
      {isIceTheme && <WinterAnimals />}
      {currentTheme === "space" && (
        <MeshyArenaErrorBoundary fallback={stageFallback}>
          <Suspense fallback={null}>
            <CosmosArenaModel />
          </Suspense>
        </MeshyArenaErrorBoundary>
      )}
      {currentTheme === "space" && <SpaceBackground />}
      {currentTheme === "saturn" && (
        <MeshyArenaErrorBoundary fallback={stageFallback}>
          <Suspense fallback={null}>
            <ArabianNightsScene />
          </Suspense>
        </MeshyArenaErrorBoundary>
      )}
      
      {customArenaId && <CustomDecorations />}
      
      {meshyArenaModelUrl && <MeshyArenaModel modelUrl={meshyArenaModelUrl} />}
      
      {/* Hide ArenaWalls when using destructible physics walls */}
      {false && <ArenaWalls theme={currentTheme as "grass" | "ice" | "lava" | "space" | "saturn"} />}
      
      {/* Walls removed - DestructibleRingWall and OuterRingWall disabled */}
      
      <group 
        position={[
          elementTransforms.zonesOffset.x,
          elementTransforms.zonesOffset.y,
          elementTransforms.zonesOffset.z
        ]}
        rotation={[
          elementTransforms.zonesRotation?.x ?? 0,
          elementTransforms.zonesRotation?.y ?? 0,
          elementTransforms.zonesRotation?.z ?? 0
        ]}
      >
        <ControlPointZones
          enabled={true}
          zoneRadius={ARENA_RADIUS + 6}
          captureSpeed={0.5}
          scorePerSecond={1}
        />
      </group>
      
      {/* InnerBarrierWalls removed */}
      
      <PinballBumpers />
      <EditorWallBlocks />
      <EditorScoringZones />
      
      {(gameMode === "classic" || gameMode === "ringer_royale" || gameMode === "local_multiplayer" || gameMode === "practice") && (
        <ScoringZones />
      )}
      
      {gameMode === "map_editor" && (
        <KnockoffBoundaryRing 
          radius={wallSettings.knockoffBoundaryRadius ?? 12} 
          width={wallSettings.knockoffBoundaryWidth ?? 0.3}
          offset={elementTransforms.knockoffBoundaryOffset}
          rotation={elementTransforms.knockoffBoundaryRotation}
          isGameplay={false}
        />
      )}
      
      {(gameMode === "classic" || gameMode === "ringer_royale" || gameMode === "local_multiplayer" || gameMode === "practice") && (
        <KnockoffBoundaryRing 
          radius={wallSettings.knockoffBoundaryRadius ?? 21} 
          width={wallSettings.knockoffBoundaryWidth ?? 0.5}
          offset={elementTransforms.knockoffBoundaryOffset}
          rotation={elementTransforms.knockoffBoundaryRotation}
          isGameplay={true}
        />
      )}
    </group>
  );
}

function PlayfieldDisk({ radius, color }: { radius: number; color: string }) {
  return (
    <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0, 0]} receiveShadow>
      <circleGeometry args={[radius, 64]} />
      <meshStandardMaterial color={color} />
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
          color="#FFD700" 
          transparent 
          opacity={0.4}
          emissive="#FFD700"
          emissiveIntensity={0.2}
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
        color="#FF0000" 
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

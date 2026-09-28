import * as THREE from "three";
import { useRef, useMemo, useEffect, Suspense, Component, ReactNode } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF, useAnimations, Center } from "@react-three/drei";
import { useZoogiGame, CustomArenaDecoration } from "@/lib/stores/useZoogiGame";
import { usePhysicsWorld } from "@/lib/physics/usePhysicsWorld";
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

export { ARENA_RADIUS };

function FloatingIslandScene() {
  const groupRef = useRef<THREE.Group>(null);
  const { scene, animations } = useGLTF("/models/floating_island_stage.glb");
  const { actions } = useAnimations(animations, groupRef);
  const backgroundSettings = useZoogiGame((state) => state.backgroundSettings);
  const elementTransforms = useZoogiGame((state) => state.elementTransforms);
  
  const modelX = backgroundSettings.modelPositionX ?? 0;
  const modelY = backgroundSettings.modelPositionY ?? -0.5;
  const modelZ = backgroundSettings.modelPositionZ ?? 0;
  const modelScale = backgroundSettings.modelScale ?? 3;
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
  
  const modelX = backgroundSettings.modelPositionX ?? 0;
  const modelY = backgroundSettings.modelPositionY ?? -0.5;
  const modelZ = backgroundSettings.modelPositionZ ?? 0;
  const modelScale = backgroundSettings.modelScale ?? 1.8;
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
  const { createTrimeshCollider, removeBody, isInitialized } = usePhysicsWorld();
  const colliderCreated = useRef(false);

  useEffect(() => {
    if (!isInitialized || colliderCreated.current) return;

    const Y_THRESHOLD = 14;
    const rawVertices: number[] = [];
    const rawIndices: number[] = [];
    let vertexOffset = 0;

    scene.updateMatrixWorld(true);
    const sceneInverse = scene.matrixWorld.clone().invert();

    scene.traverse((child) => {
      if (!(child instanceof THREE.Mesh) || !child.geometry) return;
      const geometry = child.geometry;
      const positionAttr = geometry.getAttribute('position');
      if (!positionAttr) return;

      const meshToSceneMatrix = sceneInverse.clone().multiply(child.matrixWorld);

      for (let i = 0; i < positionAttr.count; i++) {
        const vertex = new THREE.Vector3(
          positionAttr.getX(i),
          positionAttr.getY(i),
          positionAttr.getZ(i)
        );
        vertex.applyMatrix4(meshToSceneMatrix);
        rawVertices.push(vertex.x, vertex.y, vertex.z);
      }

      const indexAttr = geometry.getIndex();
      if (indexAttr) {
        for (let i = 0; i < indexAttr.count; i++) {
          rawIndices.push(indexAttr.getX(i) + vertexOffset);
        }
      } else {
        for (let i = 0; i < positionAttr.count; i++) {
          rawIndices.push(i + vertexOffset);
        }
      }
      vertexOffset += positionAttr.count;
    });

    const usedVertexSet = new Set<number>();
    const filteredTriIndices: number[] = [];
    for (let i = 0; i < rawIndices.length; i += 3) {
      const i0 = rawIndices[i], i1 = rawIndices[i + 1], i2 = rawIndices[i + 2];
      const y0 = rawVertices[i0 * 3 + 1];
      const y1 = rawVertices[i1 * 3 + 1];
      const y2 = rawVertices[i2 * 3 + 1];
      if (y0 > Y_THRESHOLD && y1 > Y_THRESHOLD && y2 > Y_THRESHOLD) continue;
      filteredTriIndices.push(i0, i1, i2);
      usedVertexSet.add(i0);
      usedVertexSet.add(i1);
      usedVertexSet.add(i2);
    }

    const oldToNew = new Map<number, number>();
    const compactedVerts: number[] = [];
    let newIdx = 0;
    for (const oldIdx of usedVertexSet) {
      oldToNew.set(oldIdx, newIdx++);
      compactedVerts.push(
        rawVertices[oldIdx * 3],
        rawVertices[oldIdx * 3 + 1],
        rawVertices[oldIdx * 3 + 2]
      );
    }
    const compactedIndices = filteredTriIndices.map(idx => oldToNew.get(idx)!);

    if (compactedVerts.length > 0 && compactedIndices.length > 0) {
      const vertices = new Float32Array(compactedVerts);
      const indices = new Uint32Array(compactedIndices);
      createTrimeshCollider(
        "winter-terrain",
        vertices,
        indices,
        [0, -4.2, 0],
        [0.9, 0.9, 0.9]
      );
      colliderCreated.current = true;
      console.log("[ICE] Winter terrain trimesh collider created with", compactedVerts.length / 3, "vertices (filtered from", rawVertices.length / 3, ")");
    }

    return () => {
      if (colliderCreated.current) {
        removeBody("winter-terrain");
        colliderCreated.current = false;
      }
    };
  }, [isInitialized, scene, createTrimeshCollider, removeBody]);

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

  return (
    <group ref={groupRef} position={[0, -4.2, 0]} scale={[0.9, 0.9, 0.9]}>
      <primitive object={scene} />
    </group>
  );
}

useGLTF.preload("/models/winter_location.glb");

function CosmosArenaModel() {
  const groupRef = useRef<THREE.Group>(null);
  const { scene } = useGLTF("/models/cosmos_arena.glb");
  const { createTrimeshCollider, removeBody, isInitialized, getBody } = usePhysicsWorld();
  const colliderCreated = useRef(false);
  const backgroundSettings = useZoogiGame((state) => state.backgroundSettings);
  const elementTransforms = useZoogiGame((state) => state.elementTransforms);
  
  const arenaOffset = elementTransforms.arenaModelOffset;
  const modelX = (backgroundSettings.modelPositionX ?? 0) + arenaOffset.x;
  const modelY = (backgroundSettings.modelPositionY ?? -0.5) + arenaOffset.y;
  const modelZ = (backgroundSettings.modelPositionZ ?? 0) + arenaOffset.z;
  const scale = backgroundSettings.modelScale ?? 3;
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
  
  // Create trimesh collider from the GLB geometry - DISABLED to prevent invisible wall collision
  // The game uses flat floor physics from RapierPhysicsManager instead
  useEffect(() => {
    if (!isInitialized) return;
    
    // Always remove existing collider first when settings change
    removeBody("cosmos-arena-floor");
    colliderCreated.current = false;
    
    // DISABLED: Don't create trimesh collider - it causes invisible walls from the 3D model geometry
    console.log("Cosmos arena trimesh collider DISABLED - using flat floor physics only");
    return;
    
    const allVertices: number[] = [];
    const allIndices: number[] = [];
    let vertexOffset = 0;
    
    // Find only floor-related meshes (not decorative elements)
    scene.traverse((child) => {
      if (child instanceof THREE.Mesh && child.geometry) {
        const meshName = child.name.toLowerCase();
        // Only include floor/arena surface meshes, not buildings or decorations
        const isFloorMesh = meshName.includes('floor') || 
                           meshName.includes('arena') ||
                           meshName.includes('ground') ||
                           meshName.includes('ramp');
        
        if (!isFloorMesh) return;
        
        const geometry = child.geometry;
        const positionAttr = geometry.getAttribute('position');
        
        if (positionAttr) {
          // Compute mesh's transform relative to scene root (not world)
          // This avoids including our group's position/scale which is applied separately
          const meshToSceneMatrix = new THREE.Matrix4();
          let current: THREE.Object3D | null = child;
          const matrices: THREE.Matrix4[] = [];
          while (current && current !== scene) {
            matrices.push(current.matrix.clone());
            current = current.parent;
          }
          // Apply matrices in reverse order (from scene root to mesh)
          for (let i = matrices.length - 1; i >= 0; i--) {
            meshToSceneMatrix.multiply(matrices[i]);
          }
          
          for (let i = 0; i < positionAttr.count; i++) {
            const vertex = new THREE.Vector3(
              positionAttr.getX(i),
              positionAttr.getY(i),
              positionAttr.getZ(i)
            );
            // Apply mesh transform within GLB (scene-local space)
            vertex.applyMatrix4(meshToSceneMatrix);
            // Apply our model scale and position
            vertex.x = vertex.x * modelScale[0] + modelPosition[0];
            vertex.y = vertex.y * modelScale[1] + modelPosition[1];
            vertex.z = vertex.z * modelScale[2] + modelPosition[2];
            
            allVertices.push(vertex.x, vertex.y, vertex.z);
          }
          
          // Get indices
          const indexAttr = geometry.getIndex();
          if (indexAttr) {
            for (let i = 0; i < indexAttr.count; i++) {
              allIndices.push(indexAttr.getX(i) + vertexOffset);
            }
          } else {
            // Non-indexed geometry - create indices
            for (let i = 0; i < positionAttr.count; i++) {
              allIndices.push(i + vertexOffset);
            }
          }
          
          vertexOffset += positionAttr.count;
        }
      }
    });
    
    if (allVertices.length > 0 && allIndices.length > 0) {
      // Filter out triangles that are above floor level (Y > 1.5) to prevent wall collision
      const MAX_FLOOR_Y = 1.5;
      const filteredVertices: number[] = [];
      const filteredIndices: number[] = [];
      const vertexMap = new Map<number, number>();
      
      // Process triangles (every 3 indices)
      for (let i = 0; i < allIndices.length; i += 3) {
        const i0 = allIndices[i];
        const i1 = allIndices[i + 1];
        const i2 = allIndices[i + 2];
        
        // Get Y values for each vertex in the triangle
        const y0 = allVertices[i0 * 3 + 1];
        const y1 = allVertices[i1 * 3 + 1];
        const y2 = allVertices[i2 * 3 + 1];
        
        // Skip triangles where ALL vertices are above floor level
        if (y0 > MAX_FLOOR_Y && y1 > MAX_FLOOR_Y && y2 > MAX_FLOOR_Y) {
          continue;
        }
        
        // Add this triangle (map old indices to new)
        for (const oldIdx of [i0, i1, i2]) {
          if (!vertexMap.has(oldIdx)) {
            const newIdx = filteredVertices.length / 3;
            vertexMap.set(oldIdx, newIdx);
            filteredVertices.push(
              allVertices[oldIdx * 3],
              allVertices[oldIdx * 3 + 1],
              allVertices[oldIdx * 3 + 2]
            );
          }
          filteredIndices.push(vertexMap.get(oldIdx)!);
        }
      }
      
      const vertices = new Float32Array(filteredVertices);
      const indices = new Uint32Array(filteredIndices);
      
      // Debug: Calculate Y bounds of the floor collider
      let minY = Infinity, maxY = -Infinity;
      for (let i = 1; i < filteredVertices.length; i += 3) {
        minY = Math.min(minY, filteredVertices[i]);
        maxY = Math.max(maxY, filteredVertices[i]);
      }
      console.log("Floor collider Y bounds:", minY.toFixed(2), "to", maxY.toFixed(2), "(filtered from", allVertices.length / 3, "to", filteredVertices.length / 3, "vertices)");
      
      // Pass identity transform since vertices are already in final world space
      createTrimeshCollider(
        "cosmos-arena-floor",
        vertices,
        indices,
        [0, 0, 0],
        [1, 1, 1]
      );
      
      colliderCreated.current = true;
      console.log("Cosmos arena trimesh collider created with", filteredVertices.length / 3, "vertices");
    }
    
    return () => {
      if (colliderCreated.current) {
        removeBody("cosmos-arena-floor");
        colliderCreated.current = false;
      }
    };
  }, [isInitialized, scene, modelX, modelY, modelZ, scale]);
  
  const arenaRotation = elementTransforms.arenaModelRotation;
  const modelRotation: [number, number, number] = [
    arenaRotation?.x ?? 0,
    arenaRotation?.y ?? 0,
    arenaRotation?.z ?? 0
  ];
  
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
  const backgroundSettings = useZoogiGame(state => state.backgroundSettings);
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

  const isSpaceTheme = currentTheme === "space";
  const isGrassTheme = currentTheme === "grass";
  const isIceTheme = currentTheme === "ice";
  const useGLBFloor = isSpaceTheme || isGrassTheme || isIceTheme;
  
  return (
    <group>
      {/* Hide ground plane for themes using GLB model floor */}
      {!useGLBFloor && (
        <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0, 0]} receiveShadow>
          <circleGeometry args={[ARENA_RADIUS * (backgroundSettings.groundScale ?? 3), 64]} />
          <meshStandardMaterial color={colors.platform} />
        </mesh>
      )}

      {!useGLBFloor && <EdgeRing radius={ARENA_RADIUS} color={colors.edge} />}

      {!useGLBFloor && <DangerZone radius={ARENA_RADIUS} color={colors.edge} />}
      
      {currentTheme === "lava" && <FallingRocks />}
      {currentTheme === "lava" && <DesertHoodoos />}
      {currentTheme === "grass" && (
        <Suspense fallback={null}>
          <FloatingIslandScene />
        </Suspense>
      )}
      {isIceTheme && (
        <Suspense fallback={null}>
          <WinterLocationScene />
        </Suspense>
      )}
      {isIceTheme && <IcePatches />}
      {isIceTheme && <Snowmen />}
      {isIceTheme && <SnowfallEffect />}
      {isIceTheme && <WinterAnimals />}
      {currentTheme === "space" && (
        <Suspense fallback={null}>
          <CosmosArenaModel />
        </Suspense>
      )}
      {currentTheme === "space" && <SpaceBackground />}
      {currentTheme === "saturn" && (
        <Suspense fallback={null}>
          <ArabianNightsScene />
        </Suspense>
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

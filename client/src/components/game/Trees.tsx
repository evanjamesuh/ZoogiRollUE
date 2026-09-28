import * as THREE from "three";
import { useMemo, useRef, Suspense, useEffect } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { getTreePositions as getTreePositionsFromConstants } from "@/lib/arenaConstants";
import { getTreeOffset, getTreeRotation } from "@/lib/treeOffsets";

interface TreeData {
  id: string;
  position: [number, number, number];
  scale: number;
  rotation: number;
  modelType: "oak" | "pine";
}

interface TreeProps {
  tree: TreeData;
}

function StylizedTree({ tree }: TreeProps) {
  const groupRef = useRef<THREE.Group>(null);
  const wobbleRef = useRef({ intensity: 0, direction: 0 });
  const lastHitTimeRef = useRef(0);
  
  const oakModel = useGLTF("/models/stylized_oak_tree.glb");
  const pineModel = useGLTF("/models/stylized_pine_tree.glb");
  
  const model = tree.modelType === "oak" ? oakModel : pineModel;
  const clonedScene = useMemo(() => model.scene.clone(), [model.scene]);
  
  const playerEntity = useZoogiGame(state => state.playerEntity);
  const enemies = useZoogiGame(state => state.enemies);
  const { playSound } = useAudio();
  
  useFrame((state, delta) => {
    if (!groupRef.current) return;
    
    const checkCollision = (pos: [number, number, number]) => {
      const dx = pos[0] - tree.position[0];
      const dz = pos[2] - tree.position[2];
      const dist = Math.sqrt(dx * dx + dz * dz);
      return { hit: dist < 1.6, dx, dz };
    };
    
    let hitDirection: number | null = null;
    if (playerEntity) {
      const result = checkCollision(playerEntity.position);
      if (result.hit) {
        const now = Date.now();
        if (now - lastHitTimeRef.current > 300) {
          hitDirection = Math.atan2(result.dz, result.dx);
          lastHitTimeRef.current = now;
        }
      }
    }
    
    if (hitDirection === null) {
      enemies.forEach(enemy => {
        const result = checkCollision(enemy.position);
        if (result.hit) {
          const now = Date.now();
          if (now - lastHitTimeRef.current > 300) {
            hitDirection = Math.atan2(result.dz, result.dx);
            lastHitTimeRef.current = now;
          }
        }
      });
    }
    
    if (hitDirection !== null) {
      wobbleRef.current.intensity = 1;
      wobbleRef.current.direction = hitDirection;
      playSound("collision_tree");
    }
    
    const wobble = wobbleRef.current;
    if (wobble.intensity > 0) {
      const swayAngle = Math.sin(state.clock.elapsedTime * 15) * wobble.intensity * 0.5;
      const bendX = Math.cos(wobble.direction) * swayAngle;
      const bendZ = Math.sin(wobble.direction) * swayAngle;
      
      groupRef.current.rotation.x = bendX;
      groupRef.current.rotation.z = bendZ;
      
      const squash = 1 + Math.sin(state.clock.elapsedTime * 20) * wobble.intensity * 0.15;
      groupRef.current.scale.set(
        tree.scale * squash,
        tree.scale * (2 - squash),
        tree.scale * squash
      );
      
      wobble.intensity = Math.max(0, wobble.intensity - delta * 1.5);
    } else {
      groupRef.current.rotation.x = 0;
      groupRef.current.rotation.z = 0;
      groupRef.current.scale.set(tree.scale, tree.scale, tree.scale);
    }
  });

  const adjustedPosition: [number, number, number] = [tree.position[0], tree.position[1] + 0.75, tree.position[2]];
  
  return (
    <group ref={groupRef} position={adjustedPosition} scale={tree.scale} rotation={[0, tree.rotation, 0]}>
      <primitive object={clonedScene} />
    </group>
  );
}

function FallbackTree({ tree }: TreeProps) {
  const groupRef = useRef<THREE.Group>(null);
  const wobbleRef = useRef({ intensity: 0, direction: 0 });
  const lastHitTimeRef = useRef(0);
  
  const playerEntity = useZoogiGame(state => state.playerEntity);
  const enemies = useZoogiGame(state => state.enemies);
  const { playSound } = useAudio();
  
  const leafColor = tree.modelType === "oak" ? "#228B22" : "#2E8B2E";
  
  useFrame((state, delta) => {
    if (!groupRef.current) return;
    
    const checkCollision = (pos: [number, number, number]) => {
      const dx = pos[0] - tree.position[0];
      const dz = pos[2] - tree.position[2];
      const dist = Math.sqrt(dx * dx + dz * dz);
      return { hit: dist < 1.6, dx, dz };
    };
    
    let hitDirection: number | null = null;
    if (playerEntity) {
      const result = checkCollision(playerEntity.position);
      if (result.hit) {
        const now = Date.now();
        if (now - lastHitTimeRef.current > 300) {
          hitDirection = Math.atan2(result.dz, result.dx);
          lastHitTimeRef.current = now;
        }
      }
    }
    
    if (hitDirection === null) {
      enemies.forEach(enemy => {
        const result = checkCollision(enemy.position);
        if (result.hit) {
          const now = Date.now();
          if (now - lastHitTimeRef.current > 300) {
            hitDirection = Math.atan2(result.dz, result.dx);
            lastHitTimeRef.current = now;
          }
        }
      });
    }
    
    if (hitDirection !== null) {
      wobbleRef.current.intensity = 1;
      wobbleRef.current.direction = hitDirection;
      playSound("collision_tree");
    }
    
    const wobble = wobbleRef.current;
    if (wobble.intensity > 0) {
      const swayAngle = Math.sin(state.clock.elapsedTime * 15) * wobble.intensity * 0.5;
      const bendX = Math.cos(wobble.direction) * swayAngle;
      const bendZ = Math.sin(wobble.direction) * swayAngle;
      
      groupRef.current.rotation.x = bendX;
      groupRef.current.rotation.z = bendZ;
      
      const squash = 1 + Math.sin(state.clock.elapsedTime * 20) * wobble.intensity * 0.15;
      groupRef.current.scale.set(
        tree.scale * squash,
        tree.scale * (2 - squash),
        tree.scale * squash
      );
      
      wobble.intensity = Math.max(0, wobble.intensity - delta * 1.5);
    } else {
      groupRef.current.rotation.x = 0;
      groupRef.current.rotation.z = 0;
      groupRef.current.scale.set(tree.scale, tree.scale, tree.scale);
    }
  });

  return (
    <group ref={groupRef} position={tree.position} scale={tree.scale}>
      <mesh position={[0, 0.6, 0]} castShadow>
        <cylinderGeometry args={[0.15, 0.2, 1.2, 8]} />
        <meshStandardMaterial color="#8B4513" roughness={0.8} />
      </mesh>
      
      <mesh position={[0, 1.5, 0]} castShadow>
        <coneGeometry args={[0.8, 1.5, 8]} />
        <meshStandardMaterial color={leafColor} roughness={0.7} />
      </mesh>
      
      <mesh position={[0, 2.3, 0]} castShadow>
        <coneGeometry args={[0.6, 1.2, 8]} />
        <meshStandardMaterial color={leafColor} roughness={0.7} />
      </mesh>
      
      <mesh position={[0, 2.9, 0]} castShadow>
        <coneGeometry args={[0.4, 0.9, 8]} />
        <meshStandardMaterial color={leafColor} roughness={0.7} />
      </mesh>
    </group>
  );
}

function TreeWithFallback({ tree }: TreeProps) {
  return (
    <Suspense fallback={<FallbackTree tree={tree} />}>
      <StylizedTree tree={tree} />
    </Suspense>
  );
}

export function Trees() {
  const moveUpdateCounter = useZoogiGame((state) => state.moveUpdateCounter);
  
  const treeData = useMemo<(TreeData & { originalIndex: number })[]>(() => {
    const sharedPositions = getTreePositionsFromConstants();
    return sharedPositions.map((tree, i) => ({
      id: `tree-${i}`,
      position: tree.position,
      scale: 2.5 + (i % 4) * 0.3,
      rotation: (i * 1.7) % (Math.PI * 2),
      modelType: (i % 3 === 0 ? "oak" : "pine") as "oak" | "pine",
      originalIndex: i
    }));
  }, []);

  return (
    <group>
      {treeData.map((tree) => {
        const offset = getTreeOffset(tree.originalIndex);
        const rotationOffset = getTreeRotation(tree.originalIndex);
        const adjustedTree = {
          ...tree,
          position: [
            tree.position[0] + offset[0],
            tree.position[1] + offset[1],
            tree.position[2] + offset[2]
          ] as [number, number, number],
          rotation: tree.rotation + rotationOffset
        };
        return <TreeWithFallback key={`${tree.id}-${moveUpdateCounter}`} tree={adjustedTree} />;
      })}
    </group>
  );
}

useGLTF.preload("/models/stylized_oak_tree.glb");
useGLTF.preload("/models/stylized_pine_tree.glb");

import * as THREE from "three";
import { useMemo, useRef, Suspense } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { getSnowPinePositions } from "@/lib/arenaConstants";
import { getElementOffset } from "@/lib/treeOffsets";

interface SnowPineData {
  id: string;
  position: [number, number, number];
  scale: number;
  rotation: number;
  originalIndex: number;
}

function StylizedSnowPine({ tree }: { tree: SnowPineData }) {
  const groupRef = useRef<THREE.Group>(null);
  const wobbleRef = useRef({ intensity: 0, direction: 0 });
  const lastHitTimeRef = useRef(0);
  
  const model = useGLTF("/models/stylized_snow_pine.glb");
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
      const swayAngle = Math.sin(state.clock.elapsedTime * 15) * wobble.intensity * 0.4;
      const bendX = Math.cos(wobble.direction) * swayAngle;
      const bendZ = Math.sin(wobble.direction) * swayAngle;
      
      groupRef.current.rotation.x = bendX;
      groupRef.current.rotation.z = bendZ;
      
      wobble.intensity = Math.max(0, wobble.intensity - delta * 1.5);
    } else {
      groupRef.current.rotation.x = 0;
      groupRef.current.rotation.z = 0;
    }
  });

  return (
    <group ref={groupRef} position={tree.position} scale={tree.scale} rotation={[0, tree.rotation, 0]}>
      <primitive object={clonedScene} />
    </group>
  );
}

function FallbackSnowPine({ tree }: { tree: SnowPineData }) {
  return (
    <group position={tree.position} scale={tree.scale}>
      <mesh position={[0, 0.5, 0]} castShadow>
        <cylinderGeometry args={[0.12, 0.18, 1, 8]} />
        <meshStandardMaterial color="#5D4037" roughness={0.8} />
      </mesh>
      <mesh position={[0, 1.4, 0]} castShadow>
        <coneGeometry args={[0.7, 1.4, 8]} />
        <meshStandardMaterial color="#B8D4E3" roughness={0.7} />
      </mesh>
      <mesh position={[0, 2.2, 0]} castShadow>
        <coneGeometry args={[0.5, 1.1, 8]} />
        <meshStandardMaterial color="#C5E1EF" roughness={0.7} />
      </mesh>
      <mesh position={[0, 2.8, 0]} castShadow>
        <coneGeometry args={[0.3, 0.8, 8]} />
        <meshStandardMaterial color="#DCEEF7" roughness={0.7} />
      </mesh>
    </group>
  );
}

function SnowPineWithFallback({ tree }: { tree: SnowPineData }) {
  return (
    <Suspense fallback={<FallbackSnowPine tree={tree} />}>
      <StylizedSnowPine tree={tree} />
    </Suspense>
  );
}

export function SnowPines() {
  const moveUpdateCounter = useZoogiGame((state) => state.moveUpdateCounter);
  const selectedMap = useZoogiGame((state) => state.selectedMap) || "ice";
  
  const treeData = useMemo<SnowPineData[]>(() => {
    const sharedPositions = getSnowPinePositions();
    return sharedPositions.map((pos, i) => ({
      id: `snow-pine-${i}`,
      position: pos.position,
      scale: 2.5 + (i % 4) * 0.4,
      rotation: (i * 2.1) % (Math.PI * 2),
      originalIndex: i
    }));
  }, []);

  return (
    <group>
      {treeData.map((tree) => {
        const offset = getElementOffset(selectedMap, "snowPine", tree.originalIndex);
        const adjustedTree = {
          ...tree,
          position: [
            tree.position[0] + offset[0],
            tree.position[1] + offset[1],
            tree.position[2] + offset[2]
          ] as [number, number, number]
        };
        return <SnowPineWithFallback key={`${tree.id}-${moveUpdateCounter}`} tree={adjustedTree} />;
      })}
    </group>
  );
}

useGLTF.preload("/models/stylized_snow_pine.glb");

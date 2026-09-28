import * as THREE from "three";
import { useMemo, useRef, Suspense } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";

interface HoodooData {
  id: string;
  position: [number, number, number];
  scale: number;
  rotation: number;
}

function StylizedHoodoo({ hoodoo }: { hoodoo: HoodooData }) {
  const groupRef = useRef<THREE.Group>(null);
  const wobbleRef = useRef({ intensity: 0 });
  const lastHitTimeRef = useRef(0);
  
  const model = useGLTF("/models/stylized_desert_hoodoo.glb");
  const clonedScene = useMemo(() => model.scene.clone(), [model.scene]);
  
  const playerEntity = useZoogiGame(state => state.playerEntity);
  const enemies = useZoogiGame(state => state.enemies);
  const { playSound } = useAudio();
  
  useFrame((state, delta) => {
    if (!groupRef.current) return;
    
    const checkCollision = (pos: [number, number, number]) => {
      const dx = pos[0] - hoodoo.position[0];
      const dz = pos[2] - hoodoo.position[2];
      const dist = Math.sqrt(dx * dx + dz * dz);
      return dist < 1.8;
    };
    
    let wasHit = false;
    if (playerEntity && checkCollision(playerEntity.position)) {
      const now = Date.now();
      if (now - lastHitTimeRef.current > 500) {
        wasHit = true;
        lastHitTimeRef.current = now;
      }
    }
    
    enemies.forEach(enemy => {
      if (checkCollision(enemy.position)) {
        const now = Date.now();
        if (now - lastHitTimeRef.current > 500) {
          wasHit = true;
          lastHitTimeRef.current = now;
        }
      }
    });
    
    if (wasHit) {
      wobbleRef.current.intensity = 1;
      playSound("collision_mushroom");
    }
    
    if (wobbleRef.current.intensity > 0) {
      const shake = Math.sin(state.clock.elapsedTime * 30) * wobbleRef.current.intensity * 0.05;
      groupRef.current.rotation.z = shake;
      wobbleRef.current.intensity = Math.max(0, wobbleRef.current.intensity - delta * 2);
    } else {
      groupRef.current.rotation.z = 0;
    }
  });

  return (
    <group ref={groupRef} position={hoodoo.position} scale={hoodoo.scale} rotation={[0, hoodoo.rotation, 0]}>
      <primitive object={clonedScene} />
    </group>
  );
}

function FallbackHoodoo({ hoodoo }: { hoodoo: HoodooData }) {
  return (
    <group position={hoodoo.position} scale={hoodoo.scale}>
      <mesh position={[0, 0.8, 0]} castShadow>
        <cylinderGeometry args={[0.3, 0.5, 1.6, 8]} />
        <meshStandardMaterial color="#CD853F" roughness={0.9} />
      </mesh>
      <mesh position={[0, 2.0, 0]} castShadow>
        <cylinderGeometry args={[0.6, 0.3, 0.8, 8]} />
        <meshStandardMaterial color="#D2691E" roughness={0.9} />
      </mesh>
    </group>
  );
}

function HoodooWithFallback({ hoodoo }: { hoodoo: HoodooData }) {
  return (
    <Suspense fallback={<FallbackHoodoo hoodoo={hoodoo} />}>
      <StylizedHoodoo hoodoo={hoodoo} />
    </Suspense>
  );
}

export function DesertHoodoos() {
  const hoodooData = useMemo<HoodooData[]>(() => {
    const positions: HoodooData[] = [];
    const count = 6;
    
    for (let i = 0; i < count; i++) {
      const angle = (i / count) * Math.PI * 2 + 0.3;
      const radius = 10 + (i % 2) * 4;
      const x = Math.cos(angle) * radius;
      const z = Math.sin(angle) * radius;
      
      if (z > 8 && Math.abs(x) < 5) continue;
      
      const scale = 2.0 + (i % 3) * 0.4;
      positions.push({
        id: `hoodoo-${i}`,
        position: [x, 0, z],
        scale,
        rotation: (i * 1.8) % (Math.PI * 2)
      });
    }
    
    return positions;
  }, []);

  return (
    <group>
      {hoodooData.map((hoodoo) => (
        <HoodooWithFallback key={hoodoo.id} hoodoo={hoodoo} />
      ))}
    </group>
  );
}

export function getHoodooPositions(): { position: [number, number, number]; radius: number }[] {
  const positions: { position: [number, number, number]; radius: number }[] = [];
  const count = 6;
  
  for (let i = 0; i < count; i++) {
    const angle = (i / count) * Math.PI * 2 + 0.3;
    const radius = 10 + (i % 2) * 4;
    const x = Math.cos(angle) * radius;
    const z = Math.sin(angle) * radius;
    
    if (z > 8 && Math.abs(x) < 5) continue;
    
    positions.push({
      position: [x, 0, z],
      radius: 1.2
    });
  }
  
  return positions;
}

useGLTF.preload("/models/stylized_desert_hoodoo.glb");

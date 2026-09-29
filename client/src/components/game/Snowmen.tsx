import * as THREE from "three";
import { useRef, useMemo, useState } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { getSnowmanPositions } from "@/lib/arenaConstants";
import { SNOWMAN_RADIUS } from "@/lib/arenaColliders";
import { getElementOffset } from "@/lib/treeOffsets";

const scarfMaterial = new THREE.MeshStandardMaterial({ color: "#e23b4a", roughness: 0.55 });
const coalMaterial = new THREE.MeshStandardMaterial({ color: "#161616", roughness: 0.45 });
const twigMaterial = new THREE.MeshStandardMaterial({ color: "#6a4328", roughness: 0.8 });

interface Snowman {
  id: string;
  position: [number, number, number];
  originalIndex: number;
}

export function Snowmen() {
  const moveUpdateCounter = useZoogiGame((state) => state.moveUpdateCounter);
  const selectedMap = useZoogiGame((state) => state.selectedMap) || "ice";
  
  const snowmenData = useMemo<Snowman[]>(() => {
    const sharedPositions = getSnowmanPositions();
    return sharedPositions.map((pos, i) => ({
      id: `snowman-${i}`,
      position: pos.position,
      originalIndex: i
    }));
  }, []);

  return (
    <group>
      {snowmenData.map((snowman) => {
        const offset = getElementOffset(selectedMap, "snowman", snowman.originalIndex);
        const adjustedSnowman = {
          ...snowman,
          position: [
            snowman.position[0] + offset[0],
            snowman.position[1] + offset[1],
            snowman.position[2] + offset[2]
          ] as [number, number, number]
        };
        return <SnowmanMesh key={`${snowman.id}-${moveUpdateCounter}`} snowman={adjustedSnowman} />;
      })}
    </group>
  );
}

function SnowmanMesh({ snowman }: { snowman: Snowman }) {
  const groupRef = useRef<THREE.Group>(null);
  const [jiggleIntensity, setJiggleIntensity] = useState(0);
  const lastHitTimeRef = useRef(0);
  
  const { playerEntity, enemies } = useZoogiGame();
  const { playSound } = useAudio();
  
  useFrame((state, delta) => {
    if (!groupRef.current) return;
    
    const checkCollision = (pos: [number, number, number]) => {
      const dx = pos[0] - snowman.position[0];
      const dz = pos[2] - snowman.position[2];
      const dist = Math.sqrt(dx * dx + dz * dz);
      return dist < 1.2;
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
      setJiggleIntensity(1);
      playSound("collision_snowman");
    }
    
    if (jiggleIntensity > 0) {
      const jiggle = Math.sin(state.clock.elapsedTime * 25) * jiggleIntensity * 0.1;
      groupRef.current.rotation.z = jiggle;
      groupRef.current.scale.set(
        1 + jiggle * 0.5,
        1 - jiggle * 0.3,
        1 + jiggle * 0.5
      );
      setJiggleIntensity(prev => Math.max(0, prev - delta * 3));
    } else {
      groupRef.current.rotation.z = 0;
      groupRef.current.scale.set(1, 1, 1);
    }
  });

  return (
    <group ref={groupRef} position={snowman.position}>
      <mesh position={[0, SNOWMAN_RADIUS, 0]} castShadow>
        <sphereGeometry args={[SNOWMAN_RADIUS, 12, 8]} />
        <meshStandardMaterial color="#FFFFFF" roughness={0.8} />
      </mesh>
      
      <mesh position={[0, 1.1, 0]} castShadow>
        <sphereGeometry args={[0.35, 12, 8]} />
        <meshStandardMaterial color="#FFFFFF" roughness={0.8} />
      </mesh>
      
      <mesh position={[0, 1.55, 0]} castShadow>
        <sphereGeometry args={[0.25, 12, 8]} />
        <meshStandardMaterial color="#FFFFFF" roughness={0.8} />
      </mesh>
      
      <mesh position={[0.1, 1.6, 0.24]}>
        <sphereGeometry args={[0.06, 8, 6]} />
        <meshStandardMaterial color="#000000" />
      </mesh>
      <mesh position={[-0.1, 1.6, 0.24]}>
        <sphereGeometry args={[0.06, 8, 6]} />
        <meshStandardMaterial color="#000000" />
      </mesh>
      
      <mesh position={[0, 1.5, 0.24]} rotation={[Math.PI / 2, 0, 0]}>
        <coneGeometry args={[0.05, 0.2, 8]} />
        <meshStandardMaterial color="#FF6600" />
      </mesh>

      <mesh position={[0, 1.32, 0]} rotation={[0.2, 0.4, 0]} material={scarfMaterial}>
        <torusGeometry args={[0.27, 0.055, 6, 12]} />
      </mesh>
      <mesh position={[0.16, 1.16, 0.2]} rotation={[0.5, 0.2, 0.6]} material={scarfMaterial}>
        <boxGeometry args={[0.07, 0.24, 0.04]} />
      </mesh>
      {[
        [1.24, 0.3],
        [1.08, 0.33],
        [0.94, 0.3],
      ].map(([y, z]) => (
        <mesh key={y} position={[0, y, z]} material={coalMaterial}>
          <sphereGeometry args={[0.045, 6, 6]} />
        </mesh>
      ))}
      <mesh position={[0.46, 1.12, 0]} rotation={[0, 0, Math.PI / 2.6]} material={twigMaterial}>
        <cylinderGeometry args={[0.028, 0.028, 0.42, 5]} />
      </mesh>
      <mesh position={[-0.46, 1.12, 0]} rotation={[0, 0, -Math.PI / 2.6]} material={twigMaterial}>
        <cylinderGeometry args={[0.028, 0.028, 0.42, 5]} />
      </mesh>
      
      <mesh position={[0, 1.85, 0]}>
        <cylinderGeometry args={[0.18, 0.22, 0.15, 12]} />
        <meshStandardMaterial color="#2a2a2a" />
      </mesh>
      <mesh position={[0, 1.95, 0]}>
        <cylinderGeometry args={[0.12, 0.12, 0.2, 12]} />
        <meshStandardMaterial color="#2a2a2a" />
      </mesh>
    </group>
  );
}

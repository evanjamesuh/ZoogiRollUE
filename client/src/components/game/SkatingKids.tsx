import * as THREE from "three";
import { useRef, useState, useEffect, type MutableRefObject } from "react";
import { useFrame } from "@react-three/fiber";
import { ARENA_RADIUS, setSkaterPositions } from "@/lib/arenaConstants";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

const SKATER_COUNT = 5;
const SKATER_RADIUS = 0.6;

const SNOWMAN_POSITIONS: [number, number, number][] = [];
for (let i = 0; i < 8; i++) {
  const angle = (i / 8) * Math.PI * 2 + 0.4;
  const radius = 7 + (i % 3) * 4;
  SNOWMAN_POSITIONS.push([Math.cos(angle) * radius, 0, Math.sin(angle) * radius]);
}

interface Skater {
  id: number;
  color: string;
  hatColor: string;
  position: [number, number, number];
  velocity: [number, number, number];
  targetX: number;
  targetZ: number;
  speed: number;
}


function SkaterMesh({ skaterRef, index }: { skaterRef: MutableRefObject<Skater[]>; index: number }) {
  const groupRef = useRef<THREE.Group>(null);
  const skater = skaterRef.current[index];
  
  useFrame(() => {
    if (groupRef.current && skaterRef.current[index]) {
      const s = skaterRef.current[index];
      groupRef.current.position.set(s.position[0], s.position[1], s.position[2]);
      
      const speed = Math.sqrt(s.velocity[0] ** 2 + s.velocity[2] ** 2);
      if (speed > 0.01) {
        const angle = Math.atan2(s.velocity[0], s.velocity[2]);
        groupRef.current.rotation.y = angle;
      }
    }
  });
  
  return (
    <group ref={groupRef} position={skater.position}>
      <mesh position={[0, 0.35, 0]} castShadow>
        <capsuleGeometry args={[0.18, 0.35, 8, 8]} />
        <meshStandardMaterial color={skater.color} />
      </mesh>
      
      <mesh position={[0, 0.7, 0]} castShadow>
        <sphereGeometry args={[0.16, 16, 16]} />
        <meshStandardMaterial color="#FFE4C4" />
      </mesh>
      
      <mesh position={[0, 0.88, 0]} castShadow>
        <cylinderGeometry args={[0.12, 0.18, 0.15, 16]} />
        <meshStandardMaterial color={skater.hatColor} />
      </mesh>
      
      <mesh position={[-0.22, 0.35, 0]} rotation={[0, 0, -0.4]} castShadow>
        <capsuleGeometry args={[0.05, 0.2, 4, 4]} />
        <meshStandardMaterial color={skater.color} />
      </mesh>
      <mesh position={[0.22, 0.35, 0]} rotation={[0, 0, 0.4]} castShadow>
        <capsuleGeometry args={[0.05, 0.2, 4, 4]} />
        <meshStandardMaterial color={skater.color} />
      </mesh>
      
      <group position={[-0.07, 0.08, 0]}>
        <mesh castShadow>
          <boxGeometry args={[0.1, 0.08, 0.22]} />
          <meshStandardMaterial color="#4A3728" />
        </mesh>
        <mesh position={[0, -0.05, 0]}>
          <boxGeometry args={[0.02, 0.04, 0.35]} />
          <meshStandardMaterial color="#C0C0C0" metalness={0.9} roughness={0.1} />
        </mesh>
      </group>
      
      <group position={[0.07, 0.08, 0]}>
        <mesh castShadow>
          <boxGeometry args={[0.1, 0.08, 0.22]} />
          <meshStandardMaterial color="#4A3728" />
        </mesh>
        <mesh position={[0, -0.05, 0]}>
          <boxGeometry args={[0.02, 0.04, 0.35]} />
          <meshStandardMaterial color="#C0C0C0" metalness={0.9} roughness={0.1} />
        </mesh>
      </group>
    </group>
  );
}

export function SkatingKids() {
  const skatersRef = useRef<Skater[]>([]);
  const [initialized, setInitialized] = useState(false);
  const playerEntity = useZoogiGame(state => state.playerEntity);
  const enemies = useZoogiGame(state => state.enemies);
  
  useEffect(() => {
    const colors = ["#3B82F6", "#EF4444", "#22C55E", "#F59E0B", "#8B5CF6"];
    const hatColors = ["#DC2626", "#2563EB", "#059669", "#D97706", "#7C3AED"];
    
    skatersRef.current = Array.from({ length: SKATER_COUNT }, (_, i) => {
      const angle = (i / SKATER_COUNT) * Math.PI * 2 + Math.random() * 0.5;
      const radius = 5 + Math.random() * 8;
      const x = Math.cos(angle) * radius;
      const z = Math.sin(angle) * radius;
      
      return {
        id: i,
        color: colors[i % colors.length],
        hatColor: hatColors[i % hatColors.length],
        position: [x, 0, z] as [number, number, number],
        velocity: [0, 0, 0] as [number, number, number],
        targetX: (Math.random() - 0.5) * (ARENA_RADIUS * 1.5),
        targetZ: (Math.random() - 0.5) * (ARENA_RADIUS * 1.5),
        speed: 0.04 + Math.random() * 0.03
      };
    });
    setInitialized(true);
    
    return () => {
      setSkaterPositions([]);
      skatersRef.current = [];
    };
  }, []);
  
  useFrame((_, delta) => {
    if (!initialized) return;
    
    skatersRef.current.forEach((skater) => {
      let avoidX = 0;
      let avoidZ = 0;
      
      SNOWMAN_POSITIONS.forEach(snowmanPos => {
        const dx = skater.position[0] - snowmanPos[0];
        const dz = skater.position[2] - snowmanPos[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        const avoidRadius = 2.5;
        if (dist < avoidRadius && dist > 0.1) {
          const force = (avoidRadius - dist) / avoidRadius * 0.15;
          avoidX += (dx / dist) * force;
          avoidZ += (dz / dist) * force;
        }
      });
      
      const zoogiPositions: [number, number, number][] = [];
      if (playerEntity) {
        zoogiPositions.push(playerEntity.position);
      }
      enemies.forEach(enemy => {
        zoogiPositions.push(enemy.position);
      });
      
      zoogiPositions.forEach(zoogiPos => {
        const dx = skater.position[0] - zoogiPos[0];
        const dz = skater.position[2] - zoogiPos[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        const avoidRadius = 3.0;
        if (dist < avoidRadius && dist > 0.1) {
          const force = (avoidRadius - dist) / avoidRadius * 0.25;
          avoidX += (dx / dist) * force;
          avoidZ += (dz / dist) * force;
        }
      });
      
      const toTargetX = skater.targetX - skater.position[0];
      const toTargetZ = skater.targetZ - skater.position[2];
      const distToTarget = Math.sqrt(toTargetX * toTargetX + toTargetZ * toTargetZ);
      
      if (distToTarget < 1.5 || Math.random() < 0.008) {
        const newAngle = Math.random() * Math.PI * 2;
        const newRadius = 3 + Math.random() * (ARENA_RADIUS - 5);
        skater.targetX = Math.cos(newAngle) * newRadius;
        skater.targetZ = Math.sin(newAngle) * newRadius;
      }
      
      if (distToTarget > 0.1) {
        const dirX = toTargetX / distToTarget;
        const dirZ = toTargetZ / distToTarget;
        
        skater.velocity[0] += (dirX * skater.speed + avoidX) * delta * 60;
        skater.velocity[2] += (dirZ * skater.speed + avoidZ) * delta * 60;
        
        const maxSpeed = 0.18;
        const currentSpeed = Math.sqrt(skater.velocity[0] ** 2 + skater.velocity[2] ** 2);
        if (currentSpeed > maxSpeed) {
          skater.velocity[0] = (skater.velocity[0] / currentSpeed) * maxSpeed;
          skater.velocity[2] = (skater.velocity[2] / currentSpeed) * maxSpeed;
        }
      }
      
      skater.velocity[0] *= 0.97;
      skater.velocity[2] *= 0.97;
      
      skater.position[0] += skater.velocity[0];
      skater.position[2] += skater.velocity[2];
      
      const distFromCenter = Math.sqrt(skater.position[0] ** 2 + skater.position[2] ** 2);
      const maxDist = ARENA_RADIUS - 1;
      if (distFromCenter > maxDist) {
        const nx = skater.position[0] / distFromCenter;
        const nz = skater.position[2] / distFromCenter;
        skater.position[0] = nx * maxDist;
        skater.position[2] = nz * maxDist;
        
        const dot = skater.velocity[0] * nx + skater.velocity[2] * nz;
        skater.velocity[0] -= 1.5 * dot * nx;
        skater.velocity[2] -= 1.5 * dot * nz;
        
        const newAngle = Math.random() * Math.PI * 2;
        const newRadius = 3 + Math.random() * 5;
        skater.targetX = Math.cos(newAngle) * newRadius;
        skater.targetZ = Math.sin(newAngle) * newRadius;
      }
    });
    
    setSkaterPositions(skatersRef.current.map(s => ({
      position: [...s.position] as [number, number, number],
      velocity: [...s.velocity] as [number, number, number],
      radius: SKATER_RADIUS
    })));
  });
  
  if (!initialized) return null;
  
  return (
    <group>
      {skatersRef.current.map((_, index) => (
        <SkaterMesh key={index} skaterRef={skatersRef} index={index} />
      ))}
    </group>
  );
}

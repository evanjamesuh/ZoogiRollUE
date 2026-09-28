import * as THREE from "three";
import { useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { ARENA_RADIUS } from "@/lib/arenaConstants";

const GROUND_LEVEL = -2.9;
const GROUND_SIZE = ARENA_RADIUS * 4;

function FallingOrb({ position, color, hasLanded }: { position: [number, number, number]; color: string; hasLanded: boolean }) {
  const meshRef = useRef<THREE.Mesh>(null);
  
  useFrame((state) => {
    if (meshRef.current && !hasLanded) {
      meshRef.current.rotation.x += 0.1;
      meshRef.current.rotation.z += 0.05;
    }
  });
  
  return (
    <mesh ref={meshRef} position={position}>
      <sphereGeometry args={[0.4, 16, 16]} />
      <meshStandardMaterial 
        color={color} 
        emissive={color} 
        emissiveIntensity={hasLanded ? 0.1 : 0.3}
        transparent
        opacity={hasLanded ? 0.5 : 1}
      />
    </mesh>
  );
}

function FallingZoogi({ position, color, zoogiId, hasLanded }: { position: [number, number, number]; color: string; zoogiId?: string; hasLanded: boolean }) {
  const meshRef = useRef<THREE.Group>(null);
  
  useFrame((state) => {
    if (meshRef.current && !hasLanded) {
      meshRef.current.rotation.x += 0.15;
      meshRef.current.rotation.z += 0.1;
    }
  });
  
  return (
    <group ref={meshRef} position={position}>
      <mesh>
        <sphereGeometry args={[0.5, 16, 16]} />
        <meshStandardMaterial 
          color={color}
          transparent
          opacity={hasLanded ? 0.5 : 1}
        />
      </mesh>
      {!hasLanded && (
        <pointLight color={color} intensity={0.5} distance={3} />
      )}
    </group>
  );
}

export function FallingEntities() {
  const fallingEntities = useZoogiGame((state) => state.fallingEntities);
  
  if (!fallingEntities || fallingEntities.length === 0) return null;
  
  return (
    <group>
      {fallingEntities.map((entity) => {
        if (entity.entityType === "orb") {
          return (
            <FallingOrb
              key={entity.id}
              position={entity.position}
              color={entity.color}
              hasLanded={entity.hasLanded}
            />
          );
        } else {
          return (
            <FallingZoogi
              key={entity.id}
              position={entity.position}
              color={entity.color}
              zoogiId={entity.zoogiId}
              hasLanded={entity.hasLanded}
            />
          );
        }
      })}
    </group>
  );
}

export function GroundPlane() {
  const selectedMap = useZoogiGame(state => state.selectedMap);
  
  const groundColors: Record<string, { color: string; emissive: string }> = {
    grass: { color: "#2D5A27", emissive: "#1a3a18" },
    ice: { color: "#1a4a6a", emissive: "#0a2a3a" },
    lava: { color: "#1a0a0a", emissive: "#3a1a0a" },
    space: { color: "#0a0a1a", emissive: "#1a0a3a" },
    saturn: { color: "#1a1a0a", emissive: "#0a0a0a" }
  };
  
  const theme = selectedMap || "grass";
  const colors = groundColors[theme] || groundColors.grass;
  
  return (
    <group>
      <mesh 
        rotation={[-Math.PI / 2, 0, 0]} 
        position={[0, GROUND_LEVEL, 0]} 
        receiveShadow
      >
        <planeGeometry args={[GROUND_SIZE, GROUND_SIZE]} />
        <meshStandardMaterial 
          color={colors.color}
          emissive={colors.emissive}
          emissiveIntensity={0.2}
          roughness={0.9}
        />
      </mesh>
      
      <ambientLight intensity={0.1} />
    </group>
  );
}

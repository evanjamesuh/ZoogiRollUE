import * as THREE from "three";
import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { ARENA_RADIUS } from "@/lib/arenaConstants";

interface RockData {
  position: [number, number, number];
  scale: number;
  rotation: [number, number, number];
  color: string;
  roughness: number;
}

interface GrassClumpData {
  position: [number, number, number];
  rotation: number;
  scale: number;
  bladeCount: number;
}

function Rock({ data }: { data: RockData }) {
  const geometryRef = useRef<THREE.IcosahedronGeometry>(null);
  
  const geometry = useMemo(() => {
    const geo = new THREE.IcosahedronGeometry(1, 0);
    const positionAttr = geo.getAttribute('position');
    const positions = positionAttr.array as Float32Array;
    
    for (let i = 0; i < positions.length; i += 3) {
      positions[i] *= 0.8 + Math.random() * 0.4;
      positions[i + 1] *= 0.6 + Math.random() * 0.3;
      positions[i + 2] *= 0.8 + Math.random() * 0.4;
    }
    
    positionAttr.needsUpdate = true;
    geo.computeVertexNormals();
    return geo;
  }, []);

  return (
    <mesh 
      position={data.position} 
      scale={data.scale} 
      rotation={data.rotation}
      geometry={geometry}
    >
      <meshStandardMaterial 
        color={data.color} 
        roughness={data.roughness}
        metalness={0.1}
      />
    </mesh>
  );
}

function GrassClump({ data }: { data: GrassClumpData }) {
  const groupRef = useRef<THREE.Group>(null);
  
  useFrame((state) => {
    if (groupRef.current) {
      const windOffset = data.position[0] * 0.1 + data.position[2] * 0.1;
      const sway = Math.sin(state.clock.elapsedTime * 2 + windOffset) * 0.1;
      groupRef.current.rotation.z = sway;
    }
  });

  const blades = useMemo(() => {
    const result = [];
    for (let i = 0; i < data.bladeCount; i++) {
      const angle = (i / data.bladeCount) * Math.PI * 2;
      const radius = Math.random() * 0.3;
      const x = Math.cos(angle) * radius;
      const z = Math.sin(angle) * radius;
      const height = 0.4 + Math.random() * 0.4;
      const rotY = Math.random() * Math.PI;
      result.push({ x, z, height, rotY, key: i });
    }
    return result;
  }, [data.bladeCount]);

  return (
    <group ref={groupRef} position={data.position} rotation={[0, data.rotation, 0]} scale={data.scale}>
      {blades.map((blade) => (
        <mesh key={blade.key} position={[blade.x, blade.height / 2, blade.z]} rotation={[0, blade.rotY, 0]}>
          <coneGeometry args={[0.05, blade.height, 4]} />
          <meshStandardMaterial color="#3a7d32" />
        </mesh>
      ))}
    </group>
  );
}

function FlowerCluster({ position, color }: { position: [number, number, number]; color: string }) {
  const flowers = useMemo(() => {
    const result = [];
    const count = 3 + Math.floor(Math.random() * 4);
    for (let i = 0; i < count; i++) {
      const angle = (i / count) * Math.PI * 2;
      const radius = Math.random() * 0.5;
      const x = Math.cos(angle) * radius;
      const z = Math.sin(angle) * radius;
      const stemHeight = 0.3 + Math.random() * 0.3;
      result.push({ x, z, stemHeight, key: i });
    }
    return result;
  }, []);

  return (
    <group position={position}>
      {flowers.map((flower) => (
        <group key={flower.key} position={[flower.x, 0, flower.z]}>
          <mesh position={[0, flower.stemHeight / 2, 0]}>
            <cylinderGeometry args={[0.02, 0.02, flower.stemHeight, 4]} />
            <meshStandardMaterial color="#2d5a27" />
          </mesh>
          <mesh position={[0, flower.stemHeight, 0]}>
            <sphereGeometry args={[0.08, 8, 8]} />
            <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.2} />
          </mesh>
        </group>
      ))}
    </group>
  );
}

export function EnvironmentDecorations() {
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  
  const decorations = useMemo(() => {
    const rocks: RockData[] = [];
    const grassClumps: GrassClumpData[] = [];
    const flowers: { position: [number, number, number]; color: string; key: number }[] = [];
    
    const startRadius = ARENA_RADIUS + 2;
    const endRadius = ARENA_RADIUS * 2.5;
    
    const rockColors: Record<string, string[]> = {
      grass: ["#5a5a5a", "#6a6a6a", "#4a4a4a", "#7a7a7a"],
      ice: ["#8ab4c4", "#9ac4d4", "#7aa4b4", "#aad4e4"],
      lava: ["#2a1a1a", "#3a2020", "#1a0a0a", "#4a2a2a"],
      space: ["#3a3a5a", "#4a4a6a", "#2a2a4a", "#5a5a7a"],
      saturn: ["#6a5a4a", "#7a6a5a", "#5a4a3a", "#8a7a6a"],
      tomb: ["#c4a070", "#b89060", "#d7b48a", "#a07848"]
    };
    
    const flowerColors = ["#ff69b4", "#ffd700", "#ff4500", "#9932cc", "#00ced1"];
    
    const theme = selectedMap || "grass";
    const colors = rockColors[theme] || rockColors.grass;
    
    for (let i = 0; i < 25; i++) {
      const angle = (i / 25) * Math.PI * 2 + Math.random() * 0.3;
      const radius = startRadius + Math.random() * (endRadius - startRadius);
      const x = Math.cos(angle) * radius;
      const z = Math.sin(angle) * radius;
      
      rocks.push({
        position: [x, 0.5 + Math.random() * 0.3, z],
        scale: 0.3 + Math.random() * 0.6,
        rotation: [Math.random() * 0.3, Math.random() * Math.PI * 2, Math.random() * 0.3],
        color: colors[Math.floor(Math.random() * colors.length)],
        roughness: 0.7 + Math.random() * 0.3
      });
    }
    
    if (theme === "grass" || theme === "saturn") {
      for (let i = 0; i < 40; i++) {
        const angle = Math.random() * Math.PI * 2;
        const radius = startRadius + Math.random() * (endRadius - startRadius);
        const x = Math.cos(angle) * radius;
        const z = Math.sin(angle) * radius;
        
        grassClumps.push({
          position: [x, 0, z],
          rotation: Math.random() * Math.PI * 2,
          scale: 0.8 + Math.random() * 0.6,
          bladeCount: 5 + Math.floor(Math.random() * 6)
        });
      }
      
      for (let i = 0; i < 15; i++) {
        const angle = Math.random() * Math.PI * 2;
        const radius = startRadius + Math.random() * (endRadius - startRadius);
        const x = Math.cos(angle) * radius;
        const z = Math.sin(angle) * radius;
        
        flowers.push({
          position: [x, 0, z],
          color: flowerColors[Math.floor(Math.random() * flowerColors.length)],
          key: i
        });
      }
    }
    
    return { rocks, grassClumps, flowers };
  }, [selectedMap]);

  return (
    <group>
      {decorations.rocks.map((rock, i) => (
        <Rock key={`rock-${i}`} data={rock} />
      ))}
      {decorations.grassClumps.map((clump, i) => (
        <GrassClump key={`grass-${i}`} data={clump} />
      ))}
      {decorations.flowers.map((flower) => (
        <FlowerCluster key={`flower-${flower.key}`} position={flower.position} color={flower.color} />
      ))}
    </group>
  );
}

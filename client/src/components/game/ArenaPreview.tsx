import { Canvas, useFrame } from "@react-three/fiber";
import { useRef, useMemo } from "react";
import * as THREE from "three";
import { MapTheme } from "@/lib/stores/useZoogiGame";

interface ArenaPreviewProps {
  mapId: MapTheme;
  large?: boolean;
}

function MiniArena({ mapId }: { mapId: MapTheme }) {
  const groupRef = useRef<THREE.Group>(null);
  
  useFrame((_, delta) => {
    if (groupRef.current) {
      groupRef.current.rotation.y += delta * 0.3;
    }
  });
  
  const arenaColor = useMemo(() => {
    switch (mapId) {
      case "grass": return "#4CAF50";
      case "ice": return "#81D4FA";
      case "lava": return "#FF5722";
      case "space": return "#7C4DFF";
      default: return "#4CAF50";
    }
  }, [mapId]);
  
  const obstacles = useMemo(() => {
    switch (mapId) {
      case "grass":
        return (
          <>
            <StylizedOakTree position={[0.6, 0, 0.2]} />
            <StylizedPineTree position={[-0.5, 0, -0.4]} />
            <StylizedOakTree position={[0.1, 0, -0.6]} />
            <Mushroom position={[0.4, 0, -0.3]} color="#EF4444" />
            <Mushroom position={[-0.3, 0, 0.5]} color="#F97316" />
            <FlowerPatch position={[0.2, 0, 0.5]} color="#FF69B4" />
            <FlowerPatch position={[-0.6, 0, 0.2]} color="#FFEB3B" />
          </>
        );
      case "ice":
        return (
          <>
            <Snowman position={[0.5, 0, 0.2]} />
            <Snowman position={[-0.4, 0, -0.3]} />
            <SnowPine position={[0.6, 0, -0.4]} />
            <SnowPine position={[-0.6, 0, 0.3]} />
            <SnowPine position={[0.1, 0, 0.6]} />
            <IcePatch position={[0, 0.01, 0]} />
          </>
        );
      case "lava":
        return (
          <>
            <LavaRock position={[0.4, 0, 0.2]} />
            <LavaRock position={[-0.5, 0, -0.2]} />
            <DesertHoodoo position={[0.6, 0, -0.3]} />
            <DesertHoodoo position={[-0.6, 0, 0.4]} />
            <DesertHoodoo position={[0.1, 0, -0.6]} />
            <LavaGlow position={[0, 0.02, 0]} />
          </>
        );
      case "space":
        return (
          <>
            <Alien position={[0.4, 0, 0.3]} />
            <Alien position={[-0.3, 0, -0.4]} />
            <AlienCrystalSpire position={[0.6, 0, -0.3]} />
            <AlienCrystalSpire position={[-0.6, 0, 0.2]} />
            <AlienCrystalSpire position={[0.1, 0, 0.6]} />
            <Crystal position={[0.5, 0, 0.5]} />
          </>
        );
      default:
        return null;
    }
  }, [mapId]);
  
  return (
    <group ref={groupRef}>
      <mesh position={[0, -0.06, 0]}>
        <cylinderGeometry args={[1.0, 1.0, 0.12, 32]} />
        <meshStandardMaterial color="#1a1a2e" />
      </mesh>
      
      <mesh position={[0, 0.01, 0]}>
        <cylinderGeometry args={[0.95, 0.95, 0.02, 32]} />
        <meshStandardMaterial color={arenaColor} />
      </mesh>
      
      <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.85, 0.95, 32]} />
        <meshStandardMaterial color="#ffffff" opacity={0.3} transparent side={THREE.DoubleSide} />
      </mesh>
      
      {obstacles}
      
      {mapId === "grass" && <MovingTrain />}
      {mapId === "ice" && <SkatingKids />}
    </group>
  );
}

function MovingTrain() {
  const trainRef = useRef<THREE.Group>(null);
  const angleRef = useRef(0);
  
  useFrame((_, delta) => {
    if (trainRef.current) {
      angleRef.current += delta * 1.2;
      const radius = 0.65;
      trainRef.current.position.x = Math.cos(angleRef.current) * radius;
      trainRef.current.position.z = Math.sin(angleRef.current) * radius;
      trainRef.current.rotation.y = -angleRef.current + Math.PI / 2;
    }
  });
  
  return (
    <group ref={trainRef} position={[0.65, 0.08, 0]}>
      <mesh position={[0, 0, 0]}>
        <boxGeometry args={[0.12, 0.08, 0.06]} />
        <meshStandardMaterial color="#E53935" />
      </mesh>
      <mesh position={[0.03, 0.05, 0]}>
        <boxGeometry args={[0.04, 0.04, 0.05]} />
        <meshStandardMaterial color="#1565C0" />
      </mesh>
      <mesh position={[-0.08, 0, 0]}>
        <boxGeometry args={[0.06, 0.06, 0.05]} />
        <meshStandardMaterial color="#FFC107" />
      </mesh>
      <mesh position={[-0.14, 0, 0]}>
        <boxGeometry args={[0.05, 0.05, 0.04]} />
        <meshStandardMaterial color="#4CAF50" />
      </mesh>
      <mesh position={[0.06, -0.03, 0.02]}>
        <cylinderGeometry args={[0.015, 0.015, 0.01, 8]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      <mesh position={[0.06, -0.03, -0.02]}>
        <cylinderGeometry args={[0.015, 0.015, 0.01, 8]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      <mesh position={[-0.04, -0.03, 0.02]}>
        <cylinderGeometry args={[0.015, 0.015, 0.01, 8]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      <mesh position={[-0.04, -0.03, -0.02]}>
        <cylinderGeometry args={[0.015, 0.015, 0.01, 8]} />
        <meshStandardMaterial color="#333" />
      </mesh>
    </group>
  );
}

function SkatingKid({ startAngle, speed, color }: { startAngle: number; speed: number; color: string }) {
  const kidRef = useRef<THREE.Group>(null);
  const angleRef = useRef(startAngle);
  
  useFrame((_, delta) => {
    if (kidRef.current) {
      angleRef.current += delta * speed;
      const radius = 0.5;
      kidRef.current.position.x = Math.cos(angleRef.current) * radius;
      kidRef.current.position.z = Math.sin(angleRef.current) * radius;
      kidRef.current.rotation.y = -angleRef.current + Math.PI / 2;
    }
  });
  
  return (
    <group ref={kidRef} position={[0.5, 0.08, 0]}>
      <mesh position={[0, 0.04, 0]}>
        <capsuleGeometry args={[0.025, 0.04, 4, 8]} />
        <meshStandardMaterial color={color} />
      </mesh>
      <mesh position={[0, 0.1, 0]}>
        <sphereGeometry args={[0.025, 8, 8]} />
        <meshStandardMaterial color="#FFCCBC" />
      </mesh>
    </group>
  );
}

function SkatingKids() {
  return (
    <>
      <SkatingKid startAngle={0} speed={1.5} color="#E91E63" />
      <SkatingKid startAngle={Math.PI * 0.66} speed={1.3} color="#2196F3" />
      <SkatingKid startAngle={Math.PI * 1.33} speed={1.7} color="#4CAF50" />
    </>
  );
}

function StylizedOakTree({ position }: { position: [number, number, number] }) {
  return (
    <group position={position}>
      <mesh position={[0, 0.08, 0]}>
        <cylinderGeometry args={[0.02, 0.035, 0.12, 6]} />
        <meshStandardMaterial color="#5D4037" />
      </mesh>
      <mesh position={[0, 0.22, 0]}>
        <sphereGeometry args={[0.12, 8, 8]} />
        <meshStandardMaterial color="#2E7D32" />
      </mesh>
      <mesh position={[0.06, 0.18, 0.04]}>
        <sphereGeometry args={[0.06, 6, 6]} />
        <meshStandardMaterial color="#388E3C" />
      </mesh>
      <mesh position={[-0.05, 0.2, -0.03]}>
        <sphereGeometry args={[0.05, 6, 6]} />
        <meshStandardMaterial color="#43A047" />
      </mesh>
    </group>
  );
}

function StylizedPineTree({ position }: { position: [number, number, number] }) {
  return (
    <group position={position}>
      <mesh position={[0, 0.06, 0]}>
        <cylinderGeometry args={[0.015, 0.025, 0.1, 6]} />
        <meshStandardMaterial color="#4E342E" />
      </mesh>
      <mesh position={[0, 0.14, 0]}>
        <coneGeometry args={[0.08, 0.12, 6]} />
        <meshStandardMaterial color="#1B5E20" />
      </mesh>
      <mesh position={[0, 0.22, 0]}>
        <coneGeometry args={[0.06, 0.1, 6]} />
        <meshStandardMaterial color="#2E7D32" />
      </mesh>
      <mesh position={[0, 0.28, 0]}>
        <coneGeometry args={[0.04, 0.08, 6]} />
        <meshStandardMaterial color="#388E3C" />
      </mesh>
    </group>
  );
}

function FlowerPatch({ position, color }: { position: [number, number, number]; color: string }) {
  return (
    <group position={position}>
      <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <circleGeometry args={[0.12, 8]} />
        <meshStandardMaterial color="#7CB342" />
      </mesh>
      <mesh position={[0, 0.04, 0]}>
        <sphereGeometry args={[0.025, 6, 6]} />
        <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.2} />
      </mesh>
      <mesh position={[0.04, 0.035, 0.02]}>
        <sphereGeometry args={[0.02, 6, 6]} />
        <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.2} />
      </mesh>
      <mesh position={[-0.03, 0.035, -0.02]}>
        <sphereGeometry args={[0.018, 6, 6]} />
        <meshStandardMaterial color={color} emissive={color} emissiveIntensity={0.2} />
      </mesh>
    </group>
  );
}

function Mushroom({ position, color }: { position: [number, number, number]; color: string }) {
  return (
    <group position={position}>
      <mesh position={[0, 0.06, 0]}>
        <cylinderGeometry args={[0.015, 0.02, 0.08, 8]} />
        <meshStandardMaterial color="#F5DEB3" />
      </mesh>
      <mesh position={[0, 0.12, 0]}>
        <sphereGeometry args={[0.06, 16, 8, 0, Math.PI * 2, 0, Math.PI / 2]} />
        <meshStandardMaterial color={color} />
      </mesh>
    </group>
  );
}

function Snowman({ position }: { position: [number, number, number] }) {
  return (
    <group position={position}>
      <mesh position={[0, 0.08, 0]}>
        <sphereGeometry args={[0.08, 16, 16]} />
        <meshStandardMaterial color="white" />
      </mesh>
      <mesh position={[0, 0.18, 0]}>
        <sphereGeometry args={[0.055, 16, 16]} />
        <meshStandardMaterial color="white" />
      </mesh>
      <mesh position={[0, 0.26, 0]}>
        <sphereGeometry args={[0.04, 16, 16]} />
        <meshStandardMaterial color="white" />
      </mesh>
      <mesh position={[0.02, 0.27, 0.03]}>
        <sphereGeometry args={[0.008, 8, 8]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      <mesh position={[-0.02, 0.27, 0.03]}>
        <sphereGeometry args={[0.008, 8, 8]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      <mesh position={[0, 0.25, 0.04]} rotation={[Math.PI / 2, 0, 0]}>
        <coneGeometry args={[0.012, 0.04, 8]} />
        <meshStandardMaterial color="#FF6600" />
      </mesh>
    </group>
  );
}

function SnowPine({ position }: { position: [number, number, number] }) {
  return (
    <group position={position}>
      <mesh position={[0, 0.05, 0]}>
        <cylinderGeometry args={[0.015, 0.02, 0.08, 6]} />
        <meshStandardMaterial color="#5D4037" />
      </mesh>
      <mesh position={[0, 0.12, 0]}>
        <coneGeometry args={[0.07, 0.1, 6]} />
        <meshStandardMaterial color="#E8F5E9" />
      </mesh>
      <mesh position={[0, 0.19, 0]}>
        <coneGeometry args={[0.05, 0.08, 6]} />
        <meshStandardMaterial color="#C8E6C9" />
      </mesh>
      <mesh position={[0, 0.24, 0]}>
        <coneGeometry args={[0.035, 0.06, 6]} />
        <meshStandardMaterial color="#FFFFFF" />
      </mesh>
    </group>
  );
}

function IcePatch({ position }: { position: [number, number, number] }) {
  return (
    <mesh position={position} rotation={[-Math.PI / 2, 0, 0]}>
      <circleGeometry args={[0.5, 32]} />
      <meshStandardMaterial color="#E3F2FD" transparent opacity={0.5} />
    </mesh>
  );
}

function LavaRock({ position }: { position: [number, number, number] }) {
  return (
    <mesh position={[position[0], position[1] + 0.08, position[2]]}>
      <dodecahedronGeometry args={[0.08, 0]} />
      <meshStandardMaterial color="#4A4A4A" roughness={0.9} />
    </mesh>
  );
}

function DesertHoodoo({ position }: { position: [number, number, number] }) {
  return (
    <group position={position}>
      <mesh position={[0, 0.08, 0]}>
        <cylinderGeometry args={[0.04, 0.06, 0.14, 6]} />
        <meshStandardMaterial color="#D4A574" />
      </mesh>
      <mesh position={[0, 0.18, 0]}>
        <cylinderGeometry args={[0.05, 0.04, 0.08, 6]} />
        <meshStandardMaterial color="#C4956A" />
      </mesh>
      <mesh position={[0, 0.25, 0]}>
        <sphereGeometry args={[0.055, 6, 6]} />
        <meshStandardMaterial color="#B4855A" />
      </mesh>
    </group>
  );
}

function LavaGlow({ position }: { position: [number, number, number] }) {
  return (
    <mesh position={position} rotation={[-Math.PI / 2, 0, 0]}>
      <circleGeometry args={[0.3, 16]} />
      <meshStandardMaterial 
        color="#FF4500" 
        emissive="#FF4500" 
        emissiveIntensity={0.5}
        transparent 
        opacity={0.4} 
      />
    </mesh>
  );
}

function Alien({ position }: { position: [number, number, number] }) {
  return (
    <group position={position}>
      <mesh position={[0, 0.1, 0]}>
        <capsuleGeometry args={[0.04, 0.08, 8, 16]} />
        <meshStandardMaterial color="#7CFC00" emissive="#7CFC00" emissiveIntensity={0.3} />
      </mesh>
      <mesh position={[0, 0.2, 0]}>
        <sphereGeometry args={[0.05, 16, 16]} />
        <meshStandardMaterial color="#7CFC00" emissive="#7CFC00" emissiveIntensity={0.3} />
      </mesh>
      <mesh position={[0.02, 0.21, 0.04]}>
        <sphereGeometry args={[0.015, 8, 8]} />
        <meshStandardMaterial color="#000" />
      </mesh>
      <mesh position={[-0.02, 0.21, 0.04]}>
        <sphereGeometry args={[0.015, 8, 8]} />
        <meshStandardMaterial color="#000" />
      </mesh>
    </group>
  );
}

function Crystal({ position }: { position: [number, number, number] }) {
  const crystalRef = useRef<THREE.Mesh>(null);
  
  useFrame((state) => {
    if (crystalRef.current) {
      crystalRef.current.position.y = position[1] + 0.15 + Math.sin(state.clock.elapsedTime * 2) * 0.02;
    }
  });
  
  return (
    <mesh ref={crystalRef} position={[position[0], position[1] + 0.15, position[2]]}>
      <octahedronGeometry args={[0.06, 0]} />
      <meshStandardMaterial 
        color="#E040FB" 
        emissive="#E040FB" 
        emissiveIntensity={0.6}
        transparent
        opacity={0.85}
      />
    </mesh>
  );
}

function AlienCrystalSpire({ position }: { position: [number, number, number] }) {
  const spireRef = useRef<THREE.Group>(null);
  
  useFrame((state) => {
    if (spireRef.current) {
      spireRef.current.position.y = position[1] + Math.sin(state.clock.elapsedTime * 1.5 + position[0]) * 0.02;
      spireRef.current.rotation.y += 0.005;
    }
  });
  
  return (
    <group ref={spireRef} position={position}>
      <mesh position={[0, 0.1, 0]}>
        <cylinderGeometry args={[0.02, 0.05, 0.15, 5]} />
        <meshStandardMaterial 
          color="#9C27B0" 
          emissive="#9C27B0" 
          emissiveIntensity={0.4}
          transparent
          opacity={0.9}
        />
      </mesh>
      <mesh position={[0, 0.2, 0]}>
        <octahedronGeometry args={[0.04, 0]} />
        <meshStandardMaterial 
          color="#E040FB" 
          emissive="#E040FB" 
          emissiveIntensity={0.7}
          transparent
          opacity={0.85}
        />
      </mesh>
      <pointLight color="#aa66ff" intensity={0.5} distance={0.5} position={[0, 0.15, 0]} />
    </group>
  );
}

export function ArenaPreview({ mapId, large = false }: ArenaPreviewProps) {
  return (
    <div className={`w-full ${large ? 'h-full' : 'h-20'} rounded-lg overflow-hidden`}>
      <Canvas
        camera={{ 
          position: large ? [0, 1.8, 1.8] : [0, 1.2, 1.5], 
          fov: large ? 40 : 45 
        }}
        gl={{ antialias: true, alpha: true }}
      >
        <ambientLight intensity={0.7} />
        <directionalLight position={[3, 5, 3]} intensity={1} />
        <pointLight position={[-2, 3, -2]} intensity={0.4} color="#ffffff" />
        <MiniArena mapId={mapId} />
      </Canvas>
    </div>
  );
}

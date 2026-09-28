import * as THREE from "three";
import { useMemo, useRef, Suspense } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";

interface CrystalData {
  id: string;
  position: [number, number, number];
  scale: number;
  rotation: number;
  floatOffset: number;
}

function StylizedCrystal({ crystal }: { crystal: CrystalData }) {
  const groupRef = useRef<THREE.Group>(null);
  const model = useGLTF("/models/stylized_alien_crystal.glb");
  const clonedScene = useMemo(() => model.scene.clone(), [model.scene]);
  
  useFrame((state) => {
    if (!groupRef.current) return;
    const floatY = Math.sin(state.clock.elapsedTime * 0.8 + crystal.floatOffset) * 0.3;
    groupRef.current.position.y = crystal.position[1] + floatY;
    groupRef.current.rotation.y += 0.003;
  });

  return (
    <group 
      ref={groupRef} 
      position={crystal.position} 
      scale={crystal.scale} 
      rotation={[0, crystal.rotation, 0]}
    >
      <primitive object={clonedScene} />
      <pointLight color="#aa66ff" intensity={2} distance={8} position={[0, 1, 0]} />
    </group>
  );
}

function FallbackCrystal({ crystal }: { crystal: CrystalData }) {
  const groupRef = useRef<THREE.Group>(null);
  
  useFrame((state) => {
    if (!groupRef.current) return;
    const floatY = Math.sin(state.clock.elapsedTime * 0.8 + crystal.floatOffset) * 0.3;
    groupRef.current.position.y = crystal.position[1] + floatY;
    groupRef.current.rotation.y += 0.003;
  });

  return (
    <group ref={groupRef} position={crystal.position} scale={crystal.scale}>
      <mesh castShadow>
        <octahedronGeometry args={[0.6, 0]} />
        <meshStandardMaterial 
          color="#9966ff" 
          emissive="#6633cc" 
          emissiveIntensity={0.5}
          roughness={0.2}
          metalness={0.8}
        />
      </mesh>
      <mesh position={[0.5, 0.3, 0]} rotation={[0, 0, 0.3]} castShadow>
        <octahedronGeometry args={[0.35, 0]} />
        <meshStandardMaterial 
          color="#66ccff" 
          emissive="#3399cc" 
          emissiveIntensity={0.5}
          roughness={0.2}
          metalness={0.8}
        />
      </mesh>
      <mesh position={[-0.4, 0.5, 0.2]} rotation={[0.2, 0, -0.2]} castShadow>
        <octahedronGeometry args={[0.25, 0]} />
        <meshStandardMaterial 
          color="#ff66ff" 
          emissive="#cc33cc" 
          emissiveIntensity={0.5}
          roughness={0.2}
          metalness={0.8}
        />
      </mesh>
      <pointLight color="#aa66ff" intensity={2} distance={8} position={[0, 1, 0]} />
    </group>
  );
}

function CrystalWithFallback({ crystal }: { crystal: CrystalData }) {
  return (
    <Suspense fallback={<FallbackCrystal crystal={crystal} />}>
      <StylizedCrystal crystal={crystal} />
    </Suspense>
  );
}

export function AlienCrystals() {
  const crystalData = useMemo<CrystalData[]>(() => {
    const positions: CrystalData[] = [];
    const crystalCount = 8;
    
    for (let i = 0; i < crystalCount; i++) {
      const angle = (i / crystalCount) * Math.PI * 2 + 0.5;
      const radius = 10 + (i % 3) * 3;
      const x = Math.cos(angle) * radius;
      const z = Math.sin(angle) * radius;
      
      if (z > 7 && Math.abs(x) < 4) continue;
      
      const scale = 2.0 + (i % 3) * 0.5;
      positions.push({
        id: `crystal-${i}`,
        position: [x, 0.5, z],
        scale,
        rotation: (i * 1.3) % (Math.PI * 2),
        floatOffset: (i * 0.8) % (Math.PI * 2)
      });
    }
    
    return positions;
  }, []);

  return (
    <group>
      {crystalData.map((crystal) => (
        <CrystalWithFallback key={crystal.id} crystal={crystal} />
      ))}
    </group>
  );
}

useGLTF.preload("/models/stylized_alien_crystal.glb");

import * as THREE from "three";
import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";

interface IcePatch {
  id: string;
  position: [number, number, number];
  radius: number;
  rotation: number;
}

export function IcePatches() {
  const patches = useMemo<IcePatch[]>(() => {
    const result: IcePatch[] = [];
    const patchCount = 4;
    
    for (let i = 0; i < patchCount; i++) {
      const angle = (i / patchCount) * Math.PI * 2 + 0.3;
      const distFromCenter = 8 + (i % 2) * 4;
      const x = Math.cos(angle) * distFromCenter;
      const z = Math.sin(angle) * distFromCenter;
      const radius = 3.0 + (i % 2) * 0.5;
      
      result.push({
        id: `ice-${i}`,
        position: [x, 0.02, z],
        radius,
        rotation: angle
      });
    }
    
    return result;
  }, []);

  return (
    <group>
      {patches.map((patch) => (
        <IcePatchMesh key={patch.id} patch={patch} />
      ))}
    </group>
  );
}

function IcePatchMesh({ patch }: { patch: IcePatch }) {
  const meshRef = useRef<THREE.Mesh>(null);
  
  useFrame((state) => {
    if (meshRef.current) {
      const material = meshRef.current.material as THREE.MeshStandardMaterial;
      material.opacity = 0.6 + Math.sin(state.clock.elapsedTime * 2 + patch.rotation) * 0.1;
    }
  });

  return (
    <mesh
      ref={meshRef}
      position={patch.position}
      rotation={[-Math.PI / 2, 0, patch.rotation]}
    >
      <circleGeometry args={[patch.radius, 12]} />
      <meshStandardMaterial
        color="#B8E8FF"
        transparent
        opacity={0.6}
        roughness={0.1}
        metalness={0.3}
        emissive="#88D4FF"
        emissiveIntensity={0.2}
      />
    </mesh>
  );
}

export function getIcePatchPositions(): { position: [number, number, number]; radius: number }[] {
  const patches: { position: [number, number, number]; radius: number }[] = [];
  const patchCount = 4;
  
  for (let i = 0; i < patchCount; i++) {
    const angle = (i / patchCount) * Math.PI * 2 + 0.3;
    const distFromCenter = 8 + (i % 2) * 4;
    const x = Math.cos(angle) * distFromCenter;
    const z = Math.sin(angle) * distFromCenter;
    const radius = 3.0 + (i % 2) * 0.5;
    
    patches.push({
      position: [x, 0.02, z],
      radius
    });
  }
  
  return patches;
}

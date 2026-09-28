import * as THREE from "three";
import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { getIcePatches } from "@/lib/arenaColliders";

interface IcePatch {
  id: string;
  position: [number, number, number];
  radius: number;
  rotation: number;
}

export function IcePatches() {
  const patches = useMemo<IcePatch[]>(() => {
    return getIcePatches().map((patch) => ({
      id: patch.id,
      position: [patch.x, 0.02, patch.z] as [number, number, number],
      radius: patch.radius,
      rotation: patch.rotation,
    }));
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
  return getIcePatches().map((patch) => ({
    position: [patch.x, 0.02, patch.z],
    radius: patch.radius,
  }));
}

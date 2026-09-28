import * as THREE from "three";
import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";

interface FlowerPatchData {
  id: string;
  position: [number, number, number];
  radius: number;
  rotation: number;
}

function Flower({ position, scale = 1, color }: { position: [number, number, number]; scale?: number; color: string }) {
  const flowerRef = useRef<THREE.Group>(null);
  
  useFrame((state) => {
    if (flowerRef.current) {
      flowerRef.current.rotation.y = Math.sin(state.clock.elapsedTime * 0.5 + position[0]) * 0.1;
    }
  });

  return (
    <group ref={flowerRef} position={position} scale={scale}>
      <mesh position={[0, 0.15, 0]}>
        <cylinderGeometry args={[0.02, 0.02, 0.3, 6]} />
        <meshStandardMaterial color="#228B22" />
      </mesh>
      
      {[0, 1, 2, 3, 4, 5].map((i) => (
        <mesh
          key={i}
          position={[
            Math.cos((i / 6) * Math.PI * 2) * 0.12,
            0.35,
            Math.sin((i / 6) * Math.PI * 2) * 0.12
          ]}
          rotation={[0.3, (i / 6) * Math.PI * 2, 0]}
        >
          <sphereGeometry args={[0.08, 8, 8]} />
          <meshStandardMaterial color={color} />
        </mesh>
      ))}
      
      <mesh position={[0, 0.35, 0]}>
        <sphereGeometry args={[0.08, 8, 8]} />
        <meshStandardMaterial color="#FFD700" emissive="#FFD700" emissiveIntensity={0.2} />
      </mesh>
    </group>
  );
}

function FlowerPatch({ patch }: { patch: FlowerPatchData }) {
  const flowers = useMemo(() => {
    const flowerList: { pos: [number, number, number]; scale: number; color: string }[] = [];
    const flowerCount = Math.floor(patch.radius * 38);
    const colors = ["#FF69B4", "#FF1493", "#FFB6C1", "#FFC0CB", "#DB7093", "#FF00FF"];
    
    for (let i = 0; i < flowerCount; i++) {
      const angle = (i / flowerCount) * Math.PI * 2 + patch.rotation;
      const dist = (0.3 + Math.random() * 0.6) * patch.radius;
      flowerList.push({
        pos: [
          patch.position[0] + Math.cos(angle) * dist,
          0,
          patch.position[2] + Math.sin(angle) * dist
        ],
        scale: 0.6 + Math.random() * 0.4,
        color: colors[i % colors.length]
      });
    }
    return flowerList;
  }, [patch]);

  return (
    <group>
      {flowers.map((flower, i) => (
        <Flower key={i} position={flower.pos} scale={flower.scale} color={flower.color} />
      ))}
    </group>
  );
}

export function FlowerPatches() {
  const patch = useMemo<FlowerPatchData>(() => ({
    id: "flower-patch-main",
    position: [-12, 0, 0],
    radius: 8,
    rotation: 0
  }), []);

  return (
    <group>
      <FlowerPatch patch={patch} />
    </group>
  );
}

export function getFlowerPatchPositions(): { position: [number, number, number]; radius: number }[] {
  return [{
    position: [-12, 0, 0],
    radius: 8
  }];
}

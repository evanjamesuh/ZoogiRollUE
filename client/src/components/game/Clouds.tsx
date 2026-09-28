import * as THREE from "three";
import { useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";

interface Cloud {
  id: string;
  position: [number, number, number];
  scale: [number, number, number];
  speed: number;
}

export function Clouds() {
  const groupRef = useRef<THREE.Group>(null);
  
  const clouds = useMemo<Cloud[]>(() => {
    const result: Cloud[] = [];
    const cloudCount = 12;
    
    for (let i = 0; i < cloudCount; i++) {
      const angle = (i / cloudCount) * Math.PI * 2;
      const radius = 25 + (i % 3) * 10;
      const height = 15 + (i % 4) * 5;
      
      result.push({
        id: `cloud-${i}`,
        position: [
          Math.cos(angle) * radius,
          height,
          Math.sin(angle) * radius
        ],
        scale: [
          2 + (i % 3),
          1 + (i % 2) * 0.5,
          2 + (i % 4) * 0.5
        ],
        speed: 0.1 + (i % 3) * 0.05
      });
    }
    
    return result;
  }, []);

  useFrame((state) => {
    if (groupRef.current) {
      groupRef.current.rotation.y = state.clock.elapsedTime * 0.01;
    }
  });

  return (
    <group ref={groupRef}>
      {clouds.map((cloud) => (
        <CloudMesh key={cloud.id} cloud={cloud} />
      ))}
    </group>
  );
}

function CloudMesh({ cloud }: { cloud: Cloud }) {
  const groupRef = useRef<THREE.Group>(null);
  const baseY = cloud.position[1];
  
  useFrame((state) => {
    if (groupRef.current) {
      groupRef.current.position.y = baseY + Math.sin(state.clock.elapsedTime * cloud.speed) * 0.5;
    }
  });

  return (
    <group ref={groupRef} position={cloud.position}>
      <mesh scale={cloud.scale}>
        <sphereGeometry args={[1, 8, 6]} />
        <meshStandardMaterial
          color="#FFFFFF"
          transparent
          opacity={0.9}
          roughness={1}
        />
      </mesh>
      
      <mesh position={[-1, -0.2, 0]} scale={[cloud.scale[0] * 0.7, cloud.scale[1] * 0.8, cloud.scale[2] * 0.8]}>
        <sphereGeometry args={[1, 8, 6]} />
        <meshStandardMaterial
          color="#FFFFFF"
          transparent
          opacity={0.85}
          roughness={1}
        />
      </mesh>
      
      <mesh position={[1.2, -0.1, 0.3]} scale={[cloud.scale[0] * 0.6, cloud.scale[1] * 0.7, cloud.scale[2] * 0.7]}>
        <sphereGeometry args={[1, 8, 6]} />
        <meshStandardMaterial
          color="#FFFFFF"
          transparent
          opacity={0.85}
          roughness={1}
        />
      </mesh>
      
      <mesh position={[0.3, 0.3, -0.5]} scale={[cloud.scale[0] * 0.5, cloud.scale[1] * 0.6, cloud.scale[2] * 0.5]}>
        <sphereGeometry args={[1, 8, 6]} />
        <meshStandardMaterial
          color="#FFFFFF"
          transparent
          opacity={0.8}
          roughness={1}
        />
      </mesh>
    </group>
  );
}

import { useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";

export function ChristmasSleigh() {
  const sleighRef = useRef<THREE.Group>(null);
  
  const sleighPosition: [number, number, number] = [8, 2.5, -5];
  
  useFrame((state) => {
    if (sleighRef.current) {
      sleighRef.current.position.y = 2.5 + Math.sin(state.clock.elapsedTime * 0.5) * 0.15;
      sleighRef.current.rotation.y = Math.sin(state.clock.elapsedTime * 0.3) * 0.05;
    }
  });

  return (
    <group>
      <group ref={sleighRef} position={sleighPosition} rotation={[0, Math.PI / 6, 0]}>
        <mesh position={[0, 0, 0]} castShadow>
          <boxGeometry args={[3, 0.3, 1.5]} />
          <meshStandardMaterial color="#8B0000" />
        </mesh>
        
        <mesh position={[-1.2, 0.15, 0.9]} castShadow>
          <boxGeometry args={[0.6, 0.6, 0.1]} />
          <meshStandardMaterial color="#8B0000" />
        </mesh>
        <mesh position={[1.2, 0.15, 0.9]} castShadow>
          <boxGeometry args={[0.6, 0.6, 0.1]} />
          <meshStandardMaterial color="#8B0000" />
        </mesh>
        <mesh position={[0, 0.15, 0.9]} castShadow>
          <boxGeometry args={[1.8, 0.4, 0.1]} />
          <meshStandardMaterial color="#8B0000" />
        </mesh>
        
        <mesh position={[0, 0.15, -0.9]} castShadow>
          <boxGeometry args={[3, 0.8, 0.15]} />
          <meshStandardMaterial color="#8B0000" />
        </mesh>
        
        <Runner position={[-0.8, -0.4, 0]} />
        <Runner position={[0.8, -0.4, 0]} />
        
        <mesh position={[0, 0.5, 0]} castShadow>
          <boxGeometry args={[2, 0.8, 1]} />
          <meshStandardMaterial color="#228B22" />
        </mesh>
        
        <mesh position={[-0.3, 1, 0.2]}>
          <sphereGeometry args={[0.2, 16, 16]} />
          <meshStandardMaterial color="#FFD700" emissive="#FFD700" emissiveIntensity={0.3} />
        </mesh>
        <mesh position={[0.3, 1.1, -0.2]}>
          <sphereGeometry args={[0.15, 16, 16]} />
          <meshStandardMaterial color="#FF0000" emissive="#FF0000" emissiveIntensity={0.2} />
        </mesh>
        <mesh position={[0, 0.9, 0.3]}>
          <sphereGeometry args={[0.18, 16, 16]} />
          <meshStandardMaterial color="#4169E1" emissive="#4169E1" emissiveIntensity={0.2} />
        </mesh>
      </group>
    </group>
  );
}

function Runner({ position }: { position: [number, number, number] }) {
  return (
    <group position={position}>
      <mesh rotation={[0, 0, Math.PI / 2]} castShadow>
        <capsuleGeometry args={[0.08, 1.4, 4, 8]} />
        <meshStandardMaterial color="#CD853F" metalness={0.3} roughness={0.7} />
      </mesh>
      
      <mesh position={[0, -0.15, 0]} rotation={[Math.PI / 2, 0, 0]} castShadow>
        <boxGeometry args={[0.1, 1.6, 0.05]} />
        <meshStandardMaterial color="#B8860B" metalness={0.5} roughness={0.5} />
      </mesh>
    </group>
  );
}


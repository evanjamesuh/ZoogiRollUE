import * as THREE from "three";
import { useRef, useState, useEffect } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

export const ALIEN_COLLISION_RADIUS = 1.5;

export function Aliens() {
  const { mushrooms } = useZoogiGame();
  
  return (
    <group>
      {mushrooms.map((mushroom) => (
        <Alien key={mushroom.id} mushroom={mushroom} />
      ))}
    </group>
  );
}

interface AlienProps {
  mushroom: {
    id: string;
    position: [number, number, number];
    color: "red" | "white";
    repelForce: number;
    lastHitTime?: number;
  };
}

function Alien({ mushroom }: AlienProps) {
  const groupRef = useRef<THREE.Group>(null);
  const bodyRef = useRef<THREE.Mesh>(null);
  const bobOffset = useRef(Math.random() * Math.PI * 2);
  const [jiggleIntensity, setJiggleIntensity] = useState(0);
  const lastHitTimeRef = useRef(0);
  
  const isStrongAlien = mushroom.color === "red";
  const bodyColor = isStrongAlien ? "#00FF00" : "#7CFC00";
  const eyeColor = "#000000";
  const glowColor = isStrongAlien ? "#00FF88" : "#98FB98";
  
  useEffect(() => {
    if (mushroom.lastHitTime && mushroom.lastHitTime > lastHitTimeRef.current) {
      lastHitTimeRef.current = mushroom.lastHitTime;
      setJiggleIntensity(1);
    }
  }, [mushroom.lastHitTime]);
  
  useFrame((state, delta) => {
    if (groupRef.current) {
      const bob = Math.sin(state.clock.elapsedTime * 2 + bobOffset.current) * 0.1;
      groupRef.current.position.y = mushroom.position[1] + 0.5 + bob;
      groupRef.current.rotation.y = state.clock.elapsedTime * 0.5;
    }
    
    if (bodyRef.current && jiggleIntensity > 0) {
      const jiggle = Math.sin(state.clock.elapsedTime * 30) * jiggleIntensity * 0.2;
      bodyRef.current.scale.set(
        1 + jiggle,
        1 - jiggle * 0.3,
        1 + jiggle
      );
      setJiggleIntensity(prev => Math.max(0, prev - delta * 3));
    } else if (bodyRef.current) {
      bodyRef.current.scale.set(1, 1, 1);
    }
  });
  
  return (
    <group 
      ref={groupRef} 
      position={[mushroom.position[0], mushroom.position[1] + 0.5, mushroom.position[2]]}
    >
      <mesh ref={bodyRef} castShadow>
        <capsuleGeometry args={[0.8, 1.2, 8, 16]} />
        <meshStandardMaterial 
          color={bodyColor} 
          emissive={glowColor}
          emissiveIntensity={0.3}
        />
      </mesh>
      
      <mesh position={[0, 1.3, 0]} castShadow>
        <sphereGeometry args={[0.6, 16, 16]} />
        <meshStandardMaterial 
          color={bodyColor}
          emissive={glowColor}
          emissiveIntensity={0.3}
        />
      </mesh>
      
      <mesh position={[-0.25, 1.45, 0.4]}>
        <sphereGeometry args={[0.2, 12, 12]} />
        <meshStandardMaterial color="#FFFFFF" />
      </mesh>
      <mesh position={[0.25, 1.45, 0.4]}>
        <sphereGeometry args={[0.2, 12, 12]} />
        <meshStandardMaterial color="#FFFFFF" />
      </mesh>
      
      <mesh position={[-0.25, 1.45, 0.55]}>
        <sphereGeometry args={[0.08, 8, 8]} />
        <meshStandardMaterial color={eyeColor} />
      </mesh>
      <mesh position={[0.25, 1.45, 0.55]}>
        <sphereGeometry args={[0.08, 8, 8]} />
        <meshStandardMaterial color={eyeColor} />
      </mesh>
      
      <mesh position={[-0.5, 1.8, 0]} rotation={[0, 0, -0.3]}>
        <capsuleGeometry args={[0.08, 0.4, 4, 8]} />
        <meshStandardMaterial color={bodyColor} emissive={glowColor} emissiveIntensity={0.2} />
      </mesh>
      <mesh position={[0.5, 1.8, 0]} rotation={[0, 0, 0.3]}>
        <capsuleGeometry args={[0.08, 0.4, 4, 8]} />
        <meshStandardMaterial color={bodyColor} emissive={glowColor} emissiveIntensity={0.2} />
      </mesh>
      
      <mesh position={[-0.55, 2.15, 0]}>
        <sphereGeometry args={[0.12, 8, 8]} />
        <meshStandardMaterial color={isStrongAlien ? "#FF00FF" : "#00FFFF"} emissive={isStrongAlien ? "#FF00FF" : "#00FFFF"} emissiveIntensity={0.5} />
      </mesh>
      <mesh position={[0.55, 2.15, 0]}>
        <sphereGeometry args={[0.12, 8, 8]} />
        <meshStandardMaterial color={isStrongAlien ? "#FF00FF" : "#00FFFF"} emissive={isStrongAlien ? "#FF00FF" : "#00FFFF"} emissiveIntensity={0.5} />
      </mesh>
      
      <pointLight
        position={[0, 1.5, 0]}
        color={glowColor}
        intensity={0.8}
        distance={6}
      />
    </group>
  );
}

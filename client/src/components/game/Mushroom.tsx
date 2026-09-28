import * as THREE from "three";
import { useRef, useState, useEffect } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";

export const MUSHROOM_COLLISION_RADIUS = 1.5;

export function Mushrooms() {
  const { mushrooms, selectedMap } = useZoogiGame();
  const isIceMap = selectedMap === "ice";
  
  return (
    <group>
      {mushrooms.map((mushroom) => (
        isIceMap ? 
          <SnowmanHead key={mushroom.id} mushroom={mushroom} /> :
          <Mushroom key={mushroom.id} mushroom={mushroom} />
      ))}
    </group>
  );
}

interface MushroomProps {
  mushroom: {
    id: string;
    position: [number, number, number];
    color: "red" | "white";
    repelForce: number;
    lastHitTime?: number;
  };
}

function Mushroom({ mushroom }: MushroomProps) {
  const groupRef = useRef<THREE.Group>(null);
  const meshRef = useRef<THREE.Mesh>(null);
  const bobOffset = useRef(Math.random() * Math.PI * 2);
  const [jiggleIntensity, setJiggleIntensity] = useState(0);
  const lastHitTimeRef = useRef(0);
  const { playSound } = useAudio();
  
  const isRed = mushroom.color === "red";
  const capColor = isRed ? "#E53E3E" : "#F5F5F5";
  const spotsColor = isRed ? "#FFFFFF" : "#E53E3E";
  
  useEffect(() => {
    if (mushroom.lastHitTime && mushroom.lastHitTime > lastHitTimeRef.current) {
      lastHitTimeRef.current = mushroom.lastHitTime;
      setJiggleIntensity(1);
      playSound("collision_mushroom");
    }
  }, [mushroom.lastHitTime, playSound]);
  
  useFrame((state, delta) => {
    if (groupRef.current) {
      const bob = Math.sin(state.clock.elapsedTime * 2 + bobOffset.current) * 0.03;
      groupRef.current.position.y = mushroom.position[1] + bob;
    }
    
    if (meshRef.current && jiggleIntensity > 0) {
      const jiggle = Math.sin(state.clock.elapsedTime * 30) * jiggleIntensity * 0.15;
      meshRef.current.scale.set(
        1 + jiggle,
        1 - jiggle * 0.5,
        1 + jiggle
      );
      setJiggleIntensity(prev => Math.max(0, prev - delta * 3));
    } else if (meshRef.current) {
      meshRef.current.scale.set(1, 1, 1);
    }
  });
  
  return (
    <group 
      ref={groupRef} 
      position={[mushroom.position[0], mushroom.position[1], mushroom.position[2]]}
    >
      <mesh ref={meshRef} position={[0, 0, 0]} castShadow>
        <sphereGeometry args={[1.5, 24, 12, 0, Math.PI * 2, 0, Math.PI / 2]} />
        <meshStandardMaterial color={capColor} />
      </mesh>
      
      {[0, 45, 90, 135, 180, 225, 270, 315].map((angle, i) => {
        const rad = (angle * Math.PI) / 180;
        const spotX = Math.cos(rad) * 0.9;
        const spotZ = Math.sin(rad) * 0.9;
        return (
          <mesh 
            key={i} 
            position={[spotX, 0.6, spotZ]} 
            rotation={[-0.5, rad, 0]}
          >
            <circleGeometry args={[0.25, 8]} />
            <meshStandardMaterial color={spotsColor} />
          </mesh>
        );
      })}
      
      <mesh position={[0, 1.1, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <circleGeometry args={[0.3, 8]} />
        <meshStandardMaterial color={spotsColor} />
      </mesh>
      
      <pointLight
        position={[0, 1.5, 0]}
        color={isRed ? "#FF6B6B" : "#FFFFFF"}
        intensity={0.6}
        distance={6}
      />
    </group>
  );
}

function SnowmanHead({ mushroom }: MushroomProps) {
  const groupRef = useRef<THREE.Group>(null);
  const meshRef = useRef<THREE.Mesh>(null);
  const bobOffset = useRef(Math.random() * Math.PI * 2);
  const [jiggleIntensity, setJiggleIntensity] = useState(0);
  const lastHitTimeRef = useRef(0);
  const { playSound } = useAudio();
  
  useEffect(() => {
    if (mushroom.lastHitTime && mushroom.lastHitTime > lastHitTimeRef.current) {
      lastHitTimeRef.current = mushroom.lastHitTime;
      setJiggleIntensity(1);
      playSound("collision_snowman");
    }
  }, [mushroom.lastHitTime, playSound]);
  
  useFrame((state, delta) => {
    if (groupRef.current) {
      const bob = Math.sin(state.clock.elapsedTime * 1.5 + bobOffset.current) * 0.02;
      groupRef.current.position.y = mushroom.position[1] + bob;
    }
    
    if (meshRef.current && jiggleIntensity > 0) {
      const jiggle = Math.sin(state.clock.elapsedTime * 30) * jiggleIntensity * 0.15;
      meshRef.current.scale.set(
        1 + jiggle,
        1 - jiggle * 0.5,
        1 + jiggle
      );
      setJiggleIntensity(prev => Math.max(0, prev - delta * 3));
    } else if (meshRef.current) {
      meshRef.current.scale.set(1, 1, 1);
    }
  });
  
  return (
    <group 
      ref={groupRef} 
      position={[mushroom.position[0], mushroom.position[1], mushroom.position[2]]}
    >
      <mesh ref={meshRef} position={[0, 0.75, 0]} castShadow>
        <sphereGeometry args={[1.5, 24, 24]} />
        <meshStandardMaterial color="#FFFFFF" roughness={0.8} />
      </mesh>
      
      <mesh position={[0, 0.75, 2.0]} rotation={[Math.PI / 2, 0, 0]} castShadow>
        <coneGeometry args={[0.3, 1.2, 8]} />
        <meshStandardMaterial color="#FF6B00" />
      </mesh>
      
      <mesh position={[-0.45, 1.1, 1.25]} castShadow>
        <sphereGeometry args={[0.3, 12, 12]} />
        <meshStandardMaterial color="#1a1a1a" />
      </mesh>
      <mesh position={[0.45, 1.1, 1.25]} castShadow>
        <sphereGeometry args={[0.3, 12, 12]} />
        <meshStandardMaterial color="#1a1a1a" />
      </mesh>
      
      <mesh position={[-0.38, 1.18, 1.45]} castShadow>
        <sphereGeometry args={[0.12, 8, 8]} />
        <meshStandardMaterial color="#FFFFFF" />
      </mesh>
      <mesh position={[0.52, 1.18, 1.45]} castShadow>
        <sphereGeometry args={[0.12, 8, 8]} />
        <meshStandardMaterial color="#FFFFFF" />
      </mesh>
      
      <pointLight
        position={[0, 2, 0]}
        color="#E8F4FF"
        intensity={0.5}
        distance={5}
      />
    </group>
  );
}

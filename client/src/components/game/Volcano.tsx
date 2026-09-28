import * as THREE from "three";
import { useRef, useMemo, useState } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

export function Volcano() {
  const groupRef = useRef<THREE.Group>(null);
  const lavaRef = useRef<THREE.Mesh>(null);
  const glowRef = useRef<THREE.PointLight>(null);
  const particlesRef = useRef<THREE.Points>(null);
  const [jiggleIntensity, setJiggleIntensity] = useState(0);
  const lastHitTimeRef = useRef(0);
  
  const { playerEntity, enemies } = useZoogiGame();
  
  const particles = useMemo(() => {
    const count = 30;
    const positions = new Float32Array(count * 3);
    const velocities: number[] = [];
    
    for (let i = 0; i < count; i++) {
      const angle = Math.random() * Math.PI * 2;
      const radius = Math.random() * 0.5;
      positions[i * 3] = Math.cos(angle) * radius;
      positions[i * 3 + 1] = 2 + Math.random() * 2;
      positions[i * 3 + 2] = Math.sin(angle) * radius;
      velocities.push(Math.random() * 0.5 + 0.5);
    }
    
    return { positions, velocities };
  }, []);
  
  useFrame((state, delta) => {
    if (lavaRef.current) {
      const material = lavaRef.current.material as THREE.MeshStandardMaterial;
      material.emissiveIntensity = 0.8 + Math.sin(state.clock.elapsedTime * 3) * 0.2;
    }
    
    if (glowRef.current) {
      glowRef.current.intensity = 2 + Math.sin(state.clock.elapsedTime * 2) * 0.5;
    }
    
    if (particlesRef.current) {
      const positions = particlesRef.current.geometry.attributes.position.array as Float32Array;
      for (let i = 0; i < positions.length / 3; i++) {
        positions[i * 3 + 1] += particles.velocities[i] * 0.02;
        if (positions[i * 3 + 1] > 5) {
          const angle = Math.random() * Math.PI * 2;
          const radius = Math.random() * 0.5;
          positions[i * 3] = Math.cos(angle) * radius;
          positions[i * 3 + 1] = 2;
          positions[i * 3 + 2] = Math.sin(angle) * radius;
        }
      }
      particlesRef.current.geometry.attributes.position.needsUpdate = true;
    }
    
    const checkCollision = (pos: [number, number, number]) => {
      const dx = pos[0];
      const dz = pos[2];
      const dist = Math.sqrt(dx * dx + dz * dz);
      return dist < 3.5;
    };
    
    let wasHit = false;
    if (playerEntity && checkCollision(playerEntity.position)) {
      const now = Date.now();
      if (now - lastHitTimeRef.current > 500) {
        wasHit = true;
        lastHitTimeRef.current = now;
      }
    }
    
    enemies.forEach(enemy => {
      if (checkCollision(enemy.position)) {
        const now = Date.now();
        if (now - lastHitTimeRef.current > 500) {
          wasHit = true;
          lastHitTimeRef.current = now;
        }
      }
    });
    
    if (wasHit) {
      setJiggleIntensity(1);
    }
    
    if (groupRef.current && jiggleIntensity > 0) {
      const jiggle = Math.sin(state.clock.elapsedTime * 20) * jiggleIntensity * 0.08;
      groupRef.current.rotation.z = jiggle;
      groupRef.current.scale.set(
        1 + jiggle * 0.3,
        1 - jiggle * 0.2,
        1 + jiggle * 0.3
      );
      setJiggleIntensity(prev => Math.max(0, prev - delta * 3));
    } else if (groupRef.current) {
      groupRef.current.rotation.z = 0;
      groupRef.current.scale.set(1, 1, 1);
    }
  });

  return (
    <group ref={groupRef} position={[0, 0, 0]}>
      <mesh position={[0, 0, 0]} castShadow>
        <coneGeometry args={[3, 4, 16]} />
        <meshStandardMaterial 
          color="#4A3728" 
          roughness={0.9}
        />
      </mesh>
      
      <mesh position={[0, 1.8, 0]}>
        <coneGeometry args={[1.2, 0.8, 16]} />
        <meshStandardMaterial 
          color="#2D1F14" 
          roughness={0.95}
        />
      </mesh>
      
      <mesh ref={lavaRef} position={[0, 2.2, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <circleGeometry args={[0.9, 16]} />
        <meshStandardMaterial 
          color="#FF4500"
          emissive="#FF2200"
          emissiveIntensity={0.8}
        />
      </mesh>
      
      <mesh position={[0, 2.15, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.7, 1, 16]} />
        <meshStandardMaterial 
          color="#FF6B00"
          emissive="#FF4400"
          emissiveIntensity={0.6}
        />
      </mesh>
      
      <pointLight
        ref={glowRef}
        position={[0, 3, 0]}
        color="#FF4500"
        intensity={2}
        distance={12}
      />
      
      <points ref={particlesRef}>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={particles.positions.length / 3}
            array={particles.positions}
            itemSize={3}
          />
        </bufferGeometry>
        <pointsMaterial
          color="#FF6B00"
          size={0.15}
          transparent
          opacity={0.8}
        />
      </points>
    </group>
  );
}

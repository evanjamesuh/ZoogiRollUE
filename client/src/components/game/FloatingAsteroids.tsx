import { useRef, useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";

interface Asteroid {
  id: number;
  position: [number, number, number];
  size: number;
  rotationSpeed: [number, number, number];
  floatSpeed: number;
  floatOffset: number;
  color: string;
}

export function FloatingAsteroids() {
  const groupRef = useRef<THREE.Group>(null);
  
  const asteroids = useMemo<Asteroid[]>(() => {
    const items: Asteroid[] = [];
    const colors = ["#4A4A4A", "#5C5C5C", "#6E6E6E", "#3D3D3D", "#7E7E7E"];
    
    for (let i = 0; i < 15; i++) {
      const angle = (i / 15) * Math.PI * 2;
      const radius = 22 + Math.random() * 10;
      const height = 5 + Math.random() * 15;
      
      items.push({
        id: i,
        position: [
          Math.cos(angle) * radius,
          height,
          Math.sin(angle) * radius
        ],
        size: 0.5 + Math.random() * 1.5,
        rotationSpeed: [
          (Math.random() - 0.5) * 0.02,
          (Math.random() - 0.5) * 0.02,
          (Math.random() - 0.5) * 0.02
        ],
        floatSpeed: 0.3 + Math.random() * 0.5,
        floatOffset: Math.random() * Math.PI * 2,
        color: colors[Math.floor(Math.random() * colors.length)]
      });
    }
    
    for (let i = 0; i < 8; i++) {
      const angle = Math.random() * Math.PI * 2;
      const radius = Math.random() * 25;
      const height = 20 + Math.random() * 10;
      
      items.push({
        id: 15 + i,
        position: [
          Math.cos(angle) * radius,
          height,
          Math.sin(angle) * radius
        ],
        size: 0.3 + Math.random() * 0.8,
        rotationSpeed: [
          (Math.random() - 0.5) * 0.03,
          (Math.random() - 0.5) * 0.03,
          (Math.random() - 0.5) * 0.03
        ],
        floatSpeed: 0.5 + Math.random() * 0.7,
        floatOffset: Math.random() * Math.PI * 2,
        color: colors[Math.floor(Math.random() * colors.length)]
      });
    }
    
    return items;
  }, []);

  return (
    <group ref={groupRef}>
      {asteroids.map((asteroid) => (
        <FloatingAsteroid key={asteroid.id} asteroid={asteroid} />
      ))}
      
      <SpaceDebris />
      
      <DistantStars />
    </group>
  );
}

function FloatingAsteroid({ asteroid }: { asteroid: Asteroid }) {
  const meshRef = useRef<THREE.Mesh>(null);
  
  useFrame((state) => {
    if (meshRef.current) {
      meshRef.current.rotation.x += asteroid.rotationSpeed[0];
      meshRef.current.rotation.y += asteroid.rotationSpeed[1];
      meshRef.current.rotation.z += asteroid.rotationSpeed[2];
      
      const floatY = Math.sin(state.clock.elapsedTime * asteroid.floatSpeed + asteroid.floatOffset) * 0.5;
      const floatX = Math.cos(state.clock.elapsedTime * asteroid.floatSpeed * 0.7 + asteroid.floatOffset) * 0.3;
      
      meshRef.current.position.y = asteroid.position[1] + floatY;
      meshRef.current.position.x = asteroid.position[0] + floatX;
    }
  });

  return (
    <mesh
      ref={meshRef}
      position={asteroid.position}
      castShadow
    >
      <dodecahedronGeometry args={[asteroid.size, 0]} />
      <meshStandardMaterial 
        color={asteroid.color}
        roughness={0.9}
        metalness={0.1}
      />
    </mesh>
  );
}

function SpaceDebris() {
  const debrisRef = useRef<THREE.Points>(null);
  const particleCount = 600;
  
  const particles = useMemo(() => {
    const positions = new Float32Array(particleCount * 3);
    const colors = new Float32Array(particleCount * 3);
    
    for (let i = 0; i < particleCount; i++) {
      const angle = Math.random() * Math.PI * 2;
      const radius = 5 + Math.random() * 35;
      const height = Math.random() * 35;
      
      positions[i * 3] = Math.cos(angle) * radius;
      positions[i * 3 + 1] = height;
      positions[i * 3 + 2] = Math.sin(angle) * radius;
      
      colors[i * 3] = 1;
      colors[i * 3 + 1] = 1;
      colors[i * 3 + 2] = 1;
    }
    
    return { positions, colors };
  }, []);

  useFrame((state) => {
    if (debrisRef.current) {
      debrisRef.current.rotation.y = state.clock.elapsedTime * 0.01;
    }
  });

  return (
    <points ref={debrisRef}>
      <bufferGeometry>
        <bufferAttribute
          attach="attributes-position"
          count={particleCount}
          array={particles.positions}
          itemSize={3}
        />
        <bufferAttribute
          attach="attributes-color"
          count={particleCount}
          array={particles.colors}
          itemSize={3}
        />
      </bufferGeometry>
      <pointsMaterial
        size={0.12}
        vertexColors
        transparent
        opacity={0.95}
      />
    </points>
  );
}

function DistantStars() {
  const starsRef = useRef<THREE.Points>(null);
  
  const stars = useMemo(() => {
    const positions = new Float32Array(500 * 3);
    
    for (let i = 0; i < 500; i++) {
      const theta = Math.random() * Math.PI * 2;
      const phi = Math.acos(2 * Math.random() - 1);
      const radius = 80 + Math.random() * 40;
      
      positions[i * 3] = radius * Math.sin(phi) * Math.cos(theta);
      positions[i * 3 + 1] = radius * Math.cos(phi);
      positions[i * 3 + 2] = radius * Math.sin(phi) * Math.sin(theta);
    }
    
    return positions;
  }, []);

  useFrame((state) => {
    if (starsRef.current) {
      starsRef.current.rotation.y = state.clock.elapsedTime * 0.005;
    }
  });

  return (
    <points ref={starsRef}>
      <bufferGeometry>
        <bufferAttribute
          attach="attributes-position"
          count={500}
          array={stars}
          itemSize={3}
        />
      </bufferGeometry>
      <pointsMaterial
        size={0.3}
        color="#FFFFFF"
        transparent
        opacity={0.9}
      />
    </points>
  );
}

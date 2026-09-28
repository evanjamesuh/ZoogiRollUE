import { useRef, useState, useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

interface FallingRock {
  id: string;
  position: [number, number, number];
  velocity: [number, number, number];
  size: number;
  rotation: [number, number, number];
  rotationSpeed: [number, number, number];
  landed: boolean;
}

interface LandedRock {
  id: string;
  position: [number, number, number];
  size: number;
  rotation: [number, number, number];
}

export function FallingRocks() {
  const [fallingRocks, setFallingRocks] = useState<FallingRock[]>([]);
  const [landedRocks, setLandedRocks] = useState<LandedRock[]>([]);
  const lastSpawnTime = useRef(0);
  const { phase, addLandedRock } = useZoogiGame();

  const MAX_LANDED_ROCKS = 10;
  
  useFrame((state) => {
    if (phase !== "playing") return;
    
    const now = state.clock.elapsedTime;
    
    if (now - lastSpawnTime.current > 60 && landedRocks.length < MAX_LANDED_ROCKS) {
      lastSpawnTime.current = now;
      
      const angle = Math.random() * Math.PI * 2;
      const radius = 5 + Math.random() * 10;
      const startX = Math.cos(angle) * radius;
      const startZ = Math.sin(angle) * radius;
      
      const newRock: FallingRock = {
        id: `rock-${now}-${Math.random()}`,
        position: [startX, 25 + Math.random() * 10, startZ],
        velocity: [
          (Math.random() - 0.5) * 0.1,
          -0.3 - Math.random() * 0.2,
          (Math.random() - 0.5) * 0.1
        ],
        size: 0.8 + Math.random() * 0.4,
        rotation: [Math.random() * Math.PI, Math.random() * Math.PI, Math.random() * Math.PI],
        rotationSpeed: [
          (Math.random() - 0.5) * 0.15,
          (Math.random() - 0.5) * 0.15,
          (Math.random() - 0.5) * 0.15
        ],
        landed: false
      };
      
      setFallingRocks(prev => [...prev, newRock]);
    }
  });

  const handleLand = (rock: FallingRock, finalPos: [number, number, number]) => {
    setFallingRocks(prev => prev.filter(r => r.id !== rock.id));
    const newLandedRock: LandedRock = {
      id: rock.id,
      position: finalPos,
      size: rock.size,
      rotation: rock.rotation
    };
    setLandedRocks(prev => [...prev, newLandedRock]);
    addLandedRock({
      id: rock.id,
      position: finalPos,
      radius: rock.size * 1.2,
      repelForce: 0.3
    });
  };

  return (
    <group>
      {fallingRocks.map((rock) => (
        <FallingRockMesh
          key={rock.id}
          rock={rock}
          onLand={(pos) => handleLand(rock, pos)}
        />
      ))}
      
      {landedRocks.map((rock) => (
        <LandedRockMesh key={rock.id} rock={rock} />
      ))}
    </group>
  );
}

function FallingRockMesh({ 
  rock, 
  onLand
}: { 
  rock: FallingRock;
  onLand: (pos: [number, number, number]) => void;
}) {
  const meshRef = useRef<THREE.Mesh>(null);
  const posRef = useRef<[number, number, number]>([...rock.position]);
  const velRef = useRef<[number, number, number]>([...rock.velocity]);
  const rotRef = useRef<[number, number, number]>([...rock.rotation]);
  const landedRef = useRef(false);
  
  const { playerEntity, enemies, orbs, updatePlayerVelocity, updateEnemy, updateOrb } = useZoogiGame();

  useFrame(() => {
    if (!meshRef.current || landedRef.current) return;
    
    velRef.current[1] -= 0.015;
    
    posRef.current[0] += velRef.current[0];
    posRef.current[1] += velRef.current[1];
    posRef.current[2] += velRef.current[2];
    
    rotRef.current[0] += rock.rotationSpeed[0];
    rotRef.current[1] += rock.rotationSpeed[1];
    rotRef.current[2] += rock.rotationSpeed[2];
    
    if (posRef.current[1] <= rock.size) {
      posRef.current[1] = rock.size;
      landedRef.current = true;
      
      const IMPACT_RADIUS = 2;
      const IMPACT_FORCE = 0.15;
      
      if (playerEntity) {
        const dx = playerEntity.position[0] - posRef.current[0];
        const dz = playerEntity.position[2] - posRef.current[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        if (dist < IMPACT_RADIUS && dist > 0.1) {
          const force = (1 - dist / IMPACT_RADIUS) * IMPACT_FORCE;
          const newVel: [number, number, number] = [
            playerEntity.velocity[0] + (dx / dist) * force,
            0,
            playerEntity.velocity[2] + (dz / dist) * force
          ];
          updatePlayerVelocity(newVel);
        }
      }
      
      enemies.forEach(enemy => {
        const dx = enemy.position[0] - posRef.current[0];
        const dz = enemy.position[2] - posRef.current[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        if (dist < IMPACT_RADIUS && dist > 0.1) {
          const force = (1 - dist / IMPACT_RADIUS) * IMPACT_FORCE;
          updateEnemy(enemy.id, {
            velocity: [
              enemy.velocity[0] + (dx / dist) * force,
              0,
              enemy.velocity[2] + (dz / dist) * force
            ]
          });
        }
      });
      
      orbs.forEach(orb => {
        if (!orb.isActive) return;
        const dx = orb.position[0] - posRef.current[0];
        const dz = orb.position[2] - posRef.current[2];
        const dist = Math.sqrt(dx * dx + dz * dz);
        if (dist < IMPACT_RADIUS && dist > 0.1) {
          const force = (1 - dist / IMPACT_RADIUS) * IMPACT_FORCE * 0.5;
          updateOrb(orb.id, {
            velocity: [
              orb.velocity[0] + (dx / dist) * force,
              0,
              orb.velocity[2] + (dz / dist) * force
            ]
          });
        }
      });
      
      onLand([...posRef.current]);
    }
    
    meshRef.current.position.set(posRef.current[0], posRef.current[1], posRef.current[2]);
    meshRef.current.rotation.set(rotRef.current[0], rotRef.current[1], rotRef.current[2]);
  });

  return (
    <mesh ref={meshRef} position={rock.position} castShadow>
      <dodecahedronGeometry args={[rock.size, 0]} />
      <meshStandardMaterial 
        color="#2D2D2D"
        roughness={0.9}
        emissive="#FF4500"
        emissiveIntensity={0.3}
      />
    </mesh>
  );
}

function LandedRockMesh({ rock }: { rock: LandedRock }) {
  const groupRef = useRef<THREE.Group>(null);
  const particlesRef = useRef<THREE.Points>(null);
  
  const particles = useMemo(() => {
    const count = 30;
    const positions = new Float32Array(count * 3);
    const velocities: { speed: number; angle: number; ySpeed: number }[] = [];
    
    for (let i = 0; i < count; i++) {
      positions[i * 3] = 0;
      positions[i * 3 + 1] = 0;
      positions[i * 3 + 2] = 0;
      
      velocities.push({
        angle: Math.random() * Math.PI * 2,
        speed: 0.02 + Math.random() * 0.03,
        ySpeed: 0.05 + Math.random() * 0.1
      });
    }
    
    return { positions, velocities };
  }, []);
  
  const positionsRef = useRef(new Float32Array(particles.positions));
  const particleAges = useRef(particles.velocities.map(() => Math.random() * 2));

  useFrame((state, delta) => {
    if (!particlesRef.current) return;
    
    const positions = positionsRef.current;
    const velocities = particles.velocities;
    
    for (let i = 0; i < 30; i++) {
      particleAges.current[i] += delta;
      
      if (particleAges.current[i] > 2) {
        particleAges.current[i] = 0;
        positions[i * 3] = 0;
        positions[i * 3 + 1] = rock.size * 0.5;
        positions[i * 3 + 2] = 0;
      } else {
        const age = particleAges.current[i];
        positions[i * 3] += Math.cos(velocities[i].angle) * velocities[i].speed;
        positions[i * 3 + 1] += velocities[i].ySpeed * (1 - age / 2) - 0.02;
        positions[i * 3 + 2] += Math.sin(velocities[i].angle) * velocities[i].speed;
        
        if (positions[i * 3 + 1] < 0) {
          particleAges.current[i] = 2;
        }
      }
    }
    
    particlesRef.current.geometry.attributes.position.needsUpdate = true;
  });

  return (
    <group ref={groupRef} position={rock.position}>
      <mesh rotation={rock.rotation} castShadow>
        <dodecahedronGeometry args={[rock.size, 0]} />
        <meshStandardMaterial 
          color="#2D2D2D"
          roughness={0.9}
          emissive="#FF4500"
          emissiveIntensity={0.4}
        />
      </mesh>
      
      <points ref={particlesRef}>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={30}
            array={positionsRef.current}
            itemSize={3}
          />
        </bufferGeometry>
        <pointsMaterial
          size={0.15}
          color="#FF6600"
          transparent
          opacity={0.9}
        />
      </points>
      
      <pointLight 
        position={[0, rock.size, 0]} 
        color="#FF4500" 
        intensity={0.5} 
        distance={3} 
      />
    </group>
  );
}

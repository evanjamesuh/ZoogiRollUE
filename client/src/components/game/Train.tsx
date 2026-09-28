import * as THREE from "three";
import { useRef, useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import { ARENA_RADIUS, setTrainCarPositions } from "@/lib/arenaConstants";

const TRAIN_RADIUS = ARENA_RADIUS - 3;
const TRAIN_SPEED = 0.3;
const CAR_COUNT = 6;
const CAR_SPACING = (Math.PI * 2) / CAR_COUNT;

let currentTrainAngle = 0;

interface TrainCar {
  id: number;
  color: string;
  baseAngle: number;
}

function TrainCarMesh({ car, currentAngle }: { car: TrainCar; currentAngle: number }) {
  const groupRef = useRef<THREE.Group>(null);
  
  const angle = currentAngle + car.baseAngle;
  const x = Math.cos(angle) * TRAIN_RADIUS;
  const z = Math.sin(angle) * TRAIN_RADIUS;
  
  return (
    <group ref={groupRef} position={[x, 0.6, z]} rotation={[0, -angle + Math.PI / 2, 0]}>
      <mesh castShadow>
        <boxGeometry args={[2.2, 1, 1.2]} />
        <meshStandardMaterial color={car.color} />
      </mesh>
      
      <mesh position={[0, 0.6, 0]} castShadow>
        <boxGeometry args={[1.8, 0.4, 1]} />
        <meshStandardMaterial color={car.color} />
      </mesh>
      
      <mesh position={[-0.8, -0.3, 0.5]}>
        <cylinderGeometry args={[0.2, 0.2, 0.3, 16]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      <mesh position={[0.8, -0.3, 0.5]}>
        <cylinderGeometry args={[0.2, 0.2, 0.3, 16]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      <mesh position={[-0.8, -0.3, -0.5]}>
        <cylinderGeometry args={[0.2, 0.2, 0.3, 16]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      <mesh position={[0.8, -0.3, -0.5]}>
        <cylinderGeometry args={[0.2, 0.2, 0.3, 16]} />
        <meshStandardMaterial color="#333" />
      </mesh>
      
      {car.id === 0 && (
        <>
          <mesh position={[1.3, 0.4, 0]} castShadow>
            <cylinderGeometry args={[0.25, 0.2, 0.6, 12]} />
            <meshStandardMaterial color="#333" />
          </mesh>
          <pointLight position={[1.5, 0.4, 0]} color="#FFD700" intensity={0.5} distance={3} />
        </>
      )}
    </group>
  );
}

export function Train() {
  const angleRef = useRef(0);
  const groupRef = useRef<THREE.Group>(null);
  
  const cars = useMemo<TrainCar[]>(() => {
    const colors = ["#8B4513", "#A0522D", "#CD853F", "#DEB887", "#D2691E", "#B8860B"];
    return Array.from({ length: CAR_COUNT }, (_, i) => ({
      id: i,
      color: colors[i % colors.length],
      baseAngle: i * CAR_SPACING
    }));
  }, []);
  
  useFrame((state, delta) => {
    updateTrainAngle(delta);
    angleRef.current = currentTrainAngle;
    
    const positions: { position: [number, number, number]; velocity: [number, number, number]; radius: number }[] = [];
    for (let i = 0; i < CAR_COUNT; i++) {
      const angle = currentTrainAngle + i * CAR_SPACING;
      const x = Math.cos(angle) * TRAIN_RADIUS;
      const z = Math.sin(angle) * TRAIN_RADIUS;
      const tangentX = -Math.sin(angle) * TRAIN_SPEED * TRAIN_RADIUS;
      const tangentZ = Math.cos(angle) * TRAIN_SPEED * TRAIN_RADIUS;
      positions.push({
        position: [x, 0.6, z],
        velocity: [tangentX * 0.02, 0, tangentZ * 0.02],
        radius: 1.2
      });
    }
    setTrainCarPositions(positions);
  });
  
  return (
    <group ref={groupRef}>
      {cars.map((car) => (
        <TrainCarMesh 
          key={car.id} 
          car={car} 
          currentAngle={angleRef.current}
        />
      ))}
      
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.05, 0]}>
        <ringGeometry args={[TRAIN_RADIUS - 0.3, TRAIN_RADIUS + 0.3, 64]} />
        <meshStandardMaterial color="#4a3728" />
      </mesh>
      
      {Array.from({ length: 24 }).map((_, i) => {
        const angle = (i / 24) * Math.PI * 2;
        const x = Math.cos(angle) * TRAIN_RADIUS;
        const z = Math.sin(angle) * TRAIN_RADIUS;
        return (
          <mesh key={i} position={[x, 0.08, z]} rotation={[0, angle, 0]}>
            <boxGeometry args={[0.8, 0.08, 0.15]} />
            <meshStandardMaterial color="#5D4037" />
          </mesh>
        );
      })}
    </group>
  );
}

export function getTrainCarPositions(): { position: [number, number, number]; radius: number; velocity: [number, number, number] }[] {
  const positions: { position: [number, number, number]; radius: number; velocity: [number, number, number] }[] = [];
  
  for (let i = 0; i < CAR_COUNT; i++) {
    const angle = currentTrainAngle + i * CAR_SPACING;
    const x = Math.cos(angle) * TRAIN_RADIUS;
    const z = Math.sin(angle) * TRAIN_RADIUS;
    
    const tangentX = -Math.sin(angle) * TRAIN_SPEED * TRAIN_RADIUS;
    const tangentZ = Math.cos(angle) * TRAIN_SPEED * TRAIN_RADIUS;
    
    positions.push({
      position: [x, 0.6, z],
      radius: 1.2,
      velocity: [tangentX * 0.02, 0, tangentZ * 0.02]
    });
  }
  
  return positions;
}

export function updateTrainAngle(delta: number) {
  currentTrainAngle += delta * TRAIN_SPEED;
  if (currentTrainAngle > Math.PI * 2) {
    currentTrainAngle -= Math.PI * 2;
  }
}

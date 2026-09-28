import * as THREE from "three";
import { useRef } from "react";
import { useFrame } from "@react-three/fiber";

interface PowerUpProps {
  type: "speed" | "shield" | "power";
  position: [number, number, number];
  collected: boolean;
}

const POWER_UP_COLORS = {
  speed: "#3B82F6",
  shield: "#22C55E",
  power: "#F59E0B"
};

const POWER_UP_SHAPES = {
  speed: "cone",
  shield: "octahedron",
  power: "box"
};

export function PowerUp({ type, position, collected }: PowerUpProps) {
  const meshRef = useRef<THREE.Mesh>(null);
  const glowRef = useRef<THREE.Mesh>(null);

  useFrame((state) => {
    if (meshRef.current && !collected) {
      meshRef.current.rotation.y += 0.02;
      meshRef.current.position.y = position[1] + Math.sin(state.clock.elapsedTime * 2) * 0.1;
    }
    if (glowRef.current && !collected) {
      const scale = 1 + Math.sin(state.clock.elapsedTime * 3) * 0.1;
      glowRef.current.scale.set(scale, scale, scale);
    }
  });

  if (collected) return null;

  const color = POWER_UP_COLORS[type];

  return (
    <group position={position}>
      <mesh ref={glowRef}>
        <sphereGeometry args={[0.6, 16, 16]} />
        <meshBasicMaterial color={color} transparent opacity={0.2} />
      </mesh>

      <mesh ref={meshRef} castShadow>
        {type === "speed" && <coneGeometry args={[0.3, 0.6, 6]} />}
        {type === "shield" && <octahedronGeometry args={[0.35]} />}
        {type === "power" && <boxGeometry args={[0.4, 0.4, 0.4]} />}
        <meshStandardMaterial
          color={color}
          emissive={color}
          emissiveIntensity={0.5}
          metalness={0.8}
          roughness={0.2}
        />
      </mesh>

      <pointLight color={color} intensity={0.5} distance={3} />
    </group>
  );
}

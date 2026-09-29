import * as THREE from "three";
import { Component, type ReactNode, useRef, useState, useEffect, useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { BUMPER_MODEL_URL } from "@/lib/arenaColliders";

/** A missing bumper model should not take down the whole court. */
class BumperBoundary extends Component<{ children: ReactNode; position: [number, number, number] }, { failed: boolean }> {
  state = { failed: false };
  static getDerivedStateFromError() {
    return { failed: true };
  }
  render() {
    if (this.state.failed) {
      const [x, y, z] = this.props.position;
      return (
        <mesh position={[x, (y || 0) + 0.7, z]}>
          <cylinderGeometry args={[0.7, 0.82, 1.3, 16]} />
          <meshStandardMaterial color="#c45512" roughness={0.45} metalness={0.2} />
        </mesh>
      );
    }
    return this.props.children;
  }
}

export function PinballBumpers() {
  const { pinballBumpers, selectedMap } = useZoogiGame();
  if (selectedMap === "neon") return null;

  return (
    <group>
      {pinballBumpers.map((bumper) => (
        <BumperBoundary key={bumper.id} position={bumper.position}>
          <PinballBumper bumper={bumper} />
        </BumperBoundary>
      ))}
    </group>
  );
}

interface PinballBumperProps {
  bumper: {
    id: string;
    position: [number, number, number];
    repelForce: number;
    lastHitTime?: number;
    pointValue: number;
  };
}

function PinballBumper({ bumper }: PinballBumperProps) {
  const groupRef = useRef<THREE.Group>(null);
  const glowRef = useRef<THREE.PointLight>(null);
  const [jiggleIntensity, setJiggleIntensity] = useState(0);
  const [glowIntensity, setGlowIntensity] = useState(0);
  const lastHitTimeRef = useRef(0);
  const { playSound } = useAudio();
  const { scene } = useGLTF(BUMPER_MODEL_URL);
  const cloned = useMemo(() => scene.clone(true), [scene]);

  useEffect(() => {
    if (bumper.lastHitTime && bumper.lastHitTime > lastHitTimeRef.current) {
      lastHitTimeRef.current = bumper.lastHitTime;
      setJiggleIntensity(1);
      setGlowIntensity(2);
      playSound("collision_mushroom");
    }
  }, [bumper.lastHitTime, playSound]);

  useFrame((_state, delta) => {
    if (groupRef.current && jiggleIntensity > 0) {
      const jiggle = Math.sin(Date.now() * 0.03) * jiggleIntensity * 0.2;
      groupRef.current.scale.set(
        1 + jiggle,
        1 - jiggle * 0.5,
        1 + jiggle
      );
      setJiggleIntensity(prev => Math.max(0, prev - delta * 4));
    } else if (groupRef.current) {
      groupRef.current.scale.set(1, 1, 1);
    }

    if (glowRef.current) {
      if (glowIntensity > 0) {
        glowRef.current.intensity = glowIntensity;
        setGlowIntensity(prev => Math.max(0, prev - delta * 5));
      } else {
        glowRef.current.intensity = 0.3;
      }
    }
  });

  return (
    <group
      ref={groupRef}
      position={[bumper.position[0], bumper.position[1], bumper.position[2]]}
      scale={[1, 1, 1]}
    >
      <primitive object={cloned} />
      <pointLight
        ref={glowRef}
        position={[0, 1, 0]}
        color="#ff6600"
        intensity={0.3}
        distance={5}
      />
    </group>
  );
}


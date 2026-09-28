import * as THREE from "three";
import { useRef, useState, useEffect } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";

export function PinballBumpers() {
  const { pinballBumpers } = useZoogiGame();

  return (
    <group>
      {pinballBumpers.map((bumper) => (
        <PinballBumper key={bumper.id} bumper={bumper} />
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
  const { scene } = useGLTF("/models/bumper.glb");

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
        1.5 + jiggle,
        1.5 - jiggle * 0.5,
        1.5 + jiggle
      );
      setJiggleIntensity(prev => Math.max(0, prev - delta * 4));
    } else if (groupRef.current) {
      groupRef.current.scale.set(1.5, 1.5, 1.5);
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
      scale={[1.5, 1.5, 1.5]}
    >
      <primitive object={scene.clone()} />
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

useGLTF.preload("/models/bumper.glb");

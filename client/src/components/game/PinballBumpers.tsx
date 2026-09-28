import * as THREE from "three";
import { useRef, useState, useEffect, useMemo, Suspense, Component, ReactNode } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { BUMPER_MODEL_URL, BUMPER_RADIUS } from "@/lib/arenaColliders";

class BumperErrorBoundary extends Component<{ children: ReactNode; fallback: ReactNode }, { hasError: boolean }> {
  constructor(props: { children: ReactNode; fallback: ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() { return { hasError: true }; }
  render() { return this.state.hasError ? this.props.fallback : this.props.children; }
}

function FallbackBumper({ position }: { position: [number, number, number] }) {
  return (
    <mesh position={[position[0], 0.55, position[2]]} castShadow>
      <cylinderGeometry args={[BUMPER_RADIUS, BUMPER_RADIUS * 0.85, 1.1, 20]} />
      <meshStandardMaterial color="#ff4fd8" metalness={0.35} roughness={0.35} />
    </mesh>
  );
}

export function PinballBumpers() {
  const { pinballBumpers } = useZoogiGame();

  return (
    <group>
      {pinballBumpers.map((bumper) => (
        <BumperErrorBoundary key={bumper.id} fallback={<FallbackBumper position={bumper.position} />}>
          <Suspense fallback={<FallbackBumper position={bumper.position} />}>
            <PinballBumper bumper={bumper} />
          </Suspense>
        </BumperErrorBoundary>
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

useGLTF.preload(BUMPER_MODEL_URL);

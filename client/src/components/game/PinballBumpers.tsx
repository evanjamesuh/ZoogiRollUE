import * as THREE from "three";
import { useRef, useMemo, Suspense, Component, ReactNode } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { BUMPER_MODEL_URL, BUMPER_RADIUS } from "@/lib/arenaColliders";

class BumperModelErrorBoundary extends Component<{ children: ReactNode; fallback: ReactNode }, { hasError: boolean }> {
  state = { hasError: false };
  static getDerivedStateFromError() {
    return { hasError: true };
  }
  render() {
    return this.state.hasError ? this.props.fallback : this.props.children;
  }
}

function BumperStandIn() {
  return (
    <mesh position={[0, 0.55, 0]} castShadow>
      <cylinderGeometry args={[BUMPER_RADIUS, BUMPER_RADIUS, 1.1, 20]} />
      <meshStandardMaterial color="#F59E0B" emissive="#F59E0B" emissiveIntensity={0.35} />
    </mesh>
  );
}

function BumperModel() {
  const { scene } = useGLTF(BUMPER_MODEL_URL);
  const cloned = useMemo(() => scene.clone(true), [scene]);
  return <primitive object={cloned} />;
}

export function PinballBumpers() {
  const { pinballBumpers, selectedMap } = useZoogiGame();
  if (selectedMap === "neon") return null;

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
  const jiggleRef = useRef(0);
  const glowAmtRef = useRef(0);
  const lastHitTimeRef = useRef(0);
  const { playSound } = useAudio();

  useFrame((_state, delta) => {
    if (bumper.lastHitTime && bumper.lastHitTime > lastHitTimeRef.current) {
      lastHitTimeRef.current = bumper.lastHitTime;
      jiggleRef.current = 1;
      glowAmtRef.current = 2;
      playSound("collision_mushroom");
    }

    if (groupRef.current && jiggleRef.current > 0) {
      const jiggle = Math.sin(Date.now() * 0.03) * jiggleRef.current * 0.2;
      groupRef.current.scale.set(
        1 + jiggle,
        1 - jiggle * 0.5,
        1 + jiggle
      );
      jiggleRef.current = Math.max(0, jiggleRef.current - delta * 4);
    } else if (groupRef.current) {
      groupRef.current.scale.set(1, 1, 1);
    }

    if (glowRef.current) {
      if (glowAmtRef.current > 0) {
        glowRef.current.intensity = glowAmtRef.current;
        glowAmtRef.current = Math.max(0, glowAmtRef.current - delta * 5);
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
      <BumperModelErrorBoundary fallback={<BumperStandIn />}>
        <Suspense fallback={<BumperStandIn />}>
          <BumperModel />
        </Suspense>
      </BumperModelErrorBoundary>
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

import * as THREE from "three";
import { useMemo, useRef, Suspense, Component, ReactNode } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { getHoodooDecor, type HoodooDecor } from "@/lib/arenaColliders";
import { fittedHoodooGeometry, HOODOO_LOCAL_COLLIDER_RADIUS } from "./volcanicHoodooFit";

type HoodooData = HoodooDecor;

class HoodooErrorBoundary extends Component<{ children: ReactNode; fallback: ReactNode }, { hasError: boolean }> {
  constructor(props: { children: ReactNode; fallback: ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() { return { hasError: true }; }
  render() { return this.state.hasError ? this.props.fallback : this.props.children; }
}

function StylizedHoodoo({ hoodoo }: { hoodoo: HoodooData }) {
  const groupRef = useRef<THREE.Group>(null);
  const wobbleRef = useRef({ intensity: 0 });
  const lastHitTimeRef = useRef(0);
  
  const model = useGLTF("/models/stylized_desert_hoodoo.glb");
  const clonedScene = useMemo(() => {
    const root = model.scene.clone(true);
    root.traverse((obj) => {
      if (!(obj instanceof THREE.Mesh)) return;
      obj.geometry = fittedHoodooGeometry(obj.geometry);
    });
    return root;
  }, [model.scene]);
  
  const playerEntity = useZoogiGame(state => state.playerEntity);
  const enemies = useZoogiGame(state => state.enemies);
  const { playSound } = useAudio();
  
  useFrame((state, delta) => {
    if (!groupRef.current) return;
    
    const checkCollision = (pos: [number, number, number]) => {
      const dx = pos[0] - hoodoo.position[0];
      const dz = pos[2] - hoodoo.position[2];
      const dist = Math.sqrt(dx * dx + dz * dz);
      return dist < hoodoo.radius + 0.5;
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
      wobbleRef.current.intensity = 1;
      playSound("collision_mushroom");
    }
    
    if (wobbleRef.current.intensity > 0) {
      const shake = Math.sin(state.clock.elapsedTime * 30) * wobbleRef.current.intensity * 0.05;
      groupRef.current.rotation.z = shake;
      wobbleRef.current.intensity = Math.max(0, wobbleRef.current.intensity - delta * 2);
    } else {
      groupRef.current.rotation.z = 0;
    }
  });

  return (
    <group ref={groupRef} position={hoodoo.position} scale={hoodoo.scale} rotation={[0, hoodoo.rotation, 0]}>
      <primitive object={clonedScene} />
    </group>
  );
}

function FallbackHoodoo({ hoodoo }: { hoodoo: HoodooData }) {
  // Same local space as the fitted mesh: radius 0.25, base on the ground after the lift.
  const height = 0.75;
  const base = -0.367;
  return (
    <group position={hoodoo.position} scale={hoodoo.scale} rotation={[0, hoodoo.rotation, 0]}>
      <mesh position={[0, base + height / 2, 0]} castShadow>
        <cylinderGeometry args={[HOODOO_LOCAL_COLLIDER_RADIUS, HOODOO_LOCAL_COLLIDER_RADIUS, height, 12]} />
        <meshStandardMaterial color="#3a332c" roughness={0.94} />
      </mesh>
    </group>
  );
}

function HoodooWithFallback({ hoodoo }: { hoodoo: HoodooData }) {
  return (
    <HoodooErrorBoundary fallback={<FallbackHoodoo hoodoo={hoodoo} />}>
      <Suspense fallback={<FallbackHoodoo hoodoo={hoodoo} />}>
        <StylizedHoodoo hoodoo={hoodoo} />
      </Suspense>
    </HoodooErrorBoundary>
  );
}

export function DesertHoodoos() {
  const hoodooData = useMemo<HoodooData[]>(() => getHoodooDecor(), []);

  return (
    <group>
      {hoodooData.map((hoodoo) => (
        <HoodooWithFallback key={hoodoo.id} hoodoo={hoodoo} />
      ))}
    </group>
  );
}

export function getHoodooPositions(): { position: [number, number, number]; radius: number }[] {
  return getHoodooDecor().map((hoodoo) => ({
    position: hoodoo.position,
    radius: hoodoo.radius,
  }));
}

useGLTF.preload("/models/stylized_desert_hoodoo.glb");

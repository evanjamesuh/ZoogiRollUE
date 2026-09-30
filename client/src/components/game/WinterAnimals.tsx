import * as THREE from "three";
import { useRef, useMemo, Suspense, Component, ReactNode } from "react";
import { useFrame } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useAudio } from "@/lib/stores/useAudio";
import { ARENA_RADIUS } from "@/lib/arenaConstants";

interface AnimalData {
  id: string;
  type: "penguin" | "fox" | "polar_bear" | "walrus" | "fish";
  modelPath: string;
  position: [number, number, number];
  scale: number;
  rotationY: number;
}

const ICE_SKATE_RADIUS = ARENA_RADIUS - 2;

function outsideRing(angleDeg: number, distance: number, y = 0): [number, number, number] {
  const angle = (angleDeg * Math.PI) / 180;
  return [Math.cos(angle) * distance, y, Math.sin(angle) * distance];
}

/** Camp animals stand outside the 15.5 knockoff, around the snowy rim. */
const ANIMAL_PLACEMENTS: AnimalData[] = [
  { id: "fox-0", type: "fox", modelPath: "/models/winter/fox.glb", position: outsideRing(210, 21.5), scale: 1.5, rotationY: Math.PI / 6 },
  { id: "polar-bear-0", type: "polar_bear", modelPath: "/models/winter/polar_bear.glb", position: outsideRing(80, 22.5), scale: 1.8, rotationY: Math.PI },
  { id: "walrus-0", type: "walrus", modelPath: "/models/winter/walrus.glb", position: outsideRing(10, 21), scale: 1.4, rotationY: -Math.PI / 2 },
  { id: "fish-0", type: "fish", modelPath: "/models/winter/fish_1.glb", position: outsideRing(140, 20.2, 0.3), scale: 0.8, rotationY: Math.PI / 3 },
  { id: "fish-1", type: "fish", modelPath: "/models/winter/fish_2.glb", position: outsideRing(300, 20.5, 0.3), scale: 0.7, rotationY: -Math.PI / 4 },
  { id: "fish-2", type: "fish", modelPath: "/models/winter/fish_1.glb", position: outsideRing(250, 19.8, 0.3), scale: 0.9, rotationY: Math.PI / 2 },
];

function getRandomIceTarget(): [number, number] {
  const angle = Math.random() * Math.PI * 2;
  const dist = Math.random() * ICE_SKATE_RADIUS * 0.8;
  return [Math.cos(angle) * dist, Math.sin(angle) * dist];
}

function SkatingPenguin({ animal }: { animal: AnimalData }) {
  const groupRef = useRef<THREE.Group>(null);
  const jiggleRef = useRef(0);
  const lastHitTimeRef = useRef(0);
  const posRef = useRef({ x: animal.position[0], z: animal.position[2] });
  const targetRef = useRef<{ x: number; z: number }>({ x: 0, z: 0 });
  const facingRef = useRef(animal.rotationY);
  const waddlePhase = useRef(Math.random() * Math.PI * 2);
  const waitTimer = useRef(0);
  const isWaiting = useRef(false);

  useMemo(() => {
    const [tx, tz] = getRandomIceTarget();
    targetRef.current = { x: tx, z: tz };
  }, []);

  const { scene } = useGLTF(animal.modelPath);
  const clonedScene = useMemo(() => {
    const clone = scene.clone();
    clone.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
    return clone;
  }, [scene]);

  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const enemies = useZoogiGame((state) => state.enemies);
  const { playSound } = useAudio();

  useFrame((state, delta) => {
    if (!groupRef.current) return;

    const time = state.clock.elapsedTime;
    const SKATE_SPEED = 2.5;
    const ARRIVE_DIST = 1.5;

    if (isWaiting.current) {
      waitTimer.current -= delta;
      if (waitTimer.current <= 0) {
        isWaiting.current = false;
        const [tx, tz] = getRandomIceTarget();
        targetRef.current = { x: tx, z: tz };
      }
    } else {
      const dx = targetRef.current.x - posRef.current.x;
      const dz = targetRef.current.z - posRef.current.z;
      const dist = Math.sqrt(dx * dx + dz * dz);

      if (dist < ARRIVE_DIST) {
        isWaiting.current = true;
        waitTimer.current = 1.0 + Math.random() * 2.0;
      } else {
        const nx = dx / dist;
        const nz = dz / dist;
        posRef.current.x += nx * SKATE_SPEED * delta;
        posRef.current.z += nz * SKATE_SPEED * delta;

        const targetAngle = Math.atan2(nx, nz);
        let angleDiff = targetAngle - facingRef.current;
        while (angleDiff > Math.PI) angleDiff -= Math.PI * 2;
        while (angleDiff < -Math.PI) angleDiff += Math.PI * 2;
        facingRef.current += angleDiff * Math.min(1, delta * 5);
      }
    }

    const checkCollision = (pos: [number, number, number]) => {
      const cdx = pos[0] - posRef.current.x;
      const cdz = pos[2] - posRef.current.z;
      return Math.sqrt(cdx * cdx + cdz * cdz) < 2.0;
    };

    let wasHit = false;
    if (playerEntity && checkCollision(playerEntity.position)) {
      const now = Date.now();
      if (now - lastHitTimeRef.current > 500) {
        wasHit = true;
        lastHitTimeRef.current = now;
      }
    }

    if (!wasHit) {
      enemies.forEach((enemy) => {
        if (checkCollision(enemy.position)) {
          const now = Date.now();
          if (now - lastHitTimeRef.current > 500) {
            wasHit = true;
            lastHitTimeRef.current = now;
          }
        }
      });
    }

    if (wasHit) {
      jiggleRef.current = 1;
      playSound("collision_snowman");
      const [tx, tz] = getRandomIceTarget();
      targetRef.current = { x: tx, z: tz };
      isWaiting.current = false;
    }

    waddlePhase.current += delta * 8;
    const waddle = isWaiting.current ? 0 : Math.sin(waddlePhase.current) * 0.08;

    if (jiggleRef.current > 0) {
      const jiggle = Math.sin(time * 25) * jiggleRef.current * 0.12;
      groupRef.current.rotation.z = jiggle;
      groupRef.current.rotation.x = jiggle * 0.5;
      groupRef.current.scale.set(
        animal.scale * (1 + jiggle * 0.4),
        animal.scale * (1 - jiggle * 0.3),
        animal.scale * (1 + jiggle * 0.4)
      );
      jiggleRef.current = Math.max(0, jiggleRef.current - delta * 3);
    } else {
      groupRef.current.rotation.z = waddle;
      groupRef.current.rotation.x = 0;
      groupRef.current.scale.set(animal.scale, animal.scale, animal.scale);
    }

    groupRef.current.position.set(posRef.current.x, animal.position[1], posRef.current.z);
    groupRef.current.rotation.y = facingRef.current;
  });

  return (
    <group
      ref={groupRef}
      position={animal.position}
      scale={[animal.scale, animal.scale, animal.scale]}
      rotation={[0, animal.rotationY, 0]}
    >
      <primitive object={clonedScene} />
    </group>
  );
}

function AnimalModel({ animal }: { animal: AnimalData }) {
  const groupRef = useRef<THREE.Group>(null);
  const jiggleRef = useRef(0);
  const lastHitTimeRef = useRef(0);
  const bobPhaseRef = useRef(Math.random() * Math.PI * 2);
  const lookPhaseRef = useRef(Math.random() * Math.PI * 2);

  const { scene } = useGLTF(animal.modelPath);
  const clonedScene = useMemo(() => {
    const clone = scene.clone();
    clone.traverse((child) => {
      if (child instanceof THREE.Mesh) {
        child.castShadow = true;
        child.receiveShadow = true;
      }
    });
    return clone;
  }, [scene]);

  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const enemies = useZoogiGame((state) => state.enemies);
  const { playSound } = useAudio();

  useFrame((state, delta) => {
    if (!groupRef.current) return;

    const time = state.clock.elapsedTime;
    const bobOffset = Math.sin(time * 1.5 + bobPhaseRef.current) * 0.08;
    const lookOffset = Math.sin(time * 0.4 + lookPhaseRef.current) * 0.15;

    const checkCollision = (pos: [number, number, number]) => {
      const dx = pos[0] - animal.position[0];
      const dz = pos[2] - animal.position[2];
      return Math.sqrt(dx * dx + dz * dz) < 2.0;
    };

    let wasHit = false;
    if (playerEntity && checkCollision(playerEntity.position)) {
      const now = Date.now();
      if (now - lastHitTimeRef.current > 500) {
        wasHit = true;
        lastHitTimeRef.current = now;
      }
    }

    if (!wasHit) {
      enemies.forEach((enemy) => {
        if (checkCollision(enemy.position)) {
          const now = Date.now();
          if (now - lastHitTimeRef.current > 500) {
            wasHit = true;
            lastHitTimeRef.current = now;
          }
        }
      });
    }

    if (wasHit) {
      jiggleRef.current = 1;
      playSound("collision_snowman");
    }

    if (jiggleRef.current > 0) {
      const jiggle = Math.sin(time * 25) * jiggleRef.current * 0.12;
      groupRef.current.rotation.z = jiggle;
      groupRef.current.rotation.x = jiggle * 0.5;
      groupRef.current.scale.set(
        animal.scale * (1 + jiggle * 0.4),
        animal.scale * (1 - jiggle * 0.3),
        animal.scale * (1 + jiggle * 0.4)
      );
      jiggleRef.current = Math.max(0, jiggleRef.current - delta * 3);
    } else {
      groupRef.current.rotation.z = 0;
      groupRef.current.rotation.x = 0;
      groupRef.current.scale.set(animal.scale, animal.scale, animal.scale);
    }

    groupRef.current.position.y = animal.position[1] + bobOffset;
    groupRef.current.rotation.y = animal.rotationY + lookOffset;
  });

  return (
    <group
      ref={groupRef}
      position={animal.position}
      scale={[animal.scale, animal.scale, animal.scale]}
      rotation={[0, animal.rotationY, 0]}
    >
      <primitive object={clonedScene} />
    </group>
  );
}

function FallbackAnimal({ animal }: { animal: AnimalData }) {
  const color =
    animal.type === "penguin" ? "#1a1a2e" :
    animal.type === "fox" ? "#D2691E" :
    animal.type === "polar_bear" ? "#F5F5F5" :
    animal.type === "walrus" ? "#8B7355" :
    "#87CEEB";

  return (
    <group position={animal.position} scale={[animal.scale, animal.scale, animal.scale]}>
      <mesh position={[0, 0.3, 0]} castShadow>
        <sphereGeometry args={[0.3, 8, 8]} />
        <meshStandardMaterial color={color} />
      </mesh>
      <mesh position={[0, 0.7, 0]} castShadow>
        <sphereGeometry args={[0.2, 8, 8]} />
        <meshStandardMaterial color={color} />
      </mesh>
    </group>
  );
}

class AnimalErrorBoundary extends Component<{ children: ReactNode; fallback: ReactNode }, { hasError: boolean }> {
  constructor(props: { children: ReactNode; fallback: ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() { return { hasError: true }; }
  render() { return this.state.hasError ? this.props.fallback : this.props.children; }
}

function AnimalWithFallback({ animal }: { animal: AnimalData }) {
  const model = animal.type === "penguin"
    ? <SkatingPenguin animal={animal} />
    : <AnimalModel animal={animal} />;
  return (
    <AnimalErrorBoundary fallback={<FallbackAnimal animal={animal} />}>
      <Suspense fallback={<FallbackAnimal animal={animal} />}>
        {model}
      </Suspense>
    </AnimalErrorBoundary>
  );
}

export function WinterAnimals() {
  return (
    <group>
      {ANIMAL_PLACEMENTS.map((animal) => (
        <AnimalWithFallback key={animal.id} animal={animal} />
      ))}
    </group>
  );
}

import * as THREE from "three";
import { useRef, useMemo, useState, useEffect } from "react";
import { useFrame } from "@react-three/fiber";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { useGameFeel } from "@/lib/stores/useGameFeel";

const ZOOGI_COLORS: Record<string, string> = {
  wolfgang: "#6B7280",
  hotstreak: "#F97316",
  lars: "#3B82F6",
  pinpoint: "#8B5CF6",
  bolt: "#FBBF24",
};

interface TrailPoint {
  position: [number, number, number];
  timestamp: number;
}

function MotionTrail({ position, velocity, color }: { 
  position: [number, number, number]; 
  velocity: [number, number, number];
  color: string;
}) {
  const trailRef = useRef<TrailPoint[]>([]);
  const meshRefs = useRef<(THREE.Mesh | null)[]>([]);
  const TRAIL_LENGTH = 12;
  const TRAIL_LIFETIME = 300;
  
  useFrame(() => {
    const speed = Math.sqrt(velocity[0] ** 2 + velocity[2] ** 2);
    const now = Date.now();
    
    if (speed > 0.05) {
      trailRef.current.push({ position: [...position], timestamp: now });
    }
    
    trailRef.current = trailRef.current.filter(p => now - p.timestamp < TRAIL_LIFETIME);
    
    if (trailRef.current.length > TRAIL_LENGTH) {
      trailRef.current = trailRef.current.slice(-TRAIL_LENGTH);
    }
    
    meshRefs.current.forEach((mesh, i) => {
      if (mesh) {
        if (i < trailRef.current.length) {
          const point = trailRef.current[i];
          const age = (now - point.timestamp) / TRAIL_LIFETIME;
          const scale = Math.max(0.1, (1 - age) * 0.4);
          
          mesh.visible = true;
          mesh.position.set(point.position[0], point.position[1], point.position[2]);
          mesh.scale.setScalar(scale);
          (mesh.material as THREE.MeshBasicMaterial).opacity = (1 - age) * 0.6;
        } else {
          mesh.visible = false;
        }
      }
    });
  });
  
  return (
    <group>
      {Array.from({ length: TRAIL_LENGTH }).map((_, i) => (
        <mesh 
          key={i}
          ref={(el) => { meshRefs.current[i] = el; }}
          visible={false}
        >
          <sphereGeometry args={[0.5, 8, 8]} />
          <meshBasicMaterial color={color} transparent opacity={0.6} />
        </mesh>
      ))}
    </group>
  );
}

export function MotionTrails() {
  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const enemies = useZoogiGame((state) => state.enemies);
  const gameMode = useZoogiGame((state) => state.gameMode);
  const localPlayers = useZoogiGame((state) => state.localPlayers);
  
  if (gameMode === "local_multiplayer") {
    return (
      <>
        {localPlayers.map((player, index) => {
          const entity = index === 0 ? playerEntity : enemies[index - 1];
          if (!entity) return null;
          const color = ZOOGI_COLORS[entity.zoogi.id] || entity.zoogi.color;
          return (
            <MotionTrail 
              key={`local-trail-${index}`}
              position={entity.position}
              velocity={entity.velocity}
              color={color}
            />
          );
        })}
      </>
    );
  }
  
  return (
    <>
      {playerEntity && (
        <MotionTrail 
          position={playerEntity.position}
          velocity={playerEntity.velocity}
          color={ZOOGI_COLORS[playerEntity.zoogi.id] || playerEntity.zoogi.color}
        />
      )}
      {enemies.map((enemy) => (
        <MotionTrail 
          key={`trail-${enemy.id}`}
          position={enemy.position}
          velocity={enemy.velocity}
          color={ZOOGI_COLORS[enemy.zoogi.id] || enemy.zoogi.color}
        />
      ))}
    </>
  );
}

export function SpeedLines() {
  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const linesRef = useRef<THREE.Group>(null);
  const lineDataRef = useRef<{ offset: number; speed: number; angle: number }[]>(
    Array.from({ length: 20 }).map(() => ({
      offset: Math.random() * 10,
      speed: 0.5 + Math.random() * 0.5,
      angle: Math.random() * Math.PI * 2
    }))
  );
  
  useFrame((state) => {
    if (!playerEntity || !linesRef.current) return;
    
    const speed = Math.sqrt(playerEntity.velocity[0] ** 2 + playerEntity.velocity[2] ** 2);
    const visible = speed > 0.3;
    
    linesRef.current.visible = visible;
    
    if (visible) {
      const moveAngle = Math.atan2(playerEntity.velocity[0], playerEntity.velocity[2]);
      linesRef.current.position.set(
        playerEntity.position[0],
        playerEntity.position[1] + 0.3,
        playerEntity.position[2]
      );
      linesRef.current.rotation.y = moveAngle;
      
      linesRef.current.children.forEach((child, i) => {
        const data = lineDataRef.current[i];
        const mesh = child as THREE.Mesh;
        data.offset = (data.offset + speed * data.speed * 0.1) % 4;
        mesh.position.z = -data.offset - 1;
        mesh.position.x = Math.sin(data.angle) * 1.5;
        mesh.position.y = Math.cos(data.angle) * 0.5;
        const opacity = Math.min(speed * 2, 0.4);
        (mesh.material as THREE.MeshBasicMaterial).opacity = opacity;
      });
    }
  });
  
  return (
    <group ref={linesRef} visible={false}>
      {lineDataRef.current.map((_, i) => (
        <mesh key={i}>
          <boxGeometry args={[0.02, 0.02, 0.8]} />
          <meshBasicMaterial color="#FFFFFF" transparent opacity={0.3} />
        </mesh>
      ))}
    </group>
  );
}

interface Particle {
  id: number;
  position: [number, number, number];
  velocity: [number, number, number];
  life: number;
  maxLife: number;
  color: string;
  size: number;
}

interface ParticleView {
  id: number;
  color: string;
  size: number;
}

function sameParticleIds(live: { id: number }[], published: number[]): boolean {
  if (live.length !== published.length) return false;
  for (let i = 0; i < live.length; i++) {
    if (live[i].id !== published[i]) return false;
  }
  return true;
}

function publishParticleViews(
  live: Particle[],
  publishedIds: { current: number[] },
  setViews: (views: ParticleView[]) => void
) {
  if (sameParticleIds(live, publishedIds.current)) return;
  publishedIds.current = live.map((p) => p.id);
  setViews(live.map((p) => ({ id: p.id, color: p.color, size: p.size })));
}

function placeParticleMesh(mesh: THREE.Mesh, p: Particle, opacityFactor: number, shrink: boolean) {
  mesh.position.set(p.position[0], p.position[1], p.position[2]);
  const life = p.life > 0 ? p.life : 0;
  if (shrink) mesh.scale.setScalar(life);
  const mat = mesh.material;
  if (!Array.isArray(mat)) mat.opacity = life * opacityFactor;
  mesh.visible = life > 0;
}

export function LaunchBurst() {
  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const [particles, setParticles] = useState<ParticleView[]>([]);
  const liveRef = useRef<Particle[]>([]);
  const publishedIds = useRef<number[]>([]);
  const meshRefs = useRef<Map<number, THREE.Mesh>>(new Map());
  const prevSpeedRef = useRef(0);
  const particleIdRef = useRef(0);
  
  useFrame(() => {
    if (!playerEntity) return;
    
    const speed = Math.sqrt(playerEntity.velocity[0] ** 2 + playerEntity.velocity[2] ** 2);
    let spawned = false;
    
    if (speed > 0.5 && prevSpeedRef.current < 0.1) {
      const color = ZOOGI_COLORS[playerEntity.zoogi.id] || playerEntity.zoogi.color;
      for (let i = 0; i < 15; i++) {
        const angle = Math.random() * Math.PI * 2;
        const upward = 0.5 + Math.random() * 1;
        const outward = 1 + Math.random() * 2;
        liveRef.current.push({
          id: particleIdRef.current++,
          position: [...playerEntity.position],
          velocity: [
            Math.cos(angle) * outward * 0.05,
            upward * 0.05,
            Math.sin(angle) * outward * 0.05
          ],
          life: 1,
          maxLife: 1,
          color,
          size: 0.1 + Math.random() * 0.1
        });
      }
      spawned = true;
    }
    prevSpeedRef.current = speed;

    if (!spawned && liveRef.current.length === 0) return;

    let died = false;
    for (const p of liveRef.current) {
      p.position[0] += p.velocity[0];
      p.position[1] += p.velocity[1];
      p.position[2] += p.velocity[2];
      p.velocity[1] -= 0.002;
      p.life -= 0.03;
      const mesh = meshRefs.current.get(p.id);
      if (mesh) placeParticleMesh(mesh, p, 0.8, true);
      if (p.life <= 0) died = true;
    }

    if (died) liveRef.current = liveRef.current.filter((p) => p.life > 0);
    if (spawned || died) publishParticleViews(liveRef.current, publishedIds, setParticles);
  });
  
  return (
    <group>
      {particles.map(p => (
        <mesh
          key={p.id}
          ref={(el) => {
            if (el) {
              meshRefs.current.set(p.id, el);
              const live = liveRef.current.find((item) => item.id === p.id);
              if (live) placeParticleMesh(el, live, 0.8, true);
            } else {
              meshRefs.current.delete(p.id);
            }
          }}
        >
          <sphereGeometry args={[p.size, 6, 6]} />
          <meshBasicMaterial color={p.color} transparent opacity={0.8} />
        </mesh>
      ))}
    </group>
  );
}

export function WindParticles() {
  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const [particles, setParticles] = useState<ParticleView[]>([]);
  const liveRef = useRef<Particle[]>([]);
  const publishedIds = useRef<number[]>([]);
  const meshRefs = useRef<Map<number, THREE.Mesh>>(new Map());
  const particleIdRef = useRef(0);
  const lastSpawnRef = useRef(0);
  
  useFrame(() => {
    if (!playerEntity) return;
    
    const speed = Math.sqrt(playerEntity.velocity[0] ** 2 + playerEntity.velocity[2] ** 2);
    const now = Date.now();
    let spawned = false;
    
    if (speed > 0.2 && now - lastSpawnRef.current > 50) {
      lastSpawnRef.current = now;
      const moveAngle = Math.atan2(playerEntity.velocity[0], playerEntity.velocity[2]);
      const offsetAngle = moveAngle + (Math.random() - 0.5) * 1;
      const distance = 1 + Math.random() * 2;
      
      liveRef.current.push({
        id: particleIdRef.current++,
        position: [
          playerEntity.position[0] + Math.sin(offsetAngle) * distance,
          playerEntity.position[1] + (Math.random() - 0.5) * 0.5,
          playerEntity.position[2] + Math.cos(offsetAngle) * distance
        ],
        velocity: [
          -playerEntity.velocity[0] * 0.3,
          (Math.random() - 0.5) * 0.01,
          -playerEntity.velocity[2] * 0.3
        ],
        life: 1,
        maxLife: 1,
        color: "#FFFFFF",
        size: 0.03 + Math.random() * 0.03
      });
      if (liveRef.current.length > 31) {
        liveRef.current = liveRef.current.slice(-31);
      }
      spawned = true;
    }

    if (!spawned && liveRef.current.length === 0) return;

    let died = false;
    for (const p of liveRef.current) {
      p.position[0] += p.velocity[0];
      p.position[1] += p.velocity[1];
      p.position[2] += p.velocity[2];
      p.life -= 0.05;
      const mesh = meshRefs.current.get(p.id);
      if (mesh) placeParticleMesh(mesh, p, 0.4, false);
      if (p.life <= 0) died = true;
    }

    if (died) liveRef.current = liveRef.current.filter((p) => p.life > 0);
    if (spawned || died) publishParticleViews(liveRef.current, publishedIds, setParticles);
  });
  
  return (
    <group>
      {particles.map(p => (
        <mesh
          key={p.id}
          ref={(el) => {
            if (el) {
              meshRefs.current.set(p.id, el);
              const live = liveRef.current.find((item) => item.id === p.id);
              if (live) placeParticleMesh(el, live, 0.4, false);
            } else {
              meshRefs.current.delete(p.id);
            }
          }}
        >
          <sphereGeometry args={[p.size, 4, 4]} />
          <meshBasicMaterial color={p.color} transparent opacity={0.4} />
        </mesh>
      ))}
    </group>
  );
}

export function ImpactSparks() {
  const [sparks, setSparks] = useState<ParticleView[]>([]);
  const playerEntity = useZoogiGame((state) => state.playerEntity);
  const enemies = useZoogiGame((state) => state.enemies);
  const liveRef = useRef<Particle[]>([]);
  const publishedIds = useRef<number[]>([]);
  const meshRefs = useRef<Map<number, THREE.Mesh>>(new Map());
  const prevVelocitiesRef = useRef<Map<string, number>>(new Map());
  const particleIdRef = useRef(0);
  const lastSparkTimeRef = useRef<Map<string, number>>(new Map());
  
  useFrame(() => {
    if (!playerEntity) return;
    const now = Date.now();
    let spawned = false;
    
    const allEntities = [
      { id: "player", entity: playerEntity },
      ...enemies.map(e => ({ id: e.id, entity: e }))
    ];
    
    allEntities.forEach(({ id, entity }) => {
      const speed = Math.sqrt(entity.velocity[0] ** 2 + entity.velocity[2] ** 2);
      const prevSpeed = prevVelocitiesRef.current.get(id) || 0;
      const lastSparkTime = lastSparkTimeRef.current.get(id) || 0;
      
      const speedDrop = prevSpeed - speed;
      if (speedDrop > 0.15 && prevSpeed > 0.2 && now - lastSparkTime > 200) {
        lastSparkTimeRef.current.set(id, now);
        const color = ZOOGI_COLORS[entity.zoogi.id] || entity.zoogi.color;
        const sparkCount = Math.min(12, Math.floor(speedDrop * 15));
        const kept = liveRef.current.slice(-50);
        liveRef.current = kept;
        for (let i = 0; i < sparkCount; i++) {
          const angle = Math.random() * Math.PI * 2;
          liveRef.current.push({
            id: particleIdRef.current++,
            position: [...entity.position],
            velocity: [
              Math.cos(angle) * 0.1,
              0.06 + Math.random() * 0.06,
              Math.sin(angle) * 0.1
            ],
            life: 1,
            maxLife: 1,
            color,
            size: 0.1
          });
        }
        spawned = true;
      }
      prevVelocitiesRef.current.set(id, speed);
    });

    if (!spawned && liveRef.current.length === 0) return;

    let died = false;
    for (const p of liveRef.current) {
      p.position[0] += p.velocity[0];
      p.position[1] += p.velocity[1];
      p.position[2] += p.velocity[2];
      p.velocity[0] *= 0.95;
      p.velocity[1] -= 0.003;
      p.velocity[2] *= 0.95;
      p.life -= 0.04;
      const mesh = meshRefs.current.get(p.id);
      if (mesh) placeParticleMesh(mesh, p, 1, true);
      if (p.life <= 0) died = true;
    }

    if (died) liveRef.current = liveRef.current.filter((p) => p.life > 0);
    if (spawned || died) publishParticleViews(liveRef.current, publishedIds, setSparks);
  });
  
  return (
    <group>
      {sparks.map(p => (
        <mesh
          key={p.id}
          ref={(el) => {
            if (el) {
              meshRefs.current.set(p.id, el);
              const live = liveRef.current.find((item) => item.id === p.id);
              if (live) placeParticleMesh(el, live, 1, true);
            } else {
              meshRefs.current.delete(p.id);
            }
          }}
        >
          <octahedronGeometry args={[p.size]} />
          <meshBasicMaterial color={p.color} transparent opacity={1} />
        </mesh>
      ))}
    </group>
  );
}

export function WolfClones() {
  const wolfClones = useZoogiGame((state) => state.wolfClones);
  
  return (
    <>
      {wolfClones.filter(c => c.isActive).map((clone) => (
        <WolfClone key={clone.id} clone={clone} />
      ))}
    </>
  );
}

function WolfClone({ clone }: { clone: { id: string; position: [number, number, number]; velocity: [number, number, number]; isActive: boolean } }) {
  const meshRef = useRef<THREE.Mesh>(null);
  
  useFrame(() => {
    if (meshRef.current) {
      meshRef.current.position.set(clone.position[0], clone.position[1], clone.position[2]);
      const speed = Math.sqrt(clone.velocity[0] ** 2 + clone.velocity[2] ** 2);
      if (speed > 0.01) {
        meshRef.current.rotation.x += speed * 0.5;
      }
    }
  });
  
  if (!clone.isActive) return null;
  
  return (
    <group>
      <mesh ref={meshRef} position={clone.position} castShadow>
        <sphereGeometry args={[0.5, 16, 16]} />
        <meshStandardMaterial
          color="#9CA3AF"
          emissive="#6B7280"
          emissiveIntensity={0.3}
          transparent
          opacity={0.8}
        />
      </mesh>
      <pointLight
        position={[clone.position[0], clone.position[1] + 0.5, clone.position[2]]}
        color="#6B7280"
        intensity={0.2}
        distance={2}
      />
    </group>
  );
}

export function ExplosionEffect() {
  const showExplosion = useZoogiGame((state) => state.showExplosion);
  const scaleRef = useRef(0);
  const opacityRef = useRef(1);
  const meshRef = useRef<THREE.Mesh>(null);
  const lightningRef = useRef<THREE.Group>(null);
  
  useFrame((_, delta) => {
    if (!showExplosion || !meshRef.current) return;
    
    const elapsed = (Date.now() - showExplosion.timestamp) / 1000;
    
    if (elapsed < 0.8) {
      scaleRef.current = Math.min(6, elapsed * 12);
      opacityRef.current = 1 - elapsed * 1.25;
      
      meshRef.current.scale.setScalar(scaleRef.current);
      (meshRef.current.material as THREE.MeshBasicMaterial).opacity = Math.max(0, opacityRef.current);
      
      if (lightningRef.current) {
        lightningRef.current.rotation.y += delta * 20;
      }
    }
  });
  
  if (!showExplosion) return null;
  
  const elapsed = (Date.now() - showExplosion.timestamp) / 1000;
  if (elapsed > 0.8) return null;
  
  const isYellow = showExplosion.color === "yellow";
  const primaryColor = isYellow ? "#FFFF00" : "#FF4500";
  const secondaryColor = isYellow ? "#00BFFF" : "#FFD700";
  
  return (
    <group position={showExplosion.position}>
      <mesh ref={meshRef}>
        <sphereGeometry args={[1, 16, 16]} />
        <meshBasicMaterial
          color={primaryColor}
          transparent
          opacity={0.8}
          side={THREE.DoubleSide}
        />
      </mesh>
      <mesh scale={[scaleRef.current * 0.8, scaleRef.current * 0.8, scaleRef.current * 0.8]}>
        <sphereGeometry args={[1, 16, 16]} />
        <meshBasicMaterial
          color={secondaryColor}
          transparent
          opacity={0.5}
        />
      </mesh>
      {isYellow && (
        <group ref={lightningRef}>
          {[0, 1, 2, 3, 4, 5, 6, 7].map((i) => {
            const angle = (i / 8) * Math.PI * 2;
            const length = 4 + Math.random() * 2;
            return (
              <mesh
                key={i}
                position={[Math.cos(angle) * length * 0.5, 0.5, Math.sin(angle) * length * 0.5]}
                rotation={[0, 0, Math.PI / 2 + angle]}
              >
                <boxGeometry args={[length, 0.15, 0.15]} />
                <meshBasicMaterial color="#FFFF00" transparent opacity={0.95} />
              </mesh>
            );
          })}
        </group>
      )}
      <pointLight
        color={primaryColor}
        intensity={isYellow ? 6 : 4}
        distance={isYellow ? 15 : 12}
      />
    </group>
  );
}


export function StunnedIndicator({ position, isStunned }: { position: [number, number, number]; isStunned: boolean }) {
  const rotationRef = useRef(0);
  const groupRef = useRef<THREE.Group>(null);
  
  useFrame((_, delta) => {
    if (groupRef.current && isStunned) {
      rotationRef.current += delta * 5;
      groupRef.current.rotation.y = rotationRef.current;
    }
  });
  
  if (!isStunned) return null;
  
  return (
    <group ref={groupRef} position={[position[0], position[1] + 1, position[2]]}>
      {[0, 1, 2].map((i) => (
        <mesh
          key={i}
          position={[
            Math.cos((i / 3) * Math.PI * 2) * 0.4,
            0,
            Math.sin((i / 3) * Math.PI * 2) * 0.4
          ]}
        >
          <octahedronGeometry args={[0.1]} />
          <meshBasicMaterial color="#FBBF24" />
        </mesh>
      ))}
    </group>
  );
}

interface CollisionBurstParticle {
  id: number;
  position: [number, number, number];
  velocity: [number, number, number];
  life: number;
  maxLife: number;
  color: string;
  size: number;
}

function CollisionBurst({ 
  position, 
  intensity, 
  color1, 
  color2, 
  timestamp 
}: { 
  position: [number, number, number]; 
  intensity: number;
  color1: string;
  color2: string;
  timestamp: number;
}) {
  const [particles, setParticles] = useState<ParticleView[]>([]);
  const liveRef = useRef<CollisionBurstParticle[]>([]);
  const publishedIds = useRef<number[]>([]);
  const meshRefs = useRef<Map<number, THREE.Mesh>>(new Map());
  const lensScaleRef = useRef(0);
  const glowScaleRef = useRef(0);
  const lensGroupRef = useRef<THREE.Group>(null);
  const lensMats = useRef<(THREE.MeshBasicMaterial | null)[]>([null, null, null]);
  const glowMeshRef = useRef<THREE.Mesh>(null);
  const glowMatRef = useRef<THREE.MeshBasicMaterial>(null);
  const particleIdRef = useRef(0);
  const initializedRef = useRef(false);
  
  useEffect(() => {
    if (initializedRef.current) return;
    initializedRef.current = true;
    
    // Reduced particle count to prevent WebGL context overload
    const particleCount = Math.floor(12 + intensity * 10);
    const newParticles: CollisionBurstParticle[] = [];
    
    for (let i = 0; i < particleCount; i++) {
      const angle = (Math.PI * 2 * i) / particleCount + Math.random() * 0.5;
      const upAngle = Math.random() * Math.PI - Math.PI / 2;
      const speed = (2 + Math.random() * 4) * intensity;
      const useColor1 = Math.random() > 0.4;
      
      newParticles.push({
        id: particleIdRef.current++,
        position: [...position],
        velocity: [
          Math.cos(angle) * Math.cos(upAngle) * speed * 0.08,
          Math.sin(upAngle) * speed * 0.06 + 0.03,
          Math.sin(angle) * Math.cos(upAngle) * speed * 0.08
        ],
        life: 1,
        maxLife: 0.5 + Math.random() * 0.3,
        color: useColor1 ? color1 : color2,
        size: (0.06 + Math.random() * 0.08) * intensity
      });
    }
    
    // Reduced spark particles
    for (let i = 0; i < 5; i++) {
      const angle = Math.random() * Math.PI * 2;
      const speed = 1 + Math.random() * 2;
      newParticles.push({
        id: particleIdRef.current++,
        position: [...position],
        velocity: [
          Math.cos(angle) * speed * 0.05,
          0.08 + Math.random() * 0.05,
          Math.sin(angle) * speed * 0.05
        ],
        life: 1,
        maxLife: 0.6 + Math.random() * 0.3,
        color: "#ffffff",
        size: 0.1 + Math.random() * 0.1
      });
    }
    
    liveRef.current = newParticles;
    lensScaleRef.current = intensity * 1.5;
    glowScaleRef.current = intensity * 2;
    publishParticleViews(newParticles, publishedIds, setParticles);
  }, [position, intensity, color1, color2]);
  
  useFrame(() => {
    if (!initializedRef.current) return;

    let died = false;
    for (const p of liveRef.current) {
      p.position[0] += p.velocity[0];
      p.position[1] += p.velocity[1];
      p.position[2] += p.velocity[2];
      p.velocity[0] *= 0.92;
      p.velocity[1] -= 0.004;
      p.velocity[2] *= 0.92;
      p.life -= 0.06;
      const mesh = meshRefs.current.get(p.id);
      if (mesh) placeParticleMesh(mesh, p, 0.9, true);
      if (p.life <= 0) died = true;
    }
    if (died) {
      liveRef.current = liveRef.current.filter((p) => p.life > 0);
      publishParticleViews(liveRef.current, publishedIds, setParticles);
    }

    lensScaleRef.current = Math.max(0, lensScaleRef.current - 0.2);
    glowScaleRef.current = Math.max(0, glowScaleRef.current - 0.18);
    const lens = lensScaleRef.current;
    const glow = glowScaleRef.current;
    if (lensGroupRef.current) {
      lensGroupRef.current.visible = lens > 0.1;
      lensGroupRef.current.scale.setScalar(lens);
    }
    const lensOpacity = [0.3, 0.15, 0.4];
    lensMats.current.forEach((mat, i) => {
      if (mat) mat.opacity = lens * lensOpacity[i];
    });
    if (glowMeshRef.current) {
      glowMeshRef.current.visible = glow > 0.1;
      glowMeshRef.current.scale.setScalar(glow);
    }
    if (glowMatRef.current) glowMatRef.current.opacity = glow * 0.25;
  });
  
  const initialLens = intensity * 1.5;
  const initialGlow = intensity * 2;

  return (
    <group>
      <group ref={lensGroupRef} position={position} scale={initialLens} visible={initialLens > 0.1}>
        <mesh>
          <ringGeometry args={[0.3, 0.5, 32]} />
          <meshBasicMaterial
            ref={(mat) => { lensMats.current[0] = mat; }}
            color={color1}
            transparent
            opacity={initialLens * 0.3}
            side={THREE.DoubleSide}
          />
        </mesh>
        <mesh>
          <ringGeometry args={[0.6, 0.8, 32]} />
          <meshBasicMaterial
            ref={(mat) => { lensMats.current[1] = mat; }}
            color={color2}
            transparent
            opacity={initialLens * 0.15}
            side={THREE.DoubleSide}
          />
        </mesh>
        <mesh>
          <circleGeometry args={[0.4, 32]} />
          <meshBasicMaterial
            ref={(mat) => { lensMats.current[2] = mat; }}
            color="#ffffff"
            transparent
            opacity={initialLens * 0.4}
            side={THREE.DoubleSide}
          />
        </mesh>
      </group>

      <mesh ref={glowMeshRef} position={position} scale={initialGlow} visible={initialGlow > 0.1}>
        <sphereGeometry args={[0.3, 16, 16]} />
        <meshBasicMaterial
          ref={glowMatRef}
          color={color1}
          transparent
          opacity={initialGlow * 0.25}
        />
      </mesh>

      {particles.map(p => (
        <mesh
          key={p.id}
          ref={(el) => {
            if (el) {
              meshRefs.current.set(p.id, el);
              const live = liveRef.current.find((item) => item.id === p.id);
              if (live) placeParticleMesh(el, live, 0.9, true);
            } else {
              meshRefs.current.delete(p.id);
            }
          }}
        >
          <sphereGeometry args={[p.size, 6, 6]} />
          <meshBasicMaterial color={p.color} transparent opacity={0.9} />
        </mesh>
      ))}
    </group>
  );
}

export function CollisionBurstEffects() {
  const collisionBursts = useGameFeel((state) => state.collisionBursts);
  
  return (
    <>
      {collisionBursts.map((burst) => (
        <CollisionBurst
          key={burst.id}
          position={burst.position}
          intensity={burst.intensity}
          color1={burst.color1}
          color2={burst.color2}
          timestamp={burst.timestamp}
        />
      ))}
    </>
  );
}

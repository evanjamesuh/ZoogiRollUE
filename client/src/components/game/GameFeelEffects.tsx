import { useEffect, useRef, useMemo } from "react";
import { useFrame } from "@react-three/fiber";
import { Html } from "@react-three/drei";
import * as THREE from "three";
import { useGameFeel } from "@/lib/stores/useGameFeel";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { emitImpact } from "@/vfx/impacts";
import { ZOOGI_FX_SCALE } from "@/lib/restHeight";

export function HitEffects() {
  const hitEffects = useGameFeel((state) => state.hitEffects);
  
  return (
    <>
      {hitEffects.map((effect) => (
        <HitParticle key={effect.id} effect={effect} />
      ))}
    </>
  );
}

interface HitParticleProps {
  effect: {
    id: string;
    position: [number, number, number];
    type: "collision" | "knockoff" | "ability" | "collect";
    timestamp: number;
  };
}

function HitParticle({ effect }: HitParticleProps) {
  const groupRef = useRef<THREE.Group>(null);
  const particlesRef = useRef<THREE.Points>(null);
  
  const particleCount = effect.type === "knockoff" ? 30 : effect.type === "ability" ? 20 : 10;
  
  const { positions, velocities, colors } = useMemo(() => {
    const pos = new Float32Array(particleCount * 3);
    const vel: THREE.Vector3[] = [];
    const cols = new Float32Array(particleCount * 3);
    
    const colorMap = {
      collision: new THREE.Color("#ffaa00"),
      knockoff: new THREE.Color("#ff4444"),
      ability: new THREE.Color("#44aaff"),
      collect: new THREE.Color("#44ff44")
    };
    
    const baseColor = colorMap[effect.type];
    
    for (let i = 0; i < particleCount; i++) {
      pos[i * 3] = 0;
      pos[i * 3 + 1] = 0;
      pos[i * 3 + 2] = 0;
      
      const angle = Math.random() * Math.PI * 2;
      const elevation = (Math.random() - 0.3) * Math.PI;
      const speed = 0.1 + Math.random() * 0.2;
      
      vel.push(new THREE.Vector3(
        Math.cos(angle) * Math.cos(elevation) * speed,
        Math.sin(elevation) * speed + 0.1,
        Math.sin(angle) * Math.cos(elevation) * speed
      ));
      
      const variation = 0.2;
      cols[i * 3] = baseColor.r + (Math.random() - 0.5) * variation;
      cols[i * 3 + 1] = baseColor.g + (Math.random() - 0.5) * variation;
      cols[i * 3 + 2] = baseColor.b + (Math.random() - 0.5) * variation;
    }
    
    return { positions: pos, velocities: vel, colors: cols };
  }, [effect.type, particleCount]);
  
  useFrame(() => {
    if (!particlesRef.current) return;
    
    const elapsed = (Date.now() - effect.timestamp) / 1000;
    const progress = Math.min(elapsed / 0.8, 1);
    
    const positionAttr = particlesRef.current.geometry.getAttribute("position");
    
    for (let i = 0; i < particleCount; i++) {
      const vel = velocities[i];
      positionAttr.setXYZ(
        i,
        vel.x * elapsed * 5,
        vel.y * elapsed * 5 - 0.5 * elapsed * elapsed * 2,
        vel.z * elapsed * 5
      );
    }
    
    positionAttr.needsUpdate = true;
    
    if (particlesRef.current.material instanceof THREE.PointsMaterial) {
      particlesRef.current.material.opacity = 1 - progress;
      particlesRef.current.material.size = ZOOGI_FX_SCALE * 0.15 * (1 - progress * 0.5);
    }
  });
  
  return (
    <group ref={groupRef} position={effect.position} scale={ZOOGI_FX_SCALE}>
      <points ref={particlesRef}>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={particleCount}
            array={positions}
            itemSize={3}
          />
          <bufferAttribute
            attach="attributes-color"
            count={particleCount}
            array={colors}
            itemSize={3}
          />
        </bufferGeometry>
        <pointsMaterial
          size={0.15 * ZOOGI_FX_SCALE}
          vertexColors
          transparent
          opacity={1}
          sizeAttenuation
          depthWrite={false}
        />
      </points>
      
      {effect.type === "knockoff" && (
        <RingExplosion timestamp={effect.timestamp} />
      )}
    </group>
  );
}

/** Pop ring for a knockout. The mesh scale is the authored 0.86-era size; the parent group applies ZOOGI_FX_SCALE. */
function RingExplosion({ timestamp }: { timestamp: number }) {
  const ringRef = useRef<THREE.Mesh>(null);
  
  useFrame(() => {
    if (!ringRef.current) return;
    
    const elapsed = (Date.now() - timestamp) / 1000;
    const progress = Math.min(elapsed / 0.5, 1);
    
    const scale = 0.5 + progress * 3;
    ringRef.current.scale.set(scale, scale, 1);
    
    if (ringRef.current.material instanceof THREE.MeshBasicMaterial) {
      ringRef.current.material.opacity = 1 - progress;
    }
  });
  
  return (
    <mesh ref={ringRef} rotation={[-Math.PI / 2, 0, 0]}>
      <ringGeometry args={[0.8, 1, 32]} />
      <meshBasicMaterial color="#ff4444" transparent opacity={1} side={THREE.DoubleSide} />
    </mesh>
  );
}

function ScreenFlashMesh({ flashScreen }: { flashScreen: { color: string; startTime: number; duration: number } }) {
  const meshRef = useRef<THREE.Mesh>(null);
  
  useFrame(() => {
    if (!meshRef.current) return;
    
    const elapsed = Date.now() - flashScreen.startTime;
    const progress = Math.min(elapsed / flashScreen.duration, 1);
    const opacity = (1 - progress) * 0.3;
    
    if (meshRef.current.material instanceof THREE.MeshBasicMaterial) {
      meshRef.current.material.opacity = opacity;
    }
  });
  
  return (
    <mesh ref={meshRef} position={[0, 50, 0]} rotation={[-Math.PI / 2, 0, 0]}>
      <planeGeometry args={[200, 200]} />
      <meshBasicMaterial 
        color={flashScreen.color} 
        transparent 
        opacity={0.3}
        depthTest={false}
        depthWrite={false}
      />
    </mesh>
  );
}

export function ScreenFlash() {
  const flashScreen = useGameFeel((state) => state.flashScreen);
  
  if (!flashScreen) return null;
  
  return <ScreenFlashMesh flashScreen={flashScreen} />;
}

export function useScreenShake() {
  const shakeOffset = useRef(new THREE.Vector3());
  const screenShake = useGameFeel((state) => state.screenShake);
  const update = useGameFeel((state) => state.update);
  
  useFrame(() => {
    update();
    
    if (screenShake) {
      const elapsed = Date.now() - screenShake.startTime;
      const progress = elapsed / screenShake.duration;
      const decay = 1 - Math.min(progress, 1);
      
      const intensity = screenShake.intensity * decay;
      
      shakeOffset.current.set(
        (Math.random() - 0.5) * intensity * 2,
        (Math.random() - 0.5) * intensity * 1,
        (Math.random() - 0.5) * intensity * 2
      );
    } else {
      shakeOffset.current.set(0, 0, 0);
    }
  });
  
  return shakeOffset;
}

export function FireBursts() {
  const fireBursts = useGameFeel((state) => state.fireBursts);
  
  return (
    <>
      {fireBursts.map((burst) => (
        <FireBurstEffect key={burst.id} burst={burst} />
      ))}
    </>
  );
}

function FireBurstEffect({ burst }: { burst: { id: string; position: [number, number, number]; timestamp: number } }) {
  const groupRef = useRef<THREE.Group>(null);
  const particlesRef = useRef<THREE.Points>(null);
  const glowRingRef = useRef<THREE.Mesh>(null);
  const coreRef = useRef<THREE.Mesh>(null);
  
  const particleCount = 60;
  
  const { positions, velocities, sizes } = useMemo(() => {
    const pos = new Float32Array(particleCount * 3);
    const vel: THREE.Vector3[] = [];
    const siz = new Float32Array(particleCount);
    
    for (let i = 0; i < particleCount; i++) {
      pos[i * 3] = 0;
      pos[i * 3 + 1] = 0;
      pos[i * 3 + 2] = 0;
      
      const angle = Math.random() * Math.PI * 2;
      const elevation = (Math.random() - 0.2) * Math.PI * 0.5;
      const speed = 0.15 + Math.random() * 0.25;
      
      vel.push(new THREE.Vector3(
        Math.cos(angle) * Math.cos(elevation) * speed,
        Math.abs(Math.sin(elevation)) * speed * 1.5 + 0.1,
        Math.sin(angle) * Math.cos(elevation) * speed
      ));
      
      siz[i] = 0.1 + Math.random() * 0.15;
    }
    
    return { positions: pos, velocities: vel, sizes: siz };
  }, []);
  
  useFrame(() => {
    if (!particlesRef.current || !glowRingRef.current || !coreRef.current) return;
    
    const elapsed = (Date.now() - burst.timestamp) / 1000;
    const progress = Math.min(elapsed / 1.2, 1);
    
    const positionAttr = particlesRef.current.geometry.getAttribute("position");
    
    for (let i = 0; i < particleCount; i++) {
      const vel = velocities[i];
      const t = elapsed * 4;
      positionAttr.setXYZ(
        i,
        vel.x * t,
        vel.y * t - 0.5 * t * t * 0.3,
        vel.z * t
      );
    }
    
    positionAttr.needsUpdate = true;
    
    if (particlesRef.current.material instanceof THREE.PointsMaterial) {
      particlesRef.current.material.opacity = Math.max(0, 1 - progress * 1.2);
    }
    
    const ringScale = 0.5 + progress * 4;
    glowRingRef.current.scale.set(ringScale, ringScale, 1);
    if (glowRingRef.current.material instanceof THREE.MeshBasicMaterial) {
      glowRingRef.current.material.opacity = Math.max(0, (1 - progress) * 0.8);
    }
    
    const coreScale = Math.max(0, 1.5 * (1 - progress * 2));
    coreRef.current.scale.set(coreScale, coreScale, coreScale);
    if (coreRef.current.material instanceof THREE.MeshBasicMaterial) {
      coreRef.current.material.opacity = Math.max(0, 1 - progress * 2);
    }
  });
  
  return (
    <group ref={groupRef} position={burst.position} scale={ZOOGI_FX_SCALE}>
      <mesh ref={coreRef}>
        <sphereGeometry args={[0.5, 16, 16]} />
        <meshBasicMaterial color="#ffffff" transparent opacity={1} />
      </mesh>
      
      <mesh ref={glowRingRef} rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.3, 0.8, 32]} />
        <meshBasicMaterial color="#ff6600" transparent opacity={0.8} side={THREE.DoubleSide} />
      </mesh>
      
      <mesh rotation={[-Math.PI / 2, 0, 0]}>
        <ringGeometry args={[0.1, 0.6, 32]} />
        <meshBasicMaterial color="#ffaa00" transparent opacity={0.6} side={THREE.DoubleSide} />
      </mesh>
      
      <points ref={particlesRef}>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={particleCount}
            array={positions}
            itemSize={3}
          />
        </bufferGeometry>
        <pointsMaterial
          size={0.2 * ZOOGI_FX_SCALE}
          color="#ff4400"
          transparent
          opacity={1}
          sizeAttenuation
          depthWrite={false}
        />
      </points>
      
      <points>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={particleCount}
            array={positions}
            itemSize={3}
          />
        </bufferGeometry>
        <pointsMaterial
          size={0.35 * ZOOGI_FX_SCALE}
          color="#ffaa00"
          transparent
          opacity={0.5}
          sizeAttenuation
          depthWrite={false}
        />
      </points>
    </group>
  );
}

export function WallSparks() {
  const wallSparks = useGameFeel((state) => state.wallSparks);
  
  return (
    <>
      {wallSparks.map((spark) => (
        <WallSparkEffect key={spark.id} spark={spark} />
      ))}
    </>
  );
}

function WallSparkEffect({ spark }: { spark: { id: string; position: [number, number, number]; normal: [number, number, number]; intensity: number; timestamp: number } }) {
  const particlesRef = useRef<THREE.Points>(null);
  const flashRef = useRef<THREE.Mesh>(null);
  
  const particleCount = 25;
  
  const { positions, velocities } = useMemo(() => {
    const pos = new Float32Array(particleCount * 3);
    const vel: THREE.Vector3[] = [];
    
    const normalVec = new THREE.Vector3(spark.normal[0], spark.normal[1], spark.normal[2]).normalize();
    
    for (let i = 0; i < particleCount; i++) {
      pos[i * 3] = 0;
      pos[i * 3 + 1] = 0;
      pos[i * 3 + 2] = 0;
      
      const spread = 0.8;
      const perpX = normalVec.z;
      const perpZ = -normalVec.x;
      
      const speed = (0.1 + Math.random() * 0.2) * spark.intensity;
      const lateralSpeed = (Math.random() - 0.5) * spread;
      const upSpeed = Math.random() * 0.15;
      
      vel.push(new THREE.Vector3(
        normalVec.x * speed + perpX * lateralSpeed,
        upSpeed,
        normalVec.z * speed + perpZ * lateralSpeed
      ));
    }
    
    return { positions: pos, velocities: vel };
  }, [spark.normal, spark.intensity]);
  
  useFrame(() => {
    if (!particlesRef.current || !flashRef.current) return;
    
    const elapsed = (Date.now() - spark.timestamp) / 1000;
    const progress = Math.min(elapsed / 0.6, 1);
    
    const positionAttr = particlesRef.current.geometry.getAttribute("position");
    
    for (let i = 0; i < particleCount; i++) {
      const vel = velocities[i];
      const t = elapsed * 6;
      positionAttr.setXYZ(
        i,
        vel.x * t,
        vel.y * t - 0.5 * t * t * 0.8,
        vel.z * t
      );
    }
    
    positionAttr.needsUpdate = true;
    
    if (particlesRef.current.material instanceof THREE.PointsMaterial) {
      particlesRef.current.material.opacity = Math.max(0, 1 - progress);
    }
    
    const flashScale = Math.max(0, 0.8 * (1 - progress * 3));
    flashRef.current.scale.set(flashScale, flashScale, flashScale);
    if (flashRef.current.material instanceof THREE.MeshBasicMaterial) {
      flashRef.current.material.opacity = Math.max(0, 1 - progress * 3);
    }
  });
  
  return (
    <group position={spark.position}>
      <mesh ref={flashRef}>
        <sphereGeometry args={[0.3, 8, 8]} />
        <meshBasicMaterial color="#ffffff" transparent opacity={1} />
      </mesh>
      
      <points ref={particlesRef}>
        <bufferGeometry>
          <bufferAttribute
            attach="attributes-position"
            count={particleCount}
            array={positions}
            itemSize={3}
          />
        </bufferGeometry>
        <pointsMaterial
          size={0.08}
          color="#ffff88"
          transparent
          opacity={1}
          sizeAttenuation
          depthWrite={false}
        />
      </points>
    </group>
  );
}

export function CartoonExplosions() {
  const cartoonExplosions = useGameFeel((state) => state.cartoonExplosions);
  
  return (
    <>
      {cartoonExplosions.map((explosion) => (
        <CartoonExplosionEffect key={explosion.id} explosion={explosion} />
      ))}
    </>
  );
}

function CartoonExplosionEffect(_explosion: { explosion: { id: string; position: [number, number, number]; timestamp: number; showBoom: boolean } }) {
  // The flat scorch disc used to sit under Hotstreak's blast. The volumetric
  // explosion draws the scorch, smoke, and flash now.
  return null;
}

export function CartoonStarbursts() {
  const starbursts = useGameFeel((state) => state.cartoonStarbursts);
  
  return (
    <>
      {starbursts.map((starburst) => (
        <CartoonStarburstEffect key={starburst.id} starburst={starburst} />
      ))}
    </>
  );
}

const replacedStarbursts = new Set<string>();

function sprayDirection(position: [number, number, number]): [number, number, number] {
  const state = useZoogiGame.getState();
  const entities = [state.playerEntity, ...state.enemies];
  let velocity: [number, number, number] | null = null;
  let best = 4;
  for (const entity of entities) {
    if (!entity) continue;
    const dx = entity.position[0] - position[0];
    const dz = entity.position[2] - position[2];
    const dist = Math.hypot(dx, dz);
    if (dist < best) {
      best = dist;
      velocity = entity.velocity;
    }
  }
  if (!velocity) return [0.25, 0.45, 0.55];
  const len = Math.hypot(velocity[0], velocity[1], velocity[2]);
  if (len < 0.05) return [0.35, 0.4, 0.25];
  return [velocity[0] / len, Math.max(0.2, velocity[1] / len), velocity[2] / len];
}

function larsIsArmedNear(position: [number, number, number]): boolean {
  const state = useZoogiGame.getState();
  const entities = [state.playerEntity, ...state.enemies];
  return entities.some((entity) => {
    if (!entity || entity.larsRicochetBoost <= 1) return false;
    const dx = entity.position[0] - position[0];
    const dz = entity.position[2] - position[2];
    return dx * dx + dz * dz < 1.44;
  });
}

function CartoonStarburstEffect({ starburst }: { starburst: { id: string; position: [number, number, number]; color: string; timestamp: number } }) {
  const color = starburst.color.toLowerCase();
  const moody = color === "#fde047" || color === "#3b82f6" || color === "#d4c4b0" || color === "#b69cff";
  useEffect(() => {
    if (!moody || replacedStarbursts.has(starburst.id)) return;
    replacedStarbursts.add(starburst.id);
    if (color === "#b69cff") return;
    if (color === "#3b82f6" && !larsIsArmedNear(starburst.position)) {
      emitImpact("spark", starburst.position, sprayDirection(starburst.position));
    } else if (color === "#d4c4b0") {
      emitImpact("dust", starburst.position);
    }
  }, [color, moody, starburst.id, starburst.position]);
  if (moody) return null;
  return <LegacyStarburst starburst={starburst} />;
}

function LegacyStarburst({ starburst }: { starburst: { id: string; position: [number, number, number]; color: string; timestamp: number } }) {
  const groupRef = useRef<THREE.Group>(null);
  const spikesRef = useRef<THREE.Group>(null);
  const coreRef = useRef<THREE.Mesh>(null);
  
  const spikeCount = 12;
  
  const spikeData = useMemo(() => {
    const data = [];
    for (let i = 0; i < spikeCount; i++) {
      const angle = (i / spikeCount) * Math.PI * 2;
      const length = i % 2 === 0 ? 2.0 : 1.2;
      data.push({ angle, length });
    }
    return data;
  }, []);
  
  useFrame(() => {
    if (!groupRef.current || !spikesRef.current || !coreRef.current) return;
    
    const elapsed = (Date.now() - starburst.timestamp) / 1000;
    const progress = Math.min(elapsed / 0.4, 1);
    
    const expandScale = Math.min(elapsed * 8, 1);
    const fadeOut = Math.max(0, 1 - progress * 1.5);
    
    spikesRef.current.children.forEach((spikeGroup, i) => {
      const data = spikeData[i];
      const spikeScale = expandScale * (1 - progress * 0.5);
      spikeGroup.scale.set(0.15, data.length * spikeScale, 0.15);
      spikeGroup.position.y = data.length * spikeScale * 0.5;
      
      spikeGroup.traverse((child) => {
        if ((child as THREE.Mesh).isMesh && (child as THREE.Mesh).material instanceof THREE.MeshBasicMaterial) {
          ((child as THREE.Mesh).material as THREE.MeshBasicMaterial).opacity = fadeOut;
        }
      });
    });
    
    const coreScale = expandScale * (1 - progress);
    coreRef.current.scale.setScalar(Math.max(0.01, coreScale * 1.5));
    if (coreRef.current.material instanceof THREE.MeshBasicMaterial) {
      coreRef.current.material.opacity = fadeOut;
    }
  });
  
  return (
    <group ref={groupRef} position={starburst.position}>
      <mesh ref={coreRef}>
        <sphereGeometry args={[0.5, 16, 16]} />
        <meshBasicMaterial color="#ffffff" transparent opacity={1} />
      </mesh>
      
      <group ref={spikesRef}>
        {spikeData.map((data, i) => (
          <group key={i} rotation={[0, 0, data.angle]}>
            <mesh rotation={[0, 0, Math.PI / 2]}>
              <coneGeometry args={[0.15, 2, 8]} />
              <meshBasicMaterial color={starburst.color} transparent opacity={1} />
            </mesh>
          </group>
        ))}
      </group>
    </group>
  );
}

export function CartoonSparks() {
  const cartoonSparks = useGameFeel((state) => state.cartoonSparks);
  
  return (
    <>
      {cartoonSparks.map((spark) => (
        <CartoonSparkEffect key={spark.id} spark={spark} />
      ))}
    </>
  );
}

function CartoonSparkEffect({ spark }: { spark: { id: string; position: [number, number, number]; direction: [number, number, number]; color: string; timestamp: number } }) {
  const groupRef = useRef<THREE.Group>(null);
  const sparksRef = useRef<THREE.Group>(null);
  
  const sparkCount = 8;
  
  const sparkData = useMemo(() => {
    const data = [];
    const baseDir = new THREE.Vector3(spark.direction[0], spark.direction[1], spark.direction[2]).normalize();
    
    for (let i = 0; i < sparkCount; i++) {
      const spreadAngle = (Math.random() - 0.5) * Math.PI * 0.6;
      const spreadY = (Math.random() - 0.3) * 0.5;
      
      const velocity = new THREE.Vector3(
        baseDir.x + Math.sin(spreadAngle) * 0.5,
        spreadY + 0.2,
        baseDir.z + Math.cos(spreadAngle) * 0.5
      ).normalize().multiplyScalar(0.3 + Math.random() * 0.3);
      
      data.push({
        velocity,
        length: 0.3 + Math.random() * 0.4,
        delay: Math.random() * 0.1
      });
    }
    return data;
  }, [spark.direction]);
  
  useFrame(() => {
    if (!groupRef.current || !sparksRef.current) return;
    
    const elapsed = (Date.now() - spark.timestamp) / 1000;
    const progress = Math.min(elapsed / 0.6, 1);
    
    sparksRef.current.children.forEach((sparkMesh, i) => {
      const data = sparkData[i];
      const t = Math.max(0, elapsed - data.delay) * 5;
      
      sparkMesh.position.x = data.velocity.x * t;
      sparkMesh.position.y = data.velocity.y * t - 0.5 * t * t * 0.5;
      sparkMesh.position.z = data.velocity.z * t;
      
      sparkMesh.lookAt(
        sparkMesh.position.x + data.velocity.x,
        sparkMesh.position.y + data.velocity.y,
        sparkMesh.position.z + data.velocity.z
      );
      
      const fadeProgress = Math.max(0, (progress - 0.3) / 0.7);
      const scale = data.length * (1 - fadeProgress);
      sparkMesh.scale.set(0.08, Math.max(0.01, scale), 0.08);
      
      if ((sparkMesh as THREE.Mesh).material instanceof THREE.MeshBasicMaterial) {
        ((sparkMesh as THREE.Mesh).material as THREE.MeshBasicMaterial).opacity = 1 - fadeProgress;
      }
    });
  });
  
  return (
    <group ref={groupRef} position={spark.position}>
      <group ref={sparksRef}>
        {sparkData.map((data, i) => (
          <mesh key={i}>
            <coneGeometry args={[0.08, 0.4, 6]} />
            <meshBasicMaterial color={spark.color} transparent opacity={1} />
          </mesh>
        ))}
      </group>
      
      <mesh>
        <sphereGeometry args={[0.2, 8, 8]} />
        <meshBasicMaterial color="#ffffff" transparent opacity={0.8} />
      </mesh>
    </group>
  );
}

export function RespawnArrows() {
  const gameMode = useZoogiGame(state => state.gameMode);
  const RESPAWN_PAD_DISTANCE = 22;
  const RESPAWN_PAD_ANGLES = [0, Math.PI / 2, Math.PI, Math.PI * 1.5];
  
  const pads = useMemo(() => RESPAWN_PAD_ANGLES.map((angle, i) => ({
    id: i,
    position: [
      Math.cos(angle) * RESPAWN_PAD_DISTANCE,
      0.1,
      Math.sin(angle) * RESPAWN_PAD_DISTANCE
    ] as [number, number, number],
    rotation: angle + Math.PI
  })), []);
  
  if (gameMode === "map_editor") {
    return null;
  }
  
  return (
    <group>
      {pads.map((pad) => (
        <RespawnArrowIndicator key={pad.id} position={pad.position} rotation={pad.rotation} />
      ))}
    </group>
  );
}

function RespawnArrowIndicator({ position, rotation }: { position: [number, number, number]; rotation: number }) {
  const groupRef = useRef<THREE.Group>(null);
  const pulseRef = useRef(0);
  
  useFrame((_, delta) => {
    if (!groupRef.current) return;
    pulseRef.current += delta * 3;
    const pulse = 0.8 + Math.sin(pulseRef.current) * 0.2;
    groupRef.current.scale.setScalar(pulse);
  });
  
  return (
    <group position={position} rotation={[0, rotation, 0]}>
      <group ref={groupRef}>
        <mesh position={[0, 0.5, 0]} rotation={[-Math.PI / 2, 0, 0]}>
          <coneGeometry args={[0.8, 2, 8]} />
          <meshBasicMaterial color="#00ff88" transparent opacity={0.7} />
        </mesh>
        
        <mesh position={[0, 0.02, 0]} rotation={[-Math.PI / 2, 0, 0]}>
          <ringGeometry args={[1.2, 1.8, 16]} />
          <meshBasicMaterial color="#00ff88" transparent opacity={0.5} side={2} />
        </mesh>
        
        <mesh position={[0, 0.03, 1.5]} rotation={[-Math.PI / 2, 0, 0]}>
          <planeGeometry args={[0.3, 1.5]} />
          <meshBasicMaterial color="#00ff88" transparent opacity={0.6} side={2} />
        </mesh>
        <mesh position={[0, 0.03, -1.5]} rotation={[-Math.PI / 2, 0, 0]}>
          <planeGeometry args={[0.3, 1.5]} />
          <meshBasicMaterial color="#00ff88" transparent opacity={0.6} side={2} />
        </mesh>
      </group>
    </group>
  );
}

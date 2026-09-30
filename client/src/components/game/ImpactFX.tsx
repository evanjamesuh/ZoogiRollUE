import { useMemo, useRef, type MutableRefObject } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { addTrauma } from "@/lib/cameraRig";
import { ZOOGI_FX_SCALE } from "@/lib/restHeight";

const SPARK_COUNT = 56;
const SPARK_COLORS = ["#fff4c8", "#7ef6ff", "#ff4ad8", "#d08bff"];

interface Spark {
  life: number;
  max: number;
  x: number;
  y: number;
  z: number;
  vx: number;
  vy: number;
  vz: number;
  color: THREE.Color;
}

interface Burst {
  x: number;
  y: number;
  z: number;
  age: number;
}

function freshSpark(): Spark {
  return {
    life: 0,
    max: 0.32,
    x: 0,
    y: 0,
    z: 0,
    vx: 0,
    vy: 0,
    vz: 0,
    color: new THREE.Color("#fff4c8"),
  };
}

/**
 * Sparks on hard hits, a short freeze on the big ones, and a burst when a
 * Zoogi crosses the knockout line. Small bumps only throw a few sparks.
 */
export function ImpactFX() {
  const sparks = useRef<Spark[]>(Array.from({ length: SPARK_COUNT }, freshSpark));
  const bursts = useRef<Burst[]>([]);
  const meshRef = useRef<THREE.InstancedMesh>(null);
  const dummy = useRef(new THREE.Object3D());
  const color = useRef(new THREE.Color());
  const prev = useRef(new Map<string, { speed: number; vx: number; vz: number; ko: boolean }>());
  const cursor = useRef(0);
  const spawn = (x: number, y: number, z: number, nx: number, nz: number, count: number) => {
    for (let i = 0; i < count; i++) {
      const spark = sparks.current[cursor.current % SPARK_COUNT];
      cursor.current += 1;
      const side = (Math.random() - 0.5) * 2;
      const along = 0.4 + Math.random() * 1.6;
      spark.life = 0.16 + Math.random() * 0.22;
      spark.max = spark.life;
      spark.x = x;
      spark.y = y + 0.35;
      spark.z = z;
      spark.vx = nx * along + nz * side * 1.4;
      spark.vy = 1.2 + Math.random() * 2.4;
      spark.vz = nz * along - nx * side * 1.4;
      spark.color.set(SPARK_COLORS[i % SPARK_COLORS.length]);
    }
  };

  useFrame((_, delta) => {
    const dt = Math.min(delta, 0.05);
    const state = useZoogiGame.getState();
    if (state.phase !== "playing") return;

    const bodies: Array<{ id: string; position: [number, number, number]; velocity: [number, number, number]; ko: boolean }> = [];
    if (state.playerEntity) {
      bodies.push({
        id: state.playerEntity.id,
        position: state.playerEntity.position,
        velocity: state.playerEntity.velocity,
        ko: state.playerEntity.isKnockedOut,
      });
    }
    for (const enemy of state.enemies) {
      bodies.push({
        id: enemy.id,
        position: enemy.position,
        velocity: enemy.velocity,
        ko: enemy.isKnockedOut,
      });
    }

    for (const body of bodies) {
      const speed = Math.hypot(body.velocity[0], body.velocity[2]);
      const previous = prev.current.get(body.id);
      if (previous) {
        const dvx = body.velocity[0] - previous.vx;
        const dvz = body.velocity[2] - previous.vz;
        const impulse = Math.hypot(dvx, dvz);
        const launched = previous.speed < 0.05 && speed > previous.speed;
        if (!launched && impulse > 0.45) {
          const inv = impulse || 1;
          spawn(body.position[0], body.position[1], body.position[2], dvx / inv, dvz / inv, impulse > 1.6 ? 10 : 5);
          if (impulse > 1.45) {
            addTrauma(Math.min(0.55, impulse * 0.12));
          }
        }
        if (!previous.ko && body.ko) {
          bursts.current.push({
            x: body.position[0],
            y: body.position[1],
            z: body.position[2],
            age: 0,
          });
          spawn(body.position[0], body.position[1], body.position[2], 0, 1, 16);
          addTrauma(0.85);
        }
      }
      prev.current.set(body.id, { speed, vx: body.velocity[0], vz: body.velocity[2], ko: body.ko });
    }

    const mesh = meshRef.current;
    if (!mesh) return;
    let alive = 0;
    for (let i = 0; i < SPARK_COUNT; i++) {
      const spark = sparks.current[i];
      dummy.current.position.set(0, -20, 0);
      dummy.current.scale.setScalar(0.001);
      if (spark.life > 0) {
        spark.life -= dt;
        spark.x += spark.vx * dt;
        spark.y += spark.vy * dt;
        spark.z += spark.vz * dt;
        spark.vy -= 6 * dt;
        const left = Math.max(0, spark.life / spark.max);
        dummy.current.position.set(spark.x, spark.y, spark.z);
        dummy.current.scale.setScalar(0.08 + left * 0.14);
        color.current.copy(spark.color);
        mesh.setColorAt(i, color.current);
        alive += 1;
      }
      dummy.current.updateMatrix();
      mesh.setMatrixAt(i, dummy.current.matrix);
    }
    mesh.instanceMatrix.needsUpdate = true;
    if (mesh.instanceColor) mesh.instanceColor.needsUpdate = true;
    if (alive === 0 && mesh.instanceColor) mesh.instanceColor.needsUpdate = true;

    bursts.current = bursts.current.filter((burst) => {
      burst.age += dt;
      return burst.age < 0.55;
    });
  }, -1);

  return (
    <group>
      <instancedMesh ref={meshRef} args={[undefined, undefined, SPARK_COUNT]} frustumCulled={false}>
        <sphereGeometry args={[1, 6, 6]} />
        <meshBasicMaterial color="#ffffff" toneMapped={false} transparent opacity={0.95} />
      </instancedMesh>
      <KoBursts burstsRef={bursts} />
    </group>
  );
}

function KoBursts({ burstsRef }: { burstsRef: MutableRefObject<Burst[]> }) {
  const rings = useRef<THREE.Mesh[]>([]);
  const label = useRef<THREE.Sprite[]>([]);

  useFrame(() => {
    const bursts = burstsRef.current;
    rings.current.forEach((ring, index) => {
      const burst = bursts[index];
      if (!ring) return;
      if (!burst) {
        ring.visible = false;
        if (label.current[index]) label.current[index].visible = false;
        return;
      }
      const t = burst.age / 0.55;
      ring.visible = true;
      ring.position.set(burst.x, 0.08, burst.z);
      const scale = (0.4 + t * 3.4) * ZOOGI_FX_SCALE;
      ring.scale.setScalar(scale);
      const material = ring.material as THREE.MeshBasicMaterial;
      material.opacity = (1 - t) * 0.9;
      const sprite = label.current[index];
      if (sprite) {
        sprite.visible = true;
        sprite.position.set(burst.x, 1.6 + t * 1.4, burst.z);
        const s = 1.1 + t * 0.8;
        sprite.scale.set(s, s * 0.5, 1);
        (sprite.material as THREE.SpriteMaterial).opacity = 1 - t;
      }
    });
  });

  const koTexture = useMemo(() => {
    if (typeof document === "undefined") return null;
    const canvas = document.createElement("canvas");
    canvas.width = 256;
    canvas.height = 128;
    const ctx = canvas.getContext("2d");
    if (ctx) {
      ctx.clearRect(0, 0, 256, 128);
      ctx.font = "900 92px sans-serif";
      ctx.textAlign = "center";
      ctx.textBaseline = "middle";
      ctx.lineWidth = 14;
      ctx.strokeStyle = "#1a0420";
      ctx.strokeText("KO", 128, 68);
      ctx.fillStyle = "#fff6d8";
      ctx.fillText("KO", 128, 68);
    }
    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    return texture;
  }, []);

  return (
    <group>
      {Array.from({ length: 4 }, (_, index) => (
        <group key={index}>
          <mesh
            ref={(node) => {
              if (node) rings.current[index] = node;
            }}
            rotation={[-Math.PI / 2, 0, 0]}
            visible={false}
          >
            <ringGeometry args={[0.72, 0.92, 28]} />
            <meshBasicMaterial color="#ff4ad8" toneMapped={false} transparent opacity={0.9} side={THREE.DoubleSide} />
          </mesh>
          {koTexture && (
            <sprite
              ref={(node) => {
                if (node) label.current[index] = node;
              }}
              visible={false}
              scale={[1.2, 0.6, 1]}
            >
              <spriteMaterial map={koTexture} transparent depthWrite={false} toneMapped={false} />
            </sprite>
          )}
        </group>
      ))}
    </group>
  );
}

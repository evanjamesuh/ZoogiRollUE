import { useMemo } from "react";
import * as THREE from "three";
import { BUMPER_RADIUS } from "@/lib/arenaColliders";
import {
  NEON_HALF_X,
  NEON_HALF_Z,
  neonBumpers,
  neonRails,
} from "@/lib/neonCourt";

const FLOOR_Y = 0;
const RAIL_HEIGHT = 1.25;

function useCourtTexture(): THREE.CanvasTexture {
  return useMemo(() => {
    const canvas = document.createElement("canvas");
    canvas.width = 1024;
    canvas.height = 1024;
    const ctx = canvas.getContext("2d");
    if (ctx) {
      ctx.fillStyle = "#14161f";
      ctx.fillRect(0, 0, 1024, 1024);
      ctx.strokeStyle = "#3a4258";
      ctx.lineWidth = 2;
      for (let i = 0; i <= 1024; i += 64) {
        ctx.beginPath();
        ctx.moveTo(i, 0);
        ctx.lineTo(i, 1024);
        ctx.stroke();
        ctx.beginPath();
        ctx.moveTo(0, i);
        ctx.lineTo(1024, i);
        ctx.stroke();
      }
      ctx.strokeStyle = "#b7c0d4";
      ctx.lineWidth = 6;
      ctx.beginPath();
      ctx.moveTo(512, 430);
      ctx.lineTo(590, 512);
      ctx.lineTo(512, 594);
      ctx.lineTo(434, 512);
      ctx.closePath();
      ctx.stroke();
      ctx.strokeStyle = "#3d4458";
      ctx.lineWidth = 10;
      ctx.strokeRect(72, 72, 880, 880);
    }
    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.anisotropy = 4;
    return texture;
  }, []);
}

function Rail({
  minX,
  maxX,
  minZ,
  maxZ,
  color,
}: {
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
  color: string;
}) {
  const width = Math.max(0.05, maxX - minX);
  const depth = Math.max(0.05, maxZ - minZ);
  return (
    <mesh position={[(minX + maxX) / 2, RAIL_HEIGHT / 2, (minZ + maxZ) / 2]} castShadow>
      <boxGeometry args={[width, RAIL_HEIGHT, depth]} />
      <meshStandardMaterial
        color="#140818"
        emissive={color}
        emissiveIntensity={3.4}
        toneMapped={false}
        roughness={0.28}
        metalness={0.45}
      />
    </mesh>
  );
}

function BumperPost({ x, z, tint }: { x: number; z: number; tint: string }) {
  return (
    <group position={[x, 0, z]}>
      <mesh position={[0, 0.72, 0]} castShadow>
        <cylinderGeometry args={[BUMPER_RADIUS, BUMPER_RADIUS * 1.05, 1.44, 20]} />
        <meshStandardMaterial
          color="#1a1024"
          emissive={tint}
          emissiveIntensity={2.8}
          toneMapped={false}
          roughness={0.22}
          metalness={0.55}
        />
      </mesh>
      <mesh position={[0, 1.5, 0]}>
        <sphereGeometry args={[0.28, 16, 16]} />
        <meshStandardMaterial color="#fff6e8" emissive={tint} emissiveIntensity={4} toneMapped={false} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.04, 0]}>
        <ringGeometry args={[BUMPER_RADIUS + 0.08, BUMPER_RADIUS + 0.22, 24]} />
        <meshBasicMaterial color={tint} toneMapped={false} transparent opacity={0.85} side={THREE.DoubleSide} />
      </mesh>
    </group>
  );
}

function KnockoutLip() {
  const thickness = 0.16;
  const y = 0.05;
  const x = NEON_HALF_X + 0.08;
  const z = NEON_HALF_Z + 0.08;
  return (
    <group>
      <mesh position={[0, y, z]} rotation={[-Math.PI / 2, 0, 0]}>
        <planeGeometry args={[x * 2, thickness]} />
        <meshStandardMaterial color="#2a0618" emissive="#ff3ec8" emissiveIntensity={3.6} toneMapped={false} roughness={0.4} />
      </mesh>
      <mesh position={[0, y, -z]} rotation={[-Math.PI / 2, 0, 0]}>
        <planeGeometry args={[x * 2, thickness]} />
        <meshStandardMaterial color="#2a0618" emissive="#ff3ec8" emissiveIntensity={3.6} toneMapped={false} roughness={0.4} />
      </mesh>
      <mesh position={[x, y, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <planeGeometry args={[thickness, z * 2]} />
        <meshStandardMaterial color="#2a0618" emissive="#ff3ec8" emissiveIntensity={3.6} toneMapped={false} roughness={0.4} />
      </mesh>
      <mesh position={[-x, y, 0]} rotation={[-Math.PI / 2, 0, 0]}>
        <planeGeometry args={[thickness, z * 2]} />
        <meshStandardMaterial color="#2a0618" emissive="#ff3ec8" emissiveIntensity={3.6} toneMapped={false} roughness={0.4} />
      </mesh>
    </group>
  );
}

function CityBlock({
  position,
  size,
  windows,
}: {
  position: [number, number, number];
  size: [number, number, number];
  windows: string;
}) {
  return (
    <group position={position}>
      <mesh position={[0, size[1] / 2, 0]}>
        <boxGeometry args={size} />
        <meshStandardMaterial color="#12141c" roughness={0.9} metalness={0.05} />
      </mesh>
      <mesh position={[0, size[1] * 0.55, size[2] / 2 + 0.02]}>
        <planeGeometry args={[size[0] * 0.72, size[1] * 0.45]} />
        <meshStandardMaterial color="#0b0d14" emissive={windows} emissiveIntensity={1.6} toneMapped={false} />
      </mesh>
    </group>
  );
}

export function NeonCourtLights() {
  return (
    <>
      <ambientLight intensity={0.18} color="#9aa4c7" />
      <directionalLight
        position={[6, 18, 14]}
        intensity={0.55}
        color="#d5defa"
        castShadow
        shadow-mapSize-width={2048}
        shadow-mapSize-height={2048}
        shadow-camera-far={60}
        shadow-camera-left={-18}
        shadow-camera-right={18}
        shadow-camera-top={14}
        shadow-camera-bottom={-14}
      />
      <directionalLight position={[0, 8, -16]} intensity={0.22} color="#7ef6ff" />
      <directionalLight position={[-10, 6, 4]} intensity={0.2} color="#ff4ad8" />
    </>
  );
}

const BLOCKS: Array<{ position: [number, number, number]; size: [number, number, number]; windows: string }> = [
  { position: [-28, 0, -22], size: [6, 14, 5], windows: "#7ef6ff" },
  { position: [-18, 0, -30], size: [4.5, 22, 4], windows: "#ff4ad8" },
  { position: [-8, 0, -34], size: [7, 11, 5], windows: "#c084fc" },
  { position: [6, 0, -36], size: [5, 18, 4.5], windows: "#7ef6ff" },
  { position: [18, 0, -28], size: [6, 26, 5], windows: "#ff4ad8" },
  { position: [30, 0, -18], size: [4, 12, 6], windows: "#fbbf24" },
  { position: [-34, 0, -6], size: [5, 16, 5], windows: "#7ef6ff" },
  { position: [34, 0, 2], size: [5, 20, 4], windows: "#c084fc" },
  { position: [-26, 0, 16], size: [7, 9, 5], windows: "#ff4ad8" },
  { position: [22, 0, 20], size: [6, 13, 5], windows: "#7ef6ff" },
  { position: [0, 0, 32], size: [8, 8, 4], windows: "#fbbf24" },
  { position: [-12, 0, 30], size: [4, 15, 4], windows: "#c084fc" },
];

export function NeonCourtArena() {
  const floorTexture = useCourtTexture();
  const rails = neonRails();
  const posts = neonBumpers();
  const tints = ["#22e7ff", "#ff2bd6", "#b026ff"];

  return (
    <group>
      <mesh position={[0, FLOOR_Y - 0.08, 0]} receiveShadow>
        <boxGeometry args={[NEON_HALF_X * 2, 0.16, NEON_HALF_Z * 2]} />
        <meshStandardMaterial
          color="#14161f"
          map={floorTexture}
          roughness={0.92}
          metalness={0.04}
          emissive="#000000"
          emissiveIntensity={0}
        />
      </mesh>

      <KnockoutLip />

      {rails.map((rail) => (
        <Rail key={rail.id} {...rail} />
      ))}

      {posts.map((post, index) => (
        <BumperPost key={post.id} x={post.x} z={post.z} tint={tints[index % tints.length]} />
      ))}

      {/* Corner mouths read as the way out: a short magenta dash in each gap. */}
      {[
        [-NEON_HALF_X + 1.15, -NEON_HALF_Z + 1.15],
        [NEON_HALF_X - 1.15, -NEON_HALF_Z + 1.15],
        [-NEON_HALF_X + 1.15, NEON_HALF_Z - 1.15],
        [NEON_HALF_X - 1.15, NEON_HALF_Z - 1.15],
      ].map(([x, z]) => (
        <mesh key={`${x}-${z}`} position={[x, 0.06, z]} rotation={[-Math.PI / 2, 0, Math.PI / 4]}>
          <planeGeometry args={[1.5, 0.12]} />
          <meshStandardMaterial color="#2a0618" emissive="#ff5ad4" emissiveIntensity={3.2} toneMapped={false} roughness={0.4} />
        </mesh>
      ))}

      <mesh position={[0, -0.2, 0]}>
        <cylinderGeometry args={[70, 78, 0.2, 8]} />
        <meshStandardMaterial color="#07080d" roughness={1} metalness={0} />
      </mesh>

      {BLOCKS.map((block) => (
        <CityBlock key={block.position.join(",")} {...block} />
      ))}

      <mesh scale={[180, 180, 180]}>
        <sphereGeometry args={[1, 24, 16]} />
        <shaderMaterial
          side={THREE.BackSide}
          depthWrite={false}
          args={[{
            uniforms: {
              top: { value: new THREE.Color("#070814") },
              bottom: { value: new THREE.Color("#14102a") },
            },
            vertexShader: `
              varying vec3 vPos;
              void main() {
                vPos = position;
                gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
              }
            `,
            fragmentShader: `
              varying vec3 vPos;
              uniform vec3 top;
              uniform vec3 bottom;
              void main() {
                float h = normalize(vPos).y * 0.5 + 0.5;
                gl_FragColor = vec4(mix(bottom, top, h), 1.0);
              }
            `,
          }]}
        />
      </mesh>

    </group>
  );
}

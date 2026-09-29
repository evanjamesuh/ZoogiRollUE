import { useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { useThree } from "@react-three/fiber";
import * as THREE from "three";
import { RoomEnvironment } from "three/examples/jsm/environments/RoomEnvironment.js";
import { BUMPER_RADIUS } from "@/lib/arenaColliders";
import { ARENA_SCALE } from "@/lib/arenaScale";
import {
  NEON_CORNER_GAP,
  NEON_HALF_X,
  NEON_HALF_Z,
  neonBumpers,
  neonObstacles,
  neonRails,
} from "@/lib/neonCourt";

const RAIL_BODY_H = 0.96;
const RAIL_CAP_H = 0.22;
const TEX_W = 1536;
const TEX_H = 1024;
const PX = TEX_W / (NEON_HALF_X * 2);

const METAL = {
  color: "#2c3242",
  roughness: 0.36,
  metalness: 0.74,
  emissive: "#000000",
  emissiveIntensity: 0,
} as const;

const METAL_CAP = {
  color: "#3a4254",
  roughness: 0.3,
  metalness: 0.8,
  emissive: "#000000",
  emissiveIntensity: 0,
} as const;

function worldToCanvas(x: number, z: number): [number, number] {
  return [(x + NEON_HALF_X) * PX, (z + NEON_HALF_Z) * PX];
}

function paintHazard(ctx: CanvasRenderingContext2D, x0: number, y0: number, x1: number, y1: number) {
  ctx.save();
  ctx.beginPath();
  ctx.rect(Math.min(x0, x1), Math.min(y0, y1), Math.abs(x1 - x0), Math.abs(y1 - y0));
  ctx.clip();
  const stripe = 22;
  for (let i = -TEX_W; i < TEX_W * 2; i += stripe) {
    ctx.strokeStyle = Math.floor(i / stripe) % 2 === 0 ? "#b86a1a" : "#24180c";
    ctx.lineWidth = 12;
    ctx.beginPath();
    ctx.moveTo(x0 + i, y0);
    ctx.lineTo(x0 + i + (y1 - y0), y1);
    ctx.stroke();
  }
  ctx.restore();
}

function paintEndZone(
  ctx: CanvasRenderingContext2D,
  x0: number,
  z0: number,
  x1: number,
  z1: number,
  color: string,
) {
  const [ax, ay] = worldToCanvas(Math.min(x0, x1), Math.min(z0, z1));
  const [bx, by] = worldToCanvas(Math.max(x0, x1), Math.max(z0, z1));
  ctx.fillStyle = color;
  ctx.fillRect(ax, ay, bx - ax, by - ay);
}

function paintChevron(ctx: CanvasRenderingContext2D, x: number, z: number, dir: number, color: string) {
  const [px, py] = worldToCanvas(x, z);
  const s = 0.32 * PX;
  ctx.strokeStyle = color;
  ctx.lineWidth = 5;
  ctx.beginPath();
  ctx.moveTo(px - dir * s, py - s);
  ctx.lineTo(px + dir * s * 0.15, py);
  ctx.lineTo(px - dir * s, py + s);
  ctx.stroke();
}

function paintCourt(ctx: CanvasRenderingContext2D) {
  ctx.fillStyle = "#243044";
  ctx.fillRect(0, 0, TEX_W, TEX_H);

  const panel = 2 * PX;
  for (let x = 0; x < TEX_W; x += panel) {
    for (let y = 0; y < TEX_H; y += panel) {
      const alt = (Math.round(x / panel) + Math.round(y / panel)) % 2 === 0;
      ctx.fillStyle = alt ? "#2a3850" : "#223044";
      ctx.fillRect(x + 1, y + 1, panel - 2, panel - 2);
    }
  }

  ctx.strokeStyle = "#46556e";
  ctx.lineWidth = 2;
  for (let i = 0; i <= TEX_W; i += panel) {
    ctx.beginPath();
    ctx.moveTo(i, 0);
    ctx.lineTo(i, TEX_H);
    ctx.stroke();
    ctx.beginPath();
    ctx.moveTo(0, i);
    ctx.lineTo(TEX_W, i);
    ctx.stroke();
  }

  const [cx, cy] = worldToCanvas(0, 0);
  ctx.strokeStyle = "#8b9bb8";
  ctx.lineWidth = 5;
  const S = ARENA_SCALE;
  ctx.beginPath();
  ctx.arc(cx, cy, 2.35 * S * PX, 0, Math.PI * 2);
  ctx.stroke();
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.arc(cx, cy, 1.15 * S * PX, 0, Math.PI * 2);
  ctx.stroke();

  ctx.setLineDash([18, 16]);
  ctx.lineWidth = 3;
  ctx.beginPath();
  ctx.moveTo(cx - 7.2 * S * PX, cy - 2.15 * S * PX);
  ctx.lineTo(cx + 7.2 * S * PX, cy - 2.15 * S * PX);
  ctx.moveTo(cx - 7.2 * S * PX, cy + 2.15 * S * PX);
  ctx.lineTo(cx + 7.2 * S * PX, cy + 2.15 * S * PX);
  ctx.stroke();
  ctx.beginPath();
  ctx.moveTo(cx - 4.4 * S * PX, cy - 5.4 * S * PX);
  ctx.lineTo(cx - 4.4 * S * PX, cy + 5.4 * S * PX);
  ctx.moveTo(cx + 4.4 * S * PX, cy - 5.4 * S * PX);
  ctx.lineTo(cx + 4.4 * S * PX, cy + 5.4 * S * PX);
  ctx.stroke();
  ctx.setLineDash([]);

  ctx.strokeStyle = "#6d7e98";
  ctx.lineWidth = 4;
  ctx.beginPath();
  ctx.arc(cx, cy, 5.6 * S * PX, -0.55, 0.55);
  ctx.stroke();
  ctx.beginPath();
  ctx.arc(cx, cy, 5.6 * S * PX, Math.PI - 0.55, Math.PI + 0.55);
  ctx.stroke();

  paintEndZone(ctx, -11.3 * S, -3.6 * S, -6.35 * S, 3.6 * S, "rgba(18, 120, 150, 0.34)");
  paintEndZone(ctx, 6.35 * S, -3.6 * S, 11.3 * S, 3.6 * S, "rgba(150, 28, 90, 0.34)");

  ctx.strokeStyle = "#d5deee";
  ctx.lineWidth = 4;
  ctx.beginPath();
  ctx.moveTo(cx, cy - 6.4 * S * PX);
  ctx.lineTo(cx, cy - 2.5 * S * PX);
  ctx.moveTo(cx, cy + 2.5 * S * PX);
  ctx.lineTo(cx, cy + 6.4 * S * PX);
  ctx.stroke();
  ctx.lineWidth = 3;
  for (let z = -5.2 * S; z <= 5.2 * S; z += 1.3 * S) {
    if (Math.abs(z) < 2.2 * S) continue;
    const [hx, hy] = worldToCanvas(0, z);
    ctx.beginPath();
    ctx.moveTo(hx - 10, hy);
    ctx.lineTo(hx + 10, hy);
    ctx.stroke();
  }

  for (const x of [-9.4, -8.5, -7.6]) paintChevron(ctx, x * ARENA_SCALE, 2.35 * ARENA_SCALE, 1, "#7ee7ff");
  for (const x of [7.6, 8.5, 9.4]) paintChevron(ctx, x * ARENA_SCALE, -2.35 * ARENA_SCALE, -1, "#ff7ad4");

  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.font = "700 72px sans-serif";
  ctx.fillStyle = "rgba(150, 230, 245, 0.72)";
  const [n1x, n1y] = worldToCanvas(-8.3 * ARENA_SCALE, 2.55 * ARENA_SCALE);
  ctx.fillText("1", n1x, n1y);
  ctx.fillStyle = "rgba(255, 150, 200, 0.72)";
  const [n2x, n2y] = worldToCanvas(8.45 * ARENA_SCALE, -2.55 * ARENA_SCALE);
  ctx.fillText("2", n2x, n2y);

  // Original center mark: a marble, a roll crescent, and the letters ZR.
  ctx.fillStyle = "#243044";
  ctx.beginPath();
  ctx.arc(cx, cy, 0.72 * ARENA_SCALE * PX, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#d7deea";
  ctx.beginPath();
  ctx.arc(cx - 6, cy + 4, 0.34 * ARENA_SCALE * PX, 0, Math.PI * 2);
  ctx.fill();
  ctx.fillStyle = "#8ea0bd";
  ctx.beginPath();
  ctx.arc(cx - 12, cy - 2, 0.12 * ARENA_SCALE * PX, 0, Math.PI * 2);
  ctx.fill();
  ctx.strokeStyle = "#9eb0cc";
  ctx.lineWidth = 5;
  ctx.beginPath();
  ctx.arc(cx + 10, cy - 2, 0.42 * ARENA_SCALE * PX, Math.PI * 0.85, Math.PI * 1.85);
  ctx.stroke();
  ctx.fillStyle = "#c5d0e4";
  ctx.font = "700 34px sans-serif";
  ctx.textAlign = "center";
  ctx.textBaseline = "middle";
  ctx.fillText("ZR", cx + 2, cy + 28);

  for (const post of neonBumpers()) {
    const [px, py] = worldToCanvas(post.x, post.z);
    const g = ctx.createRadialGradient(px, py, 8, px, py, 1.35 * ARENA_SCALE * PX);
    g.addColorStop(0, "rgba(0,0,0,0.5)");
    g.addColorStop(1, "rgba(0,0,0,0)");
    ctx.fillStyle = g;
    ctx.beginPath();
    ctx.ellipse(px, py, 1.45 * ARENA_SCALE * PX, 1.15 * ARENA_SCALE * PX, 0, 0, Math.PI * 2);
    ctx.fill();
  }

  const vignette = ctx.createRadialGradient(cx, cy, 3.2 * ARENA_SCALE * PX, cx, cy, 11.5 * ARENA_SCALE * PX);
  vignette.addColorStop(0, "rgba(0,0,0,0)");
  vignette.addColorStop(1, "rgba(0,0,0,0.28)");
  ctx.fillStyle = vignette;
  ctx.fillRect(0, 0, TEX_W, TEX_H);

  const gapX = NEON_HALF_X - NEON_CORNER_GAP;
  const gapZ = NEON_HALF_Z - NEON_CORNER_GAP;
  for (const sx of [-1, 1]) {
    for (const sz of [-1, 1]) {
      const [x0, y0] = worldToCanvas(sx * gapX, sz * gapZ);
      const [x1, y1] = worldToCanvas(sx * NEON_HALF_X, sz * NEON_HALF_Z);
      paintHazard(ctx, x0, y0, x1, y1);
    }
  }
}

function useCourtTexture(): THREE.CanvasTexture {
  return useMemo(() => {
    const canvas = document.createElement("canvas");
    canvas.width = TEX_W;
    canvas.height = TEX_H;
    const ctx = canvas.getContext("2d");
    if (ctx) paintCourt(ctx);
    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.anisotropy = 8;
    texture.needsUpdate = true;
    return texture;
  }, []);
}

function useFloorEnv(): THREE.Texture | null {
  const gl = useThree((state) => state.gl);
  const [env, setEnv] = useState<THREE.Texture | null>(null);
  useEffect(() => {
    const pmrem = new THREE.PMREMGenerator(gl);
    const room = new RoomEnvironment();
    const tex = pmrem.fromScene(room, 0.04).texture;
    room.traverse((obj) => {
      const mesh = obj as THREE.Mesh;
      mesh.geometry?.dispose();
      const mat = mesh.material;
      if (Array.isArray(mat)) mat.forEach((m) => m.dispose());
      else mat?.dispose();
    });
    pmrem.dispose();
    setEnv(tex);
    return () => {
      tex.dispose();
    };
  }, [gl]);
  return env;
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
  const cx = (minX + maxX) / 2;
  const cz = (minZ + maxZ) / 2;
  const alongX = width >= depth;
  const inset = 0.07;
  const inner = Math.abs(minZ) < Math.abs(maxZ) ? minZ : maxZ;
  const innerX = Math.abs(minX) < Math.abs(maxX) ? minX : maxX;
  const capY = RAIL_BODY_H + RAIL_CAP_H / 2;
  const trimY = RAIL_BODY_H + RAIL_CAP_H - 0.03;

  const trim = alongX
    ? {
        pos: [cx, trimY, inner] as [number, number, number],
        args: [width - 0.2, 0.07, 0.07] as [number, number, number],
        face: [cx, RAIL_BODY_H * 0.62, inner] as [number, number, number],
        faceArgs: [width - 0.28, 0.11, 0.045] as [number, number, number],
      }
    : {
        pos: [innerX, trimY, cz] as [number, number, number],
        args: [0.07, 0.07, depth - 0.2] as [number, number, number],
        face: [innerX, RAIL_BODY_H * 0.62, cz] as [number, number, number],
        faceArgs: [0.045, 0.11, depth - 0.28] as [number, number, number],
      };

  return (
    <group>
      <mesh position={[cx, RAIL_BODY_H / 2, cz]} castShadow receiveShadow>
        <boxGeometry args={[width, RAIL_BODY_H, depth]} />
        <meshStandardMaterial {...METAL} />
      </mesh>
      <mesh position={[cx, capY, cz]} castShadow receiveShadow>
        <boxGeometry args={[Math.max(0.05, width - inset * 2), RAIL_CAP_H, Math.max(0.05, depth - inset * 2)]} />
        <meshStandardMaterial {...METAL_CAP} />
      </mesh>
      <mesh position={trim.pos}>
        <boxGeometry args={trim.args} />
        <meshStandardMaterial color={color} emissive={color} emissiveIntensity={1.85} toneMapped={false} roughness={0.25} />
      </mesh>
      <mesh position={trim.face}>
        <boxGeometry args={trim.faceArgs} />
        <meshStandardMaterial color={color} emissive={color} emissiveIntensity={1.7} toneMapped={false} roughness={0.3} />
      </mesh>
    </group>
  );
}

function Pylon({ x, z, tint }: { x: number; z: number; tint: string }) {
  return (
    <group position={[x, 0, z]}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.028, 0]}>
        <circleGeometry args={[BUMPER_RADIUS + 0.18, 24]} />
        <meshBasicMaterial color="#05060c" transparent opacity={0.55} depthWrite={false} />
      </mesh>
      <mesh position={[0, 0.1, 0]} castShadow receiveShadow>
        <cylinderGeometry args={[BUMPER_RADIUS, BUMPER_RADIUS * 1.05, 0.2, 20]} />
        <meshStandardMaterial color="#6a7588" metalness={0.84} roughness={0.28} />
      </mesh>
      <mesh position={[0, 1.15, 0]} castShadow receiveShadow>
        <cylinderGeometry args={[0.34, 0.42, 1.9, 16]} />
        <meshStandardMaterial color="#4e586c" metalness={0.8} roughness={0.32} />
      </mesh>
      {[0.55, 1.05, 1.55].map((y) => (
        <mesh key={y} position={[0, y, 0]} rotation={[Math.PI / 2, 0, 0]}>
          <torusGeometry args={[0.48, 0.045, 8, 24]} />
          <meshStandardMaterial color={tint} emissive={tint} emissiveIntensity={1.85} toneMapped={false} roughness={0.22} />
        </mesh>
      ))}
      <mesh position={[0, 2.18, 0]}>
        <sphereGeometry args={[0.2, 14, 12]} />
        <meshStandardMaterial color="#fff6ea" emissive={tint} emissiveIntensity={2.3} toneMapped={false} roughness={0.2} />
      </mesh>
    </group>
  );
}

function usePanelTexture(): THREE.CanvasTexture {
  return useMemo(() => {
    const canvas = document.createElement("canvas");
    canvas.width = 128;
    canvas.height = 128;
    const ctx = canvas.getContext("2d");
    if (ctx) {
      ctx.fillStyle = "#8b97ab";
      ctx.fillRect(0, 0, 128, 128);
      ctx.fillStyle = "#c5d0e2";
      ctx.fillRect(6, 6, 116, 52);
      ctx.fillStyle = "#9aa6ba";
      ctx.fillRect(6, 66, 116, 56);
      ctx.strokeStyle = "#3e485c";
      ctx.lineWidth = 3;
      ctx.strokeRect(4, 4, 120, 120);
      ctx.strokeRect(6, 6, 116, 56);
      ctx.strokeStyle = "#e7eef8";
      ctx.lineWidth = 2;
      ctx.strokeRect(10, 10, 108, 48);
    }
    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    texture.wrapS = THREE.RepeatWrapping;
    texture.wrapT = THREE.RepeatWrapping;
    return texture;
  }, []);
}

function RaisedBlock({
  minX,
  maxX,
  minZ,
  maxZ,
  tint,
  height,
  panels,
}: {
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
  tint: string;
  height: number;
  panels: THREE.Texture;
}) {
  const width = Math.max(0.12, maxX - minX);
  const depth = Math.max(0.12, maxZ - minZ);
  const cx = (minX + maxX) / 2;
  const cz = (minZ + maxZ) / 2;
  const bodyH = height * 0.7;
  const capH = height * 0.22;
  const inset = Math.min(0.07, width * 0.08, depth * 0.16);
  const trim = Math.min(0.065, width * 0.2, depth * 0.22);
  const trimY = bodyH + capH - 0.01;
  const map = useMemo(() => {
    const copy = panels.clone();
    copy.repeat.set(Math.max(1, width / 1.1), Math.max(1, depth / 0.7));
    copy.needsUpdate = true;
    return copy;
  }, [panels, width, depth]);

  return (
    <group>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[cx, 0.04, cz]}>
        <planeGeometry args={[width + 0.7, depth + 0.55]} />
        <meshBasicMaterial color={tint} transparent opacity={0.46} depthWrite={false} />
      </mesh>
      <mesh position={[cx, bodyH / 2, cz]} castShadow receiveShadow>
        <boxGeometry args={[width, bodyH, depth]} />
        <meshStandardMaterial map={map} color="#f2f5fa" metalness={0.46} roughness={0.34} />
      </mesh>
      <mesh position={[cx, bodyH + capH / 2, cz]} castShadow receiveShadow>
        <boxGeometry args={[Math.max(0.06, width - inset * 2), capH, Math.max(0.06, depth - inset * 2)]} />
        <meshStandardMaterial color="#d5deec" metalness={0.58} roughness={0.28} />
      </mesh>
      <mesh position={[cx, trimY, cz + depth / 2 - trim / 2]}>
        <boxGeometry args={[Math.max(0.08, width - 0.04), 0.05, trim]} />
        <meshStandardMaterial color={tint} emissive={tint} emissiveIntensity={1.9} toneMapped={false} roughness={0.25} />
      </mesh>
      <mesh position={[cx, trimY, cz - depth / 2 + trim / 2]}>
        <boxGeometry args={[Math.max(0.08, width - 0.04), 0.05, trim]} />
        <meshStandardMaterial color={tint} emissive={tint} emissiveIntensity={1.9} toneMapped={false} roughness={0.25} />
      </mesh>
      <mesh position={[cx + width / 2 - trim / 2, trimY, cz]}>
        <boxGeometry args={[trim, 0.05, Math.max(0.04, depth - trim * 2)]} />
        <meshStandardMaterial color={tint} emissive={tint} emissiveIntensity={1.9} toneMapped={false} roughness={0.25} />
      </mesh>
      <mesh position={[cx - width / 2 + trim / 2, trimY, cz]}>
        <boxGeometry args={[trim, 0.05, Math.max(0.04, depth - trim * 2)]} />
        <meshStandardMaterial color={tint} emissive={tint} emissiveIntensity={1.9} toneMapped={false} roughness={0.25} />
      </mesh>
    </group>
  );
}

function BumperPost({ x, z, tint }: { x: number; z: number; tint: string }) {
  return (
    <group position={[x, 0, z]}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.028, 0]}>
        <circleGeometry args={[BUMPER_RADIUS + 0.22, 24]} />
        <meshBasicMaterial color="#05060c" transparent opacity={0.62} depthWrite={false} />
      </mesh>
      <mesh position={[0, 0.1, 0]} castShadow receiveShadow>
        <cylinderGeometry args={[BUMPER_RADIUS, BUMPER_RADIUS * 1.04, 0.2, 24]} />
        <meshStandardMaterial color="#6a7588" metalness={0.84} roughness={0.28} />
      </mesh>
      <mesh position={[0, 0.78, 0]} castShadow receiveShadow>
        <cylinderGeometry args={[0.5, 0.58, 1.16, 20]} />
        <meshStandardMaterial color="#5a6578" metalness={0.78} roughness={0.3} />
      </mesh>
      <mesh position={[0, 0.22, 0]} rotation={[Math.PI / 2, 0, 0]}>
        <torusGeometry args={[BUMPER_RADIUS * 0.9, 0.038, 8, 28]} />
        <meshStandardMaterial color={tint} emissive={tint} emissiveIntensity={1.75} toneMapped={false} roughness={0.22} />
      </mesh>
      <mesh position={[0, 1.12, 0]} rotation={[Math.PI / 2, 0, 0]}>
        <torusGeometry args={[0.58, 0.05, 10, 28]} />
        <meshStandardMaterial color={tint} emissive={tint} emissiveIntensity={1.9} toneMapped={false} roughness={0.2} />
      </mesh>
      <mesh position={[0, 1.46, 0]}>
        <sphereGeometry args={[0.22, 16, 12]} />
        <meshStandardMaterial color="#fff6ea" emissive={tint} emissiveIntensity={2.35} toneMapped={false} roughness={0.2} />
      </mesh>
    </group>
  );
}

function KnockoutMouths() {
  const gapX = NEON_HALF_X - NEON_CORNER_GAP;
  const gapZ = NEON_HALF_Z - NEON_CORNER_GAP;
  const mouths = [-1, 1].flatMap((sx) => [-1, 1].map((sz) => ({ sx, sz })));
  return (
    <group>
      {mouths.map(({ sx, sz }) => {
        const xBar = sx * (gapX + NEON_HALF_X) / 2;
        const zBar = sz * (gapZ + NEON_HALF_Z) / 2;
        return (
          <group key={`${sx}-${sz}`}>
            <mesh position={[xBar, 0.05, sz * NEON_HALF_Z]}>
              <boxGeometry args={[NEON_CORNER_GAP, 0.06, 0.08]} />
              <meshStandardMaterial color="#ffb020" emissive="#ffb020" emissiveIntensity={2.05} toneMapped={false} roughness={0.35} />
            </mesh>
            <mesh position={[sx * NEON_HALF_X, 0.05, zBar]}>
              <boxGeometry args={[0.08, 0.06, NEON_CORNER_GAP]} />
              <meshStandardMaterial color="#ffb020" emissive="#ffb020" emissiveIntensity={2.05} toneMapped={false} roughness={0.35} />
            </mesh>
          </group>
        );
      })}
    </group>
  );
}

const STANDS: Array<{ pos: [number, number, number]; size: [number, number, number] }> = [
  // Near and side lips. The far bowl is its own lit grandstand so it reads in frame.
  { pos: [0, 0.46, 9.55], size: [15.4, 0.92, 2.15] },
  { pos: [0, 1.02, 10.9], size: [16.2, 0.55, 1.15] },
  { pos: [13.2, 0.4, 0], size: [1.65, 0.8, 8.4] },
  { pos: [-13.2, 0.4, 0], size: [1.65, 0.8, 8.4] },
  // Outer bowl, visible when the camera opens up on a spread-out rally.
  { pos: [0, 1.55, -15.4], size: [24, 1.35, 2.6] },
  { pos: [0, 1.55, 15.4], size: [24, 1.35, 2.6] },
  { pos: [16.6, 1.15, 0], size: [2.2, 1.15, 16] },
  { pos: [-16.6, 1.15, 0], size: [2.2, 1.15, 16] },
];

function crowdSpots(): Array<[number, number, number, number]> {
  const spots: Array<[number, number, number, number]> = [];
  const row = (z: number, y: number, x0: number, x1: number, step: number) => {
    for (let x = x0; x <= x1 + 0.01; x += step) {
      const n = Math.sin(x * 12.7 + z * 4.3);
      spots.push([x, y + (n * 0.5 + 0.5) * 0.16, z + n * 0.06, 0.75 + (Math.cos(x * 2.2) * 0.5 + 0.5) * 0.4]);
    }
  };
  row(-9.35, 1.02, -6.6, 6.6, 0.72);
  row(-10.35, 1.85, -8.2, 8.2, 0.68);
  row(-10.55, 1.38, -7.2, 7.2, 0.7);
  row(9.35, 1.02, -6.6, 6.6, 0.72);
  row(10.55, 1.38, -7.2, 7.2, 0.7);
  row(-15.2, 2.3, -10, 10, 0.85);
  row(15.2, 2.3, -10, 10, 0.85);
  for (let z = -3.6; z <= 3.6; z += 0.8) {
    const n = Math.sin(z * 9.1);
    spots.push([13.15, 0.92 + (n * 0.5 + 0.5) * 0.1, z, 0.8]);
    spots.push([-13.15, 0.92 + (n * 0.5 + 0.5) * 0.1, z, 0.82]);
  }
  return spots.map(([x, y, z, s]) => [x * ARENA_SCALE, y, z * ARENA_SCALE, s] as [number, number, number, number]);
}

function Crowd() {
  const ref = useRef<THREE.InstancedMesh>(null);
  const spots = useMemo(() => crowdSpots(), []);
  useLayoutEffect(() => {
    const mesh = ref.current;
    if (!mesh) return;
    const dummy = new THREE.Object3D();
    spots.forEach(([x, y, z, s], i) => {
      dummy.position.set(x, y, z);
      dummy.scale.set(s, 0.85 + s * 0.35, s * 0.7);
      dummy.updateMatrix();
      mesh.setMatrixAt(i, dummy.matrix);
    });
    mesh.instanceMatrix.needsUpdate = true;
  }, [spots]);
  return (
    <instancedMesh ref={ref} args={[undefined, undefined, spots.length]} frustumCulled={false}>
      <boxGeometry args={[0.32, 0.62, 0.24]} />
      <meshStandardMaterial color="#10131c" roughness={1} metalness={0} />
    </instancedMesh>
  );
}

function FarBowl() {
  const tiers = [
    { z: -9.22, y: 0.58, h: 0.82, depth: 0.7, width: 16.6 },
    { z: -10.02, y: 1.16, h: 0.96, depth: 0.78, width: 17.8 },
    { z: -10.86, y: 1.74, h: 1.08, depth: 0.84, width: 18.8 },
  ].map((tier) => ({
    ...tier,
    z: tier.z * ARENA_SCALE,
    depth: tier.depth * ARENA_SCALE,
    width: tier.width * ARENA_SCALE,
  }));
  const seats = [-6.4, -4.6, -2.8, -1.0, 1.0, 2.8, 4.6, 6.4].map((x) => x * ARENA_SCALE);
  return (
    <group>
      {tiers.map((tier, index) => {
        const frontZ = tier.z + tier.depth / 2 - 0.03;
        const lipY = tier.y + tier.h / 2 + 0.02;
        const half = tier.width / 2 - 0.12;
        return (
          <group key={tier.z}>
            <mesh position={[0, tier.y, tier.z]} receiveShadow>
              <boxGeometry args={[tier.width, tier.h, tier.depth]} />
              <meshStandardMaterial
                color={index === 0 ? "#b7c4da" : "#9aabc4"}
                emissive="#7f93b4"
                emissiveIntensity={0.42}
                metalness={0.22}
                roughness={0.48}
              />
            </mesh>
            {[-6.2, -3.1, 0, 3.1, 6.2].map((rib) => rib * ARENA_SCALE).map((rib) => (
              <mesh key={`${tier.z}-${rib}`} position={[rib, tier.y, frontZ + 0.02]}>
                <boxGeometry args={[0.08, tier.h * 0.72, 0.06]} />
                <meshStandardMaterial color="#d5e0f0" metalness={0.4} roughness={0.35} />
              </mesh>
            ))}
            <mesh position={[-half / 2, lipY, frontZ]}>
              <boxGeometry args={[half, 0.08, 0.1]} />
              <meshStandardMaterial color="#22e7ff" emissive="#22e7ff" emissiveIntensity={1.9} toneMapped={false} />
            </mesh>
            <mesh position={[half / 2, lipY, frontZ]}>
              <boxGeometry args={[half, 0.08, 0.1]} />
              <meshStandardMaterial color="#ff2bd6" emissive="#ff2bd6" emissiveIntensity={1.9} toneMapped={false} />
            </mesh>
          </group>
        );
      })}
      {seats.map((x) => (
        <mesh key={`seat-${x}`} position={[x, 1.08, -9.0 * ARENA_SCALE]}>
          <boxGeometry args={[0.7, 0.07, 0.14]} />
          <meshStandardMaterial
            color={x < 0 ? "#bff8ff" : "#ffd0f4"}
            emissive={x < 0 ? "#22e7ff" : "#ff2bd6"}
            emissiveIntensity={1.65}
            toneMapped={false}
          />
        </mesh>
      ))}
      {[-8.9, 8.9].map((x) => x * ARENA_SCALE).map((x) => (
        <group key={`mast-${x}`} position={[x, 0, -10.55 * ARENA_SCALE]}>
          <mesh position={[0, 1.65, 0]}>
            <cylinderGeometry args={[0.08, 0.12, 3.3, 8]} />
            <meshStandardMaterial color="#b7c3d6" metalness={0.72} roughness={0.28} />
          </mesh>
          <mesh position={[0, 3.2, 0]}>
            <boxGeometry args={[0.55, 0.12, 0.28]} />
            <meshStandardMaterial color="#d5deec" metalness={0.6} roughness={0.3} />
          </mesh>
          <mesh position={[0, 3.38, 0]}>
            <sphereGeometry args={[0.16, 10, 8]} />
            <meshStandardMaterial
              color="#fff6ea"
              emissive={x < 0 ? "#22e7ff" : "#ff2bd6"}
              emissiveIntensity={2.35}
              toneMapped={false}
            />
          </mesh>
        </group>
      ))}
      {[-1, 1].map((sx) => (
        <group key={`pilaster-${sx}`} position={[sx * 12.55, 0, -5.5]}>
          <mesh position={[0, 1.2, 0]}>
            <boxGeometry args={[0.24, 2.4, 0.28]} />
            <meshStandardMaterial color="#3a4458" metalness={0.62} roughness={0.34} />
          </mesh>
          <mesh position={[sx * -0.02, 1.2, 0.16]}>
            <boxGeometry args={[0.07, 2.1, 0.05]} />
            <meshStandardMaterial
              color={sx < 0 ? "#22e7ff" : "#ff2bd6"}
              emissive={sx < 0 ? "#22e7ff" : "#ff2bd6"}
              emissiveIntensity={1.95}
              toneMapped={false}
            />
          </mesh>
        </group>
      ))}
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[-4.3, 0.05, -8.92]}>
        <planeGeometry args={[8.4, 0.85]} />
        <meshBasicMaterial color="#22e7ff" transparent opacity={0.28} depthWrite={false} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[4.3, 0.05, -8.92]}>
        <planeGeometry args={[8.4, 0.85]} />
        <meshBasicMaterial color="#ff2bd6" transparent opacity={0.28} depthWrite={false} />
      </mesh>
      <LightRig z={-10.4} y={2.85} />
    </group>
  );
}

function LightRig({ z, y = 6.4 }: { z: number; y?: number }) {
  const sign = z >= 0 ? 1 : -1;
  return (
    <group position={[0, y, z]}>
      <mesh>
        <boxGeometry args={[18, 0.1, 0.12]} />
        <meshStandardMaterial color="#242a38" metalness={0.7} roughness={0.35} />
      </mesh>
      {[-7, -3.5, 0, 3.5, 7].map((x) => (
        <group key={x} position={[x, -0.35, sign * 0.15]}>
          <mesh>
            <sphereGeometry args={[0.11, 10, 8]} />
            <meshStandardMaterial color="#fff4d0" emissive="#fff1c2" emissiveIntensity={2.35} toneMapped={false} />
          </mesh>
          <mesh position={[0, 0.16, 0]}>
            <boxGeometry args={[0.04, 0.18, 0.04]} />
            <meshStandardMaterial color="#2a3142" metalness={0.6} roughness={0.4} />
          </mesh>
        </group>
      ))}
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
        <meshStandardMaterial color="#10131c" roughness={0.92} metalness={0.08} />
      </mesh>
      <mesh position={[0, size[1] * 0.55, size[2] / 2 + 0.02]}>
        <planeGeometry args={[size[0] * 0.7, size[1] * 0.42]} />
        <meshStandardMaterial color="#0b0d14" emissive={windows} emissiveIntensity={0.55} roughness={0.8} />
      </mesh>
    </group>
  );
}

export function NeonCourtLights() {
  return (
    <>
      <ambientLight intensity={0.22} color="#b7c0d8" />
      <directionalLight
        position={[7, 20, 11]}
        intensity={0.82}
        color="#e7eefc"
        castShadow
        shadow-mapSize-width={2048}
        shadow-mapSize-height={2048}
        shadow-bias={-0.00035}
        shadow-camera-far={64}
        shadow-camera-left={-20}
        shadow-camera-right={20}
        shadow-camera-top={16}
        shadow-camera-bottom={-16}
      />
      <directionalLight position={[0, 9, -18]} intensity={0.26} color="#7ef6ff" />
      <directionalLight position={[-12, 7, 6]} intensity={0.16} color="#ff4ad8" />
    </>
  );
}

const BLOCKS: Array<{ position: [number, number, number]; size: [number, number, number]; windows: string }> = [
  { position: [-32, 0, -26], size: [6, 16, 5], windows: "#7ef6ff" },
  { position: [-20, 0, -34], size: [4.5, 24, 4], windows: "#ff4ad8" },
  { position: [-6, 0, -38], size: [7, 12, 5], windows: "#c084fc" },
  { position: [10, 0, -40], size: [5, 20, 4.5], windows: "#7ef6ff" },
  { position: [24, 0, -30], size: [6, 28, 5], windows: "#ff4ad8" },
  { position: [36, 0, -16], size: [4, 14, 6], windows: "#fbbf24" },
  { position: [-38, 0, -4], size: [5, 18, 5], windows: "#7ef6ff" },
  { position: [38, 0, 6], size: [5, 22, 4], windows: "#c084fc" },
  { position: [-28, 0, 22], size: [7, 10, 5], windows: "#ff4ad8" },
  { position: [26, 0, 24], size: [6, 15, 5], windows: "#7ef6ff" },
  { position: [2, 0, 36], size: [8, 9, 4], windows: "#fbbf24" },
  { position: [-14, 0, 34], size: [4, 16, 4], windows: "#c084fc" },
];

function ArenaRim() {
  const lip = (x: number, z: number, sx: number, sz: number, color: string, key: string) => (
    <mesh key={key} position={[x, 1.05, z]}>
      <boxGeometry args={[sx, 0.07, sz]} />
      <meshStandardMaterial color={color} emissive={color} emissiveIntensity={1.7} toneMapped={false} roughness={0.3} />
    </mesh>
  );
  return (
    <group>
      <mesh position={[-12.28, 0.62, 0]} castShadow>
        <boxGeometry args={[0.28, 1.24, 7.2]} />
        <meshStandardMaterial color="#1a2436" metalness={0.55} roughness={0.45} />
      </mesh>
      <mesh position={[12.28, 0.62, 0]} castShadow>
        <boxGeometry args={[0.28, 1.24, 7.2]} />
        <meshStandardMaterial color="#2a1828" metalness={0.55} roughness={0.45} />
      </mesh>
      <mesh position={[-3.6, 0.58, -8.72]}>
        <boxGeometry args={[7.2, 1.16, 0.4]} />
        <meshStandardMaterial color="#1a2436" metalness={0.5} roughness={0.48} />
      </mesh>
      <mesh position={[3.6, 0.58, -8.72]}>
        <boxGeometry args={[7.2, 1.16, 0.4]} />
        <meshStandardMaterial color="#2a1828" metalness={0.5} roughness={0.48} />
      </mesh>
      {lip(-12.28, 0, 0.08, 7.0, "#22e7ff", "west")}
      {lip(12.28, 0, 0.08, 7.0, "#ff2bd6", "east")}
      {lip(-3.6, -8.72, 7.0, 0.1, "#22e7ff", "far-cyan")}
      {lip(3.6, -8.72, 7.0, 0.1, "#ff2bd6", "far-magenta")}
      {[
        [-12.28, -3.7, "#22e7ff"],
        [-12.28, 3.7, "#22e7ff"],
        [12.28, -3.7, "#ff2bd6"],
        [12.28, 3.7, "#ff2bd6"],
      ].map(([x, z, color]) => (
        <group key={`${x}-${z}`} position={[Number(x), 0, Number(z)]}>
          <mesh position={[0, 0.7, 0]}>
            <cylinderGeometry args={[0.28, 0.34, 1.4, 16]} />
            <meshStandardMaterial color="#2a3144" metalness={0.7} roughness={0.35} />
          </mesh>
          <mesh position={[0, 1.35, 0]} rotation={[Math.PI / 2, 0, 0]}>
            <torusGeometry args={[0.36, 0.05, 8, 20]} />
            <meshStandardMaterial color={color as string} emissive={color as string} emissiveIntensity={1.9} toneMapped={false} />
          </mesh>
        </group>
      ))}
    </group>
  );
}

const SKYLINE: Array<{ x: number; z: number; h: number; w: number; tint: string }> = [
  { x: -8.2, z: -11.42, h: 2.45, w: 1.15, tint: "#22e7ff" },
  { x: -5.7, z: -11.22, h: 3.05, w: 0.9, tint: "#7ef6ff" },
  { x: -3.3, z: -11.5, h: 2.2, w: 1.3, tint: "#9ad8ff" },
  { x: -0.9, z: -11.28, h: 2.75, w: 1.0, tint: "#e7f2ff" },
  { x: 1.5, z: -11.48, h: 2.95, w: 0.88, tint: "#ff2bd6" },
  { x: 3.9, z: -11.18, h: 2.25, w: 1.25, tint: "#ff6a8a" },
  { x: 6.4, z: -11.42, h: 3.1, w: 0.9, tint: "#ff2bd6" },
  { x: 8.7, z: -11.2, h: 2.4, w: 1.1, tint: "#ff4a6a" },
];

function Skyline() {
  return (
    <group>
      {SKYLINE.map((tower) => (
        <group key={`${tower.x}-${tower.z}`} position={[tower.x, 0, tower.z]}>
          <mesh position={[0, tower.h / 2, 0]}>
            <boxGeometry args={[tower.w, tower.h, 0.7]} />
            <meshStandardMaterial color="#121722" roughness={0.9} metalness={0.08} />
          </mesh>
          <mesh position={[0, tower.h * 0.55, 0.38]}>
            <planeGeometry args={[tower.w * 0.55, tower.h * 0.38]} />
            <meshStandardMaterial color="#0c1018" emissive={tower.tint} emissiveIntensity={2.05} toneMapped={false} />
          </mesh>
          <mesh position={[0, tower.h - 0.06, 0]}>
            <boxGeometry args={[tower.w + 0.06, 0.07, 0.78]} />
            <meshStandardMaterial color={tower.tint} emissive={tower.tint} emissiveIntensity={2.1} toneMapped={false} />
          </mesh>
        </group>
      ))}
    </group>
  );
}

const BLOCK_TINT: Record<string, string> = {
  "pad-west": "#22e7ff",
  "pad-east": "#ff2bd6",
  "pad-north": "#22e7ff",
  "pad-south": "#ff2bd6",
  "channel-north": "#f4f7ff",
  "channel-south": "#f4f7ff",
};

export function NeonCourtArena() {
  const floorTexture = useCourtTexture();
  const panelTexture = usePanelTexture();
  const envMap = useFloorEnv();
  const rails = neonRails();
  const posts = neonBumpers();
  const tints = ["#22e7ff", "#ff2bd6", "#b026ff"];

  return (
    <group>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]} receiveShadow>
        <planeGeometry args={[NEON_HALF_X * 2, NEON_HALF_Z * 2]} />
        <meshPhysicalMaterial
          color="#ffffff"
          map={floorTexture}
          roughness={0.32}
          metalness={0.18}
          clearcoat={0.36}
          clearcoatRoughness={0.42}
          envMap={envMap ?? undefined}
          envMapIntensity={0.58}
          emissive="#000000"
          emissiveIntensity={0}
        />
      </mesh>
      <mesh position={[0, -0.08, 0]} receiveShadow>
        <boxGeometry args={[NEON_HALF_X * 2 + 0.08, 0.18, NEON_HALF_Z * 2 + 0.08]} />
        <meshStandardMaterial color="#121722" roughness={0.55} metalness={0.35} />
      </mesh>

      <KnockoutMouths />

      {rails.map((rail) => (
        <Rail key={rail.id} {...rail} />
      ))}

      {posts.map((post, index) => (
        post.id.includes("pylon")
          ? <Pylon key={post.id} x={post.x} z={post.z} tint={post.x < 0 ? "#22e7ff" : "#ff2bd6"} />
          : <BumperPost key={post.id} x={post.x} z={post.z} tint={tints[index % tints.length]} />
      ))}

      {neonObstacles().map((box) => (
        <RaisedBlock
          key={box.id}
          {...box}
          tint={BLOCK_TINT[box.id] ?? "#22e7ff"}
          height={box.id.startsWith("channel") ? 0.5 : 0.68}
          panels={panelTexture}
        />
      ))}

      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.16, 0]}>
        <circleGeometry args={[46 * ARENA_SCALE, 48]} />
        <meshStandardMaterial color="#090b12" roughness={1} metalness={0} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.14, 0]}>
        <ringGeometry args={[13.6 * ARENA_SCALE, 14.15 * ARENA_SCALE, 64]} />
        <meshStandardMaterial color="#1a2233" emissive="#33506a" emissiveIntensity={0.45} roughness={0.6} metalness={0.2} />
      </mesh>

      {STANDS.map((stand) => (
        <mesh
          key={stand.pos.join(",")}
          position={[stand.pos[0] * ARENA_SCALE, stand.pos[1], stand.pos[2] * ARENA_SCALE]}
          receiveShadow
        >
          <boxGeometry args={[stand.size[0] * ARENA_SCALE, stand.size[1], stand.size[2] * ARENA_SCALE]} />
          <meshStandardMaterial color="#3a455c" roughness={0.62} metalness={0.28} />
        </mesh>
      ))}
      <Crowd />
      <FarBowl />
      <ArenaRim />
      <Skyline />
      <LightRig z={12.2 * ARENA_SCALE} y={5.2} />
      {/* Dim inner lip so the bowl has an edge without blooming over the court. */}
      <mesh position={[0, 0.55, -8.42 * ARENA_SCALE]}>
        <boxGeometry args={[15.2 * ARENA_SCALE, 0.06, 0.06]} />
        <meshStandardMaterial color="#7ec8e8" emissive="#7ec8e8" emissiveIntensity={0.55} roughness={0.4} />
      </mesh>
      <mesh position={[0, 0.55, 8.42 * ARENA_SCALE]}>
        <boxGeometry args={[15.2 * ARENA_SCALE, 0.06, 0.06]} />
        <meshStandardMaterial color="#e48cff" emissive="#e48cff" emissiveIntensity={0.45} roughness={0.4} />
      </mesh>

      {[-1, 1].flatMap((sx) => [-1, 1].map((sz) => (
        <group key={`tower-${sx}-${sz}`} position={[sx * 23.5 * ARENA_SCALE, 0, sz * 15.5 * ARENA_SCALE]}>
          <mesh position={[0, 3.2, 0]}>
            <cylinderGeometry args={[0.08, 0.12, 6.4, 8]} />
            <meshStandardMaterial color="#232838" metalness={0.65} roughness={0.4} />
          </mesh>
          <mesh position={[0, 6.5, 0]}>
            <sphereGeometry args={[0.16, 10, 8]} />
            <meshStandardMaterial color="#d7e6ff" emissive={sx === sz ? "#22e7ff" : "#ff2bd6"} emissiveIntensity={2.2} toneMapped={false} />
          </mesh>
        </group>
      )))}

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
              bottom: { value: new THREE.Color("#16122e") },
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

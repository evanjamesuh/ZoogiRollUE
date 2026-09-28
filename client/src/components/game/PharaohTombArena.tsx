import * as THREE from "three";
import { useEffect, useMemo, useState } from "react";
import { TOMB_STAGE, getTombBackdrop, getTombBlocks, type TombPiece } from "@/lib/arenaColliders";

const SAND_URL = "/textures/comic/paint_sand_512.jpg";
const STONE_URL = "/textures/comic/paint_stone_512.jpg";

function useRepeatMap(url: string, repeatX: number, repeatY: number) {
  const [map, setMap] = useState<THREE.Texture | null>(null);
  useEffect(() => {
    let alive = true;
    const loader = new THREE.TextureLoader();
    loader.load(url, (tex) => {
      tex.wrapS = THREE.RepeatWrapping;
      tex.wrapT = THREE.RepeatWrapping;
      tex.repeat.set(repeatX, repeatY);
      tex.colorSpace = THREE.SRGBColorSpace;
      tex.anisotropy = 8;
      if (alive) setMap(tex);
    });
    return () => {
      alive = false;
    };
  }, [url, repeatX, repeatY]);
  return map;
}

function carvedGlyphs(): THREE.CanvasTexture {
  const canvas = document.createElement("canvas");
  canvas.width = 1024;
  canvas.height = 512;
  const ctx = canvas.getContext("2d");
  if (!ctx) {
    const fallback = new THREE.CanvasTexture(canvas);
    return fallback;
  }

  ctx.fillStyle = "#d7ae78";
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  for (let i = 0; i < 900; i++) {
    const x = Math.random() * canvas.width;
    const y = Math.random() * canvas.height;
    const shade = 170 + Math.floor(Math.random() * 70);
    ctx.fillStyle = `rgba(${shade}, ${shade - 30}, ${shade - 55}, 0.18)`;
    ctx.fillRect(x, y, 2 + Math.random() * 7, 1);
  }

  const ink = "#4a301c";
  const chip = "#f3ddb8";
  ctx.lineCap = "round";
  ctx.lineJoin = "round";

  const carve = (draw: (ox: number, oy: number) => void) => {
    ctx.save();
    ctx.strokeStyle = chip;
    ctx.fillStyle = chip;
    ctx.translate(2, 2);
    draw(0, 0);
    ctx.restore();
    ctx.strokeStyle = ink;
    ctx.fillStyle = ink;
    draw(0, 0);
  };

  const ankh = (x: number, y: number, s: number) => {
    carve(() => {
      ctx.lineWidth = s * 0.09;
      ctx.beginPath();
      ctx.arc(x, y - s * 0.28, s * 0.2, 0, Math.PI * 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(x, y - s * 0.08);
      ctx.lineTo(x, y + s * 0.42);
      ctx.moveTo(x - s * 0.28, y + s * 0.02);
      ctx.lineTo(x + s * 0.28, y + s * 0.02);
      ctx.stroke();
    });
  };

  const eye = (x: number, y: number, s: number) => {
    carve(() => {
      ctx.lineWidth = s * 0.07;
      ctx.beginPath();
      ctx.moveTo(x - s * 0.42, y);
      ctx.quadraticCurveTo(x, y - s * 0.32, x + s * 0.42, y);
      ctx.quadraticCurveTo(x, y + s * 0.28, x - s * 0.42, y);
      ctx.stroke();
      ctx.beginPath();
      ctx.arc(x + s * 0.02, y, s * 0.11, 0, Math.PI * 2);
      ctx.stroke();
      ctx.beginPath();
      ctx.moveTo(x + s * 0.42, y);
      ctx.quadraticCurveTo(x + s * 0.62, y + s * 0.28, x + s * 0.4, y + s * 0.46);
      ctx.stroke();
    });
  };

  const triangle = (x: number, y: number, s: number) => {
    carve(() => {
      ctx.lineWidth = s * 0.08;
      ctx.beginPath();
      ctx.moveTo(x, y - s * 0.36);
      ctx.lineTo(x + s * 0.34, y + s * 0.32);
      ctx.lineTo(x - s * 0.34, y + s * 0.32);
      ctx.closePath();
      ctx.stroke();
    });
  };

  const square = (x: number, y: number, s: number) => {
    carve(() => {
      ctx.lineWidth = s * 0.08;
      ctx.strokeRect(x - s * 0.26, y - s * 0.26, s * 0.52, s * 0.52);
      ctx.beginPath();
      ctx.arc(x, y, s * 0.08, 0, Math.PI * 2);
      ctx.stroke();
    });
  };

  const cols = 4;
  const rows = 2;
  const glyphs = [ankh, eye, triangle, square, eye, triangle, ankh, square];
  glyphs.forEach((glyph, i) => {
    const col = i % cols;
    const row = Math.floor(i / cols);
    const x = ((col + 0.5) * canvas.width) / cols;
    const y = ((row + 0.5) * canvas.height) / rows;
    glyph(x, y, row === 0 ? 210 : 150);
  });

  const tex = new THREE.CanvasTexture(canvas);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.anisotropy = 8;
  return tex;
}

function Sandstone({
  map,
  color = "#e4c092",
  roughness = 0.88,
}: {
  map: THREE.Texture | null;
  color?: string;
  roughness?: number;
}) {
  return <meshStandardMaterial map={map ?? undefined} color={color} roughness={roughness} metalness={0.02} />;
}

function RuinWall({ piece, stone, glyphs }: { piece: TombPiece; stone: THREE.Texture | null; glyphs: THREE.Texture }) {
  const depth = piece.radius * 2;
  const rot = -Math.PI / 2 - piece.angle;
  const merlons = 5;
  return (
    <group position={[piece.x, 0, piece.z]} rotation={[0, rot, 0]}>
      <mesh position={[0, piece.height / 2, 0]} castShadow receiveShadow>
        <boxGeometry args={[piece.width, piece.height, depth]} />
        <Sandstone map={stone} />
      </mesh>
      <mesh position={[0, piece.height / 2, depth / 2 + 0.04]}>
        <planeGeometry args={[piece.width * 0.92, piece.height * 0.78]} />
        <meshStandardMaterial map={glyphs} roughness={0.8} metalness={0} />
      </mesh>
      {Array.from({ length: merlons }, (_, i) => {
        const x = -piece.width / 2 + ((i + 0.5) * piece.width) / merlons;
        return (
          <mesh key={i} position={[x, piece.height + 0.28, 0]} castShadow>
            <boxGeometry args={[piece.width / merlons * 0.62, 0.56, depth * 0.92]} />
            <Sandstone map={stone} color="#efd0a4" />
          </mesh>
        );
      })}
    </group>
  );
}

function Pillar({ piece, stone }: { piece: TombPiece; stone: THREE.Texture | null }) {
  const r = piece.radius;
  return (
    <group position={[piece.x, 0, piece.z]}>
      <mesh position={[0, 0.18, 0]} castShadow receiveShadow>
        <cylinderGeometry args={[r * 1.25, r * 1.35, 0.36, 12]} />
        <Sandstone map={stone} color="#c99668" />
      </mesh>
      <mesh position={[0, piece.height / 2, 0]} castShadow receiveShadow>
        <cylinderGeometry args={[r * 0.82, r, piece.height - 0.5, 12]} />
        <Sandstone map={stone} />
      </mesh>
      <mesh position={[0, piece.height - 0.16, 0]} castShadow>
        <cylinderGeometry args={[r * 1.2, r * 0.9, 0.38, 12]} />
        <Sandstone map={stone} color="#efd0a4" />
      </mesh>
    </group>
  );
}

function Boulder({ piece, stone }: { piece: TombPiece; stone: THREE.Texture | null }) {
  const r = piece.radius;
  return (
    <mesh position={[piece.x, r * 0.62, piece.z]} scale={[r, r * 0.78, r * 0.92]} castShadow receiveShadow>
      <sphereGeometry args={[1, 20, 14]} />
      <Sandstone map={stone} color="#e8c49a" roughness={0.92} />
    </mesh>
  );
}

function RimBlock({ piece, stone }: { piece: TombPiece; stone: THREE.Texture | null }) {
  const r = piece.radius;
  const yScale = 0.62;
  return (
    <mesh
      position={[piece.x, r * yScale * 0.72, piece.z]}
      rotation={[0, piece.angle, 0]}
      scale={[r, r * yScale, r * 0.9]}
      castShadow
      receiveShadow
    >
      <sphereGeometry args={[1, 22, 16]} />
      <Sandstone map={stone} color="#e7c296" roughness={0.9} />
    </mesh>
  );
}

function Brazier({ piece }: { piece: TombPiece }) {
  return (
    <group position={[piece.x, 0, piece.z]}>
      <mesh position={[0, 0.45, 0]} castShadow>
        <cylinderGeometry args={[0.28, 0.38, 0.9, 10]} />
        <meshStandardMaterial color="#b88860" roughness={0.9} />
      </mesh>
      <mesh position={[0, 0.95, 0]}>
        <cylinderGeometry args={[0.42, 0.34, 0.22, 12]} />
        <meshStandardMaterial color="#8a5a3a" roughness={0.85} />
      </mesh>
      <mesh position={[0, 1.12, 0]}>
        <sphereGeometry args={[0.2, 12, 10]} />
        <meshStandardMaterial color="#ff9a32" emissive="#ff6a10" emissiveIntensity={2.2} />
      </mesh>
      <pointLight position={[0, 1.4, 0]} color="#ff9a3c" intensity={6} distance={11} decay={2} />
    </group>
  );
}

/**
 * Wraps' home court from the comic: a warm sandstone ring, hieroglyph walls
 * and rounded blocks. The floor radius is TOMB_STAGE.knockoffRadius.
 */
export function PharaohTombArena() {
  const sand = useRepeatMap(SAND_URL, 7, 7);
  const stone = useRepeatMap(STONE_URL, 1.4, 1.4);
  const glyphs = useMemo(() => carvedGlyphs(), []);
  const blocks = useMemo(() => getTombBlocks(), []);
  const backdrop = useMemo(() => getTombBackdrop(), []);
  const floor = TOMB_STAGE.knockoffRadius;

  return (
    <group>
      <mesh position={[0, -0.22, 0]} receiveShadow>
        <cylinderGeometry args={[floor, floor * 0.98, 0.44, 80]} />
        <meshStandardMaterial map={sand ?? undefined} color="#e0b88a" roughness={0.84} metalness={0.02} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.012, 0]}>
        <ringGeometry args={[floor - 0.7, floor - 0.08, 80]} />
        <meshStandardMaterial color="#a87848" roughness={0.9} />
      </mesh>

      {blocks.map((piece) => (
        <RimBlock key={piece.id} piece={piece} stone={stone} />
      ))}

      {backdrop.map((piece) => {
        if (piece.kind === "wall") return <RuinWall key={piece.id} piece={piece} stone={stone} glyphs={glyphs} />;
        if (piece.kind === "pillar") return <Pillar key={piece.id} piece={piece} stone={stone} />;
        if (piece.kind === "boulder") return <Boulder key={piece.id} piece={piece} stone={stone} />;
        return <Brazier key={piece.id} piece={piece} />;
      })}
    </group>
  );
}

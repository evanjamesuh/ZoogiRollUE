import * as THREE from "three";
import { useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { TOMB_STAGE, getMapLayout, getTombBackdrop, getTombBlocks, type TombPiece } from "@/lib/arenaColliders";

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

function useSharedMaterial(
  map: THREE.Texture | null,
  color: string,
  roughness: number,
  metalness = 0.02,
) {
  const material = useMemo(
    () => new THREE.MeshStandardMaterial({ color, roughness, metalness }),
    [color, roughness, metalness],
  );
  useEffect(() => {
    material.map = map;
    material.needsUpdate = true;
  }, [material, map]);
  useEffect(() => () => material.dispose(), [material]);
  return material;
}

function SharedMaterial({ material }: { material: THREE.Material }) {
  return <primitive object={material} attach="material" dispose={null} />;
}

const BRAZIER_STEM = new THREE.MeshStandardMaterial({ color: "#b88860", roughness: 0.9 });
const BRAZIER_BOWL = new THREE.MeshStandardMaterial({ color: "#8a5a3a", roughness: 0.85 });
const BRAZIER_FLAME = new THREE.MeshStandardMaterial({
  color: "#ff9a32",
  emissive: "#ff6a10",
  emissiveIntensity: 2.2,
});

const MERLONS_PER_WALL = 5;

function RuinWall({
  piece,
  stone,
  glyphs,
}: {
  piece: TombPiece;
  stone: THREE.Material;
  glyphs: THREE.Material;
}) {
  const depth = piece.radius * 2;
  const rot = -Math.PI / 2 - piece.angle;
  return (
    <group position={[piece.x, 0, piece.z]} rotation={[0, rot, 0]}>
      <mesh position={[0, piece.height / 2, 0]} receiveShadow>
        <boxGeometry args={[piece.width, piece.height, depth]} />
        <SharedMaterial material={stone} />
      </mesh>
      <mesh position={[0, piece.height / 2, depth / 2 + 0.04]}>
        <planeGeometry args={[piece.width * 0.92, piece.height * 0.78]} />
        <SharedMaterial material={glyphs} />
      </mesh>
    </group>
  );
}

function WallMerlons({ walls, material }: { walls: TombPiece[]; material: THREE.Material }) {
  const ref = useRef<THREE.InstancedMesh>(null);
  const geometry = useMemo(() => {
    const sample = walls[0];
    const depth = (sample?.radius ?? 0.7) * 2;
    const width = sample?.width ?? 6.4;
    return new THREE.BoxGeometry((width / MERLONS_PER_WALL) * 0.62, 0.56, depth * 0.92);
  }, [walls]);
  useEffect(() => () => geometry.dispose(), [geometry]);

  useLayoutEffect(() => {
    const mesh = ref.current;
    if (!mesh) return;
    const dummy = new THREE.Object3D();
    const parent = new THREE.Object3D();
    let i = 0;
    for (const piece of walls) {
      parent.position.set(piece.x, 0, piece.z);
      parent.rotation.set(0, -Math.PI / 2 - piece.angle, 0);
      parent.updateMatrix();
      for (let m = 0; m < MERLONS_PER_WALL; m += 1) {
        const x = -piece.width / 2 + ((m + 0.5) * piece.width) / MERLONS_PER_WALL;
        dummy.position.set(x, piece.height + 0.28, 0);
        dummy.rotation.set(0, 0, 0);
        dummy.scale.set(1, 1, 1);
        dummy.updateMatrix();
        mesh.setMatrixAt(i, dummy.matrix.premultiply(parent.matrix));
        i += 1;
      }
    }
    mesh.count = i;
    mesh.instanceMatrix.needsUpdate = true;
  }, [walls]);

  if (walls.length === 0) return null;
  return (
    <instancedMesh
      ref={ref}
      args={[geometry, material, walls.length * MERLONS_PER_WALL]}
      frustumCulled={false}
      dispose={null}
    />
  );
}

function Pillar({
  piece,
  shaft,
  base,
  cap,
}: {
  piece: TombPiece;
  shaft: THREE.Material;
  base: THREE.Material;
  cap: THREE.Material;
}) {
  const r = piece.radius;
  return (
    <group position={[piece.x, 0, piece.z]}>
      <mesh position={[0, 0.18, 0]} receiveShadow>
        <cylinderGeometry args={[r * 1.25, r * 1.35, 0.36, 12]} />
        <SharedMaterial material={base} />
      </mesh>
      <mesh position={[0, piece.height / 2, 0]} receiveShadow>
        <cylinderGeometry args={[r * 0.82, r, piece.height - 0.5, 12]} />
        <SharedMaterial material={shaft} />
      </mesh>
      <mesh position={[0, piece.height - 0.16, 0]}>
        <cylinderGeometry args={[r * 1.2, r * 0.9, 0.38, 12]} />
        <SharedMaterial material={cap} />
      </mesh>
    </group>
  );
}

function BackdropBoulder({ piece, material }: { piece: TombPiece; material: THREE.Material }) {
  const r = piece.radius;
  return (
    <mesh
      position={[piece.x, r * 0.2, piece.z]}
      rotation={[0.4, -piece.angle + 0.6, 0.25]}
      scale={[r * 1.2, r * 0.62, r * 0.95]}
      receiveShadow
    >
      <dodecahedronGeometry args={[1, 0]} />
      <SharedMaterial material={material} />
    </mesh>
  );
}

function RimBlock({ piece, material }: { piece: TombPiece; material: THREE.Material }) {
  const r = piece.radius;
  const yScale = 0.62;
  return (
    <mesh
      position={[piece.x, r * yScale * 0.72, piece.z]}
      rotation={[0, -piece.angle, 0]}
      scale={[r, r * yScale, r]}
      castShadow
      receiveShadow
    >
      <sphereGeometry args={[1, 22, 16]} />
      <SharedMaterial material={material} />
    </mesh>
  );
}

function Brazier({ piece }: { piece: TombPiece }) {
  return (
    <group position={[piece.x, 0, piece.z]}>
      <mesh position={[0, 0.45, 0]}>
        <cylinderGeometry args={[0.28, 0.38, 0.9, 10]} />
        <SharedMaterial material={BRAZIER_STEM} />
      </mesh>
      <mesh position={[0, 0.95, 0]}>
        <cylinderGeometry args={[0.42, 0.34, 0.22, 12]} />
        <SharedMaterial material={BRAZIER_BOWL} />
      </mesh>
      <mesh position={[0, 1.12, 0]}>
        <sphereGeometry args={[0.2, 12, 10]} />
        <SharedMaterial material={BRAZIER_FLAME} />
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
  const glyphMap = useMemo(() => carvedGlyphs(), []);
  const blocks = useMemo(() => {
    const placed = new Map((getMapLayout("tomb")?.scenery ?? []).map((solid) => [solid.id, solid]));
    return getTombBlocks().map((block) => {
      const solid = placed.get(block.id);
      return solid ? { ...block, x: solid.x, z: solid.z, radius: solid.radius } : block;
    });
  }, []);
  const backdrop = useMemo(() => getTombBackdrop(), []);
  const walls = useMemo(() => backdrop.filter((piece) => piece.kind === "wall"), [backdrop]);
  const floor = getMapLayout("tomb")?.knockoffRadius ?? TOMB_STAGE.knockoffRadius;
  const shaftMat = useSharedMaterial(stone, "#e4c092", 0.88);
  const capMat = useSharedMaterial(stone, "#efd0a4", 0.88);
  const baseMat = useSharedMaterial(stone, "#c99668", 0.88);
  const rimMat = useSharedMaterial(stone, "#e7c296", 0.9);
  const rubbleMat = useSharedMaterial(stone, "#6a5344", 1);
  const glyphMat = useSharedMaterial(glyphMap, "#ffffff", 0.8, 0);
  const floorMat = useSharedMaterial(sand, "#e0b88a", 0.84);
  const lipMat = useSharedMaterial(null, "#a87848", 0.9, 0);

  return (
    <group>
      <mesh position={[0, -0.22, 0]} receiveShadow>
        <cylinderGeometry args={[floor, floor * 0.98, 0.44, 80]} />
        <SharedMaterial material={floorMat} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.012, 0]}>
        <ringGeometry args={[floor - 0.7, floor - 0.08, 80]} />
        <SharedMaterial material={lipMat} />
      </mesh>

      {blocks.map((piece) => (
        <RimBlock key={piece.id} piece={piece} material={rimMat} />
      ))}

      <WallMerlons walls={walls} material={capMat} />

      {backdrop.map((piece) => {
        if (piece.kind === "wall") return <RuinWall key={piece.id} piece={piece} stone={shaftMat} glyphs={glyphMat} />;
        if (piece.kind === "pillar") {
          return <Pillar key={piece.id} piece={piece} shaft={shaftMat} base={baseMat} cap={capMat} />;
        }
        if (piece.kind === "boulder") return <BackdropBoulder key={piece.id} piece={piece} material={rubbleMat} />;
        return <Brazier key={piece.id} piece={piece} />;
      })}
    </group>
  );
}

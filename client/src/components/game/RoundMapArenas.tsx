import * as THREE from "three";
import { Component, ReactNode, Suspense, useEffect, useLayoutEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { useAnimations, useGLTF, useTexture } from "@react-three/drei";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import {
  WINTER_STAGE,
  arabianPlayTransform,
  getMapLayout,
  setWinterCampActive,
  type SolidCircle,
} from "@/lib/arenaColliders";
import { GRASS_RIM, ROUND_FLOOR_RADIUS, ROUND_KNOCKOFF_RADIUS, rimPosition, type RimMark } from "@/lib/roundRim";
import { RING_VISUAL_LIMIT, ringPieceAction, type Aabb } from "@/lib/ringPlacement";
import { MEADOW_BACKDROP_RADIUS, MEADOW_GROUND_RADIUS, MEADOW_GROUND_Y, forestTopY, meadowForestPieces, type ForestPiece } from "@/lib/meadowDressing";
import { FrozenRinkMarkings } from "./FrozenRinkMarkings";

const EDGE = ROUND_KNOCKOFF_RADIUS;
const RIM_INNER = 14.7;

class ModelErrorBoundary extends Component<{ children: ReactNode }, { hasError: boolean }> {
  constructor(props: { children: ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() {
    return { hasError: true };
  }
  render() {
    return this.state.hasError ? null : this.props.children;
  }
}

function seeded(seed: number) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

function paintedTexture(
  draw: (ctx: CanvasRenderingContext2D, w: number, h: number) => void,
  repeatX: number,
  repeatY: number,
  size = 512,
): THREE.CanvasTexture {
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d");
  if (ctx) draw(ctx, size, size);
  const tex = new THREE.CanvasTexture(canvas);
  tex.colorSpace = THREE.SRGBColorSpace;
  tex.wrapS = THREE.RepeatWrapping;
  tex.wrapT = THREE.RepeatWrapping;
  tex.repeat.set(repeatX, repeatY);
  tex.anisotropy = 8;
  return tex;
}

function grassDraw(ctx: CanvasRenderingContext2D, w: number, h: number) {
  const rand = seeded(3);
  ctx.fillStyle = "#3e9d48";
  ctx.fillRect(0, 0, w, h);
  for (let i = 0; i < 2200; i++) {
    const x = rand() * w;
    const y = rand() * h;
    const g = 100 + Math.floor(rand() * 120);
    ctx.fillStyle = `rgba(${50 + rand() * 40}, ${g}, ${40 + rand() * 40}, 0.45)`;
    ctx.fillRect(x, y, 2 + rand() * 7, 4 + rand() * 10);
  }
}

function soilDraw(ctx: CanvasRenderingContext2D, w: number, h: number) {
  const rand = seeded(8);
  ctx.fillStyle = "#6d5138";
  ctx.fillRect(0, 0, w, h);
  for (let i = 0; i < 900; i++) {
    ctx.fillStyle = `rgba(${80 + rand() * 60}, ${50 + rand() * 30}, ${24 + rand() * 20}, 0.45)`;
    ctx.fillRect(rand() * w, rand() * h, 2 + rand() * 8, 2 + rand() * 4);
  }
}

function rockDraw(ctx: CanvasRenderingContext2D, w: number, h: number) {
  const rand = seeded(12);
  ctx.fillStyle = "#7a6756";
  ctx.fillRect(0, 0, w, h);
  for (let i = 0; i < 1400; i++) {
    const shade = 70 + Math.floor(rand() * 90);
    ctx.fillStyle = `rgba(${shade}, ${shade - 12}, ${shade - 24}, 0.55)`;
    ctx.beginPath();
    ctx.arc(rand() * w, rand() * h, 1 + rand() * 4, 0, Math.PI * 2);
    ctx.fill();
  }
}

function iceDraw(ctx: CanvasRenderingContext2D, w: number, h: number) {
  const rand = seeded(21);
  ctx.fillStyle = "#c5ecff";
  ctx.fillRect(0, 0, w, h);
  for (let i = 0; i < 40; i++) {
    ctx.strokeStyle = `rgba(255, 255, 255, ${0.25 + rand() * 0.4})`;
    ctx.lineWidth = 2 + rand() * 4;
    ctx.beginPath();
    ctx.moveTo(rand() * w, rand() * h);
    ctx.bezierCurveTo(rand() * w, rand() * h, rand() * w, rand() * h, rand() * w, rand() * h);
    ctx.stroke();
  }
  for (let i = 0; i < 18; i++) {
    ctx.strokeStyle = `rgba(90, 150, 190, ${0.25 + rand() * 0.3})`;
    ctx.lineWidth = 1;
    ctx.beginPath();
    const x = rand() * w;
    const y = rand() * h;
    ctx.moveTo(x, y);
    ctx.lineTo(x + rand() * 80 - 40, y + rand() * 30 - 15);
    ctx.stroke();
  }
}

function snowDraw(ctx: CanvasRenderingContext2D, w: number, h: number) {
  const rand = seeded(27);
  ctx.fillStyle = "#f7fbff";
  ctx.fillRect(0, 0, w, h);
  for (let i = 0; i < 700; i++) {
    ctx.fillStyle = `rgba(${180 + rand() * 60}, ${200 + rand() * 55}, 230, 0.35)`;
    ctx.beginPath();
    ctx.arc(rand() * w, rand() * h, 2 + rand() * 10, 0, Math.PI * 2);
    ctx.fill();
  }
}

function tileDraw(ctx: CanvasRenderingContext2D, w: number, h: number) {
  ctx.fillStyle = "#e4c48e";
  ctx.fillRect(0, 0, w, h);
  const cell = 128;
  for (let y = 0; y < h; y += cell) {
    for (let x = 0; x < w; x += cell) {
      ctx.fillStyle = "#c99558";
      ctx.beginPath();
      ctx.moveTo(x + cell / 2, y + 16);
      ctx.lineTo(x + cell - 16, y + cell / 2);
      ctx.lineTo(x + cell / 2, y + cell - 16);
      ctx.lineTo(x + 16, y + cell / 2);
      ctx.closePath();
      ctx.fill();
      ctx.strokeStyle = "#6d4528";
      ctx.lineWidth = 5;
      ctx.stroke();
      ctx.strokeStyle = "#8d6440";
      ctx.lineWidth = 3;
      ctx.strokeRect(x + 8, y + 8, cell - 16, cell - 16);
      ctx.fillStyle = "#1a6d72";
      ctx.beginPath();
      ctx.arc(x + cell / 2, y + cell / 2, 9, 0, Math.PI * 2);
      ctx.fill();
    }
  }
}

function useFloorTextures(kind: "grass" | "ice" | "saturn") {
  return useMemo(() => {
    if (kind === "grass") {
      return {
        top: paintedTexture(grassDraw, 5, 5),
        side: paintedTexture(rockDraw, 10, 2),
        rim: paintedTexture(soilDraw, 8, 1),
      };
    }
    if (kind === "ice") {
      return {
        top: paintedTexture(iceDraw, 4, 4),
        side: paintedTexture(snowDraw, 8, 2),
        rim: paintedTexture(snowDraw, 6, 1),
      };
    }
    return {
      top: paintedTexture(tileDraw, 4, 4),
      side: paintedTexture(soilDraw, 8, 2),
      rim: paintedTexture(tileDraw, 6, 1),
    };
  }, [kind]);
}

function RoundIsland({
  kind,
  lip,
  lipEmissive,
  underside,
  sideColor,
}: {
  kind: "grass" | "ice" | "saturn";
  lip: string;
  lipEmissive?: string;
  underside: string;
  sideColor: string;
}) {
  const maps = useFloorTextures(kind);
  return (
    <group>
      <mesh position={[0, -1.35, 0]} castShadow receiveShadow>
        <cylinderGeometry args={[EDGE, EDGE + 1.2, 2.7, 80, 1, true]} />
        <meshStandardMaterial map={maps.side} color={sideColor} roughness={0.95} side={THREE.DoubleSide} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]} receiveShadow>
        <circleGeometry args={[EDGE, 80]} />
        <meshStandardMaterial
          map={maps.top}
          color={kind === "saturn" ? "#ffe7c4" : "#ffffff"}
          emissive={kind === "saturn" ? "#3a2410" : "#000000"}
          emissiveIntensity={kind === "saturn" ? 0.2 : 0}
          roughness={kind === "ice" ? 0.35 : 0.86}
          metalness={kind === "ice" ? 0.08 : 0}
        />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.045, 0]}>
        <ringGeometry args={[RIM_INNER, EDGE - 0.06, 80]} />
        <meshStandardMaterial map={maps.rim} color={kind === "ice" ? "#ffffff" : "#ffffff"} roughness={0.9} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.06, 0]}>
        <ringGeometry args={[EDGE - 0.32, EDGE - 0.02, 96]} />
        <meshStandardMaterial
          color={lip}
          emissive={lipEmissive ?? "#000000"}
          emissiveIntensity={lipEmissive ? 0.85 : 0}
          roughness={0.55}
          metalness={lipEmissive ? 0.25 : 0}
        />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -2.7, 0]}>
        <circleGeometry args={[EDGE + 1.2, 40]} />
        <meshStandardMaterial color={underside} roughness={1} />
      </mesh>
    </group>
  );
}

function sceneryOf(mapId: string): SolidCircle[] {
  return getMapLayout(mapId)?.scenery ?? [];
}

function MeadowFlowers() {
  const flowers = useMemo(() => {
    const rand = seeded(41);
    const colors = ["#ff5fa2", "#ffd24a", "#fff4ea", "#7ec8ff", "#ff7a3c"];
    return Array.from({ length: 26 }, (_, i) => {
      const angle = rand() * Math.PI * 2;
      const dist = 16.45 + rand() * 1.15;
      return {
        id: `flower-${i}`,
        x: Math.cos(angle) * dist,
        z: Math.sin(angle) * dist,
        color: colors[i % colors.length],
        scale: 0.65 + rand() * 0.55,
        rot: rand() * Math.PI,
      };
    });
  }, []);

  return (
    <group>
      {flowers.map((flower) => (
        <group key={flower.id} position={[flower.x, MEADOW_GROUND_Y, flower.z]} rotation={[0, flower.rot, 0]} scale={flower.scale}>
          <mesh position={[0, 0.16, 0]}>
            <cylinderGeometry args={[0.03, 0.03, 0.32, 5]} />
            <meshStandardMaterial color="#2f6a32" />
          </mesh>
          <mesh position={[0, 0.34, 0]}>
            <sphereGeometry args={[0.1, 8, 8]} />
            <meshStandardMaterial color={flower.color} />
          </mesh>
        </group>
      ))}
    </group>
  );
}

function useTiledGrass(repeat: number) {
  const source = useTexture("/textures/grass.png");
  const map = useMemo(() => {
    const tex = source.clone();
    tex.wrapS = THREE.RepeatWrapping;
    tex.wrapT = THREE.RepeatWrapping;
    tex.repeat.set(repeat, repeat);
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.anisotropy = 8;
    tex.needsUpdate = true;
    return tex;
  }, [source, repeat]);
  return map;
}

function MeadowFloor() {
  const grass = useTiledGrass(7);
  const outer = useTiledGrass(80);
  return (
    <group>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]} receiveShadow>
        <circleGeometry args={[ROUND_FLOOR_RADIUS, 96]} />
        <meshStandardMaterial map={grass} color="#ffffff" roughness={0.94} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.055, 0]} receiveShadow>
        <ringGeometry args={[ROUND_FLOOR_RADIUS - 0.18, ROUND_KNOCKOFF_RADIUS, 96]} />
        <meshStandardMaterial color="#d7e7a4" roughness={0.82} />
      </mesh>
      <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 0.04, 0]} castShadow receiveShadow>
        <torusGeometry args={[(ROUND_FLOOR_RADIUS + ROUND_KNOCKOFF_RADIUS) / 2, 0.16, 10, 80]} />
        <meshStandardMaterial color="#e7f0c2" roughness={0.78} />
      </mesh>
      <mesh position={[0, MEADOW_GROUND_Y / 2, 0]} receiveShadow>
        <cylinderGeometry args={[ROUND_KNOCKOFF_RADIUS, ROUND_KNOCKOFF_RADIUS + 0.22, -MEADOW_GROUND_Y, 80, 1, true]} />
        <meshStandardMaterial color="#6d8a3c" roughness={1} side={THREE.DoubleSide} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, MEADOW_GROUND_Y, 0]} receiveShadow>
        <circleGeometry args={[MEADOW_GROUND_RADIUS, 96]} />
        <meshStandardMaterial map={outer} color="#e4efc0" roughness={1} />
      </mesh>
    </group>
  );
}

/** Mossy boulder or stump whose widest point is the rim collider. */
function MeadowRimPiece({ mark, stump }: { mark: RimMark; stump: boolean }) {
  const { x, z } = rimPosition(mark);
  const moss = "#63b34a";
  if (stump) {
    const cap = 0.16;
    const body = Math.max(0.45, mark.height - cap);
    return (
      <group position={[x, 0, z]} rotation={[0, (mark.angleDeg * Math.PI) / 180, 0]}>
        <mesh position={[0, body / 2, 0]} castShadow receiveShadow>
          <cylinderGeometry args={[mark.radius * 0.78, mark.radius * 0.9, body, 8]} />
          <meshStandardMaterial color="#6b4630" roughness={0.92} />
        </mesh>
        <mesh position={[0, body + cap / 2, 0]} castShadow receiveShadow>
          <cylinderGeometry args={[mark.radius, mark.radius * 0.94, cap, 10]} />
          <meshStandardMaterial color="#8a5a38" roughness={0.88} />
        </mesh>
        <mesh position={[0.12, body + cap + 0.08, 0.05]} scale={[1, 0.45, 1]} castShadow>
          <sphereGeometry args={[mark.radius * 0.42, 8, 6]} />
          <meshStandardMaterial color={moss} roughness={0.8} />
        </mesh>
      </group>
    );
  }
  return (
    <group position={[x, 0, z]} rotation={[0, (mark.angleDeg * Math.PI) / 180, 0.08]}>
      <mesh position={[0, mark.radius * 0.38, 0]} scale={[1, 0.62, 1]} castShadow receiveShadow>
        <sphereGeometry args={[mark.radius, 14, 10]} />
        <meshStandardMaterial color="#7d7368" roughness={0.94} />
      </mesh>
      <mesh position={[0.05, mark.radius * 0.72, 0.02]} scale={[1, 0.5, 0.9]} castShadow>
        <sphereGeometry args={[mark.radius * 0.55, 10, 8]} />
        <meshStandardMaterial color={moss} roughness={0.82} />
      </mesh>
    </group>
  );
}

function MeadowRim() {
  return (
    <group>
      {GRASS_RIM.map((mark, i) => (
        <MeadowRimPiece key={mark.id} mark={mark} stump={i % 2 === 1} />
      ))}
    </group>
  );
}

const FOREST_BANDS: { max: number; leaf: string; shade: string; bark: string }[] = [
  { max: 24, leaf: "#3eae4c", shade: "#2f8a3c", bark: "#6a452c" },
  { max: 42, leaf: "#6aaa78", shade: "#568c68", bark: "#746048" },
  { max: 80, leaf: "#9cbfa8", shade: "#8aaf9c", bark: "#8a9484" },
  { max: 120, leaf: "#c5d8d4", shade: "#b7cdc8", bark: "#b0c0c0" },
  { max: 999, leaf: "#d7e7f0", shade: "#cfe3ee", bark: "#d0e0ea" },
];

function ForestBand({
  pieces,
  leaf,
  shade,
  bark,
  shadow,
}: {
  pieces: ForestPiece[];
  leaf: string;
  shade: string;
  bark: string;
  shadow: boolean;
}) {
  const canopyRef = useRef<THREE.InstancedMesh>(null);
  const puffRef = useRef<THREE.InstancedMesh>(null);
  const trunkRef = useRef<THREE.InstancedMesh>(null);
  const trees = useMemo(() => pieces.filter((piece) => piece.kind === "tree"), [pieces]);

  useLayoutEffect(() => {
    const dummy = new THREE.Object3D();
    pieces.forEach((piece, i) => {
      const centerY = forestTopY(piece) - piece.canopy;
      dummy.position.set(piece.x, centerY, piece.z);
      dummy.rotation.set(0, piece.rot, 0);
      dummy.scale.setScalar(piece.canopy);
      dummy.updateMatrix();
      canopyRef.current?.setMatrixAt(i, dummy.matrix);
      const outward = Math.hypot(piece.x, piece.z) || 1;
      dummy.position.set(
        piece.x + (piece.x / outward) * piece.canopy * 0.22,
        centerY - piece.canopy * 0.42,
        piece.z + (piece.z / outward) * piece.canopy * 0.22,
      );
      dummy.scale.setScalar(piece.canopy * 0.55);
      dummy.updateMatrix();
      puffRef.current?.setMatrixAt(i, dummy.matrix);
    });
    if (canopyRef.current) canopyRef.current.instanceMatrix.needsUpdate = true;
    if (puffRef.current) puffRef.current.instanceMatrix.needsUpdate = true;
    trees.forEach((piece, i) => {
      dummy.position.set(piece.x, MEADOW_GROUND_Y + piece.trunk / 2, piece.z);
      dummy.rotation.set(0, 0, 0);
      dummy.scale.set(0.24, piece.trunk, 0.24);
      dummy.updateMatrix();
      trunkRef.current?.setMatrixAt(i, dummy.matrix);
    });
    if (trunkRef.current) trunkRef.current.instanceMatrix.needsUpdate = true;
  }, [pieces, trees]);

  if (pieces.length === 0) return null;
  return (
    <group>
      <instancedMesh ref={canopyRef} args={[undefined, undefined, pieces.length]} castShadow={shadow} receiveShadow>
        <sphereGeometry args={[1, 8, 6]} />
        <meshStandardMaterial color={leaf} roughness={0.88} />
      </instancedMesh>
      <instancedMesh ref={puffRef} args={[undefined, undefined, pieces.length]} castShadow={shadow} receiveShadow>
        <sphereGeometry args={[1, 7, 5]} />
        <meshStandardMaterial color={shade} roughness={0.9} />
      </instancedMesh>
      {trees.length > 0 && (
        <instancedMesh ref={trunkRef} args={[undefined, undefined, trees.length]} castShadow={shadow} receiveShadow>
          <cylinderGeometry args={[1, 1.15, 1, 6]} />
          <meshStandardMaterial color={bark} roughness={0.94} />
        </instancedMesh>
      )}
    </group>
  );
}

function MeadowForest() {
  const bands = useMemo(() => {
    const pieces = meadowForestPieces();
    return FOREST_BANDS.map((band, index) => {
      const min = index === 0 ? 0 : FOREST_BANDS[index - 1].max;
      return {
        ...band,
        pieces: pieces.filter((piece) => {
          const dist = Math.hypot(piece.x, piece.z);
          return dist >= min && dist < band.max;
        }),
      };
    });
  }, []);
  return (
    <group>
      {bands.map((band) => (
        <ForestBand
          key={band.max}
          pieces={band.pieces}
          leaf={band.leaf}
          shade={band.shade}
          bark={band.bark}
          shadow={band.max <= 42}
        />
      ))}
    </group>
  );
}

function MeadowBackdrop() {
  const source = useTexture("/textures/meadow_background.png");
  const map = useMemo(() => {
    const image = source.image as CanvasImageSource & { width: number; height: number };
    const w = image.width;
    const h = image.height;
    const skyCap = Math.round(h * 0.55);
    const groundCap = Math.round(h * 0.28);
    const canvas = document.createElement("canvas");
    canvas.width = w * 2;
    canvas.height = skyCap + h + groundCap;
    const ctx = canvas.getContext("2d");
    if (ctx) {
      const sky = "#7ecbf5";
      const earth = "#d7ebf8";
      ctx.fillStyle = sky;
      ctx.fillRect(0, 0, canvas.width, canvas.height);
      const draw = (dx: number, flip: boolean) => {
        ctx.save();
        if (flip) {
          ctx.translate(dx + w, 0);
          ctx.scale(-1, 1);
          ctx.drawImage(image, 0, skyCap, w, h);
        } else {
          ctx.drawImage(image, dx, skyCap, w, h);
        }
        ctx.restore();
      };
      draw(0, false);
      draw(w, true);
      const groundFade = ctx.createLinearGradient(0, canvas.height, 0, skyCap + h * 0.62);
      groundFade.addColorStop(0, earth);
      groundFade.addColorStop(0.45, "rgba(215, 235, 248, 0.75)");
      groundFade.addColorStop(1, "rgba(215, 235, 248, 0)");
      ctx.fillStyle = groundFade;
      ctx.fillRect(0, skyCap + h * 0.62, canvas.width, canvas.height - (skyCap + h * 0.62));
      const skyFade = ctx.createLinearGradient(0, 0, 0, skyCap + h * 0.38);
      skyFade.addColorStop(0, sky);
      skyFade.addColorStop(0.42, sky);
      skyFade.addColorStop(1, "rgba(126, 203, 245, 0)");
      ctx.fillStyle = skyFade;
      ctx.fillRect(0, 0, canvas.width, skyCap + h * 0.38);
    }
    const tex = new THREE.CanvasTexture(canvas);
    tex.colorSpace = THREE.SRGBColorSpace;
    tex.wrapS = THREE.RepeatWrapping;
    tex.wrapT = THREE.ClampToEdgeWrapping;
    const height = 108;
    const tileWorldW = height * (canvas.width / canvas.height);
    tex.repeat.set((Math.PI * 2 * MEADOW_BACKDROP_RADIUS) / tileWorldW, 1);
    tex.anisotropy = 8;
    tex.needsUpdate = true;
    return tex;
  }, [source]);

  const height = 108;
  return (
    <mesh position={[0, -6 + height / 2, 0]}>
      <cylinderGeometry args={[MEADOW_BACKDROP_RADIUS, MEADOW_BACKDROP_RADIUS, height, 96, 1, true]} />
      <meshBasicMaterial map={map} side={THREE.BackSide} toneMapped={false} />
    </mesh>
  );
}

function ArabianPlanters() {
  const pots = useMemo(() => sceneryOf("saturn").filter((solid) => solid.kind === "prop"), []);
  return (
    <group>
      {pots.map((pot) => (
        <group key={pot.id} position={[pot.x, 0, pot.z]}>
          <mesh position={[0, 0.42, 0]} castShadow receiveShadow>
            <cylinderGeometry args={[pot.radius * 0.82, pot.radius, 0.84, 12]} />
            <meshStandardMaterial color="#7c3b2c" roughness={0.82} />
          </mesh>
          <mesh position={[0, 0.88, 0]} castShadow>
            <cylinderGeometry args={[pot.radius * 0.96, pot.radius * 0.78, 0.14, 12]} />
            <meshStandardMaterial color="#e6c27a" emissive="#ffb15a" emissiveIntensity={0.35} roughness={0.5} metalness={0.2} />
          </mesh>
          <mesh position={[0, 1.08, 0]} castShadow>
            <sphereGeometry args={[pot.radius * 0.38, 10, 8]} />
            <meshStandardMaterial color="#1f7a4a" roughness={0.7} />
          </mesh>
        </group>
      ))}
    </group>
  );
}

function toAabb(box: THREE.Box3): Aabb {
  return {
    minX: box.min.x,
    minY: box.min.y,
    minZ: box.min.z,
    maxX: box.max.x,
    maxY: box.max.y,
    maxZ: box.max.z,
  };
}

function applyWorldDelta(object: THREE.Object3D, dx: number, dy: number, dz: number) {
  const parent = object.parent;
  if (!parent) {
    object.position.x += dx;
    object.position.y += dy;
    object.position.z += dz;
    return;
  }
  parent.updateWorldMatrix(true, false);
  const inverse = new THREE.Matrix4().copy(parent.matrixWorld).invert();
  const origin = new THREE.Vector3(0, 0, 0).applyMatrix4(inverse);
  const moved = new THREE.Vector3(dx, dy, dz).applyMatrix4(inverse);
  object.position.x += moved.x - origin.x;
  object.position.y += moved.y - origin.y;
  object.position.z += moved.z - origin.z;
}

interface UnitMeasure {
  minR: number;
  minY: number;
  cx: number;
  cz: number;
  box: THREE.Box3;
}

function measureUnit(object: THREE.Object3D): UnitMeasure | null {
  object.updateWorldMatrix(true, true);
  const box = new THREE.Box3().setFromObject(object);
  if (box.isEmpty()) return null;
  let minR = Infinity;
  let minY = Infinity;
  let sx = 0;
  let sz = 0;
  let n = 0;
  const v = new THREE.Vector3();
  object.traverse((child) => {
    const mesh = child as THREE.Mesh;
    if (!mesh.visible || !mesh.isMesh) return;
    const pos = mesh.geometry?.getAttribute("position") as THREE.BufferAttribute | undefined;
    if (!pos) return;
    for (let i = 0; i < pos.count; i++) {
      v.fromBufferAttribute(pos, i).applyMatrix4(mesh.matrixWorld);
      const r = Math.hypot(v.x, v.z);
      if (r < minR) minR = r;
      if (v.y < minY) minY = v.y;
      sx += v.x;
      sz += v.z;
      n += 1;
    }
  });
  if (n === 0) return null;
  return { minR, minY, cx: sx / n, cz: sz / n, box };
}

function ancestorHidden(object: THREE.Object3D, root: THREE.Object3D): boolean {
  let node: THREE.Object3D | null = object;
  while (node && node !== root) {
    if (!node.visible) return true;
    node = node.parent;
  }
  return false;
}

function clipMesh(mesh: THREE.Mesh, limit: number, fullHeight: boolean) {
  const geom = mesh.geometry;
  const pos = geom.getAttribute("position") as THREE.BufferAttribute | undefined;
  if (!pos) return;
  const index = geom.getIndex();
  const triCount = index ? index.count / 3 : pos.count / 3;
  const keep: number[] = [];
  const v = new THREE.Vector3();
  const bad = (vi: number) => {
    v.fromBufferAttribute(pos, vi).applyMatrix4(mesh.matrixWorld);
    if (Math.hypot(v.x, v.z) >= limit) return false;
    if (fullHeight) return v.y > -1.5;
    return v.y > -0.45 && v.y < 22;
  };
  let removed = 0;
  for (let t = 0; t < triCount; t++) {
    const a = index ? index.getX(t * 3) : t * 3;
    const b = index ? index.getX(t * 3 + 1) : t * 3 + 1;
    const c = index ? index.getX(t * 3 + 2) : t * 3 + 2;
    if (bad(a) || bad(b) || bad(c)) {
      removed += 1;
      continue;
    }
    keep.push(a, b, c);
  }
  if (removed === 0) return;
  if (keep.length === 0) {
    mesh.visible = false;
    return;
  }
  const next = new THREE.BufferGeometry();
  for (const name of Object.keys(geom.attributes)) {
    const attr = geom.getAttribute(name) as THREE.BufferAttribute;
    const item = attr.itemSize;
    const arr = new Float32Array(keep.length * item);
    for (let i = 0; i < keep.length; i++) {
      for (let k = 0; k < item; k++) arr[i * item + k] = attr.getComponent(keep[i], k);
    }
    next.setAttribute(name, new THREE.BufferAttribute(arr, item, attr.normalized));
  }
  mesh.geometry = next;
}

function clipUnit(object: THREE.Object3D, limit: number, fullHeight: boolean) {
  object.updateWorldMatrix(true, true);
  object.traverse((child) => {
    const mesh = child as THREE.Mesh;
    if (!mesh.isMesh || !mesh.visible) return;
    clipMesh(mesh, limit, fullHeight);
  });
}

function pushUnit(object: THREE.Object3D, limit: number) {
  for (let step = 0; step < 6; step++) {
    const measured = measureUnit(object);
    if (!measured || measured.minR >= limit) return;
    let dx = measured.cx;
    let dz = measured.cz;
    const len = Math.hypot(dx, dz);
    if (len < 1e-3) return;
    dx /= len;
    dz /= len;
    const dist = Math.min(48, limit + 0.35 - measured.minR);
    if (dist <= 0.02) return;
    applyWorldDelta(object, dx * dist, 0, dz * dist);
    object.updateWorldMatrix(true, true);
  }
}

function collectUnits(root: THREE.Object3D): THREE.Object3D[] {
  const units: THREE.Object3D[] = [];
  const claimed = new Set<THREE.Object3D>();
  root.traverse((obj) => {
    if (!obj.visible || claimed.has(obj)) return;
    const parent = obj.parent;
    if (parent && /^Tree\d+$/.test(parent.name)) {
      if (!claimed.has(parent)) {
        units.push(parent);
        claimed.add(parent);
      }
      return;
    }
    if (parent?.name === "Lower_base2") {
      if (!claimed.has(parent)) {
        units.push(parent);
        claimed.add(parent);
      }
      return;
    }
    if ((obj as THREE.Mesh).isMesh) units.push(obj);
  });
  return units;
}

function arrangeRing(root: THREE.Object3D, map: "grass" | "ice" | "saturn"): { hid: number; pushed: number; clipped: number; kept: number } | null {
  if (root.userData.ringArranged) return null;
  root.userData.ringArranged = true;
  let hid = 0;
  let pushed = 0;
  let clipped = 0;
  let kept = 0;

  root.traverse((obj) => {
    if (map === "saturn" && obj.name === "FrontSide_2") {
      applyWorldDelta(obj, 0, 3.5, 0);
    }
  });

  for (const unit of collectUnits(root)) {
    if (ancestorHidden(unit, root)) continue;
    const measured = measureUnit(unit);
    if (!measured) continue;
    const aabb = toAabb(measured.box);
    const action = ringPieceAction(unit.name, aabb, measured.minR);
    if (action === "hide") {
      unit.visible = false;
      hid += 1;
      continue;
    }
    if (action === "push") {
      const before = measured.minR;
      pushUnit(unit, RING_VISUAL_LIMIT);
      const after = measureUnit(unit);
      if (after && after.minR > before + 0.05) pushed += 1;
      if (after && after.minR < RING_VISUAL_LIMIT) {
        clipUnit(unit, RING_VISUAL_LIMIT, after.minY < 5);
        clipped += 1;
      } else {
        kept += 1;
      }
      continue;
    }
    if (action === "clip") {
      clipUnit(unit, RING_VISUAL_LIMIT, measured.minY < 5);
      clipped += 1;
      continue;
    }
    kept += 1;
  }
  return { hid, pushed, clipped, kept };
}

interface IdleNode {
  obj: THREE.Object3D;
  baseY: number;
  phase: number;
  spin: boolean;
}

function collectWinterIdle(root: THREE.Object3D): IdleNode[] {
  const found: IdleNode[] = [];
  root.traverse((obj) => {
    const spin = /^coin_|^ring_001/i.test(obj.name);
    const sway = /^snowman_|^characters$/i.test(obj.name);
    if (!spin && !sway) return;
    found.push({ obj, baseY: obj.position.y, phase: Math.random() * Math.PI * 2, spin });
  });
  return found;
}

function RingStage({
  url,
  position,
  rotation,
  scale,
  arrange,
  idle,
}: {
  url: string;
  position: [number, number, number];
  rotation: [number, number, number];
  scale: number;
  arrange: boolean;
  idle: "none" | "winter";
}) {
  const { scene, animations } = useGLTF(url);
  const clone = useMemo(() => scene.clone(true), [scene]);
  const groupRef = useRef<THREE.Group>(null);
  const { actions } = useAnimations(animations, groupRef);
  const idleRef = useRef<IdleNode[]>([]);

  useEffect(() => {
    Object.values(actions).forEach((action) => action?.play());
  }, [actions]);

  useLayoutEffect(() => {
    let cancelled = false;
    let tries = 0;
    const run = () => {
      if (cancelled) return;
      const group = groupRef.current;
      if (!group || !clone.parent) {
        tries += 1;
        if (tries < 30) requestAnimationFrame(run);
        return;
      }
      group.updateMatrixWorld(true);
      if (arrange) {
        const map = url.includes("winter") ? "ice" : url.includes("arabian") ? "saturn" : "grass";
        const stats = arrangeRing(clone, map);
        if (stats) console.info(`[ring] ${url} hid ${stats.hid}, pushed ${stats.pushed}, clipped ${stats.clipped}, kept ${stats.kept}`);
        if (map === "ice") setWinterCampActive(true);
      }
      if (idle === "winter") idleRef.current = collectWinterIdle(clone);
      clone.traverse((obj) => {
        if (obj instanceof THREE.Mesh) {
          obj.castShadow = true;
          obj.receiveShadow = true;
        }
      });
    };
    run();
    return () => {
      cancelled = true;
      if (url.includes("winter")) setWinterCampActive(false);
    };
  }, [clone, arrange, idle, url]);

  useFrame((state) => {
    const t = state.clock.elapsedTime;
    for (const item of idleRef.current) {
      if (item.spin) item.obj.rotation.y += 0.03;
      const bob = item.spin ? 0.3 : 0.12;
      const speed = item.spin ? 2 : 1.2;
      item.obj.position.y = item.baseY + Math.sin(t * speed + item.phase) * bob;
    }
  });

  return (
    <group ref={groupRef} position={position} rotation={rotation} scale={scale}>
      <primitive object={clone} />
    </group>
  );
}

function StageBoundary({ children }: { children: ReactNode }) {
  return (
    <ModelErrorBoundary>
      <Suspense fallback={null}>{children}</Suspense>
    </ModelErrorBoundary>
  );
}

export function MeadowArena() {
  return (
    <group>
      <MeadowFlowers />
      <StageBoundary>
        <MeadowFloor />
        <MeadowRim />
        <MeadowForest />
        <MeadowBackdrop />
      </StageBoundary>
    </group>
  );
}

export function FrozenArena() {
  const gameMode = useZoogiGame((state) => state.gameMode);
  const background = useZoogiGame((state) => state.backgroundSettings);
  const arenaRotation = useZoogiGame((state) => state.elementTransforms.arenaModelRotation);
  const editing = gameMode === "map_editor";
  const position: [number, number, number] = editing
    ? [background.modelPositionX ?? 0, background.modelPositionY ?? WINTER_STAGE.modelOffsetY, background.modelPositionZ ?? 0]
    : [0, WINTER_STAGE.modelOffsetY, 0];
  const scale = editing ? (background.modelScale ?? WINTER_STAGE.modelScale) : WINTER_STAGE.modelScale;
  const rotation: [number, number, number] = editing
    ? [arenaRotation?.x ?? 0, arenaRotation?.y ?? 0, arenaRotation?.z ?? 0]
    : [0, 0, 0];

  return (
    <group>
      <RoundIsland kind="ice" lip="#f4fbff" underside="#16324a" sideColor="#eef7ff" />
      <FrozenRinkMarkings />
      <StageBoundary>
        <RingStage
          url="/models/winter_location.glb"
          position={position}
          rotation={rotation}
          scale={scale}
          arrange={!editing}
          idle="winter"
        />
      </StageBoundary>
    </group>
  );
}

export function ArabianArena() {
  const gameMode = useZoogiGame((state) => state.gameMode);
  const background = useZoogiGame((state) => state.backgroundSettings);
  const arenaRotation = useZoogiGame((state) => state.elementTransforms.arenaModelRotation);
  const editing = gameMode === "map_editor";
  const placed = arabianPlayTransform();
  const position: [number, number, number] = editing
    ? [background.modelPositionX ?? placed.x, background.modelPositionY ?? placed.y, background.modelPositionZ ?? placed.z]
    : [placed.x, placed.y, placed.z];
  const scale = editing ? (background.modelScale ?? placed.scale) : placed.scale;
  const rotation: [number, number, number] = editing
    ? [arenaRotation?.x ?? 0, arenaRotation?.y ?? 0, arenaRotation?.z ?? 0]
    : [0, 0, 0];

  return (
    <group>
      <RoundIsland kind="saturn" lip="#ffd78a" lipEmissive="#ffb03a" underside="#1a120c" sideColor="#c4a074" />
      <ArabianPlanters />
      <StageBoundary>
        <RingStage
          url="/models/arabian_nights_stage.glb"
          position={position}
          rotation={rotation}
          scale={scale}
          arrange={!editing}
          idle="none"
        />
      </StageBoundary>
    </group>
  );
}

useGLTF.preload("/models/winter_location.glb");
useGLTF.preload("/models/arabian_nights_stage.glb");

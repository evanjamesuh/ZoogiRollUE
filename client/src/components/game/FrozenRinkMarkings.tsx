import * as THREE from "three";
import { useLayoutEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { getSnowmanPositions } from "@/lib/arenaConstants";
import { BUMPER_RADIUS, getIcePatches, getMapLayout } from "@/lib/arenaColliders";
import { ROUND_KNOCKOFF_RADIUS } from "@/lib/roundRim";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

/**
 * Painted Frozen Ring cues. The knockout is the outer edge of the amber band
 * at ROUND_KNOCKOFF_RADIUS (15.5). Paint inside that radius is still in.
 * Paint outside it is the drop, so the line you see is the line that counts.
 */

const KNOCK = ROUND_KNOCKOFF_RADIUS;

const NAVY = "#08325c";
const CYAN = "#00e4ff";
const AMBER = "#ffb000";
const INK = "#07141e";
const SNOW = "#f4f8ff";

const MARK_Y = 0.074;

interface RingSpec {
  x: number;
  z: number;
  inner: number;
  outer: number;
}

function ringGeometry(rings: RingSpec[], segments = 80): THREE.BufferGeometry {
  const positions: number[] = [];
  const index: number[] = [];
  for (const ring of rings) {
    const base = positions.length / 3;
    for (let i = 0; i <= segments; i++) {
      const t = (i / segments) * Math.PI * 2;
      const c = Math.cos(t);
      const s = Math.sin(t);
      positions.push(ring.x + c * ring.inner, 0, ring.z + s * ring.inner);
      positions.push(ring.x + c * ring.outer, 0, ring.z + s * ring.outer);
    }
    for (let i = 0; i < segments; i++) {
      const a = base + i * 2;
      index.push(a, a + 2, a + 1, a + 1, a + 2, a + 3);
    }
  }
  const geom = new THREE.BufferGeometry();
  geom.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
  geom.setIndex(index);
  return geom;
}

function paintMaterial(color: string, pull = 4) {
  return new THREE.MeshBasicMaterial({
    color,
    toneMapped: false,
    side: THREE.DoubleSide,
    polygonOffset: true,
    polygonOffsetFactor: -pull,
    polygonOffsetUnits: -pull * 2,
  });
}

function FlatPaint({
  geometry,
  material,
  y,
}: {
  geometry: THREE.BufferGeometry;
  material: THREE.Material;
  y: number;
}) {
  return <mesh geometry={geometry} material={material} position={[0, y, 0]} frustumCulled={false} />;
}

function FootDiscs({
  items,
  material,
  y,
}: {
  items: { x: number; z: number; r: number }[];
  material: THREE.Material;
  y: number;
}) {
  const ref = useRef<THREE.InstancedMesh>(null);
  useLayoutEffect(() => {
    const dummy = new THREE.Object3D();
    items.forEach((item, i) => {
      dummy.position.set(item.x, y, item.z);
      dummy.rotation.set(-Math.PI / 2, 0, 0);
      dummy.scale.set(item.r, item.r, 1);
      dummy.updateMatrix();
      ref.current?.setMatrixAt(i, dummy.matrix);
    });
    if (ref.current) ref.current.instanceMatrix.needsUpdate = true;
  }, [items, y]);
  if (items.length === 0) return null;
  return (
    <instancedMesh ref={ref} args={[undefined, undefined, items.length]} material={material} frustumCulled={false}>
      <circleGeometry args={[1, 40]} />
    </instancedMesh>
  );
}

/** Even snow berm outside the knockout. Cosmetic only: no collider, identical mounds. */
function SymmetricSnowLip() {
  const ref = useRef<THREE.InstancedMesh>(null);
  const count = 48;
  const dist = 17.05;
  const scale = 0.9;
  const snow = useMemo(() => new THREE.MeshStandardMaterial({ color: SNOW, roughness: 0.95, metalness: 0 }), []);

  useLayoutEffect(() => {
    const dummy = new THREE.Object3D();
    for (let i = 0; i < count; i++) {
      const angle = (i / count) * Math.PI * 2;
      dummy.position.set(Math.cos(angle) * dist, scale * 0.4, Math.sin(angle) * dist);
      dummy.rotation.set(0, angle, 0);
      dummy.scale.set(scale, scale * 0.48, scale);
      dummy.updateMatrix();
      ref.current?.setMatrixAt(i, dummy.matrix);
    }
    if (ref.current) ref.current.instanceMatrix.needsUpdate = true;
  }, [scale]);

  return (
    <group>
      <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 0.18, 0]} material={snow}>
        <torusGeometry args={[dist, 0.42, 8, 72]} />
      </mesh>
      <instancedMesh ref={ref} args={[undefined, undefined, count]} material={snow} frustumCulled={false}>
        <sphereGeometry args={[1, 10, 8]} />
      </instancedMesh>
    </group>
  );
}

function KnockoutEdge() {
  const amber = useMemo(() => paintMaterial(AMBER, 0), []);
  const navy = useMemo(() => paintMaterial(NAVY, 0), []);
  const cyan = useMemo(() => paintMaterial(CYAN, 0), []);
  const ink = useMemo(() => paintMaterial(INK, 0), []);
  const flash = useZoogiGame((state) => state.knockoffBoundaryFlash);
  const flashStart = useRef<number | null>(null);
  const flashColor = useRef("#ffffff");
  const flashCount = useRef(2);

  const bands = useMemo(
    () => ({
      navy: ringGeometry([{ x: 0, z: 0, inner: 14.7, outer: 15.02 }], 160),
      cyan: ringGeometry([{ x: 0, z: 0, inner: 15.02, outer: 15.2 }], 160),
      amber: ringGeometry([{ x: 0, z: 0, inner: 15.2, outer: KNOCK }], 160),
      ink: ringGeometry([{ x: 0, z: 0, inner: KNOCK, outer: KNOCK + 0.62 }], 128),
    }),
    [],
  );

  useFrame(() => {
    if (flash && flash.timestamp !== flashStart.current) {
      flashStart.current = flash.timestamp;
      flashColor.current = flash.color;
      flashCount.current = flash.flashCount;
    }
    if (!flashStart.current) {
      amber.color.set(AMBER);
      return;
    }
    const elapsed = Date.now() - flashStart.current;
    const step = 150;
    const total = step * flashCount.current * 2;
    if (elapsed >= total) {
      amber.color.set(AMBER);
      flashStart.current = null;
      return;
    }
    const on = Math.floor(elapsed / step) % 2 === 0;
    amber.color.set(on ? flashColor.current : AMBER);
  });

  return (
    <group>
      <FlatPaint geometry={bands.navy} material={navy} y={0.088} />
      <FlatPaint geometry={bands.cyan} material={cyan} y={0.094} />
      <FlatPaint geometry={bands.amber} material={amber} y={0.1} />
      <FlatPaint geometry={bands.ink} material={ink} y={0.106} />
    </group>
  );
}

export function FrozenRinkMarkings() {
  const navy = useMemo(() => paintMaterial(NAVY, 3), []);
  const cyan = useMemo(() => paintMaterial(CYAN, 5), []);
  const amber = useMemo(() => paintMaterial(AMBER, 7), []);

  const snowmen = useMemo(
    () =>
      getSnowmanPositions().map((snowman) => ({
        x: snowman.position[0],
        z: snowman.position[2],
        r: snowman.radius,
      })),
    [],
  );
  const bumpers = useMemo(
    () =>
      (getMapLayout("ice")?.bumpers ?? []).map((bumper) => ({
        x: bumper.x,
        z: bumper.z,
        r: BUMPER_RADIUS,
      })),
    [],
  );
  const patches = useMemo(() => getIcePatches(), []);

  const rings = useMemo(() => {
    const obstacle = [
      ...snowmen.map((item) => ({ x: item.x, z: item.z, inner: item.r * 0.38, outer: item.r })),
      ...bumpers.map((item) => ({ x: item.x, z: item.z, inner: item.r * 0.55, outer: item.r })),
    ];
    const coastNavy = patches.map((patch) => ({
      x: patch.x,
      z: patch.z,
      inner: Math.max(0.2, patch.radius - 0.72),
      outer: patch.radius - 0.28,
    }));
    const coastCyan = patches.map((patch) => ({
      x: patch.x,
      z: patch.z,
      inner: patch.radius - 0.28,
      outer: patch.radius,
    }));
    return {
      obstacle: ringGeometry(obstacle, 40),
      coastNavy: ringGeometry(coastNavy, 64),
      coastCyan: ringGeometry(coastCyan, 64),
    };
  }, [bumpers, patches, snowmen]);

  return (
    <group>
      <FootDiscs items={snowmen} material={navy} y={MARK_Y} />
      <FootDiscs items={bumpers} material={navy} y={MARK_Y} />
      <FlatPaint geometry={rings.obstacle} material={amber} y={MARK_Y + 0.008} />
      <FlatPaint geometry={rings.coastNavy} material={navy} y={MARK_Y + 0.012} />
      <FlatPaint geometry={rings.coastCyan} material={cyan} y={MARK_Y + 0.018} />
      <KnockoutEdge />
      <SymmetricSnowLip />
    </group>
  );
}

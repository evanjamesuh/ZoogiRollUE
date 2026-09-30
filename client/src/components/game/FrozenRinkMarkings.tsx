import * as THREE from "three";
import { useLayoutEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { BUMPER_RADIUS, getIcePatches, getMapLayout } from "@/lib/arenaColliders";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

/**
 * Painted Frozen Ring cues. The knockout is the outer edge of the amber band
 * at the ice map's knockoff radius. Paint inside that radius is still in.
 * Paint outside it is the drop, so the line you see is the line that counts.
 *
 * Radii and positions come from getMapLayout("ice") and getIcePatches(),
 * the same reads the meshes and colliders use. Shares below keep today's
 * look and follow arenaScale when those getters change.
 */

/** Even cosmetic berm. 1.1× the knockout is the middle of the old 16.7–17.4 mounds. */
const SNOW_LIP_SHARE = 1.1;
/** Mound radius and torus tube as shares of the 17.05 berm. */
const SNOW_MOUND_SHARE = 0.9 / 17.05;
const SNOW_TUBE_SHARE = 0.42 / 17.05;

interface RinkFrame {
  navyInner: number;
  cyanInner: number;
  floor: number;
  knock: number;
  inkOuter: number;
  snowDist: number;
  snowMound: number;
  snowTube: number;
}

/** Ice layout radii. Null only if the ice map layout is missing. */
function iceRinkFrame(): RinkFrame | null {
  const layout = getMapLayout("ice");
  if (!layout) return null;
  const knock = layout.knockoffRadius;
  const floor = layout.floorRadius;
  const lip = knock - floor;
  const snowDist = knock * SNOW_LIP_SHARE;
  return {
    navyInner: floor - lip * (5 / 3),
    cyanInner: floor - lip * 0.6,
    floor,
    knock,
    inkOuter: knock * 1.04,
    snowDist,
    snowMound: snowDist * SNOW_MOUND_SHARE,
    snowTube: snowDist * SNOW_TUBE_SHARE,
  };
}

/** Patch coast, as shares of each patch radius (0.72 and 0.28 on a radius-3 patch). */
const COAST_NAVY_SHARE = 0.72 / 3;
const COAST_CYAN_SHARE = 0.28 / 3;

const NAVY = "#08325c";
const CYAN = "#00e4ff";
const AMBER = "#ffb000";
const INK = "#07141e";
const SNOW = "#f4f8ff";

/** Above the y=0 floor and the 0.06 lip, and under the collider overlay at 0.12. */
const MARK_Y = 0.072;

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
  renderOrder,
}: {
  geometry: THREE.BufferGeometry;
  material: THREE.Material;
  y: number;
  renderOrder: number;
}) {
  return (
    <mesh geometry={geometry} material={material} position={[0, y, 0]} renderOrder={renderOrder} frustumCulled={false} />
  );
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
    <instancedMesh ref={ref} args={[undefined, undefined, items.length]} material={material} renderOrder={2} frustumCulled={false}>
      <circleGeometry args={[1, 40]} />
    </instancedMesh>
  );
}

/** Even snow berm outside the knockout. Cosmetic only: no collider, identical mounds. */
function SymmetricSnowLip({ frame }: { frame: RinkFrame }) {
  const ref = useRef<THREE.InstancedMesh>(null);
  const count = 48;
  const dist = frame.snowDist;
  const scale = frame.snowMound;
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
  }, [dist, scale]);

  return (
    <group>
      <mesh rotation={[Math.PI / 2, 0, 0]} position={[0, 0.22, 0]} material={snow} renderOrder={1}>
        <torusGeometry args={[dist, frame.snowTube, 8, 72]} />
      </mesh>
      <instancedMesh ref={ref} args={[undefined, undefined, count]} material={snow} renderOrder={1} frustumCulled={false}>
        <sphereGeometry args={[1, 10, 8]} />
      </instancedMesh>
    </group>
  );
}

function KnockoutEdge({ frame }: { frame: RinkFrame }) {
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
      navy: ringGeometry([{ x: 0, z: 0, inner: frame.navyInner, outer: frame.cyanInner }], 160),
      cyan: ringGeometry([{ x: 0, z: 0, inner: frame.cyanInner, outer: frame.floor }], 160),
      amber: ringGeometry([{ x: 0, z: 0, inner: frame.floor, outer: frame.knock }], 160),
      ink: ringGeometry([{ x: 0, z: 0, inner: frame.knock, outer: frame.inkOuter }], 128),
    }),
    [frame],
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
      <FlatPaint geometry={bands.navy} material={navy} y={0.09} renderOrder={4} />
      <FlatPaint geometry={bands.cyan} material={cyan} y={0.096} renderOrder={5} />
      <FlatPaint geometry={bands.amber} material={amber} y={0.102} renderOrder={6} />
      <FlatPaint geometry={bands.ink} material={ink} y={0.108} renderOrder={7} />
    </group>
  );
}

export function FrozenRinkMarkings() {
  const navy = useMemo(() => paintMaterial(NAVY, 3), []);
  const cyan = useMemo(() => paintMaterial(CYAN, 5), []);
  const amber = useMemo(() => paintMaterial(AMBER, 7), []);

  const snowmen = useMemo(
    () =>
      (getMapLayout("ice")?.scenery ?? [])
        .filter((solid) => solid.kind === "snowman")
        .map((solid) => ({ x: solid.x, z: solid.z, r: solid.radius })),
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
  const frame = useMemo(() => iceRinkFrame(), []);

  const rings = useMemo(() => {
    const obstacle = [
      ...snowmen.map((item) => ({ x: item.x, z: item.z, inner: item.r * 0.38, outer: item.r })),
      ...bumpers.map((item) => ({ x: item.x, z: item.z, inner: item.r * 0.55, outer: item.r })),
    ];
    const coastNavy = patches.map((patch) => ({
      x: patch.x,
      z: patch.z,
      inner: patch.radius * (1 - COAST_NAVY_SHARE),
      outer: patch.radius * (1 - COAST_CYAN_SHARE),
    }));
    const coastCyan = patches.map((patch) => ({
      x: patch.x,
      z: patch.z,
      inner: patch.radius * (1 - COAST_CYAN_SHARE),
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
      <FlatPaint geometry={rings.obstacle} material={amber} y={MARK_Y + 0.008} renderOrder={3} />
      <FlatPaint geometry={rings.coastNavy} material={navy} y={MARK_Y + 0.014} renderOrder={3} />
      <FlatPaint geometry={rings.coastCyan} material={cyan} y={MARK_Y + 0.02} renderOrder={3} />
      {frame && <KnockoutEdge frame={frame} />}
      {frame && <SymmetricSnowLip frame={frame} />}
    </group>
  );
}

import type { BufferGeometry } from "three";

const BIN = 0.01;
const Y_ORIGIN = -0.42;
/**
 * Outer shell of each height slice (this fraction of that slice's radius, and
 * beyond) is pulled onto the collider circle. Points closer to the axis, such
 * as the crown center, stay inside so the top does not tear open.
 */
const SHELL = 0.55;

const fittedBySource = new WeakMap<BufferGeometry, Map<number, BufferGeometry>>();

/**
 * Reshape stylized_desert_hoodoo so the visible stone matches its collider.
 * The raw mesh is a mushroom: at marble height the stem is much thinner than
 * the hit circle, and the flared foot is about 1.37x wider than that circle.
 * `targetRadius` is the collider radius in the mesh's local space
 * (world radius divided by the group scale from getHoodooDecor).
 * Radial fit runs once per target radius when the model loads.
 */
export function fitHoodooGeometry(geometry: BufferGeometry, targetRadius: number): void {
  const pos = geometry.getAttribute("position");
  if (!pos) return;
  const count = pos.count;
  const radial = new Float32Array(count);
  let maxY = Y_ORIGIN;
  for (let i = 0; i < count; i++) {
    radial[i] = Math.hypot(pos.getX(i), pos.getZ(i));
    const y = pos.getY(i);
    if (y > maxY) maxY = y;
  }
  const bins = Math.max(1, Math.ceil((maxY - Y_ORIGIN) / BIN) + 2);
  const binMax = new Float32Array(bins);
  for (let i = 0; i < count; i++) {
    const bin = Math.max(0, Math.min(bins - 1, Math.floor((pos.getY(i) - Y_ORIGIN) / BIN)));
    if (radial[i] > binMax[bin]) binMax[bin] = radial[i];
  }
  for (let i = 0; i < count; i++) {
    const radius = radial[i];
    if (radius < 1e-5) continue;
    const bin = Math.max(0, Math.min(bins - 1, Math.floor((pos.getY(i) - Y_ORIGIN) / BIN)));
    const slice = binMax[bin];
    if (slice < 1e-4) continue;
    const outward = radius / slice;
    const fitted = targetRadius * Math.min(1, outward / SHELL);
    const scale = fitted / radius;
    pos.setX(i, pos.getX(i) * scale);
    pos.setZ(i, pos.getZ(i) * scale);
  }
  pos.needsUpdate = true;
  geometry.computeVertexNormals();
  geometry.computeBoundingBox();
  geometry.computeBoundingSphere();
}

/**
 * Fitted geometry for one collider radius. Hoodoos that share a local radius
 * share the geometry. The source mesh is left alone.
 */
export function fittedHoodooGeometry(source: BufferGeometry, targetRadius: number): BufferGeometry {
  const key = Math.round(targetRadius * 10000);
  let byRadius = fittedBySource.get(source);
  if (!byRadius) {
    byRadius = new Map();
    fittedBySource.set(source, byRadius);
  }
  const cached = byRadius.get(key);
  if (cached) return cached;
  const fitted = source.clone();
  fitHoodooGeometry(fitted, targetRadius);
  byRadius.set(key, fitted);
  return fitted;
}

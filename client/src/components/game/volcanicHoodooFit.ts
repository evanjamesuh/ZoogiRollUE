import type { BufferGeometry } from "three";

/**
 * Local collider radius of a lava hoodoo before the group scale.
 * getHoodooDecor uses world radius = 0.25 * scale. The mesh is drawn in that
 * same local space, so fitting the stone to this radius matches every scale.
 */
export const HOODOO_LOCAL_COLLIDER_RADIUS = 0.25;

const BIN = 0.01;
const Y_ORIGIN = -0.42;
/**
 * Outer shell of each height slice (this fraction of that slice's radius, and
 * beyond) is pulled onto the collider circle. Points closer to the axis, such
 * as the crown center, stay inside so the top does not tear open.
 */
const SHELL = 0.55;

const fittedBySource = new WeakMap<BufferGeometry, BufferGeometry>();

/**
 * Reshape stylized_desert_hoodoo so the visible stone matches its collider.
 * The raw mesh is a mushroom: at marble height the stem is much thinner than
 * the hit circle, and the flared foot is about 1.37x wider than that circle.
 * Radial fit runs once when the model loads.
 */
export function fitHoodooGeometry(
  geometry: BufferGeometry,
  targetRadius = HOODOO_LOCAL_COLLIDER_RADIUS,
): void {
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

/** One fitted geometry shared by every lava hoodoo. The source mesh is left alone. */
export function fittedHoodooGeometry(source: BufferGeometry): BufferGeometry {
  const cached = fittedBySource.get(source);
  if (cached) return cached;
  const fitted = source.clone();
  fitHoodooGeometry(fitted);
  fittedBySource.set(source, fitted);
  return fitted;
}

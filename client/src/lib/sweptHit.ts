/**
 * Analytic circle sweep. A step that starts and ends clear of another
 * marble can still cross it, so the hit is the first time the centres
 * are one diameter apart, not the endpoints.
 */

export interface CircleContact {
  t: number;
  ax: number;
  az: number;
  bx: number;
  bz: number;
}

export function circleTimeOfImpact(
  a0x: number,
  a0z: number,
  a1x: number,
  a1z: number,
  b0x: number,
  b0z: number,
  b1x: number,
  b1z: number,
  radiusSum: number,
): CircleContact | null {
  const r0x = a0x - b0x;
  const r0z = a0z - b0z;
  const dx = (a1x - a0x) - (b1x - b0x);
  const dz = (a1z - a0z) - (b1z - b0z);
  const reach = radiusSum * radiusSum;
  const c = r0x * r0x + r0z * r0z - reach;
  const a = dx * dx + dz * dz;
  const b = 2 * (r0x * dx + r0z * dz);

  let t = 0;
  if (c > 0) {
    if (a < 1e-12) return null;
    const disc = b * b - 4 * a * c;
    if (disc < 0) return null;
    const root = Math.sqrt(disc);
    t = (-b - root) / (2 * a);
    if (t < 0 || t > 1) {
      const exit = (-b + root) / (2 * a);
      if (exit < 0 || exit > 1) return null;
      t = exit;
    }
  }

  if (t < 0 || t > 1) return null;
  return {
    t,
    ax: a0x + (a1x - a0x) * t,
    az: a0z + (a1z - a0z) * t,
    bx: b0x + (b1x - b0x) * t,
    bz: b0z + (b1z - b0z) * t,
  };
}

import * as THREE from "three";

const dir = new THREE.Vector3();
const side = new THREE.Vector3();
const up = new THREE.Vector3(0, 1, 0);
const bin = new THREE.Vector3();

export function createRibbonGeometry(segments: number): THREE.BufferGeometry {
  const geo = new THREE.BufferGeometry();
  const verts = (segments + 1) * 2;
  const position = new Float32Array(verts * 3);
  const aSide = new Float32Array(verts);
  const aAlong = new Float32Array(verts);
  for (let i = 0; i <= segments; i++) {
    aSide[i * 2] = -1;
    aSide[i * 2 + 1] = 1;
    const along = segments === 0 ? 0 : i / segments;
    aAlong[i * 2] = along;
    aAlong[i * 2 + 1] = along;
  }
  geo.setAttribute("position", new THREE.BufferAttribute(position, 3));
  geo.setAttribute("aSide", new THREE.BufferAttribute(aSide, 1));
  geo.setAttribute("aAlong", new THREE.BufferAttribute(aAlong, 1));
  const index = new Uint16Array(segments * 6);
  for (let i = 0; i < segments; i++) {
    const a = i * 2;
    const o = i * 6;
    index[o] = a;
    index[o + 1] = a + 1;
    index[o + 2] = a + 2;
    index[o + 3] = a + 1;
    index[o + 4] = a + 3;
    index[o + 5] = a + 2;
  }
  geo.setIndex(new THREE.BufferAttribute(index, 1));
  geo.boundingSphere = new THREE.Sphere(new THREE.Vector3(), 8);
  return geo;
}

/** Writes a camera-width ribbon through `points`. `points.length` must be segments + 1. */
export function writeRibbon(geo: THREE.BufferGeometry, points: ArrayLike<THREE.Vector3>, width: number): void {
  const attr = geo.getAttribute("position") as THREE.BufferAttribute;
  const count = attr.count / 2;
  if (points.length < count) return;
  for (let i = 0; i < count; i++) {
    const prev = points[Math.max(0, i - 1)];
    const next = points[Math.min(count - 1, i + 1)];
    dir.subVectors(next, prev);
    if (dir.lengthSq() < 1e-8) dir.set(0, 0, 1);
    dir.normalize();
    side.crossVectors(dir, up);
    if (side.lengthSq() < 1e-6) side.set(1, 0, 0);
    side.normalize().multiplyScalar(width);
    const p = points[i];
    attr.setXYZ(i * 2, p.x - side.x, p.y, p.z - side.z);
    attr.setXYZ(i * 2 + 1, p.x + side.x, p.y, p.z + side.z);
  }
  attr.needsUpdate = true;
}

const axis = new THREE.Vector3();
const lateral = new THREE.Vector3();
const lift = new THREE.Vector3();

/** Fills `out` with a jagged path. Endpoints stay on `from` and `to`. */
export function layJagged(
  from: THREE.Vector3,
  to: THREE.Vector3,
  jitters: Array<[number, number]>,
  out: THREE.Vector3[],
  sag = 0.15,
): void {
  axis.subVectors(to, from);
  const len = axis.length() || 0.001;
  axis.multiplyScalar(1 / len);
  lateral.crossVectors(axis, up);
  if (lateral.lengthSq() < 1e-6) lateral.set(1, 0, 0);
  lateral.normalize();
  lift.crossVectors(lateral, axis).normalize();
  const n = out.length;
  for (let i = 0; i < n; i++) {
    const t = n === 1 ? 0 : i / (n - 1);
    const amp = Math.sin(t * Math.PI) * (0.22 + 0.45 * Math.min(1.4, len / 5));
    const j = jitters[i - 1];
    const jx = j ? j[0] * amp : 0;
    const jy = j ? j[1] * amp * 0.55 : 0;
    out[i].copy(from).addScaledVector(axis, len * t).addScaledVector(lateral, jx).addScaledVector(lift, jy + Math.sin(t * Math.PI) * sag);
  }
  out[0].copy(from);
  out[n - 1].copy(to);
}

export function layHelix(
  turns: number,
  radius: number,
  time: number,
  phase: number,
  out: THREE.Vector3[],
): void {
  const n = out.length;
  for (let i = 0; i < n; i++) {
    const t = n === 1 ? 0 : i / (n - 1);
    const ang = phase + time * 1.4 + t * Math.PI * 2 * turns;
    out[i].set(Math.cos(ang) * radius, -0.32 + t * 0.95, Math.sin(ang) * radius);
  }
}

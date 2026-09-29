/** Deterministic RNG so a blast looks the same if it is replayed. */
export function mulberry32(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a |= 0;
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export function hashSeed(x: number, y: number, z: number, time: number, salt: number): number {
  const hx = Math.imul(Math.floor(x * 100) | 0, 374761393);
  const hy = Math.imul(Math.floor(y * 100) | 0, 668265263);
  const hz = Math.imul(Math.floor(z * 100) | 0, 1274126177);
  const ht = Math.imul(time | 0, 1442695041);
  return (hx ^ hy ^ hz ^ ht ^ salt) >>> 0 || 1;
}

/** In-place xorshift step. `state` is a one-element box so callers allocate nothing per frame. */
export function nextRand(state: { n: number }): number {
  let a = state.n | 0;
  a = (a + 0x6d2b79f5) | 0;
  let t = Math.imul(a ^ (a >>> 15), 1 | a);
  t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
  state.n = a;
  return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
}

/**
 * Fixed-capacity sprite pool. Every array is allocated once.
 * Later powers (Bolt Shock, Lars Ricochet, Wraps Bind, Pinpoint, Nightshade)
 * should spawn into one of these instead of creating meshes per spark.
 */
export interface SpritePool {
  readonly capacity: number;
  px: Float32Array;
  py: Float32Array;
  pz: Float32Array;
  vx: Float32Array;
  vy: Float32Array;
  vz: Float32Array;
  life: Float32Array;
  maxLife: Float32Array;
  size: Float32Array;
  size0: Float32Array;
  grow: Float32Array;
  spin: Float32Array;
  rot: Float32Array;
  seed: Float32Array;
  r: Float32Array;
  g: Float32Array;
  b: Float32Array;
  opacity: Float32Array;
  active: Uint8Array;
  cursor: number;
  alive: number;
}

export interface SpriteSpawn {
  x: number;
  y: number;
  z: number;
  vx: number;
  vy: number;
  vz: number;
  life: number;
  size: number;
  grow: number;
  spin: number;
  r: number;
  g: number;
  b: number;
  seed: number;
}

export function createSpritePool(capacity: number): SpritePool {
  return {
    capacity,
    px: new Float32Array(capacity),
    py: new Float32Array(capacity),
    pz: new Float32Array(capacity),
    vx: new Float32Array(capacity),
    vy: new Float32Array(capacity),
    vz: new Float32Array(capacity),
    life: new Float32Array(capacity),
    maxLife: new Float32Array(capacity),
    size: new Float32Array(capacity),
    size0: new Float32Array(capacity),
    grow: new Float32Array(capacity),
    spin: new Float32Array(capacity),
    rot: new Float32Array(capacity),
    seed: new Float32Array(capacity),
    r: new Float32Array(capacity),
    g: new Float32Array(capacity),
    b: new Float32Array(capacity),
    opacity: new Float32Array(capacity),
    active: new Uint8Array(capacity),
    cursor: 0,
    alive: 0,
  };
}

export function clearSpritePool(pool: SpritePool): void {
  pool.active.fill(0);
  pool.opacity.fill(0);
  pool.cursor = 0;
  pool.alive = 0;
}

export function spawnSprite(pool: SpritePool, s: SpriteSpawn): void {
  const i = pool.cursor;
  pool.cursor = (i + 1) % pool.capacity;
  if (pool.active[i] === 0) pool.alive += 1;
  pool.active[i] = 1;
  pool.px[i] = s.x;
  pool.py[i] = s.y;
  pool.pz[i] = s.z;
  pool.vx[i] = s.vx;
  pool.vy[i] = s.vy;
  pool.vz[i] = s.vz;
  pool.life[i] = s.life;
  pool.maxLife[i] = s.life;
  pool.size0[i] = s.size;
  pool.grow[i] = s.grow;
  pool.size[i] = s.size;
  pool.spin[i] = s.spin;
  pool.rot[i] = s.seed * 0.17;
  pool.seed[i] = s.seed;
  pool.r[i] = s.r;
  pool.g[i] = s.g;
  pool.b[i] = s.b;
  pool.opacity[i] = 0;
}

export function killSprite(pool: SpritePool, index: number): void {
  if (pool.active[index] === 0) return;
  pool.active[index] = 0;
  pool.opacity[index] = 0;
  pool.size[index] = 0;
  pool.alive -= 1;
}

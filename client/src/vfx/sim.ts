import { killSprite, spawnSprite, type SpritePool } from "./pool";

export const SMOKE_CAP = 48;
export const EMBER_CAP = 64;
export const BURST_CAP = 20;
export const WISP_CAP = 18;
export const COLD_EMBER_CAP = 12;

const SMOKE_DRAG = 0.9;
const EMBER_DRAG = 1.55;
const EMBER_GRAVITY = 8.2;

function smoothstep(t: number): number {
  const x = t < 0 ? 0 : t > 1 ? 1 : t;
  return x * x * (3 - 2 * x);
}

function damp(rate: number, dt: number): number {
  return Math.exp(-rate * dt);
}

/**
 * Dark grey-brown puffs. Horizontal travel plus sprite growth stays inside
 * `radius`, which is the gameplay push radius.
 */
export function seedBlastSmoke(pool: SpritePool, radius: number, rand: () => number, originY = 0.45): void {
  const count = Math.min(pool.capacity, Math.round(26 + 14 * Math.min(radius / 8, 1.25)));
  for (let n = 0; n < count; n++) {
    const ang = rand() * Math.PI * 2;
    const reach = radius * (0.16 + rand() * 0.4);
    const speed = reach * SMOKE_DRAG;
    const lift = 0.25 + rand() * 1.15;
    const brown = 0.16 + rand() * 0.2;
    const life = 1.55 + rand() * 0.9;
    spawnSprite(pool, {
      x: Math.cos(ang) * radius * 0.05,
      y: originY + rand() * 0.35,
      z: Math.sin(ang) * radius * 0.05,
      vx: Math.cos(ang) * speed,
      vy: lift * (0.35 + radius * 0.04),
      vz: Math.sin(ang) * speed,
      life,
      size: radius * (0.1 + rand() * 0.08),
      grow: radius * (0.28 + rand() * 0.22),
      spin: (rand() - 0.5) * 0.7,
      r: brown * 0.95,
      g: brown * 0.78,
      b: brown * 0.62,
      seed: rand() * 40 + n,
    });
  }
}

export function stepSmoke(pool: SpritePool, elapsed: number, dt: number): void {
  const horiz = damp(SMOKE_DRAG, dt);
  const riseDamp = damp(0.4, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    const s = pool.seed[i];
    pool.vx[i] += Math.sin(elapsed * 1.6 + s) * 0.55 * dt;
    pool.vz[i] += Math.cos(elapsed * 1.25 + s * 1.3) * 0.55 * dt;
    pool.vx[i] *= horiz;
    pool.vz[i] *= horiz;
    pool.vy[i] = pool.vy[i] * riseDamp + 0.7 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const fadeIn = age < 0.1 ? age / 0.1 : 1;
    const fadeOut = 1 - smoothstep((age - 0.22) / 0.78);
    pool.opacity[i] = 0.34 * fadeIn * fadeOut;
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
  }
}

/** Yellow sparks that cool to deep red, arc under gravity, and die inside the blast radius. */
export function seedBlastEmbers(pool: SpritePool, radius: number, rand: () => number, originY = 0.45): void {
  const count = Math.min(pool.capacity, Math.round(34 + 22 * Math.min(radius / 8, 1.25)));
  const scale = 0.7 + 0.3 * Math.min(radius / 8, 1.4);
  for (let n = 0; n < count; n++) {
    const ang = rand() * Math.PI * 2;
    const reach = radius * (0.28 + rand() * 0.7);
    const speed = reach * EMBER_DRAG;
    const life = 0.45 + rand() * 0.85;
    spawnSprite(pool, {
      x: Math.cos(ang) * radius * 0.04,
      y: originY + rand() * 0.25,
      z: Math.sin(ang) * radius * 0.04,
      vx: Math.cos(ang) * speed,
      vy: 1.4 + rand() * 5.2,
      vz: Math.sin(ang) * speed,
      life,
      size: (0.06 + rand() * 0.16) * scale,
      grow: -0.04 * scale,
      spin: (rand() - 0.5) * 8,
      r: 1,
      g: 0.86,
      b: 0.38,
      seed: rand() * 50 + n,
    });
  }
}

export function stepEmbers(pool: SpritePool, elapsed: number, dt: number): void {
  const horiz = damp(EMBER_DRAG, dt);
  const vert = damp(0.28, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vy[i] -= EMBER_GRAVITY * dt;
    pool.vx[i] *= horiz;
    pool.vz[i] *= horiz;
    pool.vy[i] *= vert;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    if (pool.py[i] < 0.05) {
      pool.py[i] = 0.05;
      pool.vy[i] = 0;
      pool.vx[i] *= 0.2;
      pool.vz[i] *= 0.2;
      if (pool.life[i] > 0.12) pool.life[i] = 0.12;
    }
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    if (age < 0.35) {
      const t = age / 0.35;
      pool.r[i] = 1;
      pool.g[i] = 0.86 + (0.4 - 0.86) * t;
      pool.b[i] = 0.38 + (0.08 - 0.38) * t;
    } else {
      const t = (age - 0.35) / 0.65;
      pool.r[i] = 1 + (0.42 - 1) * t;
      pool.g[i] = 0.4 + (0.05 - 0.4) * t;
      pool.b[i] = 0.08 + (0.015 - 0.08) * t;
    }
    const fade = 1 - smoothstep((age - 0.45) / 0.55);
    const flick = 0.42 + 0.58 * Math.abs(Math.sin(elapsed * (18 + (pool.seed[i] % 6)) + pool.seed[i]));
    pool.opacity[i] = fade * flick;
    pool.size[i] = Math.max(0.02, pool.size0[i] + pool.grow[i] * age);
  }
}

/** Short dark puff used when spectral wolves appear and disappear. */
export function seedColdBurst(pool: SpritePool, rand: () => number): void {
  const count = Math.min(pool.capacity, 16);
  for (let n = 0; n < count; n++) {
    const ang = rand() * Math.PI * 2;
    const up = rand() * Math.PI * 0.5;
    const speed = 0.7 + rand() * 1.3;
    const life = 0.55 + rand() * 0.45;
    const shade = 0.1 + rand() * 0.12;
    spawnSprite(pool, {
      x: Math.cos(ang) * 0.15,
      y: 0.2 + rand() * 0.3,
      z: Math.sin(ang) * 0.15,
      vx: Math.cos(ang) * Math.cos(up) * speed,
      vy: 0.4 + rand() * 1.1,
      vz: Math.sin(ang) * Math.cos(up) * speed,
      life,
      size: 0.28 + rand() * 0.35,
      grow: 0.7 + rand() * 0.55,
      spin: (rand() - 0.5) * 1.1,
      r: shade * 0.75,
      g: shade * 0.85,
      b: shade + 0.06,
      seed: rand() * 20 + n,
    });
  }
}

/** One smoke wisp behind a moving wolf. Overwrites the oldest slot when the pool is full. */
export function emitWisp(
  pool: SpritePool,
  x: number,
  y: number,
  z: number,
  backX: number,
  backZ: number,
  rand: number,
): void {
  const shade = 0.12 + rand * 0.1;
  spawnSprite(pool, {
    x,
    y,
    z,
    vx: backX * (0.25 + rand * 0.4) + (rand - 0.5) * 0.2,
    vy: 0.25 + rand * 0.45,
    vz: backZ * (0.25 + rand * 0.4) + (rand - 0.5) * 0.2,
    life: 0.55 + rand * 0.45,
    size: 0.18 + rand * 0.16,
    grow: 0.35 + rand * 0.3,
    spin: (rand - 0.5) * 0.8,
    r: shade * 0.7,
    g: shade * 0.82,
    b: shade + 0.08,
    seed: rand * 30,
  });
}

export function emitColdEmber(
  pool: SpritePool,
  x: number,
  y: number,
  z: number,
  backX: number,
  backZ: number,
  rand: number,
): void {
  spawnSprite(pool, {
    x,
    y,
    z,
    vx: backX * 0.5 + (rand - 0.5) * 0.6,
    vy: 0.15 + rand * 0.7,
    vz: backZ * 0.5 + (rand - 0.5) * 0.6,
    life: 0.35 + rand * 0.4,
    size: 0.05 + rand * 0.06,
    grow: 0,
    spin: (rand - 0.5) * 4,
    r: 0.55,
    g: 0.75,
    b: 1,
    seed: rand * 30,
  });
}

export function stepWisps(pool: SpritePool, elapsed: number, dt: number): void {
  stepSmoke(pool, elapsed, dt);
}

export function stepColdEmbers(pool: SpritePool, elapsed: number, dt: number): void {
  const drag = damp(1.1, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vx[i] *= drag;
    pool.vz[i] *= drag;
    pool.vy[i] = pool.vy[i] * damp(0.5, dt) + 0.35 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    pool.r[i] = 0.62 + (0.25 - 0.62) * age;
    pool.g[i] = 0.8 + (0.4 - 0.8) * age;
    pool.b[i] = 1 + (0.62 - 1) * age;
    const flick = 0.5 + 0.5 * Math.abs(Math.sin(elapsed * 14 + pool.seed[i]));
    pool.opacity[i] = (1 - smoothstep((age - 0.2) / 0.8)) * flick;
    pool.size[i] = pool.size0[i];
    pool.rot[i] += pool.spin[i] * dt;
  }
}

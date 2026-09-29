import { killSprite, spawnSprite, type SpritePool } from "./pool";
import { particleBudget } from "./quality";

/** Pool sizes match the high budget. Low quality spawns fewer into the same pools. */
export const SMOKE_CAP = 28;
export const FIRE_CAP = 14;
export const EMBER_CAP = 42;
export const BURST_CAP = 16;
export const WISP_CAP = 24;
export const COLD_EMBER_CAP = 10;

const SMOKE_DRAG = 2.35;
const EMBER_DRAG = 1.45;
const EMBER_GRAVITY = 7.4;

function smoothstep(t: number): number {
  const x = t < 0 ? 0 : t > 1 ? 1 : t;
  return x * x * (3 - 2 * x);
}

function damp(rate: number, dt: number): number {
  return Math.exp(-rate * dt);
}

function radiusSpan(radius: number): number {
  return Math.min(1, 0.62 + radius / 18);
}

/**
 * Thick dark puffs. About 40% climb as a column; the rest billow out toward
 * the push radius. Sprite growth is included when the blast checks its edge.
 */
export function seedBlastSmoke(pool: SpritePool, radius: number, rand: () => number, originY = 0.45): void {
  const count = Math.min(pool.capacity, Math.max(8, Math.round(particleBudget().smoke * radiusSpan(radius))));
  for (let n = 0; n < count; n++) {
    const column = n < Math.round(count * 0.42);
    const ang = rand() * Math.PI * 2;
    const reach = column ? radius * (0.05 + rand() * 0.1) : radius * (0.48 + rand() * 0.44);
    const speed = reach * SMOKE_DRAG;
    const brown = column ? 0.05 + rand() * 0.04 : 0.07 + rand() * 0.05;
    const life = 1.65 + rand() * 0.45;
    spawnSprite(pool, {
      x: Math.cos(ang) * radius * (column ? 0.02 : 0.05),
      y: originY + rand() * (column ? 0.25 : 0.15),
      z: Math.sin(ang) * radius * (column ? 0.02 : 0.05),
      vx: Math.cos(ang) * speed,
      vy: column ? 4.6 + rand() * 2.2 : 1.6 + rand() * 1.8,
      vz: Math.sin(ang) * speed,
      life,
      size: radius * (0.26 + rand() * 0.1),
      grow: radius * (0.18 + rand() * 0.1),
      spin: (rand() - 0.5) * 0.9,
      r: brown,
      g: brown * 0.72,
      b: brown * 0.55,
      seed: rand() * 40 + (column ? 1000 : 0),
    });
  }
}

export function stepSmoke(pool: SpritePool, elapsed: number, dt: number): void {
  const horiz = damp(SMOKE_DRAG, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    const column = pool.seed[i] >= 1000;
    const wobble = pool.seed[i] >= 1000 ? pool.seed[i] - 1000 : pool.seed[i];
    pool.vx[i] += Math.sin(elapsed * 1.15 + wobble) * 0.35 * dt;
    pool.vz[i] += Math.cos(elapsed * 0.95 + wobble * 1.3) * 0.35 * dt;
    pool.vx[i] *= horiz;
    pool.vz[i] *= horiz;
    const lift = column ? 1.05 : 0.22;
    const rise = column ? 0.16 : 0.55;
    pool.vy[i] = pool.vy[i] * damp(rise, dt) + lift * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const fadeIn = age < 0.22 ? age / 0.22 : 1;
    const fadeOut = 1 - smoothstep((age - 0.42) / 0.58);
    pool.opacity[i] = 0.84 * fadeIn * fadeOut;
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
  }
}

/** Additive orange-yellow billow. Lives about 0.3–0.5s, then the smoke carries the blast. */
export function seedBlastFire(pool: SpritePool, radius: number, rand: () => number, originY = 0.45): void {
  const count = Math.min(pool.capacity, Math.max(4, Math.round(particleBudget().fire * radiusSpan(radius))));
  for (let n = 0; n < count; n++) {
    const ang = rand() * Math.PI * 2;
    const spread = 0.15 + rand() * 0.45;
    const life = 0.42 + rand() * 0.12;
    const breadth = 2.5 + rand() * 0.7;
    spawnSprite(pool, {
      x: Math.cos(ang) * spread * Math.min(1, radius / 6),
      y: originY + (rand() - 0.2) * 0.35,
      z: Math.sin(ang) * spread * Math.min(1, radius / 6),
      vx: Math.cos(ang) * (0.15 + rand() * 0.35),
      vy: 0.4 + rand() * 1.1,
      vz: Math.sin(ang) * (0.15 + rand() * 0.35),
      life,
      size: breadth * Math.min(1, 0.55 + radius / 16),
      grow: (0.9 + rand() * 0.6) * Math.min(1, 0.55 + radius / 16),
      spin: (rand() - 0.5) * 0.8,
      r: 1,
      g: 0.7 + rand() * 0.3,
      b: 0.2,
      seed: rand() * 20 + n,
    });
  }
}

export function stepFire(pool: SpritePool, _elapsed: number, dt: number): void {
  const drag = damp(1.6, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vx[i] *= drag;
    pool.vz[i] *= drag;
    pool.vy[i] = pool.vy[i] * damp(0.7, dt) + 0.4 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const fadeIn = age < 0.04 ? age / 0.04 : 1;
    const fadeOut = 1 - smoothstep((age - 0.58) / 0.42);
    pool.opacity[i] = fadeIn * fadeOut;
    pool.r[i] = 1;
    pool.g[i] = 0.82 - age * 0.48;
    pool.b[i] = 0.22 - age * 0.14;
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
  }
}

/** Bright sparks that fly out and up, cool from yellow to deep red, and stay inside the blast. */
export function seedBlastEmbers(pool: SpritePool, radius: number, rand: () => number, originY = 0.45): void {
  const count = Math.min(pool.capacity, Math.max(12, Math.round(particleBudget().embers * radiusSpan(radius))));
  const scale = 0.72 + 0.28 * Math.min(radius / 8, 1.35);
  for (let n = 0; n < count; n++) {
    const ang = rand() * Math.PI * 2;
    const reach = radius * (0.2 + rand() * 0.58);
    const speed = reach * EMBER_DRAG;
    const life = 0.62 + rand() * 0.7;
    spawnSprite(pool, {
      x: Math.cos(ang) * radius * 0.03,
      y: originY + rand() * 0.2,
      z: Math.sin(ang) * radius * 0.03,
      vx: Math.cos(ang) * speed,
      vy: 3.4 + rand() * 6.4,
      vz: Math.sin(ang) * speed,
      life,
      size: (0.16 + rand() * 0.22) * scale,
      grow: 0.12 * scale,
      spin: (rand() - 0.5) * 6,
      r: 1,
      g: 0.9,
      b: 0.42,
      seed: rand() * 50 + n,
    });
  }
}

export function stepEmbers(pool: SpritePool, elapsed: number, dt: number): void {
  const horiz = damp(EMBER_DRAG, dt);
  const vert = damp(0.22, dt);
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
    if (pool.py[i] < 0.06) {
      pool.py[i] = 0.06;
      pool.vy[i] = 0;
      pool.vx[i] *= 0.25;
      pool.vz[i] *= 0.25;
      if (pool.life[i] > 0.18) pool.life[i] = 0.18;
    }
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    if (age < 0.32) {
      const t = age / 0.32;
      pool.r[i] = 1;
      pool.g[i] = 0.92 + (0.42 - 0.92) * t;
      pool.b[i] = 0.45 + (0.08 - 0.45) * t;
    } else {
      const t = (age - 0.32) / 0.68;
      pool.r[i] = 1 + (0.55 - 1) * t;
      pool.g[i] = 0.42 + (0.06 - 0.42) * t;
      pool.b[i] = 0.08 + (0.02 - 0.08) * t;
    }
    const fade = 1 - smoothstep((age - 0.55) / 0.45);
    const flick = 0.55 + 0.45 * Math.abs(Math.sin(elapsed * (16 + (pool.seed[i] % 5)) + pool.seed[i]));
    pool.opacity[i] = fade * flick;
    pool.size[i] = Math.max(0.08, pool.size0[i] + pool.grow[i] * age);
  }
}

/** Short dark puff used when spectral wolves appear and disappear. */
export function seedColdBurst(pool: SpritePool, rand: () => number): void {
  const count = Math.min(pool.capacity, 12);
  for (let n = 0; n < count; n++) {
    const ang = rand() * Math.PI * 2;
    const speed = 0.6 + rand() * 1.1;
    const life = 0.5 + rand() * 0.4;
    const shade = 0.08 + rand() * 0.08;
    spawnSprite(pool, {
      x: Math.cos(ang) * 0.12,
      y: 0.15 + rand() * 0.25,
      z: Math.sin(ang) * 0.12,
      vx: Math.cos(ang) * speed,
      vy: 0.35 + rand() * 0.9,
      vz: Math.sin(ang) * speed,
      life,
      size: 0.35 + rand() * 0.4,
      grow: 0.55 + rand() * 0.4,
      spin: (rand() - 0.5) * 1.1,
      r: shade * 0.7,
      g: shade * 0.82,
      b: shade + 0.08,
      seed: rand() * 20 + n,
    });
  }
}

export function stepPuff(pool: SpritePool, elapsed: number, dt: number): void {
  const horiz = damp(1.3, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vx[i] *= horiz;
    pool.vz[i] *= horiz;
    pool.vy[i] = pool.vy[i] * damp(0.6, dt) + 0.25 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const fadeIn = age < 0.08 ? age / 0.08 : 1;
    const fadeOut = 1 - smoothstep((age - 0.25) / 0.75);
    pool.opacity[i] = 0.55 * fadeIn * fadeOut;
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
  }
}

/** Cold-blue mist streaming off a wolf. Overwrites the oldest slot when the pool is full. */
export function emitMist(
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
    vx: backX * (0.9 + rand * 1.1) + (rand - 0.5) * 0.45,
    vy: 0.15 + rand * 0.55,
    vz: backZ * (0.9 + rand * 1.1) + (rand - 0.5) * 0.45,
    life: 0.65 + rand * 0.4,
    size: 0.55 + rand * 0.75,
    grow: 0.45 + rand * 0.55,
    spin: (rand - 0.5) * 1.2,
    r: 0.42 + rand * 0.12,
    g: 0.62 + rand * 0.08,
    b: 0.96,
    seed: rand * 30,
  });
}

export function emitWisp(
  pool: SpritePool,
  x: number,
  y: number,
  z: number,
  backX: number,
  backZ: number,
  rand: number,
): void {
  emitMist(pool, x, y, z, backX, backZ, rand);
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
    vx: backX * 0.7 + (rand - 0.5) * 0.5,
    vy: 0.2 + rand * 0.6,
    vz: backZ * 0.7 + (rand - 0.5) * 0.5,
    life: 0.4 + rand * 0.35,
    size: 0.1 + rand * 0.1,
    grow: 0.04,
    spin: (rand - 0.5) * 4,
    r: 0.62,
    g: 0.82,
    b: 1,
    seed: rand * 30,
  });
}

export function stepMist(pool: SpritePool, _elapsed: number, dt: number): void {
  const drag = damp(0.9, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vx[i] *= drag;
    pool.vz[i] *= drag;
    pool.vy[i] = pool.vy[i] * damp(0.45, dt) + 0.2 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const fadeIn = age < 0.08 ? age / 0.08 : 1;
    const fadeOut = 1 - smoothstep((age - 0.28) / 0.72);
    pool.opacity[i] = 0.68 * fadeIn * fadeOut;
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
  }
}

export function stepWisps(pool: SpritePool, elapsed: number, dt: number): void {
  stepMist(pool, elapsed, dt);
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
    pool.vy[i] = pool.vy[i] * damp(0.5, dt) + 0.3 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    pool.r[i] = 0.7 + (0.3 - 0.7) * age;
    pool.g[i] = 0.86 + (0.48 - 0.86) * age;
    pool.b[i] = 1 + (0.7 - 1) * age;
    const flick = 0.6 + 0.4 * Math.abs(Math.sin(elapsed * 14 + pool.seed[i]));
    pool.opacity[i] = (1 - smoothstep((age - 0.2) / 0.8)) * flick;
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
    pool.rot[i] += pool.spin[i] * dt;
  }
}

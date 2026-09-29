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
 * Overlapping puffs of mixed sizes. They start already stacked into a column
 * so the cloud reads as one mass, then billow and mushroom.
 */
export function seedBlastSmoke(pool: SpritePool, radius: number, rand: () => number, originY = 0.45): void {
  const budget = particleBudget().smoke;
  const count = Math.min(pool.capacity, Math.max(10, Math.round(budget * 2 * radiusSpan(radius))));
  const scale = 0.82 + 0.18 * Math.min(radius / 8, 1.25);
  for (let n = 0; n < count; n++) {
    const column = n < Math.round(count * 0.72);
    const ang = rand() * Math.PI * 2;
    const band = n / count;
    const reach = column ? radius * (0.02 + rand() * 0.06) : radius * (0.08 + rand() * 0.14);
    const speed = reach * (column ? 0.55 : SMOKE_DRAG);
    const life = 2.05 + rand() * 0.55;
    const sizeRoll = rand();
    const size = (sizeRoll < 0.28 ? 0.62 + rand() * 0.35 : sizeRoll < 0.7 ? 1.05 + rand() * 0.45 : 1.7 + rand() * 0.7) * scale;
    spawnSprite(pool, {
      x: Math.cos(ang) * (column ? 0.15 + rand() * 0.55 : 0.25 + rand() * 0.7),
      y: originY + band * (column ? 2.15 : 0.35) + (rand() - 0.5) * 0.28,
      z: Math.sin(ang) * (column ? 0.15 + rand() * 0.55 : 0.25 + rand() * 0.7),
      vx: Math.cos(ang) * speed,
      vy: column ? 1.5 + rand() * 2.6 : 0.35 + rand() * 0.7,
      vz: Math.sin(ang) * speed,
      life,
      size,
      grow: (0.22 + rand() * 0.38) * scale,
      spin: (rand() - 0.5) * 0.4,
      r: 0.13,
      g: 0.135,
      b: 0.145,
      seed: rand() * 40 + (column ? 1000 : 0),
    });
  }
}

/** Low dust that crawls outward along the ground under the column. */
export function seedDustSkirt(pool: SpritePool, radius: number, rand: () => number, originY = 0.08): void {
  const count = Math.min(pool.capacity, getSkirtCount());
  for (let n = 0; n < count; n++) {
    const ang = (n / count) * Math.PI * 2 + rand() * 0.4;
    const speed = 1.1 + rand() * 1.8;
    spawnSprite(pool, {
      x: Math.cos(ang) * (0.2 + rand() * 0.35),
      y: originY + rand() * 0.08,
      z: Math.sin(ang) * (0.2 + rand() * 0.35),
      vx: Math.cos(ang) * speed * Math.min(1, radius / 8),
      vy: 0.05 + rand() * 0.2,
      vz: Math.sin(ang) * speed * Math.min(1, radius / 8),
      life: 1.5 + rand() * 0.6,
      size: (0.7 + rand() * 1.15) * Math.min(1, 0.65 + radius / 20),
      grow: 0.35 + rand() * 0.4,
      spin: (rand() - 0.5) * 0.5,
      r: 0.55,
      g: 0.48,
      b: 0.36,
      seed: rand() * 8 + n,
    });
  }
}

function getSkirtCount(): number {
  return particleBudget().smoke > 8 ? 14 : 8;
}

export function stepDustSkirt(pool: SpritePool, _elapsed: number, dt: number): void {
  const drag = damp(1.15, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vx[i] *= drag;
    pool.vz[i] *= drag;
    pool.vy[i] = Math.max(-0.05, pool.vy[i] - 0.8 * dt);
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] = Math.max(0.12, Math.min(0.42, pool.py[i] + pool.vy[i] * dt));
    pool.pz[i] += pool.vz[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    pool.opacity[i] = 0.55 * (1 - smoothstep((age - 0.35) / 0.65));
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
    pool.rot[i] += pool.spin[i] * dt;
  }
}

export function stepSmoke(pool: SpritePool, elapsed: number, dt: number): void {
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    const column = pool.seed[i] >= 1000;
    const wobble = column ? pool.seed[i] - 1000 : pool.seed[i];
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const mushroom = column && age > 0.32;
    const horiz = damp(mushroom ? 0.75 : SMOKE_DRAG, dt);
    pool.vx[i] += Math.sin(elapsed * 0.8 + wobble) * 0.2 * dt;
    pool.vz[i] += Math.cos(elapsed * 0.7 + wobble * 1.3) * 0.2 * dt;
    if (mushroom) {
      const radial = Math.hypot(pool.px[i], pool.pz[i]);
      const nx = radial > 0.08 ? pool.px[i] / radial : Math.cos(wobble);
      const nz = radial > 0.08 ? pool.pz[i] / radial : Math.sin(wobble);
      const push = 3.2 * Math.min(1, (age - 0.32) / 0.34);
      pool.vx[i] += nx * push * dt;
      pool.vz[i] += nz * push * dt;
    }
    pool.vx[i] *= horiz;
    pool.vz[i] *= horiz;
    const lift = column ? (mushroom ? 0.28 : 1.2) : 0.16;
    const rise = column ? (mushroom ? 1.1 : 0.18) : 0.55;
    pool.vy[i] = pool.vy[i] * damp(rise, dt) + lift * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    pool.rot[i] += pool.spin[i] * dt;
    const fadeIn = age < 0.06 ? age / 0.06 : 1;
    const fadeOut = 1 - smoothstep((age - 0.28) / 0.42);
    pool.opacity[i] = fadeIn * fadeOut;
    const linger = smoothstep((age - 0.16) / 0.34);
    const grey = 0.04 + (0.18 - 0.04) * linger;
    pool.r[i] = grey;
    pool.g[i] = grey;
    pool.b[i] = grey + 0.004;
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
    const point = rand() < 0.28;
    spawnSprite(pool, {
      x: Math.cos(ang) * radius * 0.03,
      y: originY + rand() * 0.2,
      z: Math.sin(ang) * radius * 0.03,
      vx: Math.cos(ang) * speed,
      vy: 3.4 + rand() * 6.4,
      vz: Math.sin(ang) * speed,
      life,
      size: (point ? 0.02 + rand() * 0.016 : 0.028 + rand() * 0.016) * scale,
      grow: 0.004 * scale,
      spin: (rand() - 0.5) * 2,
      r: 1,
      g: 0.9,
      b: 0.42,
      seed: point ? -(rand() * 40 + n + 1) : rand() * 40 + n + 1000,
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
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
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
      size: 0.2 + rand() * 0.18,
      grow: 0.14 + rand() * 0.14,
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
    vx: backX * (0.2 + rand * 0.25) + (rand - 0.5) * 0.08,
    vy: 0.04 + rand * 0.12,
    vz: backZ * (0.2 + rand * 0.25) + (rand - 0.5) * 0.08,
    life: 0.55 + rand * 0.35,
    size: 0.4 + rand * 0.75,
    grow: 0.04 + rand * 0.12,
    spin: (rand - 0.5) * 0.6,
    r: 0.46,
    g: 0.56,
    b: 0.64,
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
  const drag = damp(0.35, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vx[i] *= drag;
    pool.vz[i] *= drag;
    pool.vy[i] = pool.vy[i] * damp(0.45, dt) + 0.08 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const fadeIn = age < 0.08 ? age / 0.08 : 1;
    const fadeOut = 1 - smoothstep((age - 0.22) / 0.78);
    const vary = Math.abs(pool.seed[i] % 1);
    pool.opacity[i] = (0.15 + vary * 0.1) * fadeIn * fadeOut;
    pool.size[i] = Math.min(1.5, pool.size0[i] + pool.grow[i] * age);
  }
}

export function stepWisps(pool: SpritePool, elapsed: number, dt: number): void {
  stepMist(pool, elapsed, dt);
}

/** Short electric or metallic sparks. Leaves the authored color alone. */
export function seedBurstSparks(
  pool: SpritePool,
  rand: () => number,
  count: number,
  color: [number, number, number],
  dir: [number, number, number] = [0, 1, 0],
  scale = 1,
): void {
  const len = Math.hypot(dir[0], dir[1], dir[2]) || 1;
  const dx = dir[0] / len;
  const dy = dir[1] / len;
  const dz = dir[2] / len;
  const n = Math.min(pool.capacity, count);
  for (let i = 0; i < n; i++) {
    const point = rand() < 0.22;
    const spread = (rand() - 0.5) * 0.85;
    const side = (rand() - 0.5) * 0.85;
    const kick = 0.75 + 0.25 * scale;
    spawnSprite(pool, {
      x: (rand() - 0.5) * 0.08,
      y: 0.12 + rand() * 0.2,
      z: (rand() - 0.5) * 0.08,
      vx: (dx * (4.2 + rand() * 5.4) + spread * 1.4) * kick,
      vy: (dy * (2.2 + rand() * 3.2) + 1.1 + rand() * 1.6) * kick,
      vz: (dz * (4.2 + rand() * 5.4) + side * 1.4) * kick,
      life: 0.42 + rand() * 0.32,
      size: ((point ? 0.09 : 0.16) + rand() * 0.07) * scale,
      grow: 0,
      spin: 0,
      r: color[0],
      g: color[1],
      b: color[2],
      seed: point ? -(i + 1) : i + 3,
    });
  }
}

export function seedOzone(pool: SpritePool, rand: () => number, count: number): void {
  const n = Math.min(pool.capacity, count);
  for (let i = 0; i < n; i++) {
    const ang = rand() * Math.PI * 2;
    spawnSprite(pool, {
      x: Math.cos(ang) * (0.2 + rand() * 0.45),
      y: 0.25 + rand() * 0.35,
      z: Math.sin(ang) * (0.2 + rand() * 0.45),
      vx: Math.cos(ang) * 0.25,
      vy: 0.25 + rand() * 0.35,
      vz: Math.sin(ang) * 0.25,
      life: 0.7 + rand() * 0.35,
      size: 0.55 + rand() * 0.35,
      grow: 0.12,
      spin: (rand() - 0.5) * 0.4,
      r: 0.7,
      g: 0.8,
      b: 0.9,
      seed: rand() * 10 + i,
    });
  }
}

export function seedRisingEmbers(pool: SpritePool, rand: () => number, count: number): void {
  const n = Math.min(pool.capacity, count);
  for (let i = 0; i < n; i++) {
    const ang = rand() * Math.PI * 2;
    const point = rand() < 0.25;
    spawnSprite(pool, {
      x: Math.cos(ang) * rand() * 0.28,
      y: 0.15 + rand() * 0.2,
      z: Math.sin(ang) * rand() * 0.28,
      vx: Math.cos(ang) * (0.15 + rand() * 0.35),
      vy: 1.6 + rand() * 1.8,
      vz: Math.sin(ang) * (0.15 + rand() * 0.35),
      life: 0.75 + rand() * 0.4,
      size: point ? 0.16 : 0.22 + rand() * 0.12,
      grow: 0.01,
      spin: 0,
      r: 1,
      g: 0.72,
      b: 0.28,
      seed: point ? -(i + 1) : i + 4,
    });
  }
}

export function stepGlints(pool: SpritePool, _elapsed: number, dt: number): void {
  const drag = damp(2.4, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vx[i] *= drag;
    pool.vz[i] *= drag;
    pool.vy[i] = pool.vy[i] * damp(0.5, dt) - 1.6 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    pool.opacity[i] = 1 - smoothstep((age - 0.35) / 0.65);
    pool.size[i] = pool.size0[i];
  }
}

export function stepRising(pool: SpritePool, _elapsed: number, dt: number): void {
  const drag = damp(0.8, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    if (pool.life[i] <= 0) {
      killSprite(pool, i);
      continue;
    }
    pool.vx[i] *= drag;
    pool.vz[i] *= drag;
    pool.vy[i] = pool.vy[i] * damp(0.35, dt) + 0.55 * dt;
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] += pool.vy[i] * dt;
    pool.pz[i] += pool.vz[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const fade = 1 - smoothstep((age - 0.45) / 0.55);
    pool.opacity[i] = fade;
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
  }
}

export function seedDustPuff(pool: SpritePool, rand: () => number, count: number): void {
  const n = Math.min(pool.capacity, count);
  for (let i = 0; i < n; i++) {
    const ang = rand() * Math.PI * 2;
    const speed = 0.7 + rand() * 1.3;
    spawnSprite(pool, {
      x: Math.cos(ang) * rand() * 0.12,
      y: 0.06 + rand() * 0.12,
      z: Math.sin(ang) * rand() * 0.12,
      vx: Math.cos(ang) * speed,
      vy: 0.35 + rand() * 0.7,
      vz: Math.sin(ang) * speed,
      life: 0.4 + rand() * 0.35,
      size: 0.32 + rand() * 0.28,
      grow: 0.22,
      spin: (rand() - 0.5) * 0.8,
      r: 0.62,
      g: 0.48,
      b: 0.28,
      seed: rand() * 8 + i,
    });
  }
}

export function stepDust(pool: SpritePool, _elapsed: number, dt: number): void {
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
    pool.vy[i] = Math.max(-0.15, pool.vy[i] - 1.4 * dt);
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] = Math.max(0.04, pool.py[i] + pool.vy[i] * dt);
    pool.pz[i] += pool.vz[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    pool.opacity[i] = 0.7 * (1 - smoothstep((age - 0.2) / 0.8));
    pool.size[i] = pool.size0[i] + pool.grow[i] * age;
    pool.rot[i] += pool.spin[i] * dt;
  }
}

export function stepHaze(pool: SpritePool, elapsed: number, dt: number): void {
  stepMist(pool, elapsed, dt);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.opacity[i] *= 0.42;
  }
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

function shadowInk(rand: () => number): [number, number, number] {
  const roll = rand();
  if (roll < 0.4) return [0.055, 0.026, 0.09];
  if (roll < 0.75) return [0.11, 0.045, 0.16];
  return [0.17, 0.07, 0.26];
}

/** Dark wisps that collapse toward the caster before the wave leaves. */
export function seedShadowSuck(pool: SpritePool, rand: () => number, count: number): void {
  const n = Math.min(pool.capacity, count);
  for (let i = 0; i < n; i++) {
    const ang = rand() * Math.PI * 2;
    const dist = 1.15 + rand() * 1.7;
    const tangent = (rand() - 0.5) * 1.6;
    const speed = 4.6 + rand() * 2.6;
    const [r, g, b] = shadowInk(rand);
    spawnSprite(pool, {
      x: Math.cos(ang) * dist,
      y: 0.18 + rand() * 0.4,
      z: Math.sin(ang) * dist,
      vx: -Math.cos(ang) * speed - Math.sin(ang) * tangent,
      vy: 0.05 + rand() * 0.28,
      vz: -Math.sin(ang) * speed + Math.cos(ang) * tangent,
      life: 0.28 + rand() * 0.1,
      size: 0.85 + rand() * 0.65,
      grow: 0,
      spin: (rand() - 0.5) * 1.4,
      r,
      g,
      b,
      seed: ang + i,
    });
  }
}

export function stepShadowSuck(pool: SpritePool, _elapsed: number, dt: number): void {
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    pool.life[i] -= dt;
    const dist = Math.hypot(pool.px[i], pool.pz[i]);
    if (pool.life[i] <= 0 || dist < 0.12) {
      killSprite(pool, i);
      continue;
    }
    if (dist > 0.05) {
      pool.vx[i] += (-pool.px[i] / dist) * 9 * dt;
      pool.vz[i] += (-pool.pz[i] / dist) * 9 * dt;
    }
    pool.px[i] += pool.vx[i] * dt;
    pool.py[i] = Math.max(0.1, Math.min(0.85, pool.py[i] + pool.vy[i] * dt));
    pool.pz[i] += pool.vz[i] * dt;
    pool.rot[i] += pool.spin[i] * dt;
    const age = 1 - pool.life[i] / pool.maxLife[i];
    const fadeIn = age < 0.1 ? age / 0.1 : 1;
    const fadeOut = 1 - smoothstep((age - 0.4) / 0.6);
    pool.opacity[i] = 0.88 * fadeIn * fadeOut;
    pool.size[i] = pool.size0[i] * (1 - age * 0.4);
  }
}

/**
 * Ground wave. `grow` stores how far along the radius this puff is meant to
 * stop. The outer edge of every puff is clamped to `radius`.
 */
export function seedShadowWave(
  pool: SpritePool,
  radius: number,
  rand: () => number,
  count: number,
  sizeScale = 1,
): void {
  const n = Math.min(pool.capacity, count);
  for (let i = 0; i < n; i++) {
    const ang = (i / Math.max(1, n)) * Math.PI * 2 + (rand() - 0.5) * 0.55;
    const band = i / Math.max(1, n);
    const front = band < 0.4;
    const targetFrac = front
      ? 0.94 + rand() * 0.06
      : band < 0.72
        ? 0.46 + rand() * 0.3
        : 0.16 + rand() * 0.24;
    const size = (front ? 1.28 + rand() * 0.52 : 1.05 + rand() * 0.75) * sizeScale;
    const [r, g, b] = shadowInk(rand);
    spawnSprite(pool, {
      x: Math.cos(ang) * 0.25,
      y: 0.16,
      z: Math.sin(ang) * 0.25,
      vx: rand() * 10,
      vy: rand() * 0.08,
      vz: 0,
      life: 4,
      size,
      grow: Math.min(1, targetFrac),
      spin: (rand() - 0.5) * 0.55,
      r,
      g,
      b,
      seed: ang,
    });
  }
}

export const SHADOW_WAVE_START = 0.2;
export const SHADOW_WAVE_DUR = 0.7;

export function stepShadowWave(pool: SpritePool, radius: number, elapsed: number, dt: number): void {
  const fadeIn = smoothstep((elapsed - 0.16) / 0.16);
  const fadeOut = 1 - smoothstep((elapsed - 1.25) / 1.2);
  for (let i = 0; i < pool.capacity; i++) {
    if (pool.active[i] === 0) continue;
    const ang = pool.seed[i];
    const targetFrac = pool.grow[i];
    const delay = (1 - targetFrac) * 0.14;
    const local = smoothstep((elapsed - SHADOW_WAVE_START - delay) / SHADOW_WAVE_DUR);
    const size = pool.size0[i] * (0.58 + 0.48 * local);
    const half = size * 0.5;
    const limit = Math.max(0.2, radius - half);
    const end = Math.min(radius * targetFrac, limit);
    const start = 0.22;
    const dist = start + (end - start) * local;
    const wobble = Math.sin(elapsed * 1.7 + pool.vx[i]) * 0.14 * (1 - local * 0.35);
    let x = Math.cos(ang) * dist - Math.sin(ang) * wobble;
    let z = Math.sin(ang) * dist + Math.cos(ang) * wobble;
    const radial = Math.hypot(x, z);
    if (radial > limit) {
      const scale = limit / radial;
      x *= scale;
      z *= scale;
    }
    pool.px[i] = x;
    pool.pz[i] = z;
    const crest = targetFrac > 0.9 ? Math.sin(Math.min(1, local) * Math.PI) * 0.22 : 0.05;
    const bob = Math.sin(elapsed * 2.2 + ang * 3) * 0.025;
    pool.py[i] = 0.15 + crest + bob + pool.vy[i];
    pool.size[i] = size;
    pool.rot[i] += pool.spin[i] * dt;
    pool.opacity[i] = fadeIn * fadeOut * (0.7 + targetFrac * 0.22);
  }
}

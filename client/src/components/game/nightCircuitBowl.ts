/**
 * Night Circuit grandstand. Visual only: these boxes are not colliders.
 *
 * Every distance is measured from the live floor. NEON_HALF_X / NEON_HALF_Z
 * are the court's half-extents (12 and 8 today, 24 by 16). A later arena
 * scale that multiplies those halves, or a parent scale around this mount,
 * moves the bowl with the floor. The pit clearances below stay the gap
 * past whatever that edge is.
 */
import { NEON_HALF_X, NEON_HALF_Z } from "@/lib/neonCourt";

export type BowlFace = "north" | "south" | "east" | "west";

export type BowlDeck = {
  pos: [number, number, number];
  size: [number, number, number];
  face: BowlFace;
};

/** Meters of empty air past the floor edge. A full-speed marble drops under these faces. */
export const FAR_PIT_M = 3.45;
export const SIDE_PIT_M = 3.8;
export const NEAR_PIT_M = 10.5;

const hx = NEON_HALF_X;
const hz = NEON_HALF_Z;

function deck(pos: [number, number, number], size: [number, number, number], face: BowlFace): BowlDeck {
  return { pos, size, face };
}

function buildFar(): BowlDeck[] {
  const rows = [
    { depth: hz * 0.34, bottom: 1.25, height: 2.45, width: hx * 5.5 },
    { depth: hz * 0.42, bottom: 2.35, height: hz * 0.7, width: hx * 6.0 },
    { depth: hz * 0.46, bottom: 4.15, height: hz * 0.64, width: hx * 6.5 },
    { depth: hz * 0.5, bottom: 5.85, height: hz * 0.58, width: hx * 7.0 },
    { depth: hz * 0.54, bottom: 7.35, height: hz * 0.52, width: hx * 7.5 },
    { depth: hz * 0.58, bottom: 8.55, height: hz * 0.46, width: hx * 8.0 },
  ];
  let front = -(hz + FAR_PIT_M);
  return rows.map((row) => {
    const cz = front - row.depth / 2;
    const cy = row.bottom + row.height / 2;
    const built = deck([0, cy, cz], [row.width, row.height, row.depth], "north");
    front -= row.depth + 0.5;
    return built;
  });
}

function buildSides(): BowlDeck[] {
  const rows = [
    { thick: hx * 0.46, bottom: 1.4, height: 4.4, length: hz * 2.35 },
    { thick: hx * 0.52, bottom: 2.2, height: hz * 0.78, length: hz * 2.65 },
    { thick: hx * 0.56, bottom: 3.4, height: hz * 0.82, length: hz * 2.95 },
  ];
  const north = -(hz + FAR_PIT_M) + 0.28;
  let inner = hx + SIDE_PIT_M;
  const decks: BowlDeck[] = [];
  for (const row of rows) {
    const south = north + row.length;
    const cz = (north + south) / 2;
    const cy = row.bottom + row.height / 2;
    const cx = inner + row.thick / 2;
    decks.push(deck([-cx, cy, cz], [row.thick, row.height, row.length], "west"));
    decks.push(deck([cx, cy, cz], [row.thick, row.height, row.length], "east"));
    inner += row.thick + 0.5;
  }
  return decks;
}

function buildNear(): BowlDeck[] {
  const rows = [
    { depth: hz * 0.62, bottom: 0.45, height: 4.6, width: hx * 5.8 },
    { depth: hz * 0.7, bottom: 3.0, height: hz * 0.9, width: hx * 6.6 },
    { depth: hz * 0.76, bottom: 5.6, height: hz * 0.95, width: hx * 7.4 },
  ];
  let inner = hz + NEAR_PIT_M;
  return rows.map((row) => {
    const cz = inner + row.depth / 2;
    const cy = row.bottom + row.height / 2;
    const built = deck([0, cy, cz], [row.width, row.height, row.depth], "south");
    inner += row.depth + 0.7;
    return built;
  });
}

export const FAR_DECKS: BowlDeck[] = buildFar();
export const SIDE_DECKS: BowlDeck[] = buildSides();
export const END_DECKS: BowlDeck[] = buildNear();
export const BOWL_DECKS: BowlDeck[] = [...FAR_DECKS, ...SIDE_DECKS, ...END_DECKS];

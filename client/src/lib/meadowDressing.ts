/**
 * Forest around the meadow clearing. Pieces sit outside the knockoff line,
 * on the decorative ground, and their crowns stay under the gameplay camera.
 * The arena draws these exact sizes. Nothing here is a collider.
 */
export const MEADOW_GROUND_Y = -0.48;
export const MEADOW_BACKDROP_RADIUS = 168;
export const MEADOW_GROUND_RADIUS = 174;

export interface ForestPiece {
  id: string;
  kind: "tree" | "bush";
  x: number;
  z: number;
  rot: number;
  trunk: number;
  canopy: number;
}

function seeded(seed: number) {
  let t = seed >>> 0;
  return () => {
    t += 0x6d2b79f5;
    let x = Math.imul(t ^ (t >>> 15), 1 | t);
    x ^= x + Math.imul(x ^ (x >>> 7), 61 | x);
    return ((x ^ (x >>> 14)) >>> 0) / 4294967296;
  };
}

function ring(
  rand: () => number,
  pieces: ForestPiece[],
  id: string,
  kind: "tree" | "bush",
  count: number,
  r0: number,
  r1: number,
  trunk0: number,
  trunk1: number,
  can0: number,
  can1: number,
) {
  for (let i = 0; i < count; i += 1) {
    const angle = (i / count) * Math.PI * 2 + (rand() - 0.5) * ((Math.PI * 2) / count) * 0.65;
    const dist = r0 + rand() * (r1 - r0);
    const trunk = trunk0 + rand() * (trunk1 - trunk0);
    const canopy = can0 + rand() * (can1 - can0);
    pieces.push({
      id: `${id}-${i}`,
      kind,
      x: Math.cos(angle) * dist,
      z: Math.sin(angle) * dist,
      rot: rand() * Math.PI * 2,
      trunk,
      canopy,
    });
  }
}

export function meadowForestPieces(): ForestPiece[] {
  const rand = seeded(77);
  const pieces: ForestPiece[] = [];
  ring(rand, pieces, "bush", "bush", 28, 16.7, 17.6, 0, 0, 0.55, 0.85);
  ring(rand, pieces, "tree", "tree", 16, 18.4, 22.5, 1.15, 1.85, 0.85, 1.2);
  ring(rand, pieces, "mid", "tree", 22, 25, 38, 2.8, 4.4, 1.6, 2.3);
  ring(rand, pieces, "far", "tree", 32, 42, 72, 4.2, 6.2, 2.2, 3.2);
  ring(rand, pieces, "horizon", "tree", 48, 78, 112, 5.5, 8, 2.8, 4);
  ring(rand, pieces, "haze", "tree", 64, 118, 152, 6.5, 9.5, 3.4, 4.8);
  return pieces;
}

/** World height of the highest leaf. The mesh is built so it does not exceed this. */
export function forestTopY(piece: ForestPiece): number {
  if (piece.kind === "bush") return MEADOW_GROUND_Y + piece.canopy * 1.15;
  return MEADOW_GROUND_Y + piece.trunk + piece.canopy * 0.92;
}

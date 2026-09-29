/**
 * Forest ring around the meadow clearing. Pieces sit outside the knockoff
 * line and their crowns stay under the gameplay camera's sightlines.
 * The arena component draws these exact sizes.
 */
export const MEADOW_GROUND_Y = -0.06;

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

export function meadowForestPieces(): ForestPiece[] {
  const rand = seeded(77);
  const pieces: ForestPiece[] = [];
  for (let i = 0; i < 12; i += 1) {
    const angle = (i / 12) * Math.PI * 2 + (rand() - 0.5) * 0.18;
    const dist = 17.55 + rand() * 0.55;
    pieces.push({
      id: `tree-${i}`,
      kind: "tree",
      x: Math.cos(angle) * dist,
      z: Math.sin(angle) * dist,
      rot: rand() * Math.PI * 2,
      trunk: 0.85 + rand() * 0.35,
      canopy: 0.72 + rand() * 0.22,
    });
  }
  for (let i = 0; i < 22; i += 1) {
    const angle = (i / 22) * Math.PI * 2 + 0.07 + rand() * 0.06;
    const dist = 16.28 + (i % 2) * 0.35 + rand() * 0.2;
    pieces.push({
      id: `bush-${i}`,
      kind: "bush",
      x: Math.cos(angle) * dist,
      z: Math.sin(angle) * dist,
      rot: rand() * Math.PI,
      trunk: 0,
      canopy: 0.48 + rand() * 0.18,
    });
  }
  return pieces;
}

/** World height of the highest leaf. The mesh is built so it does not exceed this. */
export function forestTopY(piece: ForestPiece): number {
  if (piece.kind === "bush") return MEADOW_GROUND_Y + piece.canopy * 1.15;
  return MEADOW_GROUND_Y + piece.trunk + piece.canopy * 0.92;
}

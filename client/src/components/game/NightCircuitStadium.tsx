import * as THREE from "three";
import { NEON_HALF_X } from "@/lib/neonCourt";
import { NightCircuitCrowd } from "./NightCircuitCrowd";
import { END_DECKS, FAR_DECKS, SIDE_DECKS, type BowlDeck } from "./nightCircuitBowl";

const GLOSS = {
  color: "#070b16",
  emissive: "#0c1428",
  emissiveIntensity: 0.16,
  metalness: 0.84,
  roughness: 0.18,
  envMapIntensity: 0.62,
} as const;

function DeckMesh({ deck, envMap }: { deck: BowlDeck; envMap: THREE.Texture | null }) {
  return (
    <mesh position={deck.pos} receiveShadow>
      <boxGeometry args={deck.size} />
      <meshStandardMaterial {...GLOSS} envMap={envMap ?? undefined} />
    </mesh>
  );
}

/** Soft cyan / magenta line along the near top edge, with a hole behind the scoreboard. */
function RimGlow({ deck }: { deck: BowlDeck }) {
  const [cx, cy, cz] = deck.pos;
  const [sx, sy, sz] = deck.size;
  const y = cy + sy / 2 + 0.04;
  const z = cz + sz / 2 + 0.05;
  const hole = NEON_HALF_X * 0.7;
  const wing = (sx - hole) / 2;
  const xWing = hole / 2 + wing / 2;
  return (
    <group>
      <mesh position={[cx - xWing, y, z]}>
        <boxGeometry args={[wing, 0.07, 0.1]} />
        <meshStandardMaterial color="#7ee7f2" emissive="#3ec8e0" emissiveIntensity={0.72} toneMapped={false} />
      </mesh>
      <mesh position={[cx + xWing, y, z]}>
        <boxGeometry args={[wing, 0.07, 0.1]} />
        <meshStandardMaterial color="#f0a0d4" emissive="#e060b8" emissiveIntensity={0.72} toneMapped={false} />
      </mesh>
    </group>
  );
}

/** One short truss and two lamps, set out toward a side so the scoreboard stays clear. */
function SideRig({ deck, sign }: { deck: BowlDeck; sign: number }) {
  const [cx, cy, cz] = deck.pos;
  const [, sy, sz] = deck.size;
  const x = sign * NEON_HALF_X * 1.15;
  const y = cy + sy / 2 + 0.06;
  const z = cz + sz / 2 - 0.02;
  const tint = sign < 0 ? "#3ec8e0" : "#e060b8";
  return (
    <group position={[cx + x, y, z]}>
      <mesh>
        <boxGeometry args={[NEON_HALF_X * 0.42, 0.08, 0.1]} />
        <meshStandardMaterial color="#141a28" metalness={0.78} roughness={0.28} />
      </mesh>
      {[-0.32, 0.32].map((t) => (
        <mesh key={t} position={[t * NEON_HALF_X * 0.36, -0.16, 0.06]}>
          <sphereGeometry args={[0.11, 10, 8]} />
          <meshStandardMaterial color="#f4f7ff" emissive={tint} emissiveIntensity={1.15} toneMapped={false} />
        </mesh>
      ))}
    </group>
  );
}

/**
 * Doubled Night Circuit bowl. The near far-tier carries a soft rim and two
 * side rigs. Crowd lights mount with it.
 */
export function NightCircuitStadium({ envMap }: { envMap: THREE.Texture | null }) {
  const lip = FAR_DECKS[0];
  return (
    <group>
      {FAR_DECKS.map((deck) => (
        <DeckMesh key={`far-${deck.pos.join(",")}`} deck={deck} envMap={envMap} />
      ))}
      {SIDE_DECKS.map((deck) => (
        <DeckMesh key={`side-${deck.pos.join(",")}`} deck={deck} envMap={envMap} />
      ))}
      {END_DECKS.map((deck) => (
        <DeckMesh key={`end-${deck.pos.join(",")}`} deck={deck} envMap={envMap} />
      ))}
      <RimGlow deck={lip} />
      <SideRig deck={lip} sign={-1} />
      <SideRig deck={lip} sign={1} />
      <NightCircuitCrowd />
    </group>
  );
}

import * as THREE from "three";
import { NightCircuitCrowd } from "./NightCircuitCrowd";
import { END_DECKS, FAR_DECKS, SIDE_DECKS, type BowlDeck } from "./nightCircuitBowl";

const GLOSS = {
  color: "#070b16",
  emissive: "#0c1428",
  emissiveIntensity: 0.12,
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

function RoofLip({ deck, envMap }: { deck: BowlDeck; envMap: THREE.Texture | null }) {
  const [cx, cy, cz] = deck.pos;
  const [sx, sy, sz] = deck.size;
  const y = cy + sy / 2 + 0.12;
  const z = cz + sz / 2 + 0.28;
  return (
    <mesh position={[cx, y, z]} receiveShadow>
      <boxGeometry args={[sx * 0.72, 0.22, 0.7]} />
      <meshStandardMaterial {...GLOSS} envMap={envMap ?? undefined} />
    </mesh>
  );
}

/**
 * Doubled Night Circuit bowl. Decks stay outside the fall-off pit.
 * A single dark lip marks the rear roof. Crowd lights mount with it.
 */
export function NightCircuitStadium({ envMap }: { envMap: THREE.Texture | null }) {
  const upper = FAR_DECKS[FAR_DECKS.length - 1];

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
      <RoofLip deck={upper} envMap={envMap} />
      <NightCircuitCrowd />
    </group>
  );
}

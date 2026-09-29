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
  const y = cy + sy / 2 + 0.16;
  const z = cz + sz / 2 + 0.55;
  return (
    <mesh position={[cx, y, z]} receiveShadow>
      <boxGeometry args={[sx + 1.2, 0.28, 1.35]} />
      <meshStandardMaterial {...GLOSS} envMap={envMap ?? undefined} />
    </mesh>
  );
}

function CrownRig({ deck }: { deck: BowlDeck }) {
  const [cx, cy, cz] = deck.pos;
  const [sx, sy, sz] = deck.size;
  const y = cy + sy / 2 + 0.42;
  const z = cz + sz / 2 - 0.05;
  const lamps = [-0.82, -0.5, -0.18, 0.18, 0.5, 0.82];
  return (
    <group position={[cx, y, z]}>
      <mesh>
        <boxGeometry args={[sx * 0.86, 0.12, 0.16]} />
        <meshStandardMaterial color="#141a28" metalness={0.78} roughness={0.24} />
      </mesh>
      {lamps.map((t) => {
        const x = t * sx * 0.42;
        const tint = x < 0 ? "#22e7ff" : "#ff2bd6";
        return (
          <mesh key={t} position={[x, -0.22, 0.08]}>
            <sphereGeometry args={[0.16, 10, 8]} />
            <meshStandardMaterial color="#fff6ea" emissive={tint} emissiveIntensity={2.2} toneMapped={false} />
          </mesh>
        );
      })}
    </group>
  );
}

/**
 * The Night Circuit bowl: tall far tiers, flank decks, and a near end,
 * with a roof lip and a small crown rig. Crowd lights mount with it.
 */
export function NightCircuitStadium({ envMap }: { envMap: THREE.Texture | null }) {
  const crown = FAR_DECKS[2];
  const upper = FAR_DECKS[FAR_DECKS.length - 1];
  const lip = FAR_DECKS[0];
  const lipY = lip.pos[1] + lip.size[1] / 2 + 0.05;
  const lipZ = lip.pos[2] + lip.size[2] / 2 + 0.04;
  const lipHalf = lip.size[0] / 2 - 0.4;

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
      <RoofLip deck={crown} envMap={envMap} />
      <RoofLip deck={upper} envMap={envMap} />
      <CrownRig deck={crown} />
      <CrownRig deck={upper} />
      <mesh position={[-lipHalf / 2, lipY, lipZ]}>
        <boxGeometry args={[lipHalf, 0.08, 0.1]} />
        <meshStandardMaterial color="#22e7ff" emissive="#22e7ff" emissiveIntensity={1.9} toneMapped={false} />
      </mesh>
      <mesh position={[lipHalf / 2, lipY, lipZ]}>
        <boxGeometry args={[lipHalf, 0.08, 0.1]} />
        <meshStandardMaterial color="#ff2bd6" emissive="#ff2bd6" emissiveIntensity={1.9} toneMapped={false} />
      </mesh>
      <NightCircuitCrowd />
    </group>
  );
}

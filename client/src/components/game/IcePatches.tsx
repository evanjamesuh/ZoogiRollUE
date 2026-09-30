import { useMemo } from "react";
import { getIcePatches } from "@/lib/arenaColliders";

interface IcePatch {
  id: string;
  position: [number, number, number];
  radius: number;
  rotation: number;
}

export function IcePatches() {
  const patches = useMemo<IcePatch[]>(() => {
    return getIcePatches().map((patch) => ({
      id: patch.id,
      position: [patch.x, 0.03, patch.z] as [number, number, number],
      radius: patch.radius,
      rotation: patch.rotation,
    }));
  }, []);

  return (
    <group>
      {patches.map((patch) => (
        <IcePatchMesh key={patch.id} patch={patch} />
      ))}
    </group>
  );
}

function IcePatchMesh({ patch }: { patch: IcePatch }) {
  return (
    <mesh position={patch.position} rotation={[-Math.PI / 2, 0, patch.rotation]} renderOrder={1}>
      <circleGeometry args={[patch.radius, 48]} />
      <meshStandardMaterial
        color="#1486c0"
        transparent
        opacity={0.74}
        roughness={0.22}
        metalness={0.04}
        emissive="#083e68"
        emissiveIntensity={0.22}
        depthWrite={false}
        polygonOffset
        polygonOffsetFactor={-2}
        polygonOffsetUnits={-2}
      />
    </mesh>
  );
}

export function getIcePatchPositions(): { position: [number, number, number]; radius: number }[] {
  return getIcePatches().map((patch) => ({
    position: [patch.x, 0.03, patch.z],
    radius: patch.radius,
  }));
}

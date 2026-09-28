import { useZoogiGame } from "@/lib/stores/useZoogiGame";

interface ScoringZoneProps {
  position: [number, number, number];
  ownerColor: string | null;
  zoneControlActive: boolean;
}

function ScoringZone({ position, ownerColor, zoneControlActive }: ScoringZoneProps) {
  const ZONE_RADIUS = 4;

  if (!zoneControlActive) return null;

  return (
    <group position={position}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]}>
        <circleGeometry args={[ZONE_RADIUS, 32]} />
        <meshBasicMaterial
          color={ownerColor || "#333333"}
          transparent
          opacity={ownerColor ? 0.6 : 0.2}
        />
      </mesh>
    </group>
  );
}

export function ScoringZones() {
  const zoneEditorConfigs = useZoogiGame(state => state.zoneEditorConfigs);
  const zoneOwnership = useZoogiGame(state => state.zoneOwnership);
  const zoneControlActive = useZoogiGame(state => state.zoneControlActive);

  return (
    <group>
      {zoneEditorConfigs
        .filter(zone => zone.visible)
        .map(zone => {
          const x = Math.cos(zone.angle) * zone.distance;
          const z = Math.sin(zone.angle) * zone.distance;
          const ownerColor = zoneOwnership.get(zone.id) || null;
          
          return (
            <ScoringZone
              key={zone.id}
              position={[x, 0, z]}
              ownerColor={ownerColor}
              zoneControlActive={zoneControlActive}
            />
          );
        })}
    </group>
  );
}

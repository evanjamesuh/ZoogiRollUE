import { useEffect, useSyncExternalStore } from "react";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { collectMatchSolids, getMapLayout, getWinterCampVersion, subscribeWinterCamp, type SolidKind } from "@/lib/arenaColliders";
import { NEON_HALF_X, NEON_HALF_Z, neonObstacles, neonRails } from "@/lib/neonCourt";

const KIND_COLOR: Record<SolidKind, string> = {
  bumper: "#ff00ff",
  rock: "#c8c8c8",
  bush: "#39ff14",
  snowman: "#ffffff",
  hoodoo: "#ff8800",
  prop: "#00e5ff",
};

/**
 * Wireframe of every solid the simulation uses, plus the knockoff ring.
 * Press C (outside a text field) or open the game with ?debug=colliders.
 */
export function ColliderDebug() {
  const colliderDebug = useZoogiGame((state) => state.colliderDebug);
  const toggleColliderDebug = useZoogiGame((state) => state.toggleColliderDebug);
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  const pinballBumpers = useZoogiGame((state) => state.pinballBumpers);
  const landedRocks = useZoogiGame((state) => state.landedRocks);
  const editorPlacedModels = useZoogiGame((state) => state.editorPlacedModels);
  const knockoff = useZoogiGame((state) => state.wallSettings.knockoffBoundaryRadius);
  const knockoffOffset = useZoogiGame((state) => state.elementTransforms.knockoffBoundaryOffset);
  // Camp solids flip on after winter_location.glb mounts. That flag is not React state.
  useSyncExternalStore(subscribeWinterCamp, getWinterCampVersion, getWinterCampVersion);

  useEffect(() => {
    const params = new URLSearchParams(window.location.search);
    if (params.get("debug") === "colliders" && !useZoogiGame.getState().colliderDebug) {
      useZoogiGame.getState().toggleColliderDebug();
    }
    const onKey = (event: KeyboardEvent) => {
      if (event.key !== "c" && event.key !== "C") return;
      if (event.metaKey || event.ctrlKey || event.altKey) return;
      const target = event.target as HTMLElement | null;
      const tag = target?.tagName;
      if (tag === "INPUT" || tag === "TEXTAREA" || target?.isContentEditable) return;
      toggleColliderDebug();
    };
    window.addEventListener("keydown", onKey);
    return () => window.removeEventListener("keydown", onKey);
  }, [toggleColliderDebug]);

  if (!colliderDebug) return null;

  const solids = collectMatchSolids({
    map: selectedMap,
    bumpers: pinballBumpers,
    landedRocks,
    editorModels: editorPlacedModels,
  });
  const layout = getMapLayout(selectedMap);
  const ring = knockoff || layout?.knockoffRadius || 0;

  return (
    <group>
      {solids.map((solid) => (
        <mesh
          key={solid.id}
          position={[solid.x, 0.2, solid.z]}
          rotation={[-Math.PI / 2, 0, 0]}
        >
          <ringGeometry args={[Math.max(0.02, solid.radius - 0.05), solid.radius, 28]} />
          <meshBasicMaterial color={KIND_COLOR[solid.kind]} wireframe side={2} />
        </mesh>
      ))}
      {selectedMap === "neon" && neonRails().map((rail) => (
        <mesh
          key={rail.id}
          position={[(rail.minX + rail.maxX) / 2, 0.45, (rail.minZ + rail.maxZ) / 2]}
        >
          <boxGeometry args={[Math.max(0.05, rail.maxX - rail.minX), 0.9, Math.max(0.05, rail.maxZ - rail.minZ)]} />
          <meshBasicMaterial color="#ff44aa" wireframe />
        </mesh>
      ))}
      {selectedMap === "neon" && neonObstacles().map((box) => (
        <mesh
          key={box.id}
          position={[(box.minX + box.maxX) / 2, 0.32, (box.minZ + box.maxZ) / 2]}
        >
          <boxGeometry args={[Math.max(0.05, box.maxX - box.minX), 0.5, Math.max(0.05, box.maxZ - box.minZ)]} />
          <meshBasicMaterial color="#44ffee" wireframe />
        </mesh>
      ))}
      {selectedMap === "neon" && (
        <group position={[knockoffOffset?.x ?? 0, 0.14, knockoffOffset?.z ?? 0]}>
          <mesh position={[0, 0, NEON_HALF_Z]}>
            <boxGeometry args={[NEON_HALF_X * 2, 0.04, 0.08]} />
            <meshBasicMaterial color="#ffffff" wireframe />
          </mesh>
          <mesh position={[0, 0, -NEON_HALF_Z]}>
            <boxGeometry args={[NEON_HALF_X * 2, 0.04, 0.08]} />
            <meshBasicMaterial color="#ffffff" wireframe />
          </mesh>
          <mesh position={[NEON_HALF_X, 0, 0]}>
            <boxGeometry args={[0.08, 0.04, NEON_HALF_Z * 2]} />
            <meshBasicMaterial color="#ffffff" wireframe />
          </mesh>
          <mesh position={[-NEON_HALF_X, 0, 0]}>
            <boxGeometry args={[0.08, 0.04, NEON_HALF_Z * 2]} />
            <meshBasicMaterial color="#ffffff" wireframe />
          </mesh>
        </group>
      )}
      {selectedMap !== "neon" && ring > 0 && (
        <mesh position={[knockoffOffset?.x ?? 0, 0.12, knockoffOffset?.z ?? 0]} rotation={[-Math.PI / 2, 0, 0]}>
          <ringGeometry args={[ring - 0.08, ring, 72]} />
          <meshBasicMaterial color="#ffffff" wireframe side={2} />
        </mesh>
      )}
    </group>
  );
}

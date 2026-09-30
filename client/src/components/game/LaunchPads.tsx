import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { launchPadsFor } from "@/lib/launchPads";

/**
 * A flat glowing disc in the shooter's color. Original to Zoogi Roll:
 * no borrowed arena meshes, just a readable place to launch from.
 */
export function LaunchPads() {
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  const player = useZoogiGame((state) => state.playerEntity);
  const enemies = useZoogiGame((state) => state.enemies);
  const pads = launchPadsFor(selectedMap);
  const count = (player ? 1 : 0) + enemies.length;
  const colors = [
    player?.zoogi.color ?? "#ffffff",
    ...enemies.map((enemy) => enemy.zoogi.color),
  ];

  return (
    <group>
      {pads.slice(0, count).map((pad) => {
        const color = colors[pad.index] ?? "#7dd3fc";
        return (
          <group key={pad.index} position={[pad.x, 0.05, pad.z]}>
            <mesh rotation={[-Math.PI / 2, 0, 0]} receiveShadow>
              <circleGeometry args={[0.95, 28]} />
              <meshStandardMaterial
                color={color}
                emissive={color}
                emissiveIntensity={0.65}
                roughness={0.35}
                metalness={0.05}
              />
            </mesh>
            <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.02, 0]}>
              <ringGeometry args={[0.78, 0.95, 28]} />
              <meshBasicMaterial color={color} transparent opacity={0.95} />
            </mesh>
          </group>
        );
      })}
    </group>
  );
}

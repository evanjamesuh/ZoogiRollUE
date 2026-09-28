import { useZoogiGame } from "@/lib/stores/useZoogiGame";

export function Lights() {
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  const isIce = selectedMap === "ice";

  if (isIce) {
    return (
      <>
        <ambientLight intensity={0.5} color="#d0e8ff" />

        <directionalLight
          position={[15, 20, 10]}
          intensity={1.0}
          color="#fff8f0"
          castShadow
          shadow-mapSize-width={2048}
          shadow-mapSize-height={2048}
          shadow-camera-far={80}
          shadow-camera-left={-30}
          shadow-camera-right={30}
          shadow-camera-top={30}
          shadow-camera-bottom={-30}
          shadow-bias={-0.001}
        />

        <directionalLight
          position={[-12, 8, -10]}
          intensity={0.25}
          color="#a0d4ff"
        />

        <hemisphereLight
          args={["#87CEEB", "#b0956e", 0.3]}
        />
      </>
    );
  }

  return (
    <>
      <ambientLight intensity={0.4} color="#f0f4ff" />

      <directionalLight
        position={[10, 15, 10]}
        intensity={0.8}
        color="#ffffff"
        castShadow
        shadow-mapSize-width={2048}
        shadow-mapSize-height={2048}
        shadow-camera-far={50}
        shadow-camera-left={-20}
        shadow-camera-right={20}
        shadow-camera-top={20}
        shadow-camera-bottom={-20}
      />

      <directionalLight
        position={[-10, 10, -10]}
        intensity={0.3}
        color="#d4e8ff"
      />

      <hemisphereLight
        args={["#87CEEB", "#4a6a3f", 0.2]}
      />
    </>
  );
}

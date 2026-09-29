import { useZoogiGame } from "@/lib/stores/useZoogiGame";
import { NeonCourtLights } from "./NeonCourtArena";

export function Lights() {
  const selectedMap = useZoogiGame((state) => state.selectedMap);
  const isIce = selectedMap === "ice";

  if (selectedMap === "neon") {
    return <NeonCourtLights />;
  }

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

  if (selectedMap === "lava") {
    return (
      <>
        <ambientLight intensity={0.28} color="#3a2048" />
        <directionalLight
          position={[18, 14, 8]}
          intensity={1.05}
          color="#ffb070"
          castShadow
          shadow-mapSize-width={2048}
          shadow-mapSize-height={2048}
          shadow-camera-far={90}
          shadow-camera-left={-36}
          shadow-camera-right={36}
          shadow-camera-top={36}
          shadow-camera-bottom={-36}
          shadow-bias={-0.001}
        />
        <directionalLight position={[-16, 10, -12]} intensity={0.48} color="#7a4bff" />
        <hemisphereLight args={["#6a4a9a", "#ff6a22", 0.32]} />
        <pointLight position={[0, 4, 0]} color="#ff6a18" intensity={1.4} distance={34} decay={2} />
      </>
    );
  }

  if (selectedMap === "saturn") {
    return (
      <>
        <ambientLight intensity={0.36} color="#2a1848" />
        <directionalLight
          position={[-8, 22, 6]}
          intensity={0.72}
          color="#d4d0ff"
          castShadow
          shadow-mapSize-width={2048}
          shadow-mapSize-height={2048}
          shadow-camera-far={80}
          shadow-camera-left={-30}
          shadow-camera-right={30}
          shadow-camera-top={30}
          shadow-camera-bottom={-30}
        />
        <hemisphereLight args={["#14082a", "#3a2818", 0.28]} />
      </>
    );
  }

  if (selectedMap === "space") {
    return (
      <>
        <ambientLight intensity={0.2} color="#1a1038" />
        <directionalLight
          position={[8, 18, 12]}
          intensity={0.42}
          color="#c4b6ff"
          castShadow
          shadow-mapSize-width={2048}
          shadow-mapSize-height={2048}
          shadow-camera-far={70}
          shadow-camera-left={-28}
          shadow-camera-right={28}
          shadow-camera-top={28}
          shadow-camera-bottom={-28}
        />
        <directionalLight position={[12, 6, -10]} intensity={0.38} color="#3dfff0" />
        <hemisphereLight args={["#2a1060", "#061018", 0.3]} />
      </>
    );
  }

  if (selectedMap === "tomb") {
    return (
      <>
        <ambientLight intensity={0.26} color="#2a1840" />
        <directionalLight
          position={[14, 18, 10]}
          intensity={1.2}
          color="#ffd2a8"
          castShadow
          shadow-mapSize-width={2048}
          shadow-mapSize-height={2048}
          shadow-camera-far={80}
          shadow-camera-left={-32}
          shadow-camera-right={32}
          shadow-camera-top={32}
          shadow-camera-bottom={-32}
          shadow-bias={-0.001}
        />
        <directionalLight position={[-16, 9, -12]} intensity={0.62} color="#7d5cff" />
        <hemisphereLight args={["#3a2068", "#c4a070", 0.28]} />
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

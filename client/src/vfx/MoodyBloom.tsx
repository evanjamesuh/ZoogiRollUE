import { useMemo } from "react";
import { Bloom, EffectComposer } from "@react-three/postprocessing";

/**
 * Soft bloom that catches HDR sparks, the blast core, and wolf rim light.
 * The threshold sits above ordinary scene colors so the arena itself stays flat.
 */
export function MoodyBloom() {
  const mobile = useMemo(() => {
    if (typeof window === "undefined") return false;
    return window.matchMedia("(pointer: coarse)").matches || window.innerWidth < 900;
  }, []);

  return (
    <EffectComposer multisampling={0} enableNormalPass={false}>
      <Bloom
        intensity={mobile ? 0.28 : 0.38}
        luminanceThreshold={1.05}
        luminanceSmoothing={0.2}
        mipmapBlur
        radius={0.5}
        levels={4}
        resolutionScale={mobile ? 0.35 : 0.45}
      />
    </EffectComposer>
  );
}

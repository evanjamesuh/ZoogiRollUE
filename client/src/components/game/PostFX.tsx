import { EffectComposer, Bloom, ToneMapping, Vignette } from "@react-three/postprocessing";
import { ToneMappingMode } from "postprocessing";

/**
 * Bloom only the bright stuff. The threshold sits at 1 so a normal floor,
 * sky, and white court line stay matte. Neon rails and sparks are drawn
 * above 1 (emissive intensity, or a color channel past 1) so they glow.
 * Tone mapping runs after bloom, or the glow would be crushed first.
 */
export function PostFX() {
  return (
    <EffectComposer enableNormalPass={false} multisampling={4}>
      <Bloom
        intensity={0.42}
        luminanceThreshold={1.15}
        luminanceSmoothing={0.05}
        mipmapBlur
      />
      <Vignette eskil={false} offset={0.18} darkness={0.62} />
      <ToneMapping mode={ToneMappingMode.ACES_FILMIC} />
    </EffectComposer>
  );
}

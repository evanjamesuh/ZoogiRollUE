import { useEffect, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import { EffectComposer, Bloom, ToneMapping, Vignette } from "@react-three/postprocessing";
import { ToneMappingMode } from "postprocessing";
import {
  bloomParams,
  getVfxQuality,
  isVfxQualityLocked,
  setVfxQuality,
  subscribeVfxQuality,
  type VfxQuality,
} from "@/vfx/quality";
import { isMobileGraphics } from "@/lib/mobileGraphics";

/**
 * One composer for the match. Bloom follows `?vfx=high|low`, and a run of
 * slow frames drops that cost when the query does not lock it. Phones use the
 * low bloom budget. The threshold stays high enough that the court and grass
 * stay matte. Tone mapping runs after bloom so the glow is not crushed first.
 */
export function PostFX() {
  const [phone] = useState(() => isMobileGraphics());
  const [quality, setQuality] = useState<VfxQuality>(getVfxQuality);
  const watch = useRef({ warm: 0, slow: 0, dropped: false });

  useEffect(() => subscribeVfxQuality(() => setQuality(getVfxQuality())), []);

  useFrame((_, dt) => {
    const state = watch.current;
    if (state.dropped || isVfxQualityLocked()) return;
    const step = Math.min(Math.max(dt, 0), 0.25);
    state.warm += step;
    if (state.warm < 1.4) return;
    if (step > 0.042) state.slow += 1;
    else state.slow = Math.max(0, state.slow - 1);
    if (state.slow >= 20) {
      state.dropped = true;
      setVfxQuality("low");
    }
  });

  const level: VfxQuality = phone ? "low" : quality;
  const bloom = bloomParams(level);

  return (
    <EffectComposer enableNormalPass={false} multisampling={level === "low" ? 0 : 4}>
      <Bloom
        intensity={bloom.intensity}
        luminanceThreshold={1.12}
        luminanceSmoothing={0.2}
        mipmapBlur
        radius={bloom.radius}
        levels={bloom.levels}
        resolutionScale={bloom.resolutionScale}
      />
      <Vignette eskil={false} offset={0.18} darkness={0.62} />
      <ToneMapping mode={ToneMappingMode.ACES_FILMIC} />
    </EffectComposer>
  );
}

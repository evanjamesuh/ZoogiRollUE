import { useEffect, useRef, useState } from "react";
import { useFrame } from "@react-three/fiber";
import { Bloom, EffectComposer } from "@react-three/postprocessing";
import {
  bloomParams,
  getVfxQuality,
  isVfxQualityLocked,
  setVfxQuality,
  subscribeVfxQuality,
  type VfxQuality,
} from "./quality";

/**
 * Soft bloom for HDR sparks, the fireball, and wolf rim light.
 * `?vfx=high` or `?vfx=low` locks the cost. Otherwise a run of slow frames
 * drops the bloom resolution so the blast does not stall the match.
 */
export function MoodyBloom() {
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

  const bloom = bloomParams(quality);

  return (
    <EffectComposer multisampling={0} enableNormalPass={false}>
      <Bloom
        intensity={bloom.intensity}
        luminanceThreshold={1.05}
        luminanceSmoothing={0.22}
        mipmapBlur
        radius={bloom.radius}
        levels={bloom.levels}
        resolutionScale={bloom.resolutionScale}
      />
    </EffectComposer>
  );
}

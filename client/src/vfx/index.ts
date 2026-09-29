export { createSpritePool, clearSpritePool, spawnSprite, killSprite } from "./pool";
export type { SpritePool, SpriteSpawn } from "./pool";
export { InstancedSprites } from "./InstancedSprites";
export type { SpriteMode } from "./InstancedSprites";
export {
  seedBlastSmoke,
  seedBlastEmbers,
  seedColdBurst,
  stepSmoke,
  stepEmbers,
  stepWisps,
  stepColdEmbers,
  emitWisp,
  emitColdEmber,
} from "./sim";
export { retainVfxTextures, releaseVfxTextures, useVfxTextures } from "./textures";
export { ExplosionBlast } from "./ExplosionBlast";
export { SpectralWolf } from "./SpectralWolf";
export { SmokeBurstField, emitSmokeBurst } from "./bursts";
export { MoodyBloom } from "./MoodyBloom";
export { impulseShake } from "./shake";

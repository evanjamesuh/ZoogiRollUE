import { useEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import type { SpritePool } from "./pool";
import { EMBER_FRAG, FIRE_FRAG, MIST_FRAG, SMOKE_FRAG, SPRITE_VERT } from "./shaders";
import { useVfxTextures } from "./textures";

const scratch = new THREE.Matrix4();
const scratchPos = new THREE.Vector3();
const scratchQuat = new THREE.Quaternion();
const scratchScale = new THREE.Vector3(1, 1, 1);

export type SpriteMode = "smoke" | "fire" | "ember" | "mist";

function fragmentFor(mode: SpriteMode): string {
  if (mode === "fire") return FIRE_FRAG;
  if (mode === "ember") return EMBER_FRAG;
  if (mode === "mist") return MIST_FRAG;
  return SMOKE_FRAG;
}

/**
 * One instanced draw of camera-facing quads. Offsets are applied in view
 * space so the sprites stay square to the lens, including the tilted match camera.
 */
export function InstancedSprites({ pool, mode }: { pool: SpritePool; mode: SpriteMode }) {
  const textures = useVfxTextures();
  const meshRef = useRef<THREE.InstancedMesh>(null);
  const streak = mode === "ember";
  const atlas = mode === "smoke" || mode === "mist";

  const geometry = useMemo(() => {
    const geo = new THREE.PlaneGeometry(1, 1, 1, 1);
    geo.setAttribute("aOpacity", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity), 1));
    geo.setAttribute("aSpin", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity), 1));
    geo.setAttribute("aSize", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity), 1));
    geo.setAttribute("aStretch", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity), 1));
    geo.setAttribute("aVariant", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity), 1));
    geo.setAttribute("aColor", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity * 3), 3));
    geo.setAttribute("aVelocity", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity * 3), 3));
    return geo;
  }, [pool]);

  const material = useMemo(() => {
    const map = mode === "ember" ? textures.ember : textures.smoke;
    const mat = new THREE.ShaderMaterial({
      uniforms: {
        uMap: { value: map },
        uAtlas: { value: atlas ? 1 : 0 },
      },
      vertexShader: SPRITE_VERT,
      fragmentShader: fragmentFor(mode),
      transparent: true,
      depthWrite: false,
      depthTest: true,
      toneMapped: false,
      side: THREE.DoubleSide,
    });
    if (mode === "fire" || mode === "ember") {
      mat.blending = THREE.CustomBlending;
      mat.blendSrc = THREE.OneFactor;
      mat.blendDst = THREE.OneFactor;
      mat.blendEquation = THREE.AddEquation;
    } else {
      mat.blending = THREE.NormalBlending;
    }
    return mat;
  }, [mode, textures, atlas]);

  useEffect(() => {
    return () => {
      geometry.dispose();
      material.dispose();
    };
  }, [geometry, material]);

  useFrame(() => {
    const mesh = meshRef.current;
    if (!mesh) return;
    const opacity = geometry.getAttribute("aOpacity") as THREE.InstancedBufferAttribute;
    const spin = geometry.getAttribute("aSpin") as THREE.InstancedBufferAttribute;
    const size = geometry.getAttribute("aSize") as THREE.InstancedBufferAttribute;
    const stretch = geometry.getAttribute("aStretch") as THREE.InstancedBufferAttribute;
    const variant = geometry.getAttribute("aVariant") as THREE.InstancedBufferAttribute;
    const color = geometry.getAttribute("aColor") as THREE.InstancedBufferAttribute;
    const velocity = geometry.getAttribute("aVelocity") as THREE.InstancedBufferAttribute;
    const opacityArray = opacity.array as Float32Array;
    const spinArray = spin.array as Float32Array;
    const sizeArray = size.array as Float32Array;
    const stretchArray = stretch.array as Float32Array;
    const variantArray = variant.array as Float32Array;
    const colorArray = color.array as Float32Array;
    const velocityArray = velocity.array as Float32Array;
    for (let i = 0; i < pool.capacity; i++) {
      const alive = pool.active[i] === 1 && pool.opacity[i] > 0.004;
      if (!alive) {
        scratchPos.set(0, -100, 0);
        opacityArray[i] = 0;
        sizeArray[i] = 0;
        stretchArray[i] = 1;
      } else {
        scratchPos.set(pool.px[i], pool.py[i], pool.pz[i]);
        opacityArray[i] = pool.opacity[i];
        sizeArray[i] = pool.size[i];
        const speed = Math.hypot(pool.vx[i], pool.vy[i], pool.vz[i]);
        stretchArray[i] = streak ? Math.min(3.4, 1.4 + speed * 0.18) : 1;
      }
      scratchQuat.identity();
      scratch.compose(scratchPos, scratchQuat, scratchScale);
      mesh.setMatrixAt(i, scratch);
      spinArray[i] = pool.rot[i];
      variantArray[i] = Math.abs(Math.floor(pool.seed[i])) % 4;
      const c = i * 3;
      colorArray[c] = pool.r[i];
      colorArray[c + 1] = pool.g[i];
      colorArray[c + 2] = pool.b[i];
      velocityArray[c] = pool.vx[i];
      velocityArray[c + 1] = pool.vy[i];
      velocityArray[c + 2] = pool.vz[i];
    }
    mesh.instanceMatrix.needsUpdate = true;
    opacity.needsUpdate = true;
    spin.needsUpdate = true;
    size.needsUpdate = true;
    stretch.needsUpdate = true;
    variant.needsUpdate = true;
    color.needsUpdate = true;
    velocity.needsUpdate = true;
  });

  const order = mode === "fire" ? 6 : mode === "ember" ? 5 : mode === "mist" ? 3 : 2;
  return (
    <instancedMesh
      ref={meshRef}
      args={[geometry, material, pool.capacity]}
      frustumCulled={false}
      renderOrder={order}
    />
  );
}

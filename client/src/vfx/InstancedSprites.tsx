import { useEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import * as THREE from "three";
import type { SpritePool } from "./pool";
import { EMBER_FRAG, MIST_FRAG, SMOKE_FRAG, SPRITE_VERT } from "./shaders";
import { useVfxTextures } from "./textures";

const scratch = new THREE.Matrix4();
const scratchPos = new THREE.Vector3();
const scratchQuat = new THREE.Quaternion();
const scratchScale = new THREE.Vector3();

export type SpriteMode = "smoke" | "additive" | "mist";

/**
 * Draws a pooled sprite field with one instanced draw. The owner steps the
 * pool; this component only writes instance matrices and attributes.
 */
export function InstancedSprites({ pool, mode }: { pool: SpritePool; mode: SpriteMode }) {
  const textures = useVfxTextures();
  const meshRef = useRef<THREE.InstancedMesh>(null);

  const geometry = useMemo(() => {
    const geo = new THREE.PlaneGeometry(1, 1, 1, 1);
    geo.setAttribute("aOpacity", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity), 1));
    geo.setAttribute("aSpin", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity), 1));
    geo.setAttribute("aColor", new THREE.InstancedBufferAttribute(new Float32Array(pool.capacity * 3), 3));
    return geo;
  }, [pool]);

  const material = useMemo(() => {
    const map = mode === "additive" ? textures.ember : textures.smoke;
    const fragment = mode === "additive" ? EMBER_FRAG : mode === "mist" ? MIST_FRAG : SMOKE_FRAG;
    const mat = new THREE.ShaderMaterial({
      uniforms: { uMap: { value: map } },
      vertexShader: SPRITE_VERT,
      fragmentShader: fragment,
      transparent: true,
      depthWrite: false,
      depthTest: true,
      toneMapped: false,
      side: THREE.DoubleSide,
    });
    if (mode === "additive") {
      mat.blending = THREE.CustomBlending;
      mat.blendSrc = THREE.OneFactor;
      mat.blendDst = THREE.OneFactor;
      mat.blendEquation = THREE.AddEquation;
    } else {
      mat.blending = THREE.NormalBlending;
    }
    return mat;
  }, [mode, textures]);

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
    const color = geometry.getAttribute("aColor") as THREE.InstancedBufferAttribute;
    const opacityArray = opacity.array as Float32Array;
    const spinArray = spin.array as Float32Array;
    const colorArray = color.array as Float32Array;
    for (let i = 0; i < pool.capacity; i++) {
      const alive = pool.active[i] === 1 && pool.opacity[i] > 0.004;
      if (!alive) {
        scratchScale.set(0, 0, 0);
        scratchPos.set(0, 0, 0);
        opacityArray[i] = 0;
      } else {
        scratchPos.set(pool.px[i], pool.py[i], pool.pz[i]);
        scratchScale.set(pool.size[i], pool.size[i], 1);
        opacityArray[i] = pool.opacity[i];
      }
      scratchQuat.identity();
      scratch.compose(scratchPos, scratchQuat, scratchScale);
      mesh.setMatrixAt(i, scratch);
      spinArray[i] = pool.rot[i];
      const c = i * 3;
      colorArray[c] = pool.r[i];
      colorArray[c + 1] = pool.g[i];
      colorArray[c + 2] = pool.b[i];
    }
    mesh.instanceMatrix.needsUpdate = true;
    opacity.needsUpdate = true;
    spin.needsUpdate = true;
    color.needsUpdate = true;
  });

  return (
    <instancedMesh
      ref={meshRef}
      args={[geometry, material, pool.capacity]}
      frustumCulled={false}
      renderOrder={mode === "additive" ? 5 : mode === "mist" ? 3 : 2}
    />
  );
}

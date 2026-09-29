import { useGLTF } from "@react-three/drei";
import * as THREE from "three";

const retained = new Map<string, THREE.Object3D>();
let keep = new Set<string>();

function disposeMaterialTextures(material: THREE.Material) {
  const textures = new Set<THREE.Texture>();
  for (const value of Object.values(material)) {
    if (value instanceof THREE.Texture) textures.add(value);
  }
  for (const texture of textures) texture.dispose();
  material.dispose();
}

export function disposeComicScene(root: THREE.Object3D) {
  const materials = new Set<THREE.Material>();
  const geometries = new Set<THREE.BufferGeometry>();
  root.traverse((obj) => {
    const mesh = obj as THREE.Mesh;
    if (mesh.geometry) geometries.add(mesh.geometry);
    if (!mesh.material) return;
    const list = Array.isArray(mesh.material) ? mesh.material : [mesh.material];
    for (const material of list) materials.add(material);
  });
  for (const material of materials) disposeMaterialTextures(material);
  for (const geometry of geometries) geometry.dispose();
}

/** Remember a loaded panel so it can be released when the reader moves on. */
export function retainComicPanel(url: string, scene: THREE.Object3D) {
  retained.set(url, scene);
  if (keep.size > 0 && !keep.has(url)) {
    disposeComicScene(scene);
    retained.delete(url);
    useGLTF.clear(url);
  }
}

/**
 * Keep the panel on screen and the preloaded next panel. Everything else is
 * disposed and dropped from the GLTF cache so low-end devices don't hold the
 * whole comic.
 */
export function syncComicPanelCache(urlsToKeep: string[]) {
  keep = new Set(urlsToKeep);
  for (const [url, scene] of retained) {
    if (keep.has(url)) continue;
    disposeComicScene(scene);
    retained.delete(url);
    useGLTF.clear(url);
  }
}

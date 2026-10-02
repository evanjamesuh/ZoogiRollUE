import { useGLTF } from "@react-three/drei";
import * as THREE from "three";

const retained = new Map<string, THREE.Object3D>();
const warmed = new Set<string>();
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

/**
 * Fetch a neighbor into the GLTF cache. If the reader moves on before it is
 * shown, the finished file is dropped instead of kept around.
 */
export function warmComicPanel(url: string) {
  warmed.add(url);
  return Promise.resolve(useGLTF.preload(url)).then(
    () => {
      if (!keep.has(url)) {
        warmed.delete(url);
        useGLTF.clear(url);
      }
    },
    (error: unknown) => {
      warmed.delete(url);
      throw error;
    },
  );
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
 * Keep the panel on screen, the next panel, and the previous panel.
 * Everything else is disposed and dropped from the GLTF cache so a long
 * comic is never held in memory all at once.
 */
export function syncComicPanelCache(urlsToKeep: string[]) {
  keep = new Set(urlsToKeep);
  for (const [url, scene] of retained) {
    if (keep.has(url)) continue;
    disposeComicScene(scene);
    retained.delete(url);
    warmed.delete(url);
    useGLTF.clear(url);
  }
  for (const url of [...warmed]) {
    if (keep.has(url)) continue;
    warmed.delete(url);
    useGLTF.clear(url);
  }
}

import * as THREE from "three";
import { COMIC_MANIFEST } from "./comicManifest";

/**
 * The intro comic is eight glTF panels exported by the Spiraloid 3DComic
 * Toolkit. The toolkit numbers panels from 0, so panel 1 is panel01.glb and
 * its camera node is Camera.0000. Later panels keep that same global index.
 */
export const INTRO_COMIC_PAGE_COUNT =
  COMIC_MANIFEST.find((chapter) => chapter.id === "intro")?.panels.length ?? 0;

export const INTRO_COMIC_TITLES = [
  "The bright world",
  "The eclipse",
  "Nightshade appears",
  "The great theft",
  "The world fades",
  "Wolfgang",
  "Rivals everywhere",
  "Let's roll",
] as const;

/** ACES exposure. Lights are already in Blender's compatible (unitless) units. */
export const COMIC_VIEW_EXPOSURE = 1;

export const COMIC_AMBIENT_NAME = "ComicAmbient";

const LETTERING_PREFIXES = ["Balloon_", "Caption_", "Letter_", "Logo_", "Gutter_"];

export interface ComicPageRef {
  page: number;
  file: string;
  chapterId: string;
  chapterTitle: string;
}

export interface ComicChapterInfo {
  id: string;
  title: string;
  startPage: number;
  endPage: number;
}

const COMIC_PAGES: ComicPageRef[] = [];

for (const chapter of COMIC_MANIFEST) {
  for (const file of chapter.panels) {
    COMIC_PAGES.push({
      page: COMIC_PAGES.length + 1,
      file,
      chapterId: chapter.id,
      chapterTitle: chapter.title,
    });
  }
}

export const COMIC_PAGE_COUNT = COMIC_PAGES.length;

export const COMIC_CHAPTERS: readonly ComicChapterInfo[] = COMIC_MANIFEST.map((chapter) => {
  const pages = COMIC_PAGES.filter((page) => page.chapterId === chapter.id);
  return {
    id: chapter.id,
    title: chapter.title,
    startPage: pages[0]?.page ?? 1,
    endPage: pages[pages.length - 1]?.page ?? 1,
  };
});

export function comicPages(): readonly ComicPageRef[] {
  return COMIC_PAGES;
}

export function comicChapterForPage(page: number): ComicChapterInfo | undefined {
  return COMIC_CHAPTERS.find((chapter) => page >= chapter.startPage && page <= chapter.endPage);
}

export function comicChapterTitle(page: number): string {
  return comicChapterForPage(page)?.title ?? `Panel ${page}`;
}

/** Public URL for a page in the manifest. Undefined when the page is not listed. */
export function comicPanelUrl(page: number): string | undefined {
  return COMIC_PAGES[page - 1]?.file;
}

export function introComicPanelUrl(page: number): string {
  const listed = comicPanelUrl(page);
  if (listed?.startsWith("/comics/intro/")) return listed;
  const index = String(page).padStart(2, "0");
  return `/comics/intro/panel${index}.glb`;
}

export function comicCameraName(page: number): string {
  return `Camera.${String(page - 1).padStart(4, "0")}`;
}

export function comicAimName(page: number): string {
  return `Camera_aim.${String(page - 1).padStart(4, "0")}`;
}

export function comicLettersName(page: number): string {
  return `Letters_english.${String(page - 1).padStart(4, "0")}`;
}

export function introComicTitle(page: number): string {
  return INTRO_COMIC_TITLES[page - 1] ?? `Panel ${page}`;
}

export function missingComicPanelMessage(page: number): string {
  const url = comicPanelUrl(page) ?? introComicPanelUrl(page);
  const folder = url.slice(0, url.lastIndexOf("/"));
  return `The file ${url} is missing or could not be read. It belongs in client/public${folder}/.`;
}

export function stepComicPage(page: number, delta: number, count = COMIC_PAGE_COUNT): number {
  return Math.min(count, Math.max(1, page + delta));
}

/**
 * The panel on screen, plus the next one and the previous one.
 * Never a whole chapter — later books are far too big to hold at once.
 */
export function comicPanelUrlsToKeep(page: number, count = COMIC_PAGE_COUNT): string[] {
  const urls: string[] = [];
  const add = (candidate: number) => {
    if (candidate < 1 || candidate > count) return;
    const url = comicPanelUrl(candidate);
    if (url) urls.push(url);
  };
  add(page - 1);
  add(page);
  add(page + 1);
  return urls;
}

export type ComicPageCommand = "next" | "prev" | "close";

export function comicCommandForKey(key: string): ComicPageCommand | null {
  switch (key) {
    case "ArrowRight":
    case "ArrowDown":
    case "PageDown":
    case " ":
      return "next";
    case "ArrowLeft":
    case "ArrowUp":
    case "PageUp":
      return "prev";
    case "Escape":
      return "close";
    default:
      return null;
  }
}

export function comicCommandForSwipe(dx: number, dy: number, threshold = 64): ComicPageCommand | null {
  const absX = Math.abs(dx);
  const absY = Math.abs(dy);
  if (absX < threshold && absY < threshold) return null;
  if (absX >= absY) return dx < 0 ? "next" : "prev";
  return dy < 0 ? "next" : "prev";
}

/** Left edge goes back, right edge goes forward. The middle is left alone so a tap can orbit. */
export function comicCommandForTap(x: number, width: number): ComicPageCommand | null {
  if (width <= 0) return null;
  if (x < width * 0.28) return "prev";
  if (x > width * 0.72) return "next";
  return null;
}

export function isComicLetteringMaterial(name: string): boolean {
  return LETTERING_PREFIXES.some((prefix) => name.startsWith(prefix));
}

export function isUnderComicLetters(object: { name: string; parent: { name: string; parent: unknown } | null }): boolean {
  let current: { name: string; parent: unknown } | null = object;
  while (current) {
    if (current.name.startsWith("Letters_english")) return true;
    current = current.parent as { name: string; parent: unknown } | null;
  }
  return false;
}

export interface ComicOrbitLimits {
  minDistance: number;
  maxDistance: number;
  minPolarAngle: number;
  maxPolarAngle: number;
  minAzimuthAngle: number;
  maxAzimuthAngle: number;
}

/** A small orbit around the authored camera, centered on Camera_aim. */
export function comicOrbitLimits(cameraPosition: THREE.Vector3, aim: THREE.Vector3): ComicOrbitLimits {
  const offset = new THREE.Vector3().subVectors(cameraPosition, aim);
  const spherical = new THREE.Spherical().setFromVector3(offset);
  if (spherical.radius < 0.05) {
    return {
      minDistance: 0.5,
      maxDistance: 80,
      minPolarAngle: 0.2,
      maxPolarAngle: Math.PI - 0.2,
      minAzimuthAngle: -Infinity,
      maxAzimuthAngle: Infinity,
    };
  }
  const pad = 0.4;
  return {
    minDistance: spherical.radius * 0.85,
    maxDistance: spherical.radius * 1.22,
    minPolarAngle: Math.max(0.02, spherical.phi - pad),
    maxPolarAngle: Math.min(Math.PI - 0.02, spherical.phi + pad),
    minAzimuthAngle: spherical.theta - pad,
    maxAzimuthAngle: spherical.theta + pad,
  };
}

export interface PreparedComicPanel {
  camera: THREE.PerspectiveCamera;
  aim: THREE.Vector3;
  orbit: ComicOrbitLimits;
}

/** glTF node names lose "." when three.js makes them safe for animation tracks. */
export function findComicNode(scene: THREE.Object3D, rawName: string): THREE.Object3D | undefined {
  const sanitized = THREE.PropertyBinding.sanitizeNodeName(rawName);
  const direct = scene.getObjectByName(rawName) ?? scene.getObjectByName(sanitized);
  if (direct) return direct;
  let found: THREE.Object3D | undefined;
  scene.traverse((obj) => {
    if (!found && obj.userData?.name === rawName) found = obj;
  });
  return found;
}

function meshMaterials(mesh: THREE.Mesh): THREE.Material[] {
  if (!mesh.material) return [];
  return Array.isArray(mesh.material) ? mesh.material : [mesh.material];
}

/**
 * Balloons, captions, and lettering are unlit meshes parented to the camera.
 * ACES tone mapping turns their white into grey, so those materials skip it
 * and keep the color exported from Blender.
 */
export function prepareComicPanel(scene: THREE.Object3D, page: number): PreparedComicPanel {
  scene.updateMatrixWorld(true);

  scene.traverse((obj) => {
    const mesh = obj as THREE.Mesh;
    if (!mesh.isMesh) return;
    const underLetters = isUnderComicLetters(mesh);
    if (underLetters) mesh.frustumCulled = false;
    for (const material of meshMaterials(mesh)) {
      if (underLetters || isComicLetteringMaterial(material.name)) {
        material.toneMapped = false;
        material.needsUpdate = true;
      }
    }
  });

  const color = scene.userData.comic_ambient_color;
  const strength = scene.userData.comic_ambient_strength;
  if (typeof color === "string" && typeof strength === "number" && !scene.getObjectByName(COMIC_AMBIENT_NAME)) {
    const ambient = new THREE.AmbientLight(color, strength);
    ambient.name = COMIC_AMBIENT_NAME;
    scene.add(ambient);
  }

  const named = findComicNode(scene, comicCameraName(page));
  let camera = named && (named as THREE.PerspectiveCamera).isPerspectiveCamera
    ? named as THREE.PerspectiveCamera
    : null;
  if (!camera && named) {
    named.traverse((obj) => {
      if (!camera && (obj as THREE.PerspectiveCamera).isPerspectiveCamera) {
        camera = obj as THREE.PerspectiveCamera;
      }
    });
  }
  if (!camera) {
    throw new Error(`Panel ${page} has no camera named ${comicCameraName(page)}.`);
  }

  const aim = new THREE.Vector3();
  const aimNode = findComicNode(scene, comicAimName(page));
  if (aimNode) {
    aimNode.getWorldPosition(aim);
  } else {
    camera.getWorldDirection(aim).multiplyScalar(10).add(camera.getWorldPosition(new THREE.Vector3()));
  }

  return {
    camera,
    aim,
    orbit: comicOrbitLimits(camera.getWorldPosition(new THREE.Vector3()), aim),
  };
}

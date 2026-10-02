import test from "node:test";
import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import * as THREE from "three";
import {
  COMIC_CHAPTERS,
  COMIC_PAGE_COUNT,
  INTRO_COMIC_PAGE_COUNT,
  comicAimName,
  comicCameraName,
  comicChapterForPage,
  comicCommandForKey,
  comicCommandForSwipe,
  comicCommandForTap,
  comicLettersName,
  comicOrbitLimits,
  comicPages,
  comicPanelUrl,
  comicPanelUrlsToKeep,
  introComicPanelUrl,
  isComicLetteringMaterial,
  isUnderComicLetters,
  missingComicPanelMessage,
  prepareComicPanel,
  stepComicPage,
} from "./comicPanels.ts";

const comicsDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../../public/comics");

function gltfNodeNames(filePath: string): string[] {
  const buf = readFileSync(filePath);
  assert.equal(buf.readUInt32LE(0), 0x46546c67, filePath);
  const chunkLength = buf.readUInt32LE(12);
  const chunkType = buf.readUInt32LE(16);
  assert.equal(chunkType, 0x4e4f534a, filePath);
  const json = JSON.parse(buf.subarray(20, 20 + chunkLength).toString("utf8")) as {
    nodes?: { name?: string }[];
  };
  return (json.nodes ?? []).map((node) => node.name ?? "");
}

test("paging stays inside the comic and crosses chapter boundaries", () => {
  assert.equal(INTRO_COMIC_PAGE_COUNT, 8);
  assert.equal(COMIC_PAGE_COUNT, 19);
  assert.equal(stepComicPage(1, -1), 1);
  assert.equal(stepComicPage(1, 1), 2);
  assert.equal(stepComicPage(4, -1), 3);
  assert.equal(stepComicPage(8, 1), 9);
  assert.equal(stepComicPage(8, -1), 7);
  assert.equal(stepComicPage(14, 1), 15);
  assert.equal(stepComicPage(19, 1), 19);
  assert.equal(stepComicPage(19, -1), 18);
});

test("panel files and cameras use the toolkit's zero-based index", () => {
  assert.equal(introComicPanelUrl(1), "/comics/intro/panel01.glb");
  assert.equal(introComicPanelUrl(6), "/comics/intro/panel06.glb");
  assert.equal(introComicPanelUrl(8), "/comics/intro/panel08.glb");
  assert.equal(comicCameraName(1), "Camera.0000");
  assert.equal(comicCameraName(6), "Camera.0005");
  assert.equal(comicCameraName(9), "Camera.0008");
  assert.equal(comicCameraName(19), "Camera.0018");
  assert.equal(comicAimName(6), "Camera_aim.0005");
  assert.equal(comicAimName(12), "Camera_aim.0011");
  assert.equal(comicLettersName(8), "Letters_english.0007");
  assert.equal(comicLettersName(15), "Letters_english.0014");
  assert.deepEqual(comicPanelUrlsToKeep(1), [
    "/comics/intro/panel01.glb",
    "/comics/intro/panel02.glb",
  ]);
  assert.deepEqual(comicPanelUrlsToKeep(8), [
    "/comics/intro/panel07.glb",
    "/comics/intro/panel08.glb",
    "/comics/ch01/panel09.glb",
  ]);
  assert.deepEqual(comicPanelUrlsToKeep(19), [
    "/comics/ch02/panel18.glb",
    "/comics/ch02/panel19.glb",
  ]);
});

test("chapters are a flat reading order with a start page for each", () => {
  assert.deepEqual(
    COMIC_CHAPTERS.map((chapter) => [chapter.id, chapter.title, chapter.startPage, chapter.endPage]),
    [
      ["intro", "Intro", 1, 8],
      ["ch01", "The Meadow Match", 9, 14],
      ["ch02", "Volcanic Pit", 15, 19],
    ],
  );
  assert.equal(comicChapterForPage(1)?.title, "Intro");
  assert.equal(comicChapterForPage(8)?.title, "Intro");
  assert.equal(comicChapterForPage(9)?.title, "The Meadow Match");
  assert.equal(comicChapterForPage(14)?.id, "ch01");
  assert.equal(comicChapterForPage(15)?.title, "Volcanic Pit");
  assert.equal(comicChapterForPage(19)?.title, "Volcanic Pit");
  assert.equal(comicPanelUrl(9), "/comics/ch01/panel09.glb");
  assert.equal(comicPanelUrl(14), "/comics/ch01/panel14.glb");
  assert.equal(comicPanelUrl(15), "/comics/ch02/panel15.glb");
  assert.equal(comicPanelUrl(19), "/comics/ch02/panel19.glb");
  assert.equal(comicPages().length, 19);
});

test("a page keeps only its neighbors, not the rest of the chapter", () => {
  assert.deepEqual(comicPanelUrlsToKeep(12), [
    "/comics/ch01/panel11.glb",
    "/comics/ch01/panel12.glb",
    "/comics/ch01/panel13.glb",
  ]);
  const chapter = comicChapterForPage(12);
  assert.ok(chapter);
  assert.ok(comicPanelUrlsToKeep(12).length < chapter.endPage - chapter.startPage + 1);
  for (let page = 1; page <= COMIC_PAGE_COUNT; page += 1) {
    const urls = comicPanelUrlsToKeep(page);
    assert.ok(urls.length >= 1 && urls.length <= 3);
    assert.equal(urls.includes(comicPanelUrl(page)!), true);
  }
});

test("keys, swipes, and edge taps map to next, back, and close", () => {
  assert.equal(comicCommandForKey("ArrowRight"), "next");
  assert.equal(comicCommandForKey("ArrowDown"), "next");
  assert.equal(comicCommandForKey(" "), "next");
  assert.equal(comicCommandForKey("ArrowLeft"), "prev");
  assert.equal(comicCommandForKey("ArrowUp"), "prev");
  assert.equal(comicCommandForKey("Escape"), "close");
  assert.equal(comicCommandForKey("a"), null);

  assert.equal(comicCommandForSwipe(-80, 10), "next");
  assert.equal(comicCommandForSwipe(80, 5), "prev");
  assert.equal(comicCommandForSwipe(10, -90), "next");
  assert.equal(comicCommandForSwipe(4, 20), null);

  assert.equal(comicCommandForTap(10, 100), "prev");
  assert.equal(comicCommandForTap(90, 100), "next");
  assert.equal(comicCommandForTap(50, 100), null);
});

test("a missing panel tells the reader which file to put in public/comics", () => {
  assert.match(missingComicPanelMessage(3), /\/comics\/intro\/panel03\.glb/);
  assert.match(missingComicPanelMessage(3), /client\/public\/comics\/intro/);
  assert.match(missingComicPanelMessage(12), /\/comics\/ch01\/panel12\.glb/);
  assert.match(missingComicPanelMessage(12), /client\/public\/comics\/ch01/);
  assert.match(missingComicPanelMessage(19), /\/comics\/ch02\/panel19\.glb/);
  assert.match(missingComicPanelMessage(19), /client\/public\/comics\/ch02/);
});

test("shipped panels carry the toolkit camera, aim, and lettering for their page", () => {
  for (const page of comicPages()) {
    const filePath = path.join(comicsDir, page.file.replace(/^\/comics\//, ""));
    const names = gltfNodeNames(filePath);
    assert.equal(names.includes(comicCameraName(page.page)), true, page.file);
    assert.equal(names.includes(comicAimName(page.page)), true, page.file);
    assert.equal(names.includes(comicLettersName(page.page)), true, page.file);
  }
});

test("the authored camera is found after three.js strips dots from its name", () => {
  const scene = new THREE.Group();
  const camera = new THREE.PerspectiveCamera(32, 16 / 9, 0.1, 200);
  camera.name = THREE.PropertyBinding.sanitizeNodeName(comicCameraName(6));
  camera.position.set(-1.9, -0.25, 6.3);
  const aim = new THREE.Object3D();
  aim.name = THREE.PropertyBinding.sanitizeNodeName(comicAimName(6));
  aim.position.set(-0.85, 1.15, 0);
  const letters = new THREE.Group();
  letters.name = THREE.PropertyBinding.sanitizeNodeName(comicLettersName(6));
  const ink = new THREE.MeshBasicMaterial();
  ink.name = "Letter_ink";
  const text = new THREE.Mesh(new THREE.BufferGeometry(), ink);
  letters.add(text);
  camera.add(letters);
  scene.add(camera, aim);

  assert.equal(camera.name, "Camera0005");
  const prepared = prepareComicPanel(scene, 6);
  assert.equal(prepared.camera, camera);
  assert.ok(prepared.aim.distanceTo(aim.position) < 1e-4);
  assert.equal(ink.toneMapped, false);
  assert.equal(isUnderComicLetters(text), true);
});

test("balloon, caption, and lettering materials skip tone mapping", () => {
  assert.equal(isComicLetteringMaterial("Balloon_white"), true);
  assert.equal(isComicLetteringMaterial("Caption_cream"), true);
  assert.equal(isComicLetteringMaterial("Letter_ink"), true);
  assert.equal(isComicLetteringMaterial("Letter_#e2337f"), true);
  assert.equal(isComicLetteringMaterial("Logo_burst"), true);
  assert.equal(isComicLetteringMaterial("Gutter_white"), true);
  assert.equal(isComicLetteringMaterial("Meadow_grass_sunny"), false);
  assert.equal(isComicLetteringMaterial("NS_C_glass"), false);
  assert.equal(isComicLetteringMaterial("Img_Logo_zoogi_roll_arena"), false);

  const letters = new THREE.Group();
  letters.name = "Letters_english.0002";
  const logo = new THREE.Object3D();
  logo.name = "Img_Logo";
  letters.add(logo);
  const rock = new THREE.Object3D();
  rock.name = "Rock_00";
  assert.equal(isUnderComicLetters(logo), true);
  assert.equal(isUnderComicLetters(rock), false);
});

test("preparing a panel keeps the authored camera and the true lettering color", () => {
  const scene = new THREE.Group();
  scene.userData.comic_ambient_color = "#b9d4ff";
  scene.userData.comic_ambient_strength = 0.6;

  const aim = new THREE.Object3D();
  aim.name = comicAimName(1);
  aim.position.set(0, 3.6, -8);

  const camera = new THREE.PerspectiveCamera(35, 16 / 9, 0.1, 400);
  camera.name = comicCameraName(1);
  camera.position.set(0, 9.5, 38);

  const letters = new THREE.Group();
  letters.name = comicLettersName(1);
  const balloonMaterial = new THREE.MeshBasicMaterial({ color: 0xffffff });
  balloonMaterial.name = "Balloon_white";
  const balloon = new THREE.Mesh(new THREE.BufferGeometry(), balloonMaterial);
  const logoMaterial = new THREE.MeshBasicMaterial();
  logoMaterial.name = "Img_Logo_zoogi_roll_arena";
  const logo = new THREE.Mesh(new THREE.BufferGeometry(), logoMaterial);
  letters.add(balloon, logo);
  camera.add(letters);

  const grassMaterial = new THREE.MeshStandardMaterial();
  grassMaterial.name = "Meadow_grass_sunny";
  const grass = new THREE.Mesh(new THREE.BufferGeometry(), grassMaterial);

  const gutterMaterial = new THREE.MeshBasicMaterial({ color: 0xffffff });
  gutterMaterial.name = "Gutter_white";
  const gutter = new THREE.Mesh(new THREE.BufferGeometry(), gutterMaterial);

  scene.add(aim, camera, grass, gutter);

  const prepared = prepareComicPanel(scene, 1);
  prepareComicPanel(scene, 1);

  assert.equal(prepared.camera, camera);
  assert.ok(prepared.aim.distanceTo(aim.position) < 1e-4);
  assert.equal(balloonMaterial.toneMapped, false);
  assert.equal(logoMaterial.toneMapped, false);
  assert.equal(gutterMaterial.toneMapped, false);
  assert.equal(grassMaterial.toneMapped, true);
  assert.equal(balloon.frustumCulled, false);
  assert.equal(scene.getObjectsByProperty("name", "ComicAmbient").length, 1);

  const limits = comicOrbitLimits(camera.position, aim.position);
  const offset = camera.position.clone().sub(aim.position);
  const spherical = new THREE.Spherical().setFromVector3(offset);
  assert.ok(spherical.radius >= limits.minDistance && spherical.radius <= limits.maxDistance);
  assert.ok(spherical.phi >= limits.minPolarAngle && spherical.phi <= limits.maxPolarAngle);
  assert.ok(spherical.theta >= limits.minAzimuthAngle && spherical.theta <= limits.maxAzimuthAngle);
});

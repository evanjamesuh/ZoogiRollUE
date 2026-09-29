import test from "node:test";
import assert from "node:assert/strict";
import * as THREE from "three";
import {
  INTRO_COMIC_PAGE_COUNT,
  comicAimName,
  comicCameraName,
  comicCommandForKey,
  comicCommandForSwipe,
  comicCommandForTap,
  comicLettersName,
  comicOrbitLimits,
  comicPanelUrlsToKeep,
  introComicPanelUrl,
  isComicLetteringMaterial,
  isUnderComicLetters,
  missingComicPanelMessage,
  prepareComicPanel,
  stepComicPage,
} from "./comicPanels.ts";

test("paging stays inside the eight intro panels", () => {
  assert.equal(INTRO_COMIC_PAGE_COUNT, 8);
  assert.equal(stepComicPage(1, -1), 1);
  assert.equal(stepComicPage(1, 1), 2);
  assert.equal(stepComicPage(4, -1), 3);
  assert.equal(stepComicPage(8, 1), 8);
  assert.equal(stepComicPage(8, -1), 7);
});

test("panel files and cameras use the toolkit's zero-based index", () => {
  assert.equal(introComicPanelUrl(1), "/comics/intro/panel01.glb");
  assert.equal(introComicPanelUrl(6), "/comics/intro/panel06.glb");
  assert.equal(introComicPanelUrl(8), "/comics/intro/panel08.glb");
  assert.equal(comicCameraName(1), "Camera.0000");
  assert.equal(comicCameraName(6), "Camera.0005");
  assert.equal(comicAimName(6), "Camera_aim.0005");
  assert.equal(comicLettersName(8), "Letters_english.0007");
  assert.deepEqual(comicPanelUrlsToKeep(1), [
    "/comics/intro/panel01.glb",
    "/comics/intro/panel02.glb",
  ]);
  assert.deepEqual(comicPanelUrlsToKeep(8), ["/comics/intro/panel08.glb"]);
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

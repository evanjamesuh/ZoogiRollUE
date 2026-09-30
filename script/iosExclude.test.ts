import assert from "node:assert/strict";
import { readFileSync } from "node:fs";
import test from "node:test";

const excluded = readFileSync(new URL("./ios-exclude.txt", import.meta.url), "utf8")
  .split("\n")
  .map((line) => line.replace(/#.*/, "").trim())
  .filter(Boolean);

const kept = [
  "models/wolfgang.glb",
  "models/hotstreak.glb",
  "models/pinpoint.glb",
  "models/bolt.glb",
  "models/wraps.glb",
  "models/lars.glb",
  "models/nightshade.glb",
  "portraits/wolfgang.png",
  "portraits/hotstreak.png",
  "portraits/pinpoint.png",
  "portraits/bolt.png",
  "portraits/wraps.png",
  "portraits/lars.png",
  "portraits/nightshade.png",
  "models/winter_location.glb",
  "models/arabian_nights_stage.glb",
  "models/volcanic_pit.glb",
  "models/cosmos_arena.glb",
  "models/bumber1.glb",
  "models/stylized_oak_tree.glb",
  "models/stylized_pine_tree.glb",
  "models/stylized_desert_hoodoo.glb",
  "models/stylized_alien_crystal.glb",
  "models/stylized_snow_pine.glb",
  "models/winter/fox.glb",
  "models/character/body.glb",
  "textures/grass.png",
  "textures/meadows_background.png",
  "textures/meadow_background.png",
  "textures/comic/paint_sand_512.jpg",
  "textures/comic/paint_stone_512.jpg",
];

test("the iPhone pack keeps every file a match loads", () => {
  for (const file of kept) {
    assert.equal(excluded.includes(file), false, file);
  }
});

test("the iPhone pack still drops the unused stage files", () => {
  assert.ok(excluded.includes("models/floating_island_stage.glb"));
  assert.ok(excluded.includes("models/saturn_park_scene.glb"));
  assert.ok(excluded.includes("models/zoogi_town.glb"));
  assert.ok(excluded.includes("models/workshop.glb"));
  assert.ok(excluded.includes("textures/cosmos_background.mp4"));
  assert.ok(excluded.includes("textures/meadows_background.mp4"));
});

import test from "node:test";
import assert from "node:assert/strict";
import { GRASS_RIM, rimPosition } from "./roundRim.ts";
import { getMapLayout, laneIsClear, listRimGapAdjustments } from "./arenaColliders.ts";
import { arenaScaleFor } from "./arenaScale.ts";
import { RIM_GAP_FLUSH, RIM_GAP_LANE, rimGapInBand } from "./obstaclePlacement.ts";

function gapClearsBothBands(gap: number, label: string) {
  assert.equal(rimGapInBand(gap), false, `${label} sits in the snap band (${gap.toFixed(3)})`);
  assert.ok(
    gap <= RIM_GAP_FLUSH || gap >= RIM_GAP_LANE,
    `${label} gap ${gap.toFixed(3)} is neither flush (${RIM_GAP_FLUSH}) nor a lane (${RIM_GAP_LANE})`,
  );
}

test("laid-out meadow rim gaps are flush or a full lane, and the placer does not move them", () => {
  const layout = getMapLayout("grass");
  assert.ok(layout);
  const scale = arenaScaleFor("grass");
  const adjustments = listRimGapAdjustments().filter((row) => row.mapId === "grass");
  assert.deepEqual(adjustments, [], "the placer snapped or moved a meadow obstacle");

  const rocks = layout.scenery.map((solid) => ({
    ...solid,
    angle: Math.atan2(solid.z, solid.x),
  }));
  assert.equal(rocks.length, GRASS_RIM.length);

  for (const mark of GRASS_RIM) {
    const solid = rocks.find((rock) => rock.id === mark.id);
    assert.ok(solid, mark.id);
    const authored = rimPosition(mark);
    assert.ok(Math.abs(solid.x - authored.x * scale) < 1e-6, `${mark.id} x moved`);
    assert.ok(Math.abs(solid.z - authored.z * scale) < 1e-6, `${mark.id} z moved`);
    assert.equal(solid.radius, mark.radius);
    const edge = layout.knockoffRadius - Math.hypot(solid.x, solid.z) - solid.radius;
    gapClearsBothBands(edge, `${mark.id} edge`);
  }

  rocks.sort((a, b) => a.angle - b.angle);
  for (let i = 0; i < rocks.length; i++) {
    const a = rocks[i];
    const b = rocks[(i + 1) % rocks.length];
    const gap = Math.hypot(a.x - b.x, a.z - b.z) - a.radius - b.radius;
    gapClearsBothBands(gap, `${a.id} to ${b.id}`);
  }

  assert.equal(laneIsClear(layout, 0), true, "east spawn lane is blocked");
});

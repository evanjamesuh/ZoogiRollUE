/**
 * Obstacle placement at read time.
 *
 * Authored layouts stay in today's units. A dress group may scale the
 * floor, but an obstacle mesh keeps its original size: compound shapes
 * (one rock split into several hit circles) move as a rigid body, and
 * embedded meshes are counter-scaled by 1 / arenaScale.
 *
 * Scaling positions while leaving radii alone opens or closes the slot
 * between an obstacle and the knockout edge. A marble's width is its
 * collision diameter. A rim gap in [0.4, 1.7) marble widths is an opening
 * a marble almost fits through, so those slots are nudged shut or open.
 */

import { ZOOGI_DIAMETER } from "./restHeight";

/** Collision diameter. Rim gaps are measured in these widths. */
export const MARBLE_WIDTH = ZOOGI_DIAMETER;
/** A gap in [SEALED, OPEN) is nudged. The nudge lands on FLUSH or LANE, inside the rule by a margin. */
export const RIM_GAP_SEALED = 0.4;
export const RIM_GAP_OPEN = 1.7;
export const RIM_GAP_FLUSH = 0.3;
export const RIM_GAP_LANE = 1.8;
/** Float slack for the seal/lane test. Targets sit well inside this. */
export const RIM_GAP_TOLERANCE = 0.02;

export interface PlacementCircle {
  id: string;
  x: number;
  z: number;
  radius: number;
}

export interface RimGapAdjustment {
  meshKey: string;
  ids: string[];
  kind: "edge" | "mouth";
  direction: "inward" | "outward" | "closed" | "opened";
  dx: number;
  dz: number;
  distance: number;
  /** Clearances in marble widths. Edge rows are the knockout slot; mouth rows are the gap between two meshes. */
  clearances: { id: string; before: number; after: number }[];
}

export function obstacleMeshKey(id: string): string {
  const rock = /^S_\d+_rock/.exec(id);
  if (rock) return rock[0];
  const wall = /^wall-\d+/.exec(id);
  if (wall) return wall[0];
  return id;
}

/** True when a GLB node name is the mesh that owns this collider key. */
export function nodeMatchesMeshKey(name: string, meshKey: string): boolean {
  if (!name || !meshKey) return false;
  const node = name.toLowerCase();
  const key = meshKey.toLowerCase();
  if (node === key) return true;
  if (node.startsWith(`${key}_`) || node.startsWith(`${key}-`) || node.startsWith(`${key}.`)) return true;
  if (key.startsWith(`${node}_`) || key.startsWith(`${node}-`) || key.startsWith(`${node}.`)) return true;
  return false;
}

export function counterScaleFor(arenaScale: number): number {
  if (!Number.isFinite(arenaScale) || arenaScale === 0) return 1;
  return 1 / arenaScale;
}

/** World size of a mesh that sits in a dress group and is counter-scaled. */
export function embeddedWorldScale(arenaScale: number, nodeScale: number): number {
  return arenaScale * nodeScale;
}

export function rimGapWidths(worldGap: number, marbleWidth = MARBLE_WIDTH): number {
  return worldGap / marbleWidth;
}

export function rimGapInBand(worldGap: number, marbleWidth = MARBLE_WIDTH): boolean {
  const widths = rimGapWidths(worldGap, marbleWidth);
  return widths >= RIM_GAP_SEALED && widths < RIM_GAP_OPEN;
}

/**
 * True when a gap is under 0.4 or at least 1.7 marble widths.
 * `tolerance` keeps a value that lands on the boundary from failing the test.
 */
export function rimGapAllowed(worldGap: number, marbleWidth = MARBLE_WIDTH, tolerance = RIM_GAP_TOLERANCE): boolean {
  const widths = rimGapWidths(worldGap, marbleWidth);
  return widths < RIM_GAP_SEALED + tolerance || widths >= RIM_GAP_OPEN - tolerance;
}

/**
 * Snap a world-unit opening to a flush joint (0.3) or a lane (1.8).
 * The midpoint of the band opens the lane, and a hair of float around that
 * midpoint stays on the same side.
 */
export function snapRimGap(worldGap: number, marbleWidth = MARBLE_WIDTH): number {
  if (!rimGapInBand(worldGap, marbleWidth)) return worldGap;
  const widths = rimGapWidths(worldGap, marbleWidth);
  const mid = (RIM_GAP_SEALED + RIM_GAP_OPEN) / 2;
  const flush = widths < mid - 1e-4;
  return (flush ? RIM_GAP_FLUSH : RIM_GAP_LANE) * marbleWidth;
}

function radialClearance(x: number, z: number, radius: number, knockoffRadius: number): number {
  return knockoffRadius - Math.hypot(x, z) - radius;
}

function groupByKey<T extends PlacementCircle>(circles: T[]): Map<string, T[]> {
  const groups = new Map<string, T[]>();
  for (const circle of circles) {
    const key = obstacleMeshKey(circle.id);
    const list = groups.get(key);
    if (list) list.push(circle);
    else groups.set(key, [circle]);
  }
  return groups;
}

function compoundPlace<T extends PlacementCircle>(circles: T[], scale: number) {
  for (const group of groupByKey(circles).values()) {
    const cx = group.reduce((sum, circle) => sum + circle.x, 0) / group.length;
    const cz = group.reduce((sum, circle) => sum + circle.z, 0) / group.length;
    for (const circle of group) {
      const offsetX = circle.x - cx;
      const offsetZ = circle.z - cz;
      circle.x = cx * scale + offsetX;
      circle.z = cz * scale + offsetZ;
    }
  }
}

function centroid(group: PlacementCircle[]): { x: number; z: number } {
  return {
    x: group.reduce((sum, circle) => sum + circle.x, 0) / group.length,
    z: group.reduce((sum, circle) => sum + circle.z, 0) / group.length,
  };
}

function translateGroup(group: PlacementCircle[], dx: number, dz: number) {
  for (const circle of group) {
    circle.x += dx;
    circle.z += dz;
  }
}

function groupIsClear(group: PlacementCircle[], knockoffRadius: number, marbleWidth: number): boolean {
  return group.every((circle) => !rimGapInBand(radialClearance(circle.x, circle.z, circle.radius, knockoffRadius), marbleWidth));
}

function outwardShift(group: PlacementCircle[], delta: number): { dx: number; dz: number } {
  const center = centroid(group);
  const distance = Math.hypot(center.x, center.z) || 1;
  const next = Math.max(0.05, distance + delta);
  return {
    dx: (center.x * next) / distance - center.x,
    dz: (center.z * next) / distance - center.z,
  };
}

function copyShift<T extends PlacementCircle>(group: T[], dx: number, dz: number): T[] {
  return group.map((circle) => ({ ...circle, x: circle.x + dx, z: circle.z + dz }));
}

function repairEdgeGaps<T extends PlacementCircle>(
  circles: T[],
  knockoffRadius: number,
  marbleWidth: number,
  adjustments: RimGapAdjustment[],
) {
  const mid = ((RIM_GAP_SEALED + RIM_GAP_OPEN) / 2) * marbleWidth;
  for (const [meshKey, group] of groupByKey(circles)) {
    const before = group.map((circle) => ({
      id: circle.id,
      gap: radialClearance(circle.x, circle.z, circle.radius, knockoffRadius),
    }));
    if (before.every((entry) => !rimGapInBand(entry.gap, marbleWidth))) continue;
    let totalDx = 0;
    let totalDz = 0;
    for (let pass = 0; pass < 8; pass++) {
      let driver: number | null = null;
      for (const circle of group) {
        const gap = radialClearance(circle.x, circle.z, circle.radius, knockoffRadius);
        if (!rimGapInBand(gap, marbleWidth)) continue;
        if (driver === null || Math.abs(gap - mid) < Math.abs(driver - mid)) driver = gap;
      }
      if (driver === null) break;
      const delta = driver - snapRimGap(driver, marbleWidth);
      if (Math.abs(delta) < 1e-6) break;
      const shift = outwardShift(group, delta);
      translateGroup(group, shift.dx, shift.dz);
      totalDx += shift.dx;
      totalDz += shift.dz;
    }
    const distance = Math.hypot(totalDx, totalDz);
    if (distance < 1e-6) continue;
    adjustments.push({
      meshKey,
      ids: group.map((circle) => circle.id),
      kind: "edge",
      direction: totalDx * (group[0]?.x ?? 0) + totalDz * (group[0]?.z ?? 0) > 0 ? "outward" : "inward",
      dx: totalDx,
      dz: totalDz,
      distance,
      clearances: before.map((entry) => {
        const circle = group.find((item) => item.id === entry.id)!;
        return {
          id: entry.id,
          before: rimGapWidths(entry.gap, marbleWidth),
          after: rimGapWidths(radialClearance(circle.x, circle.z, circle.radius, knockoffRadius), marbleWidth),
        };
      }),
    });
  }
}

function repairMouthGaps<T extends PlacementCircle>(
  circles: T[],
  knockoffRadius: number,
  marbleWidth: number,
  adjustments: RimGapAdjustment[],
) {
  const nearRim = (circle: PlacementCircle) =>
    radialClearance(circle.x, circle.z, circle.radius, knockoffRadius) < RIM_GAP_OPEN * marbleWidth + 0.5;

  for (let pass = 0; pass < 6; pass++) {
    const edge = circles.filter(nearRim).sort((a, b) => Math.atan2(a.z, a.x) - Math.atan2(b.z, b.x));
    let changed = false;
    for (let i = 0; i < edge.length; i++) {
      const a = edge[i];
      const b = edge[(i + 1) % edge.length];
      const keyA = obstacleMeshKey(a.id);
      const keyB = obstacleMeshKey(b.id);
      if (keyA === keyB) continue;
      const distance = Math.hypot(a.x - b.x, a.z - b.z);
      const gap = distance - a.radius - b.radius;
      if (gap < 0 || !rimGapInBand(gap, marbleWidth)) continue;
      const target = snapRimGap(gap, marbleWidth);
      const unitX = (b.x - a.x) / distance;
      const unitZ = (b.z - a.z) / distance;
      const shift = (gap - target) / 2;
      const groupA = circles.filter((circle) => obstacleMeshKey(circle.id) === keyA);
      const groupB = circles.filter((circle) => obstacleMeshKey(circle.id) === keyB);
      const tryShift = (amount: number) => {
        const nextA = copyShift(groupA, unitX * amount, unitZ * amount);
        const nextB = copyShift(groupB, -unitX * amount, -unitZ * amount);
        return groupIsClear(nextA, knockoffRadius, marbleWidth) && groupIsClear(nextB, knockoffRadius, marbleWidth);
      };
      const closing = shift;
      const opening = (gap - RIM_GAP_LANE * marbleWidth) / 2;
      const amount = tryShift(closing) ? closing : tryShift(opening) ? opening : null;
      if (amount === null || Math.abs(amount) < 1e-6) continue;
      translateGroup(groupA, unitX * amount, unitZ * amount);
      translateGroup(groupB, -unitX * amount, -unitZ * amount);
      const after = Math.hypot(a.x - b.x, a.z - b.z) - a.radius - b.radius;
      adjustments.push({
        meshKey: `${keyA}|${keyB}`,
        ids: [a.id, b.id],
        kind: "mouth",
        direction: amount > 0 ? "closed" : "opened",
        dx: unitX * amount,
        dz: unitZ * amount,
        distance: Math.abs(amount) * 2,
        clearances: [
          { id: `${a.id}|${b.id}`, before: rimGapWidths(gap, marbleWidth), after: rimGapWidths(after, marbleWidth) },
        ],
      });
      changed = true;
      break;
    }
    if (!changed) break;
  }
}

/**
 * Spread authored circles by `scale`, keeping each mesh's size and the
 * offsets inside a compound mesh. Then nudge any rim slot that a marble
 * almost fits through.
 */
export function placeScaledObstacles<T extends PlacementCircle>(
  authored: readonly T[],
  scale: number,
  knockoffRadius: number,
  marbleWidth = MARBLE_WIDTH,
): { circles: T[]; adjustments: RimGapAdjustment[] } {
  const circles = authored.map((circle) => ({ ...circle }));
  compoundPlace(circles, scale);
  const adjustments: RimGapAdjustment[] = [];
  repairEdgeGaps(circles, knockoffRadius, marbleWidth, adjustments);
  repairMouthGaps(circles, knockoffRadius, marbleWidth, adjustments);
  return { circles, adjustments };
}

export interface PlacementBox {
  id: string;
  minX: number;
  maxX: number;
  minZ: number;
  maxZ: number;
}

function nearestEdge(box: PlacementBox, halfX: number, halfZ: number): { gap: number; dx: number; dz: number } {
  const options = [
    { gap: box.minX + halfX, dx: -1, dz: 0 },
    { gap: halfX - box.maxX, dx: 1, dz: 0 },
    { gap: box.minZ + halfZ, dx: 0, dz: -1 },
    { gap: halfZ - box.maxZ, dx: 0, dz: 1 },
  ];
  options.sort((a, b) => a.gap - b.gap);
  return options[0];
}

/** Slide a box toward or away from the knockout rectangle until the slot is sealed or clearly open. */
export function repairRectRimGaps<T extends PlacementBox>(
  boxes: readonly T[],
  halfX: number,
  halfZ: number,
  marbleWidth = MARBLE_WIDTH,
): { boxes: T[]; adjustments: RimGapAdjustment[] } {
  const adjustments: RimGapAdjustment[] = [];
  const placed = boxes.map((box) => ({ ...box }));
  for (const box of placed) {
    const nearest = nearestEdge(box, halfX, halfZ);
    if (!rimGapInBand(nearest.gap, marbleWidth)) continue;
    const target = snapRimGap(nearest.gap, marbleWidth);
    const shift = nearest.gap - target;
    const before = nearest.gap;
    box.minX += nearest.dx * shift;
    box.maxX += nearest.dx * shift;
    box.minZ += nearest.dz * shift;
    box.maxZ += nearest.dz * shift;
    const after = nearestEdge(box, halfX, halfZ).gap;
    adjustments.push({
      meshKey: box.id,
      ids: [box.id],
      kind: "edge",
      direction: shift > 0 ? "outward" : "inward",
      dx: nearest.dx * shift,
      dz: nearest.dz * shift,
      distance: Math.abs(shift),
      clearances: [{ id: box.id, before: rimGapWidths(before, marbleWidth), after: rimGapWidths(after, marbleWidth) }],
    });
  }
  return { boxes: placed, adjustments };
}

interface EmbeddedNode {
  name: string;
  parent: EmbeddedNode | null;
  children: EmbeddedNode[];
  position: { x: number; y: number; z: number; set: (x: number, y: number, z: number) => void; clone: () => { x: number; y: number; z: number } };
  scale: { x: number; y: number; z: number; set: (x: number, y: number, z: number) => void; clone: () => { x: number; y: number; z: number } };
  userData: Record<string, unknown>;
  traverse: (callback: (node: EmbeddedNode) => void) => void;
}

export interface ObstacleNudge {
  meshKey: string;
  dx: number;
  dz: number;
}

const FLOOR_NAME = /ground|floor|island|terrain|plaza|courtyard|platform/i;

function containsFloor(node: EmbeddedNode, keys: readonly string[]): boolean {
  let found = false;
  node.traverse((child) => {
    if (child === node || !FLOOR_NAME.test(child.name)) return;
    if (keys.some((key) => nodeMatchesMeshKey(child.name, key))) return;
    found = true;
  });
  return found;
}

function hasMatchedAncestor(node: EmbeddedNode, matched: Set<EmbeddedNode>): boolean {
  let parent = node.parent;
  while (parent) {
    if (matched.has(parent)) return true;
    parent = parent.parent;
  }
  return false;
}

/**
 * Counter-scale collider meshes that live inside a dress group, and apply
 * the same world nudge the colliders received. Returns a restore function.
 */
export function applyEmbeddedObstaclePose(
  root: { traverse: (callback: (node: EmbeddedNode) => void) => void },
  arenaScale: number,
  modelScale: number,
  meshKeys: readonly string[],
  nudges: readonly ObstacleNudge[],
): () => void {
  const graph = root;
  const touched: EmbeddedNode[] = [];
  if (!meshKeys.length || !Number.isFinite(arenaScale) || arenaScale === 0) return () => {};
  const matched = new Set<EmbeddedNode>();
  graph.traverse((node) => {
    if (node === root) return;
    if (meshKeys.some((key) => nodeMatchesMeshKey(node.name, key))) matched.add(node);
  });
  const inverse = counterScaleFor(arenaScale);
  const divisor = arenaScale * modelScale;
  for (const node of matched) {
    if (hasMatchedAncestor(node, matched)) continue;
    if (containsFloor(node, meshKeys)) continue;
    const baseScale = (node.userData.baseScale as { x: number; y: number; z: number } | undefined) ?? node.scale.clone();
    const basePosition = (node.userData.basePosition as { x: number; y: number; z: number } | undefined) ?? node.position.clone();
    node.userData.baseScale = baseScale;
    node.userData.basePosition = basePosition;
    node.scale.set(baseScale.x * inverse, baseScale.y * inverse, baseScale.z * inverse);
    let nudge: ObstacleNudge | null = null;
    let best = -1;
    for (const candidate of nudges) {
      if (nodeMatchesMeshKey(node.name, candidate.meshKey) && candidate.meshKey.length > best) {
        nudge = candidate;
        best = candidate.meshKey.length;
      }
    }
    if (nudge && divisor !== 0) {
      node.position.set(basePosition.x + nudge.dx / divisor, basePosition.y, basePosition.z + nudge.dz / divisor);
    }
    touched.push(node);
  }
  return () => {
    for (const node of touched) {
      const baseScale = node.userData.baseScale as { x: number; y: number; z: number } | undefined;
      const basePosition = node.userData.basePosition as { x: number; y: number; z: number } | undefined;
      if (baseScale) node.scale.set(baseScale.x, baseScale.y, baseScale.z);
      if (basePosition) node.position.set(basePosition.x, basePosition.y, basePosition.z);
      delete node.userData.baseScale;
      delete node.userData.basePosition;
    }
  };
}

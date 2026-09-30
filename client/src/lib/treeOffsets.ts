import { getTreePositions, getSnowmanPositions, getAlienPositions, getCrystalPositions, getRockPositions, getSnowPinePositions, getMushroomPositions } from "./arenaConstants";
import type { MapTheme } from "./stores/useZoogiGame";

type ElementType = "tree" | "snowman" | "crystal" | "alien" | "rock" | "snowPine" | "mushroom";

interface MapOffsets {
  [elementType: string]: {
    [index: number]: [number, number, number];
  };
}

interface MapRotations {
  [elementType: string]: {
    [index: number]: number;
  };
}

const mapOffsets: { [mapTheme: string]: MapOffsets } = {};
const confirmedMapOffsets: { [mapTheme: string]: MapOffsets } = {};
const mapRotations: { [mapTheme: string]: MapRotations } = {};
const confirmedMapRotations: { [mapTheme: string]: MapRotations } = {};

let currentMap: MapTheme = "grass";

export function setCurrentMap(map: MapTheme) {
  currentMap = map;
  if (!mapOffsets[map]) mapOffsets[map] = {};
  if (!confirmedMapOffsets[map]) confirmedMapOffsets[map] = {};
  if (!mapRotations[map]) mapRotations[map] = {};
  if (!confirmedMapRotations[map]) confirmedMapRotations[map] = {};
}

export function getCurrentMap(): MapTheme {
  return currentMap;
}

export function getElementOffset(map: MapTheme, elementType: ElementType, index: number): [number, number, number] {
  const pending = mapOffsets[map]?.[elementType]?.[index];
  if (pending) return pending;
  const confirmed = confirmedMapOffsets[map]?.[elementType]?.[index];
  if (confirmed) return confirmed;
  return [0, 0, 0];
}

export function getElementRotation(map: MapTheme, elementType: ElementType, index: number): number {
  const pending = mapRotations[map]?.[elementType]?.[index];
  if (pending !== undefined) return pending;
  const confirmed = confirmedMapRotations[map]?.[elementType]?.[index];
  if (confirmed !== undefined) return confirmed;
  return 0;
}

export function setElementOffset(map: MapTheme, elementType: ElementType, index: number, offset: [number, number, number]) {
  if (!mapOffsets[map]) mapOffsets[map] = {};
  if (!mapOffsets[map][elementType]) mapOffsets[map][elementType] = {};
  mapOffsets[map][elementType][index] = offset;
}

export function setElementRotation(map: MapTheme, elementType: ElementType, index: number, rotation: number) {
  if (!mapRotations[map]) mapRotations[map] = {};
  if (!mapRotations[map][elementType]) mapRotations[map][elementType] = {};
  mapRotations[map][elementType][index] = rotation;
}

export function confirmAllOffsetsForMap(map: MapTheme) {
  if (mapOffsets[map]) {
    if (!confirmedMapOffsets[map]) confirmedMapOffsets[map] = {};
    Object.keys(mapOffsets[map]).forEach((elementType) => {
      if (!confirmedMapOffsets[map][elementType]) confirmedMapOffsets[map][elementType] = {};
      Object.keys(mapOffsets[map][elementType]).forEach((indexStr) => {
        const idx = parseInt(indexStr);
        confirmedMapOffsets[map][elementType][idx] = [...mapOffsets[map][elementType][idx]];
      });
    });
  }
  
  if (mapRotations[map]) {
    if (!confirmedMapRotations[map]) confirmedMapRotations[map] = {};
    Object.keys(mapRotations[map]).forEach((elementType) => {
      if (!confirmedMapRotations[map][elementType]) confirmedMapRotations[map][elementType] = {};
      Object.keys(mapRotations[map][elementType]).forEach((indexStr) => {
        const idx = parseInt(indexStr);
        confirmedMapRotations[map][elementType][idx] = mapRotations[map][elementType][idx];
      });
    });
  }
  
  mapOffsets[map] = {};
  mapRotations[map] = {};
  
  console.log(`All placements confirmed for map: ${map}`, { 
    offsets: confirmedMapOffsets[map], 
    rotations: confirmedMapRotations[map] 
  });
}

export function getTreeOffset(index: number): [number, number, number] {
  return getElementOffset(currentMap, "tree", index);
}

export function getTreeRotation(index: number): number {
  return getElementRotation(currentMap, "tree", index);
}

export function setTreeOffset(index: number, offset: [number, number, number]) {
  setElementOffset(currentMap, "tree", index, offset);
}

export function setTreeRotation(index: number, rotation: number) {
  setElementRotation(currentMap, "tree", index, rotation);
}

export function confirmAllOffsets(): { offsets: any; rotations: any } {
  confirmAllOffsetsForMap(currentMap);
  return { 
    offsets: confirmedMapOffsets[currentMap] || {}, 
    rotations: confirmedMapRotations[currentMap] || {} 
  };
}

export function getAdjustedTreePositions(): { position: [number, number, number]; radius: number }[] {
  const basePositions = getTreePositions();
  return basePositions.map((tree, i) => {
    const offset = getTreeOffset(i);
    return {
      position: [
        tree.position[0] + offset[0],
        tree.position[1] + offset[1],
        tree.position[2] + offset[2]
      ] as [number, number, number],
      radius: tree.radius
    };
  });
}

export function getAdjustedSnowmanPositions(): { position: [number, number, number]; radius: number }[] {
  const basePositions = getSnowmanPositions();
  return basePositions.map((snowman, i) => {
    const offset = getElementOffset(currentMap, "snowman", i);
    return {
      position: [
        snowman.position[0] + offset[0],
        snowman.position[1] + offset[1],
        snowman.position[2] + offset[2]
      ] as [number, number, number],
      radius: snowman.radius
    };
  });
}

export function getMapElementTypes(map: MapTheme): ElementType[] {
  switch (map) {
    case "grass":
      return ["tree"];
    case "ice":
      return ["snowman", "snowPine", "mushroom"];
    case "lava":
      return ["rock"];
    case "space":
      return ["alien", "crystal"];
    case "saturn":
      return ["rock", "crystal"];
    case "tomb":
      return [];
    default:
      return [];
  }
}

export function exportAllOffsets(): string {
  const exportData: { [map: string]: { elements: any[] } } = {};
  
  const maps: MapTheme[] = ["grass", "ice", "lava", "space", "saturn", "tomb"];
  
  for (const map of maps) {
    const elements = getMapElements(map);
    const elementData: any[] = [];
    
    for (const element of elements) {
      const offset = getElementOffset(map, element.type, element.index);
      const rotation = getElementRotation(map, element.type, element.index);
      
      elementData.push({
        type: element.type,
        index: element.index,
        offset: [
          parseFloat(offset[0].toFixed(2)),
          parseFloat(offset[1].toFixed(2)),
          parseFloat(offset[2].toFixed(2))
        ],
        rotation: parseFloat(rotation.toFixed(2)),
        finalPosition: [
          parseFloat(element.position[0].toFixed(2)),
          parseFloat(element.position[1].toFixed(2)),
          parseFloat(element.position[2].toFixed(2))
        ]
      });
    }
    
    if (elementData.length > 0) {
      exportData[map] = { elements: elementData };
    }
  }
  
  return JSON.stringify(exportData, null, 2);
}

export function getMapElements(map: MapTheme): { type: ElementType; index: number; position: [number, number, number]; radius: number }[] {
  const elements: { type: ElementType; index: number; position: [number, number, number]; radius: number }[] = [];
  const elementTypes = getMapElementTypes(map);
  console.log(`[getMapElements] Called with map="${map}", elementTypes:`, elementTypes);
  
  for (const elementType of elementTypes) {
    if (elementType === "tree") {
      const trees = getTreePositions();
      trees.forEach((tree, i) => {
        const offset = getElementOffset(map, "tree", i);
        elements.push({
          type: "tree",
          index: i,
          position: [
            tree.position[0] + offset[0],
            tree.position[1] + offset[1],
            tree.position[2] + offset[2]
          ],
          radius: 1.5
        });
      });
    } else if (elementType === "snowman") {
      const snowmen = getSnowmanPositions();
      snowmen.forEach((snowman, i) => {
        const offset = getElementOffset(map, "snowman", i);
        elements.push({
          type: "snowman",
          index: i,
          position: [
            snowman.position[0] + offset[0],
            snowman.position[1] + offset[1],
            snowman.position[2] + offset[2]
          ],
          radius: snowman.radius
        });
      });
    } else if (elementType === "alien") {
      const aliens = getAlienPositions();
      aliens.forEach((alien, i) => {
        const offset = getElementOffset(map, "alien", i);
        elements.push({
          type: "alien",
          index: i,
          position: [
            alien.position[0] + offset[0],
            alien.position[1] + offset[1],
            alien.position[2] + offset[2]
          ],
          radius: 1.5
        });
      });
    } else if (elementType === "crystal") {
      const crystals = getCrystalPositions();
      crystals.forEach((crystal, i) => {
        const offset = getElementOffset(map, "crystal", i);
        elements.push({
          type: "crystal",
          index: i,
          position: [
            crystal.position[0] + offset[0],
            crystal.position[1] + offset[1],
            crystal.position[2] + offset[2]
          ],
          radius: 1.2
        });
      });
    } else if (elementType === "rock") {
      const rocks = getRockPositions();
      rocks.forEach((rock, i) => {
        const offset = getElementOffset(map, "rock", i);
        elements.push({
          type: "rock",
          index: i,
          position: [
            rock.position[0] + offset[0],
            rock.position[1] + offset[1],
            rock.position[2] + offset[2]
          ],
          radius: 1.0
        });
      });
    } else if (elementType === "snowPine") {
      const snowPines = getSnowPinePositions();
      snowPines.forEach((snowPine, i) => {
        const offset = getElementOffset(map, "snowPine", i);
        elements.push({
          type: "snowPine",
          index: i,
          position: [
            snowPine.position[0] + offset[0],
            snowPine.position[1] + offset[1],
            snowPine.position[2] + offset[2]
          ],
          radius: snowPine.radius
        });
      });
    } else if (elementType === "mushroom") {
      const mushrooms = getMushroomPositions();
      mushrooms.forEach((mushroom, i) => {
        const offset = getElementOffset(map, "mushroom", i);
        elements.push({
          type: "mushroom",
          index: i,
          position: [
            mushroom.position[0] + offset[0],
            mushroom.position[1] + offset[1],
            mushroom.position[2] + offset[2]
          ],
          radius: mushroom.radius
        });
      });
    }
  }
  
  return elements;
}

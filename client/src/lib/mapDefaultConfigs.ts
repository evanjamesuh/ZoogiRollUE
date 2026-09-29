import type { WallSegmentConfig } from "./stores/useZoogiGame";
import { getMapLayout } from "./arenaColliders";
import { ARENA_SCALE_BY_MAP } from "./arenaScale";

export { ARENA_SCALE_BY_MAP, arenaScaleFor } from "./arenaScale";

/** arenaScale for every map. Layout code reads ARENA_SCALE_BY_MAP. */
export const MAP_ARENA_SCALES: { id: string; arenaScale: number }[] = Object.entries(ARENA_SCALE_BY_MAP).map(
  ([id, arenaScale]) => ({ id, arenaScale }),
);

export interface ZoneEditorConfig {
  id: string;
  angle: number;
  distance: number;
  visible: boolean;
  isSpawn: boolean;
}

export interface MapDefaultConfig {
  /** Playable-area multiplier for this map. See ARENA_SCALE_BY_MAP. */
  arenaScale?: number;
  wallSettings?: {
    outerWallEnabled?: boolean;
    outerWallRadiusOffset?: number;
    outerWallRows?: number;
    outerWallBlocksPerRow?: number;
    middleWallEnabled?: boolean;
    middleWallRadiusOffset?: number;
    middleWallRows?: number;
    middleWallBlocksPerRow?: number;
    middleWallGapWidth?: number;
    innerWallEnabled?: boolean;
    innerWallRadiusPercent?: number;
    innerWallRows?: number;
    innerWallBlocksPerRow?: number;
    knockoffBoundaryRadius?: number;
    knockoffBoundaryWidth?: number;
  };
  innerWallSegmentConfigs?: WallSegmentConfig[];
  wallSegmentConfigs?: WallSegmentConfig[];
  backgroundSettings?: {
    distance?: number;
    yPos?: number;
    width?: number;
    height?: number;
    rotation?: number;
    mirrorBack?: boolean;
    mirrorFront?: boolean;
    mirrorLeft?: boolean;
    mirrorRight?: boolean;
    opacity?: number;
    visible?: boolean;
    customImage?: string | null;
    groundScale?: number;
    modelPositionX?: number;
    modelPositionY?: number;
    modelPositionZ?: number;
    modelScale?: number;
  };
  elementTransforms?: {
    arenaModelOffset?: { x: number; y: number; z: number };
    arenaModelRotation?: { x: number; y: number; z: number };
    arenaModelScale?: number;
    outerWallOffset?: { x: number; y: number; z: number };
    middleWallOffset?: { x: number; y: number; z: number };
    innerWallOffset?: { x: number; y: number; z: number };
    zonesOffset?: { x: number; y: number; z: number };
    knockoffBoundaryOffset?: { x: number; y: number; z: number };
  };
  zoneSettings?: {
    globalRotationOffset?: number;
    globalDistanceOffset?: number;
    globalScale?: number;
    zonesVisible?: boolean;
  };
  zoneEditorConfigs?: ZoneEditorConfig[];
}

export const SPACE_MAP_DEFAULT_CONFIG: MapDefaultConfig = {
  arenaScale: ARENA_SCALE_BY_MAP.space,
  backgroundSettings: {
    distance: 200,
    yPos: 70,
    width: 500,
    height: 250,
    rotation: 0,
    mirrorBack: false,
    mirrorFront: true,
    mirrorLeft: false,
    mirrorRight: true,
    opacity: 0.9,
    visible: true,
    customImage: null,
    groundScale: 3,
    modelPositionX: 0,
    modelPositionY: -0.5,
    modelPositionZ: 0,
    modelScale: 3
  },
  wallSettings: {
    outerWallEnabled: true,
    outerWallRadiusOffset: 25,
    outerWallRows: 3,
    outerWallBlocksPerRow: 64,
    middleWallEnabled: true,
    middleWallRadiusOffset: 2,
    middleWallRows: 4,
    middleWallBlocksPerRow: 48,
    middleWallGapWidth: 0.55,
    innerWallEnabled: true,
    innerWallRadiusPercent: 70,
    innerWallRows: 2,
    innerWallBlocksPerRow: 36,
    knockoffBoundaryRadius: getMapLayout("space")?.knockoffRadius ?? 18.6,
    knockoffBoundaryWidth: 0.5
  },
  wallSegmentConfigs: [
    {
      id: "middle-curved-wall-0",
      segmentIndex: 0,
      visible: true,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 0.275,
      endAngle: 1.2957963267948962
    },
    {
      id: "middle-curved-wall-1",
      segmentIndex: 1,
      visible: true,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 1.8457963267948965,
      endAngle: 2.8665926535897928
    },
    {
      id: "middle-curved-wall-2",
      segmentIndex: 2,
      visible: true,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 3.416592653589793,
      endAngle: 4.437388980384689
    },
    {
      id: "middle-curved-wall-3",
      segmentIndex: 3,
      visible: true,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 4.98738898038469,
      endAngle: 6.008185307179586
    }
  ],
  innerWallSegmentConfigs: [
    {
      id: "inner-curved-wall-0",
      segmentIndex: 0,
      visible: false,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 0.1308996938995747,
      endAngle: 0.9162978572970228
    },
    {
      id: "inner-curved-wall-1",
      segmentIndex: 1,
      visible: true,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 1.1780972450961724,
      endAngle: 1.9634954084936211
    },
    {
      id: "inner-curved-wall-2",
      segmentIndex: 2,
      visible: false,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 2.22529479629277,
      endAngle: 3.0106929596902177
    },
    {
      id: "inner-curved-wall-3",
      segmentIndex: 3,
      visible: false,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 3.2724923474893677,
      endAngle: 4.057890510886816
    },
    {
      id: "inner-curved-wall-4",
      segmentIndex: 4,
      visible: true,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 4.319689898685965,
      endAngle: 5.105088062083414
    },
    {
      id: "inner-curved-wall-5",
      segmentIndex: 5,
      visible: false,
      positionOffset: { x: 0, y: 0, z: 0 },
      startAngle: 5.366887449882563,
      endAngle: 6.152285613280012
    }
  ],
  elementTransforms: {
    arenaModelOffset: { x: 0, y: 0, z: 0 },
    arenaModelRotation: { x: 0, y: 0, z: 0 },
    arenaModelScale: 1,
    outerWallOffset: { x: 0.7, y: 0, z: 0 },
    middleWallOffset: { x: 0.8, y: 0, z: 0 },
    innerWallOffset: { x: 0.8, y: 0, z: 0 },
    zonesOffset: { x: 0.7, y: 0, z: 0 },
    knockoffBoundaryOffset: { x: 0, y: 0, z: 0 }
  },
  zoneSettings: {
    globalRotationOffset: 0,
    globalDistanceOffset: 17,
    globalScale: 1,
    zonesVisible: true
  },
  zoneEditorConfigs: (getMapLayout("space")?.zones ?? []).map((zone) => ({
    id: zone.id,
    angle: zone.angle,
    distance: zone.distance,
    visible: zone.visible,
    isSpawn: zone.isSpawn,
  }))
};

export const MAP_DEFAULT_CONFIGS: Record<string, MapDefaultConfig> = {
  space: SPACE_MAP_DEFAULT_CONFIG
};

export function getMapDefaultConfig(mapId: string): MapDefaultConfig | null {
  return MAP_DEFAULT_CONFIGS[mapId] || null;
}

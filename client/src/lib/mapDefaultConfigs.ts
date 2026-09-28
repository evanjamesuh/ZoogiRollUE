import type { WallSegmentConfig } from "./stores/useZoogiGame";

export interface ZoneEditorConfig {
  id: string;
  angle: number;
  distance: number;
  visible: boolean;
  isSpawn: boolean;
}

export interface MapDefaultConfig {
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
    knockoffBoundaryRadius: 21,
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
    knockoffBoundaryOffset: { x: 0.9, y: 0, z: 0.2 }
  },
  zoneSettings: {
    globalRotationOffset: 0,
    globalDistanceOffset: 17,
    globalScale: 1,
    zonesVisible: true
  },
  zoneEditorConfigs: [
    { id: "control-zone-0", angle: 0, distance: 27, visible: true, isSpawn: true },
    { id: "control-zone-1", angle: 1.0471975511965976, distance: 27, visible: true, isSpawn: false },
    { id: "control-zone-2", angle: 2.0943951023931953, distance: 27, visible: true, isSpawn: false },
    { id: "control-zone-3", angle: 3.141592653589793, distance: 27, visible: true, isSpawn: true },
    { id: "control-zone-4", angle: 4.1887902047863905, distance: 27, visible: true, isSpawn: false },
    { id: "control-zone-5", angle: 5.235987755982989, distance: 27, visible: true, isSpawn: false }
  ]
};

export const MAP_DEFAULT_CONFIGS: Record<string, MapDefaultConfig> = {
  space: SPACE_MAP_DEFAULT_CONFIG
};

export function getMapDefaultConfig(mapId: string): MapDefaultConfig | null {
  return MAP_DEFAULT_CONFIGS[mapId] || null;
}

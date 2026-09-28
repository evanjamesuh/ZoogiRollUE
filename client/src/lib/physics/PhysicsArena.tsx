import { useEffect, useRef } from "react";
import { usePhysicsWorld } from "./usePhysicsWorld";
import { ARENA_RADIUS, WALL_THICKNESS } from "../arenaConstants";
import { getRapier, isRapierReady } from "./rapierInit";

interface PhysicsArenaProps {
  mapTheme: string;
  hasGaps?: boolean;
  skipWalls?: boolean;
  skipFloor?: boolean;
}

const WALL_HEIGHT = 2;
const FLOOR_Y = 0;
const WALL_SEGMENTS = 32;

const ICE_WALL_RADIUS = 12;
const ICE_WALL_SEGMENTS = 32;
const ICE_WALL_RESTITUTION = 0.97;
const ICE_WALL_FRICTION = 0.02;

const ICE_BUMPER_POSITIONS: [number, number, number][] = [];
const ICE_BUMPER_RADIUS = 0.6;
const ICE_BUMPER_HEIGHT = 1.5;
const ICE_BUMPER_RESTITUTION = 0.9;

export function usePhysicsArena({ mapTheme, hasGaps = true, skipWalls = false, skipFloor = false }: PhysicsArenaProps) {
  const { world, isInitialized, bodies } = usePhysicsWorld();
  const arenaCreated = useRef(false);
  
  useEffect(() => {
    if (!isInitialized || !world || arenaCreated.current) return;
    if (!isRapierReady()) return;
    
    const RAPIER = getRapier();
    
    if (!skipFloor) {
      const floorDesc = RAPIER.RigidBodyDesc.fixed()
        .setTranslation(0, FLOOR_Y - 0.1, 0);
      const floorBody = world.createRigidBody(floorDesc);
      
      const FLOOR_RADIUS = 50;
      const floorRestitution = mapTheme === "ice" ? 0.1 : 0.3;
      const floorFriction = mapTheme === "ice" ? 0.02 : 0.5;
      const floorCollider = RAPIER.ColliderDesc.cylinder(0.1, FLOOR_RADIUS)
        .setRestitution(floorRestitution)
        .setFriction(floorFriction);
      world.createCollider(floorCollider, floorBody);
    }
    
    if (mapTheme === "ice") {
      for (let i = 0; i < ICE_WALL_SEGMENTS; i++) {
        const angle = (i / ICE_WALL_SEGMENTS) * Math.PI * 2;
        const nextAngle = ((i + 1) / ICE_WALL_SEGMENTS) * Math.PI * 2;
        const midAngle = (angle + nextAngle) / 2;

        const segLen = 2 * ICE_WALL_RADIUS * Math.sin(Math.PI / ICE_WALL_SEGMENTS);
        const wallThick = 0.4;

        const cx = Math.cos(midAngle) * ICE_WALL_RADIUS;
        const cz = Math.sin(midAngle) * ICE_WALL_RADIUS;

        const rotAngle = -(midAngle + Math.PI / 2);
        const wallDesc = RAPIER.RigidBodyDesc.fixed()
          .setTranslation(cx, WALL_HEIGHT / 2, cz)
          .setRotation({ w: Math.cos(rotAngle / 2), x: 0, y: Math.sin(rotAngle / 2), z: 0 });
        const wallBody = world.createRigidBody(wallDesc);

        const wallCollider = RAPIER.ColliderDesc.cuboid(segLen / 2, WALL_HEIGHT / 2, wallThick / 2)
          .setRestitution(ICE_WALL_RESTITUTION)
          .setFriction(ICE_WALL_FRICTION);
        world.createCollider(wallCollider, wallBody);
      }
      console.log(`Ice arena: created ${ICE_WALL_SEGMENTS}-segment circular wall at radius ${ICE_WALL_RADIUS}`);

      ICE_BUMPER_POSITIONS.forEach((pos, idx) => {
        const bumperDesc = RAPIER.RigidBodyDesc.fixed()
          .setTranslation(pos[0], pos[1], pos[2]);
        const bumperBody = world.createRigidBody(bumperDesc);

        const bumperCollider = RAPIER.ColliderDesc.cylinder(ICE_BUMPER_HEIGHT / 2, ICE_BUMPER_RADIUS)
          .setRestitution(ICE_BUMPER_RESTITUTION)
          .setFriction(0.02);
        world.createCollider(bumperCollider, bumperBody);
      });
      console.log(`Ice arena: placed ${ICE_BUMPER_POSITIONS.length} pinball bumpers`);
    }

    arenaCreated.current = true;
    console.log(`Physics arena created for theme '${mapTheme}' ${skipFloor ? 'without floor' : 'with floor'}`);
    
  }, [isInitialized, world, hasGaps, skipWalls, skipFloor, mapTheme]);
  
  useEffect(() => {
    return () => {
      arenaCreated.current = false;
    };
  }, [mapTheme]);
}

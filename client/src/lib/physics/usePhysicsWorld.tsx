import { create } from "zustand";
import RAPIER from "@dimforge/rapier3d-compat";
import { initRapier, getRapier, isRapierReady } from "./rapierInit";
import { getPhysicsConfig } from "../physicsConfig";

function getPhysicsTimestepConfig() {
  const config = getPhysicsConfig();
  return {
    FIXED_TIMESTEP: config.fixedTimestep,
    MAX_SUBSTEPS: config.maxSubsteps,
    GRAVITY: config.gravity,
  };
}

export interface PhysicsBody {
  id: string;
  rigidBody: RAPIER.RigidBody;
  collider: RAPIER.Collider;
  type: "zoogi" | "orb" | "obstacle" | "wall" | "floor";
  userData?: Record<string, unknown>;
}

interface PhysicsWorldState {
  world: RAPIER.World | null;
  bodies: Map<string, PhysicsBody>;
  isInitialized: boolean;
  accumulator: number;
  
  initialize: () => Promise<void>;
  step: (deltaTime: number) => void;
  cleanup: () => void;
  
  createDynamicBall: (id: string, position: [number, number, number], radius: number, type: PhysicsBody["type"], options?: {
    restitution?: number;
    friction?: number;
    linearDamping?: number;
    angularDamping?: number;
    mass?: number;
    userData?: Record<string, unknown>;
  }) => PhysicsBody | null;
  
  createStaticCylinder: (id: string, position: [number, number, number], radius: number, height: number) => PhysicsBody | null;
  
  createStaticFloor: (id: string, position: [number, number, number], size: number) => PhysicsBody | null;
  
  removeBody: (id: string) => void;
  
  getBody: (id: string) => PhysicsBody | undefined;
  
  applyImpulse: (id: string, impulse: { x: number; y: number; z: number }) => void;
  
  setLinearVelocity: (id: string, velocity: { x: number; y: number; z: number }) => void;
  
  setAngularVelocity: (id: string, velocity: { x: number; y: number; z: number }) => void;
  
  getPosition: (id: string) => { x: number; y: number; z: number } | null;
  
  getLinearVelocity: (id: string) => { x: number; y: number; z: number } | null;
  
  getRotation: (id: string) => { w: number; x: number; y: number; z: number } | null;
  
  setPosition: (id: string, position: { x: number; y: number; z: number }) => void;
  
  resetBody: (id: string, position: { x: number; y: number; z: number }) => void;
  
  createDynamicBox: (id: string, position: [number, number, number], size: [number, number, number], rotation?: { w: number; x: number; y: number; z: number }, options?: {
    restitution?: number;
    friction?: number;
    linearDamping?: number;
    angularDamping?: number;
    mass?: number;
    userData?: Record<string, unknown>;
  }) => PhysicsBody | null;
  
  createKinematicBox: (id: string, position: [number, number, number], size: [number, number, number], rotation?: { w: number; x: number; y: number; z: number }, options?: {
    restitution?: number;
    friction?: number;
    userData?: Record<string, unknown>;
  }) => PhysicsBody | null;
  
  convertToDynamic: (id: string, options?: {
    mass?: number;
    linearDamping?: number;
    angularDamping?: number;
  }) => void;
  
  convertToKinematic: (id: string) => void;
  
  wakeUpBody: (id: string) => void;
  
  createTrimeshCollider: (id: string, vertices: Float32Array, indices: Uint32Array, position: [number, number, number], scale: [number, number, number]) => PhysicsBody | null;
}

export const usePhysicsWorld = create<PhysicsWorldState>((set, get) => ({
  world: null,
  bodies: new Map(),
  isInitialized: false,
  accumulator: 0,
  
  initialize: async () => {
    if (get().isInitialized) return;
    
    await initRapier();
    const RAPIER = getRapier();
    
    const { GRAVITY } = getPhysicsTimestepConfig();
    const gravity = { x: 0, y: GRAVITY, z: 0 };
    const world = new RAPIER.World(gravity);
    
    set({ world, isInitialized: true, bodies: new Map() });
    console.log("Physics world created with gravity:", gravity);
  },
  
  step: (deltaTime: number) => {
    const { world, accumulator } = get();
    if (!world) return;
    
    const { FIXED_TIMESTEP, MAX_SUBSTEPS } = getPhysicsTimestepConfig();
    let newAccumulator = accumulator + deltaTime;
    let steps = 0;
    
    while (newAccumulator >= FIXED_TIMESTEP && steps < MAX_SUBSTEPS) {
      world.step();
      newAccumulator -= FIXED_TIMESTEP;
      steps++;
    }
    
    if (newAccumulator >= FIXED_TIMESTEP) {
      newAccumulator = FIXED_TIMESTEP * 0.5;
    }
    
    set({ accumulator: newAccumulator });
  },
  
  cleanup: () => {
    const { world, bodies } = get();
    if (world) {
      bodies.forEach((body) => {
        world.removeCollider(body.collider, true);
        world.removeRigidBody(body.rigidBody);
      });
      world.free();
    }
    set({ world: null, bodies: new Map(), isInitialized: false, accumulator: 0 });
  },
  
  createDynamicBall: (id, position, radius, type, options = {}) => {
    const { world, bodies } = get();
    if (!world || !isRapierReady()) return null;
    
    const RAPIER = getRapier();
    
    const {
      restitution = 0.9,
      friction = 0.3,
      linearDamping = 0.5,
      angularDamping = 0.6,
      mass = 1,
      userData = {}
    } = options;
    
    const rigidBodyDesc = RAPIER.RigidBodyDesc.dynamic()
      .setTranslation(position[0], position[1], position[2])
      .setLinearDamping(linearDamping)
      .setAngularDamping(angularDamping)
      .setCcdEnabled(true); // Enable CCD to prevent tunneling through walls at high velocity
    
    const rigidBody = world.createRigidBody(rigidBodyDesc);
    
    const colliderDesc = RAPIER.ColliderDesc.ball(radius)
      .setRestitution(restitution)
      .setFriction(friction)
      .setMass(mass);
    
    const collider = world.createCollider(colliderDesc, rigidBody);
    
    const physicsBody: PhysicsBody = {
      id,
      rigidBody,
      collider,
      type,
      userData
    };
    
    const newBodies = new Map(bodies);
    newBodies.set(id, physicsBody);
    set({ bodies: newBodies });
    
    return physicsBody;
  },
  
  createStaticCylinder: (id, position, radius, height) => {
    const { world, bodies } = get();
    if (!world || !isRapierReady()) return null;
    
    const RAPIER = getRapier();
    
    const rigidBodyDesc = RAPIER.RigidBodyDesc.fixed()
      .setTranslation(position[0], position[1], position[2]);
    
    const rigidBody = world.createRigidBody(rigidBodyDesc);
    
    const colliderDesc = RAPIER.ColliderDesc.cylinder(height / 2, radius)
      .setRestitution(0.5)
      .setFriction(0.3);
    
    const collider = world.createCollider(colliderDesc, rigidBody);
    
    const physicsBody: PhysicsBody = {
      id,
      rigidBody,
      collider,
      type: "wall"
    };
    
    const newBodies = new Map(bodies);
    newBodies.set(id, physicsBody);
    set({ bodies: newBodies });
    
    return physicsBody;
  },
  
  createStaticFloor: (id, position, size) => {
    const { world, bodies } = get();
    if (!world || !isRapierReady()) return null;
    
    const RAPIER = getRapier();
    
    const rigidBodyDesc = RAPIER.RigidBodyDesc.fixed()
      .setTranslation(position[0], position[1], position[2]);
    
    const rigidBody = world.createRigidBody(rigidBodyDesc);
    
    const colliderDesc = RAPIER.ColliderDesc.cuboid(size, 0.1, size)
      .setRestitution(0.3)
      .setFriction(0.5);
    
    const collider = world.createCollider(colliderDesc, rigidBody);
    
    const physicsBody: PhysicsBody = {
      id,
      rigidBody,
      collider,
      type: "floor"
    };
    
    const newBodies = new Map(bodies);
    newBodies.set(id, physicsBody);
    set({ bodies: newBodies });
    
    return physicsBody;
  },
  
  removeBody: (id: string) => {
    const { world, bodies } = get();
    const body = bodies.get(id);
    if (!world || !body) return;
    
    world.removeCollider(body.collider, true);
    world.removeRigidBody(body.rigidBody);
    
    const newBodies = new Map(bodies);
    newBodies.delete(id);
    set({ bodies: newBodies });
  },
  
  getBody: (id: string) => {
    return get().bodies.get(id);
  },
  
  applyImpulse: (id: string, impulse: { x: number; y: number; z: number }) => {
    const body = get().bodies.get(id);
    if (!body) return;
    body.rigidBody.applyImpulse(impulse, true);
  },
  
  setLinearVelocity: (id: string, velocity: { x: number; y: number; z: number }) => {
    const body = get().bodies.get(id);
    if (!body) return;
    body.rigidBody.setLinvel(velocity, true);
  },
  
  setAngularVelocity: (id: string, velocity: { x: number; y: number; z: number }) => {
    const body = get().bodies.get(id);
    if (!body) return;
    body.rigidBody.setAngvel(velocity, true);
  },
  
  getPosition: (id: string) => {
    const body = get().bodies.get(id);
    if (!body) return null;
    const pos = body.rigidBody.translation();
    return { x: pos.x, y: pos.y, z: pos.z };
  },
  
  getLinearVelocity: (id: string) => {
    const body = get().bodies.get(id);
    if (!body) return null;
    const vel = body.rigidBody.linvel();
    return { x: vel.x, y: vel.y, z: vel.z };
  },
  
  getRotation: (id: string) => {
    const body = get().bodies.get(id);
    if (!body) return null;
    const rot = body.rigidBody.rotation();
    return { w: rot.w, x: rot.x, y: rot.y, z: rot.z };
  },
  
  setPosition: (id: string, position: { x: number; y: number; z: number }) => {
    const body = get().bodies.get(id);
    if (!body) return;
    body.rigidBody.setTranslation(position, true);
  },
  
  resetBody: (id: string, position: { x: number; y: number; z: number }) => {
    const body = get().bodies.get(id);
    if (!body) return;
    body.rigidBody.setTranslation(position, true);
    body.rigidBody.setLinvel({ x: 0, y: 0, z: 0 }, true);
    body.rigidBody.setAngvel({ x: 0, y: 0, z: 0 }, true);
  },
  
  createDynamicBox: (id, position, size, rotation, options = {}) => {
    const { world, bodies } = get();
    if (!world || !isRapierReady()) return null;
    
    const RAPIER = getRapier();
    
    const {
      restitution = 0.3,
      friction = 0.5,
      linearDamping = 0.4,
      angularDamping = 0.4,
      mass = 1,
      userData = {}
    } = options;
    
    let rigidBodyDesc = RAPIER.RigidBodyDesc.dynamic()
      .setTranslation(position[0], position[1], position[2])
      .setLinearDamping(linearDamping)
      .setAngularDamping(angularDamping)
      .setCcdEnabled(true);
    
    if (rotation) {
      rigidBodyDesc = rigidBodyDesc.setRotation(rotation);
    }
    
    const rigidBody = world.createRigidBody(rigidBodyDesc);
    
    const colliderDesc = RAPIER.ColliderDesc.cuboid(size[0] / 2, size[1] / 2, size[2] / 2)
      .setRestitution(restitution)
      .setFriction(friction)
      .setMass(mass);
    
    const collider = world.createCollider(colliderDesc, rigidBody);
    
    const physicsBody: PhysicsBody = {
      id,
      rigidBody,
      collider,
      type: "wall",
      userData
    };
    
    const newBodies = new Map(bodies);
    newBodies.set(id, physicsBody);
    set({ bodies: newBodies });
    
    return physicsBody;
  },
  
  wakeUpBody: (id: string) => {
    const body = get().bodies.get(id);
    if (!body) return;
    body.rigidBody.wakeUp();
  },
  
  createKinematicBox: (id, position, size, rotation, options = {}) => {
    const { world, bodies } = get();
    if (!world || !isRapierReady()) return null;
    
    const RAPIER = getRapier();
    
    const {
      restitution = 0.8,
      friction = 0.3,
      userData = {}
    } = options;
    
    let rigidBodyDesc = RAPIER.RigidBodyDesc.fixed()
      .setTranslation(position[0], position[1], position[2]);
    
    if (rotation) {
      rigidBodyDesc = rigidBodyDesc.setRotation(rotation);
    }
    
    const rigidBody = world.createRigidBody(rigidBodyDesc);
    
    const colliderDesc = RAPIER.ColliderDesc.cuboid(size[0] / 2, size[1] / 2, size[2] / 2)
      .setRestitution(restitution)
      .setFriction(friction);
    
    const collider = world.createCollider(colliderDesc, rigidBody);
    
    const physicsBody: PhysicsBody = {
      id,
      rigidBody,
      collider,
      type: "wall",
      userData
    };
    
    const newBodies = new Map(bodies);
    newBodies.set(id, physicsBody);
    set({ bodies: newBodies });
    
    return physicsBody;
  },
  
  convertToDynamic: (id, options = {}) => {
    const { world, bodies } = get();
    if (!world || !isRapierReady()) return;
    
    const RAPIER = getRapier();
    const body = bodies.get(id);
    if (!body) return;
    
    const {
      mass = 0.8,
      linearDamping = 0.9,
      angularDamping = 0.9
    } = options;
    
    body.rigidBody.setBodyType(RAPIER.RigidBodyType.Dynamic, true);
    body.rigidBody.setLinearDamping(linearDamping);
    body.rigidBody.setAngularDamping(angularDamping);
    body.collider.setMass(mass);
    body.rigidBody.wakeUp();
  },
  
  convertToKinematic: (id) => {
    const { world, bodies } = get();
    if (!world || !isRapierReady()) return;
    
    const RAPIER = getRapier();
    const body = bodies.get(id);
    if (!body) return;
    
    body.rigidBody.setLinvel({ x: 0, y: 0, z: 0 }, true);
    body.rigidBody.setAngvel({ x: 0, y: 0, z: 0 }, true);
    body.rigidBody.setBodyType(RAPIER.RigidBodyType.KinematicPositionBased, true);
  },
  
  createTrimeshCollider: (id, vertices, indices, position, scale) => {
    const { world, bodies } = get();
    if (!world || !isRapierReady()) return null;
    
    const RAPIER = getRapier();
    
    // Apply scale to vertices
    const scaledVertices = new Float32Array(vertices.length);
    for (let i = 0; i < vertices.length; i += 3) {
      scaledVertices[i] = vertices[i] * scale[0];
      scaledVertices[i + 1] = vertices[i + 1] * scale[1];
      scaledVertices[i + 2] = vertices[i + 2] * scale[2];
    }
    
    const rigidBodyDesc = RAPIER.RigidBodyDesc.fixed()
      .setTranslation(position[0], position[1], position[2]);
    
    const rigidBody = world.createRigidBody(rigidBodyDesc);
    
    const colliderDesc = RAPIER.ColliderDesc.trimesh(scaledVertices, indices)
      .setRestitution(0.3)
      .setFriction(0.5);
    
    const collider = world.createCollider(colliderDesc, rigidBody);
    
    const physicsBody: PhysicsBody = {
      id,
      rigidBody,
      collider,
      type: "floor"
    };
    
    const newBodies = new Map(bodies);
    newBodies.set(id, physicsBody);
    set({ bodies: newBodies });
    
    console.log(`Trimesh collider created: ${id} with ${vertices.length / 3} vertices`);
    
    return physicsBody;
  }
}));

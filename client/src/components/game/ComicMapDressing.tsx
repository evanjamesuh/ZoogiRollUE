import * as THREE from "three";
import { Component, ReactNode, Suspense, useEffect, useLayoutEffect, useMemo, useRef } from "react";
import { useFrame, useThree } from "@react-three/fiber";
import { useGLTF } from "@react-three/drei";
import { getMapLayout } from "@/lib/arenaColliders";
import { useZoogiGame } from "@/lib/stores/useZoogiGame";

const GLOW_URL = "/textures/comic/glow_orange.png";

class ModelErrorBoundary extends Component<{ children: ReactNode; fallback?: ReactNode }, { hasError: boolean }> {
  constructor(props: { children: ReactNode; fallback?: ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }
  static getDerivedStateFromError() {
    return { hasError: true };
  }
  render() {
    return this.state.hasError ? (this.props.fallback ?? null) : this.props.children;
  }
}

function seeded(seed: number) {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) | 0;
    let t = Math.imul(a ^ (a >>> 15), 1 | a);
    t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

const skyVertex = `
  varying vec3 vWorld;
  void main() {
    vec4 world = modelMatrix * vec4(position, 1.0);
    vWorld = world.xyz;
    gl_Position = projectionMatrix * viewMatrix * world;
  }
`;

const skyFragment = `
  uniform vec3 topColor;
  uniform vec3 horizonColor;
  uniform vec3 groundColor;
  varying vec3 vWorld;
  void main() {
    float h = normalize(vWorld).y;
    vec3 col = mix(horizonColor, topColor, smoothstep(0.02, 0.7, h));
    col = mix(groundColor, col, smoothstep(-0.2, 0.12, h));
    gl_FragColor = vec4(col, 1.0);
  }
`;

function SkyDome({
  top,
  horizon,
  ground,
  stars = 0,
  starSeed = 1,
}: {
  top: string;
  horizon: string;
  ground: string;
  stars?: number;
  starSeed?: number;
}) {
  const meshRef = useRef<THREE.Mesh>(null);
  const { camera } = useThree();
  const uniforms = useMemo(
    () => ({
      topColor: { value: new THREE.Color(top) },
      horizonColor: { value: new THREE.Color(horizon) },
      groundColor: { value: new THREE.Color(ground) },
    }),
    [top, horizon, ground],
  );
  const starGeometry = useMemo(() => {
    const rand = seeded(starSeed);
    const positions = new Float32Array(stars * 3);
    for (let i = 0; i < stars; i++) {
      const theta = rand() * Math.PI * 2;
      const phi = Math.acos(1 - rand() * 1.15);
      const radius = 90 + rand() * 40;
      positions[i * 3] = radius * Math.sin(phi) * Math.cos(theta);
      positions[i * 3 + 1] = Math.abs(radius * Math.cos(phi)) * 0.85 + 8;
      positions[i * 3 + 2] = radius * Math.sin(phi) * Math.sin(theta);
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
    return geometry;
  }, [stars, starSeed]);

  useFrame(() => {
    if (meshRef.current) meshRef.current.position.copy(camera.position);
  });

  return (
    <group>
      <mesh ref={meshRef} scale={[420, 420, 420]}>
        <sphereGeometry args={[1, 32, 24]} />
        <shaderMaterial args={[{ uniforms, vertexShader: skyVertex, fragmentShader: skyFragment }]} side={THREE.BackSide} depthWrite={false} />
      </mesh>
      {stars > 0 && (
        <points geometry={starGeometry}>
          <pointsMaterial color="#fff6e8" size={0.45} sizeAttenuation transparent opacity={0.9} depthWrite={false} />
        </points>
      )}
    </group>
  );
}

function EmberField() {
  const pointsRef = useRef<THREE.Points>(null);
  useEffect(() => {
    const loader = new THREE.TextureLoader();
    loader.load(GLOW_URL, (tex) => {
      tex.colorSpace = THREE.SRGBColorSpace;
      const mat = pointsRef.current?.material;
      if (mat && !Array.isArray(mat) && mat instanceof THREE.PointsMaterial) {
        mat.map = tex;
        mat.needsUpdate = true;
      }
    });
  }, []);

  const data = useMemo(() => {
    const rand = seeded(11);
    const count = 60;
    const positions = new Float32Array(count * 3);
    const speeds = new Float32Array(count);
    for (let i = 0; i < count; i++) {
      const angle = rand() * Math.PI * 2;
      const radius = 4 + rand() * 32;
      positions[i * 3] = Math.cos(angle) * radius;
      positions[i * 3 + 1] = rand() * 12;
      positions[i * 3 + 2] = Math.sin(angle) * radius;
      speeds[i] = 0.65 + rand() * 1.35;
    }
    return { positions, speeds, count };
  }, []);

  useFrame((_, delta) => {
    const attr = pointsRef.current?.geometry.attributes.position;
    if (!attr) return;
    const arr = attr.array as Float32Array;
    for (let i = 0; i < data.count; i++) {
      arr[i * 3 + 1] += data.speeds[i] * delta;
      if (arr[i * 3 + 1] > 15) arr[i * 3 + 1] = 0.2;
    }
    attr.needsUpdate = true;
  });

  return (
    <points ref={pointsRef}>
      <bufferGeometry>
        <bufferAttribute attach="attributes-position" count={data.count} array={data.positions} itemSize={3} />
      </bufferGeometry>
      <pointsMaterial
        color="#ffb15a"
        size={0.62}
        transparent
        opacity={0.88}
        depthWrite={false}
        blending={THREE.AdditiveBlending}
        sizeAttenuation
      />
    </points>
  );
}

/**
 * Dark crust covers this share of the knockout radius, on the floor.
 * The hot rim and the deep pool are the same kind of fraction, so a larger
 * arena keeps the same three bands.
 */
const LIP_WIDTH_RATIO = 0.12;
const MELT_WIDTH_RATIO = 0.06;
const MOAT_OUTER_RATIO = 1.32;
/** Low curb, in world units. Marbles are not scaled with the arena. */
const LIP_CURB_HEIGHT = 0.16;

/**
 * Same number the fall check uses. A lava match copies
 * getMapLayout("lava").knockoffRadius into wallSettings.knockoffBoundaryRadius.
 */
function useLavaKnockoffRadius(): number {
  const live = useZoogiGame((state) => state.wallSettings.knockoffBoundaryRadius);
  if (live != null && Number.isFinite(live) && live > 0) return live;
  return getMapLayout("lava")?.knockoffRadius ?? 0;
}

function LavaMoat({ edge }: { edge: number }) {
  const meltRef = useRef<THREE.MeshBasicMaterial>(null);
  const meltOuter = edge * (1 + MELT_WIDTH_RATIO);
  const moatOuter = edge * MOAT_OUTER_RATIO;
  useFrame((state) => {
    const mat = meltRef.current;
    if (!mat) return;
    const wave = Math.sin(state.clock.elapsedTime * 1.7);
    // Linear HDR yellow. Bloom already thresholds at 1.15, so the rim glows
    // and no longer sits in the same orange as the floor.
    mat.color.r = 2.5 + wave * 0.2;
    mat.color.g = 1.05 + wave * 0.06;
    mat.color.b = 0.08;
  });
  if (edge <= 0) return null;
  return (
    <group position={[0, 0.07, 0]}>
      <mesh rotation={[-Math.PI / 2, 0, 0]} renderOrder={2}>
        <ringGeometry args={[edge, meltOuter, 80]} />
        <meshBasicMaterial ref={meltRef} color="#ffcc33" toneMapped={false} fog={false} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.012, 0]} renderOrder={1}>
        <ringGeometry args={[meltOuter, moatOuter, 80]} />
        <meshBasicMaterial color="#d01208" toneMapped={false} fog={false} />
      </mesh>
    </group>
  );
}

/** Dark crust on the floor, ending exactly on the knockout line. */
function BasaltLip({ edge }: { edge: number }) {
  if (edge <= 0) return null;
  const lipInner = edge * (1 - LIP_WIDTH_RATIO);
  return (
    <group>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.065, 0]} renderOrder={2}>
        <ringGeometry args={[lipInner, edge, 96]} />
        <meshBasicMaterial color="#100e0c" fog={false} />
      </mesh>
      <mesh position={[0, LIP_CURB_HEIGHT / 2, 0]} renderOrder={3}>
        <cylinderGeometry args={[edge, edge, LIP_CURB_HEIGHT, 96, 1, true]} />
        <meshBasicMaterial color="#161311" side={THREE.DoubleSide} fog={false} />
      </mesh>
    </group>
  );
}

function RockSpires() {
  const spires = useMemo(() => {
    const rand = seeded(19);
    return Array.from({ length: 10 }, (_, i) => {
      const angle = (i / 10) * Math.PI * 2 + 0.2;
      const distance = 30 + rand() * 12;
      const height = 7 + rand() * 9;
      const radius = 1.4 + rand() * 1.8;
      return {
        id: `spire-${i}`,
        position: [Math.cos(angle) * distance, height / 2, Math.sin(angle) * distance] as [number, number, number],
        height,
        radius,
        tilt: (rand() - 0.5) * 0.35,
      };
    });
  }, []);

  return (
    <group>
      {spires.map((spire) => (
        <mesh key={spire.id} position={spire.position} rotation={[spire.tilt, spire.position[0], 0]} castShadow>
          <coneGeometry args={[spire.radius, spire.height, 6]} />
          <meshStandardMaterial color="#1a1830" roughness={0.94} metalness={0.04} emissive="#3a1408" emissiveIntensity={0.08} />
        </mesh>
      ))}
    </group>
  );
}

function VolcanicPitModel({ edge }: { edge: number }) {
  const { scene } = useGLTF("/models/volcanic_pit.glb");
  const cloned = useMemo(() => {
    const copy = scene.clone(true);
    copy.traverse((obj) => {
      if (!(obj instanceof THREE.Mesh)) return;
      obj.castShadow = false;
      obj.receiveShadow = false;
      const mats = Array.isArray(obj.material) ? obj.material : [obj.material];
      mats.forEach((mat) => {
        if (!(mat instanceof THREE.MeshStandardMaterial)) return;
        mat.emissive = new THREE.Color("#ff6418");
        mat.emissiveMap = mat.map;
        mat.emissiveIntensity = 0.75;
      });
    });
    return copy;
  }, [scene]);

  useLayoutEffect(() => {
    cloned.scale.setScalar(1);
    cloned.position.set(0, 0, 0);
    cloned.rotation.set(0, 0.4, 0);
    cloned.updateMatrixWorld(true);
    const box = new THREE.Box3().setFromObject(cloned);
    const size = new THREE.Vector3();
    box.getSize(size);
    const span = Math.max(size.x, size.z, 1);
    const scale = 42 / span;
    cloned.scale.setScalar(scale);
    cloned.updateMatrixWorld(true);
    const fitted = new THREE.Box3().setFromObject(cloned);
    const center = new THREE.Vector3();
    fitted.getCenter(center);
    // Main places the nearest face one fixed gap past the layout knockoff.
    // That radius is the same number the fall check copies into the store.
    const knockoff = getMapLayout("lava")?.knockoffRadius ?? edge;
    const nearZ = -(knockoff + 4);
    cloned.position.set(-center.x, -fitted.min.y, nearZ - fitted.max.z);
  }, [cloned, edge]);

  return <primitive object={cloned} />;
}

/** Lava glow, embers, dusk rocks, and an optional distant pit model. */
export function VolcanicPitDressing() {
  const edge = useLavaKnockoffRadius();
  return (
    <group>
      <LavaMoat edge={edge} />
      <BasaltLip edge={edge} />
      <RockSpires />
      <EmberField />
      <ModelErrorBoundary fallback={null}>
        <Suspense fallback={null}>
          <VolcanicPitModel edge={edge} />
        </Suspense>
      </ModelErrorBoundary>
    </group>
  );
}

const LAMP_ANGLES = [0.4, 1.3, 2.2, 3.1, 4.2, 5.3];

const ARABIAN_SKYBOX = /^(FrontSide_5|FrontSide_18|FrontSide_20|BackSide_2)$/;

function warmWindowMaterials(scene: THREE.Object3D) {
  scene.traverse((obj) => {
    if (ARABIAN_SKYBOX.test(obj.name)) obj.visible = false;
    if (!(obj instanceof THREE.Mesh)) return;
    const mats = Array.isArray(obj.material) ? obj.material : [obj.material];
    mats.forEach((mat) => {
      if (!(mat instanceof THREE.MeshStandardMaterial)) return;
      const name = `${obj.name} ${mat.name}`;
      const color = mat.color;
      const alreadyLit = mat.emissiveIntensity > 0.05 && mat.emissive.getHex() !== 0;
      const named = /window|lamp|light|glow|lantern|emissive/i.test(name);
      const warmColor = color.r > 0.75 && color.g > 0.45 && color.b < 0.35;
      if (!alreadyLit && !named && !warmColor) return;
      const glow = named || warmColor ? new THREE.Color("#ffb24a") : mat.emissive.clone();
      mat.emissive = glow;
      mat.emissiveIntensity = Math.max(mat.emissiveIntensity, named ? 1.6 : 0.7);
    });
  });
}

function ArabianLamps() {
  return (
    <group>
      {LAMP_ANGLES.map((angle, i) => {
        const distance = 17.4;
        const x = Math.cos(angle) * distance;
        const z = Math.sin(angle) * distance;
        return (
          <group key={i} position={[x, 0, z]}>
            <mesh position={[0, 1.6, 0]} castShadow>
              <cylinderGeometry args={[0.08, 0.12, 3.2, 8]} />
              <meshStandardMaterial color="#3a2a22" roughness={0.8} />
            </mesh>
            <mesh position={[0, 3.35, 0]}>
              <sphereGeometry args={[0.28, 12, 10]} />
              <meshStandardMaterial color="#ffd27a" emissive="#ffb03a" emissiveIntensity={2.4} />
            </mesh>
            <pointLight position={[0, 3.5, 0]} color="#ffb15a" intensity={7} distance={16} decay={2} />
          </group>
        );
      })}
    </group>
  );
}

function ArabianGlow() {
  const { scene } = useGLTF("/models/arabian_nights_stage.glb");
  useLayoutEffect(() => {
    warmWindowMaterials(scene);
  }, [scene]);
  return null;
}

/** Night palace: warm lamps outside the plaza, and a glow boost on window materials. */
export function ArabianNightDressing() {
  return (
    <group>
      <ArabianLamps />
      <ModelErrorBoundary fallback={null}>
        <Suspense fallback={null}>
          <ArabianGlow />
        </Suspense>
      </ModelErrorBoundary>
    </group>
  );
}

// GLTFLoader rewrites spaces to underscores, so both forms have to match.
const COSMIC_BACKDROP = /Sphere_Sky|City[_ ]Floor|City[_ ]Facade|City[_ ]Towers|Entrance[_ ]Facade|Buildings|Glow[_ ]Blue|Windows|polygon3_Glow[_ ]Cyan|polygon1_Glow[_ ]White/i;

function CosmicAccent() {
  const { scene } = useGLTF("/models/cosmos_arena.glb");
  useLayoutEffect(() => {
    scene.traverse((obj) => {
      if (COSMIC_BACKDROP.test(obj.name)) {
        obj.visible = false;
        return;
      }
      if (!(obj instanceof THREE.Mesh)) return;
      const mats = Array.isArray(obj.material) ? obj.material : [obj.material];
      mats.forEach((mat) => {
        if (!(mat instanceof THREE.MeshStandardMaterial)) return;
        const name = `${obj.name} ${mat.name}`;
        if (!/cyan|glow/i.test(name)) return;
        mat.emissive = new THREE.Color("#3dfff2");
        mat.emissiveIntensity = Math.max(mat.emissiveIntensity, 2.1);
      });
    });
  }, [scene]);
  return (
    <group>
      <pointLight position={[18, 5, -8]} color="#3dfff2" intensity={5} distance={22} decay={2} />
      <pointLight position={[-16, 6, 10]} color="#7a4dff" intensity={4} distance={24} decay={2} />
    </group>
  );
}

/** Purple void accents. Floor geometry stays on the cosmic model. */
export function CosmicVoidDressing() {
  return (
    <ModelErrorBoundary fallback={null}>
      <Suspense fallback={null}>
        <CosmicAccent />
      </Suspense>
    </ModelErrorBoundary>
  );
}

/** Sky and fog for the three restyled maps plus the tomb. Meadow and the frozen ring do not use this. */
export function MapAtmosphere({ map }: { map: string | null }) {
  if (map === "lava") {
    return (
      <>
        <SkyDome top="#2a1468" horizon="#ff7a2e" ground="#1a0a16" stars={160} starSeed={3} />
        <fog attach="fog" args={["#2a1230", 52, 150]} />
      </>
    );
  }
  if (map === "saturn") {
    return (
      <>
        <SkyDome top="#070314" horizon="#241044" ground="#070314" stars={380} starSeed={5} />
        <fog attach="fog" args={["#100818", 58, 175]} />
      </>
    );
  }
  if (map === "space") {
    return (
      <>
        <SkyDome top="#14082e" horizon="#1a0840" ground="#070414" stars={0} />
        <fog attach="fog" args={["#0c0618", 80, 220]} />
      </>
    );
  }
  if (map === "tomb") {
    return (
      <>
        <SkyDome top="#140a28" horizon="#5c3c78" ground="#1a1028" stars={320} starSeed={9} />
        <fog attach="fog" args={["#1a1030", 46, 135]} />
      </>
    );
  }
  return null;
}

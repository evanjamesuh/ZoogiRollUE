import * as THREE from "three";
import { useEffect, useLayoutEffect, useMemo, useRef } from "react";
import { useFrame } from "@react-three/fiber";
import { TOMB_STAGE } from "@/lib/arenaColliders";

/**
 * Night desert beyond Pharaoh's Tomb.
 *
 * The tomb builder owns the court, the drop, the courtyard and the hieroglyph
 * wall out to about 1.55R (R = TOMB_STAGE.knockoffRadius). Every mesh here
 * starts outside that line, and the lip of the sand stays below the marble
 * fall (centres land at y = 0) so nothing sits on the court or in the drop.
 */

const R = TOMB_STAGE.knockoffRadius;
/** Wall outer face. Do not place geometry inside this. */
const CLEAR = R * 1.55;
const SAND_INNER = CLEAR + 0.55;
const SAND_OUTER = R * 6.4;

export interface TombHorizonPiece {
  id: string;
  /** Degrees, from +X toward +Z, matching the tomb layout. */
  angleDeg: number;
  distance: number;
  /** Horizontal reach back toward the origin. */
  reach: number;
  height: number;
  /** World Y of the piece's base. Negative bases stay under the fall line. */
  baseY: number;
}

function place(piece: TombHorizonPiece): [number, number, number] {
  const angle = (piece.angleDeg * Math.PI) / 180;
  return [
    Math.cos(angle) * piece.distance,
    piece.baseY + piece.height / 2,
    Math.sin(angle) * piece.distance,
  ];
}

/** Pyramids, obelisks and far torches. Distances are multiples of R so a court-radius change keeps them outside the wall. */
export function tombHorizonPieces(): TombHorizonPiece[] {
  const d = (mul: number) => R * mul;
  return [
    { id: "pyramid-west", angleDeg: 214, distance: d(2.15), reach: d(0.26), height: 7.2, baseY: -2.2 },
    { id: "pyramid-gap", angleDeg: 272, distance: d(2.28), reach: d(0.36), height: 16, baseY: -11 },
    { id: "pyramid-far", angleDeg: 286, distance: d(2.5), reach: d(0.32), height: 14, baseY: -9.5 },
    { id: "pyramid-east", angleDeg: 328, distance: d(2.12), reach: d(0.24), height: 6.6, baseY: -1.6 },
    { id: "obelisk-w", angleDeg: 206, distance: d(2.02), reach: 0.9, height: 8.4, baseY: -1.6 },
    { id: "obelisk-gap", angleDeg: 264, distance: d(2.42), reach: 0.85, height: 12, baseY: -7.2 },
    { id: "obelisk-far", angleDeg: 296, distance: d(2.62), reach: 0.85, height: 11, baseY: -6.4 },
    { id: "obelisk-e", angleDeg: 336, distance: d(2.08), reach: 0.9, height: 8.2, baseY: -1.5 },
  ];
}

export function tombTorchPositions(): Array<[number, number, number]> {
  const spots = [
    [200, 2.12, 2.8],
    [214, 2.28, 3.6],
    [230, 2.4, 2.2],
    [262, 2.36, -3.8],
    [278, 2.48, -5.2],
    [328, 2.22, 3.3],
    [344, 2.4, 2.1],
    [186, 2.32, 1.6],
  ] as const;
  return spots.map(([deg, mul, y]) => {
    const angle = (deg * Math.PI) / 180;
    const distance = R * mul;
    return [Math.cos(angle) * distance, y, Math.sin(angle) * distance];
  });
}

/** Smallest distance from the origin to any horizon prop. Sand lip is SAND_INNER. */
export function tombBackdropMinInner(): number {
  let min = SAND_INNER;
  for (const piece of tombHorizonPieces()) {
    min = Math.min(min, piece.distance - piece.reach);
  }
  for (const [x, , z] of tombTorchPositions()) {
    min = Math.min(min, Math.hypot(x, z) - 0.4);
  }
  return min;
}

const skyVertex = `
  varying vec3 vDir;
  void main() {
    vec4 world = modelMatrix * vec4(position, 1.0);
    vDir = world.xyz - cameraPosition;
    gl_Position = projectionMatrix * viewMatrix * world;
  }
`;

const skyFragment = `
  uniform vec3 zenith;
  uniform vec3 night;
  uniform vec3 horizon;
  uniform vec3 dust;
  uniform vec3 ground;
  uniform vec3 moonColor;
  uniform vec3 moonDir;
  varying vec3 vDir;

  float hash(vec3 p) {
    return fract(sin(dot(p, vec3(127.1, 311.7, 74.7))) * 43758.5453);
  }

  void main() {
    vec3 dir = normalize(vDir);
    float h = dir.y;

    // The match camera looks steeply down, so the whole backdrop band sits near h = -0.55.
    // Keep that band a deep indigo and put the warmth in the moon halo, not a flat wash.
    vec3 col = mix(ground, night, smoothstep(-0.98, -0.72, h));
    col = mix(col, dust, smoothstep(-0.78, -0.5, h) * (1.0 - smoothstep(-0.5, -0.28, h)));
    col = mix(col, zenith, smoothstep(-0.15, 0.7, h));

    float moon = dot(dir, normalize(moonDir));
    float disc = smoothstep(0.9968, 0.9988, moon);
    float halo = pow(max(moon, 0.0), 10.0);
    col = mix(col, horizon, halo * 0.72);
    col += moonColor * (disc * 1.8 + halo * 0.28);

    vec3 cell = floor(dir * 170.0);
    float stars = hash(cell);
    float spark = step(0.955, stars) * smoothstep(-0.98, -0.2, h);
    col += vec3(1.0, 0.97, 0.9) * spark * (0.55 + hash(cell + 2.0) * 1.25);

    gl_FragColor = vec4(col, 1.0);
  }
`;

function NightSky() {
  const groupRef = useRef<THREE.Group>(null);
  const uniforms = useMemo(
    () => ({
      zenith: { value: new THREE.Color("#07091c") },
      night: { value: new THREE.Color("#1a1440") },
      horizon: { value: new THREE.Color("#d4895a") },
      dust: { value: new THREE.Color("#1c1638") },
      ground: { value: new THREE.Color("#120e12") },
      moonColor: { value: new THREE.Color("#fff1cc") },
      // Sits in the band the steep gameplay camera actually sees, just left of the far gap.
      moonDir: { value: new THREE.Vector3(0.012, -0.54, -0.842).normalize() },
    }),
    [],
  );

  const stars = useMemo(() => {
    const count = 420;
    const positions = new Float32Array(count * 3);
    let s = 9;
    const rand = () => {
      s = (s + 0x6d2b79f5) | 0;
      let t = Math.imul(s ^ (s >>> 15), 1 | s);
      t = (t + Math.imul(t ^ (t >>> 7), 61 | t)) ^ t;
      return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
    };
    for (let i = 0; i < count; i++) {
      const az = rand() * Math.PI * 2;
      // Bias into the downward band the match camera frames, plus a true upper sky.
      const upper = rand() > 0.62;
      const el = upper ? rand() * 1.15 : -0.25 - rand() * 0.7;
      const radius = 150 + rand() * 40;
      const cy = Math.sin(el);
      const cx = Math.cos(el);
      positions[i * 3] = Math.cos(az) * cx * radius;
      positions[i * 3 + 1] = cy * radius;
      positions[i * 3 + 2] = Math.sin(az) * cx * radius;
    }
    const geometry = new THREE.BufferGeometry();
    geometry.setAttribute("position", new THREE.BufferAttribute(positions, 3));
    return geometry;
  }, []);

  useEffect(() => () => stars.dispose(), [stars]);

  useFrame(({ camera }) => {
    if (groupRef.current) groupRef.current.position.copy(camera.position);
  });

  return (
    <group ref={groupRef}>
      <mesh scale={420} renderOrder={-20} frustumCulled={false}>
        <sphereGeometry args={[1, 32, 24]} />
        <shaderMaterial
          args={[{ uniforms, vertexShader: skyVertex, fragmentShader: skyFragment }]}
          side={THREE.BackSide}
          depthWrite={false}
          toneMapped={false}
        />
      </mesh>
      <points geometry={stars} renderOrder={-10} frustumCulled={false}>
        <pointsMaterial
          color="#fff8ea"
          size={2.15}
          sizeAttenuation
          transparent
          opacity={0.92}
          depthWrite={false}
          toneMapped={false}
          fog={false}
        />
      </points>
    </group>
  );
}

function duneHeight(angle: number, radius: number): number {
  const lobe = (centerDeg: number, width: number) => {
    const center = (centerDeg * Math.PI) / 180;
    const d = Math.atan2(Math.sin(angle - center), Math.cos(angle - center));
    return Math.exp(-(d * d) / (width * width));
  };
  // Stay under the fall line until well past the wall, then rise into the skyline.
  const t = THREE.MathUtils.smoothstep(radius, SAND_INNER + R * 0.42, SAND_INNER + R * 0.95);
  const waves =
    Math.pow(Math.sin(angle * 3.0 + 0.6), 2) * 2.1 +
    Math.pow(Math.sin(angle * 5.0 - 1.1), 2) * 1.3 +
    Math.sin(radius * 0.31 + angle * 2.0) * 0.4;
  const flanks = lobe(214, 0.32) * 5.8 + lobe(198, 0.2) * 2.4 + lobe(328, 0.3) * 5.4 + lobe(348, 0.18) * 2.2;
  return -1.75 + (waves * 0.8 + flanks) * t;
}

/** Flanking dune arcs. The far gap (about 246° to 308°) stays open so the moon and stars read from the match camera. */
function duneFieldGeometry(): THREE.BufferGeometry {
  const sectors: Array<[number, number]> = [
    [150, 246],
    [308, 400],
  ];
  const segA = 32;
  const segR = 14;
  const positions: number[] = [];
  const colors: number[] = [];
  const indices: number[] = [];
  const valley = new THREE.Color("#1a120e");
  const sand = new THREE.Color("#5c4030");
  const crest = new THREE.Color("#c49262");
  const color = new THREE.Color();

  for (const [a0, a1] of sectors) {
    const base = positions.length / 3;
    for (let ia = 0; ia <= segA; ia++) {
      const deg = a0 + ((a1 - a0) * ia) / segA;
      const angle = (deg * Math.PI) / 180;
      for (let ir = 0; ir <= segR; ir++) {
        const radius = SAND_INNER + ((SAND_OUTER - SAND_INNER) * ir) / segR;
        const y = duneHeight(angle, radius);
        positions.push(Math.cos(angle) * radius, y, Math.sin(angle) * radius);
        const shade = THREE.MathUtils.clamp((y + 1.75) / 6.2, 0, 1);
        color.copy(valley).lerp(sand, Math.min(1, shade * 1.35));
        if (shade > 0.4) color.lerp(crest, (shade - 0.4) / 0.6);
        colors.push(color.r, color.g, color.b);
      }
    }
    const cols = segR + 1;
    for (let ia = 0; ia < segA; ia++) {
      for (let ir = 0; ir < segR; ir++) {
        const a = base + ia * cols + ir;
        indices.push(a, a + cols, a + 1, a + 1, a + cols, a + cols + 1);
      }
    }
  }

  const geometry = new THREE.BufferGeometry();
  geometry.setAttribute("position", new THREE.Float32BufferAttribute(positions, 3));
  geometry.setAttribute("color", new THREE.Float32BufferAttribute(colors, 3));
  geometry.setIndex(indices);
  geometry.computeVertexNormals();
  return geometry;
}

function useLitMaterial(color: string, roughness: number, emissive = "#000000", emissiveIntensity = 0) {
  const material = useMemo(
    () =>
      new THREE.MeshStandardMaterial({
        color,
        roughness,
        metalness: 0.04,
        emissive,
        emissiveIntensity,
        vertexColors: false,
      }),
    [color, roughness, emissive, emissiveIntensity],
  );
  useEffect(() => () => material.dispose(), [material]);
  return material;
}

function HorizonProps() {
  const pyramidsRef = useRef<THREE.InstancedMesh>(null);
  const obelisksRef = useRef<THREE.InstancedMesh>(null);
  const capsRef = useRef<THREE.InstancedMesh>(null);
  const polesRef = useRef<THREE.InstancedMesh>(null);
  const flamesRef = useRef<THREE.InstancedMesh>(null);
  const pieces = useMemo(() => tombHorizonPieces(), []);
  const pyramids = useMemo(() => pieces.filter((piece) => piece.id.startsWith("pyramid")), [pieces]);
  const obelisks = useMemo(() => pieces.filter((piece) => piece.id.startsWith("obelisk")), [pieces]);
  const torches = useMemo(() => tombTorchPositions(), []);

  const pyramidGeo = useMemo(() => new THREE.ConeGeometry(1, 1, 4), []);
  const obeliskGeo = useMemo(() => new THREE.BoxGeometry(1, 1, 1), []);
  const capGeo = useMemo(() => new THREE.ConeGeometry(1, 1, 4), []);
  const poleGeo = useMemo(() => new THREE.CylinderGeometry(0.12, 0.16, 1, 6), []);
  const flameGeo = useMemo(() => new THREE.SphereGeometry(0.28, 8, 6), []);
  useEffect(
    () => () => {
      pyramidGeo.dispose();
      obeliskGeo.dispose();
      capGeo.dispose();
      poleGeo.dispose();
      flameGeo.dispose();
    },
    [pyramidGeo, obeliskGeo, capGeo, poleGeo, flameGeo],
  );

  const stone = useMemo(
    () => new THREE.MeshBasicMaterial({ color: "#140e16", fog: false, toneMapped: false }),
    [],
  );
  const shaft = useMemo(
    () => new THREE.MeshBasicMaterial({ color: "#1c1418", fog: false, toneMapped: false }),
    [],
  );
  useEffect(() => () => {
    stone.dispose();
    shaft.dispose();
  }, [stone, shaft]);
  const gold = useLitMaterial("#e7c27a", 0.45, "#ffb450", 1.35);
  const poleMat = useLitMaterial("#2a211c", 0.88);
  const flameMat = useMemo(() => {
    const material = new THREE.MeshBasicMaterial({ color: "#ffb15a", toneMapped: false });
    return material;
  }, []);
  useEffect(() => () => flameMat.dispose(), [flameMat]);

  const glowTexture = useMemo(() => {
    const canvas = document.createElement("canvas");
    canvas.width = 64;
    canvas.height = 64;
    const ctx = canvas.getContext("2d");
    if (!ctx) return new THREE.CanvasTexture(canvas);
    const grd = ctx.createRadialGradient(32, 32, 0, 32, 32, 32);
    grd.addColorStop(0, "rgba(255, 236, 196, 1)");
    grd.addColorStop(0.22, "rgba(255, 150, 48, 0.72)");
    grd.addColorStop(1, "rgba(255, 70, 0, 0)");
    ctx.fillStyle = grd;
    ctx.fillRect(0, 0, 64, 64);
    const texture = new THREE.CanvasTexture(canvas);
    texture.colorSpace = THREE.SRGBColorSpace;
    return texture;
  }, []);
  useEffect(() => () => glowTexture.dispose(), [glowTexture]);

  const glowPositions = useMemo(() => {
    const array = new Float32Array(torches.length * 3);
    torches.forEach((torch, i) => {
      array[i * 3] = torch[0];
      array[i * 3 + 1] = torch[1] + 0.35;
      array[i * 3 + 2] = torch[2];
    });
    return array;
  }, [torches]);

  useLayoutEffect(() => {
    const dummy = new THREE.Object3D();
    const mesh = pyramidsRef.current;
    if (!mesh) return;
    pyramids.forEach((piece, i) => {
      const [x, y, z] = place(piece);
      dummy.position.set(x, y, z);
      dummy.scale.set(piece.reach, piece.height, piece.reach);
      dummy.rotation.set(0, -piece.angleDeg * 0.17, 0);
      dummy.updateMatrix();
      mesh.setMatrixAt(i, dummy.matrix);
    });
    mesh.count = pyramids.length;
    mesh.instanceMatrix.needsUpdate = true;
  }, [pyramids]);

  useLayoutEffect(() => {
    const dummy = new THREE.Object3D();
    const shafts = obelisksRef.current;
    const caps = capsRef.current;
    if (!shafts || !caps) return;
    obelisks.forEach((piece, i) => {
      const angle = (piece.angleDeg * Math.PI) / 180;
      const x = Math.cos(angle) * piece.distance;
      const z = Math.sin(angle) * piece.distance;
      dummy.position.set(x, piece.baseY + piece.height / 2, z);
      dummy.scale.set(piece.reach * 2, piece.height, piece.reach * 2);
      dummy.rotation.set(0, angle, 0);
      dummy.updateMatrix();
      shafts.setMatrixAt(i, dummy.matrix);

      dummy.position.set(x, piece.baseY + piece.height + 0.7, z);
      dummy.scale.set(piece.reach * 1.35, 1.4, piece.reach * 1.35);
      dummy.rotation.set(0, angle + 0.4, 0);
      dummy.updateMatrix();
      caps.setMatrixAt(i, dummy.matrix);
    });
    shafts.count = obelisks.length;
    caps.count = obelisks.length;
    shafts.instanceMatrix.needsUpdate = true;
    caps.instanceMatrix.needsUpdate = true;
  }, [obelisks]);

  useLayoutEffect(() => {
    const dummy = new THREE.Object3D();
    const poles = polesRef.current;
    const flames = flamesRef.current;
    if (!poles || !flames) return;
    torches.forEach((torch, i) => {
      dummy.position.set(torch[0], torch[1] - 0.85, torch[2]);
      dummy.scale.set(1, 1.7, 1);
      dummy.rotation.set(0, 0, 0);
      dummy.updateMatrix();
      poles.setMatrixAt(i, dummy.matrix);

      dummy.position.set(torch[0], torch[1] + 0.15, torch[2]);
      dummy.scale.set(1, 1.25, 1);
      dummy.updateMatrix();
      flames.setMatrixAt(i, dummy.matrix);
    });
    poles.count = torches.length;
    flames.count = torches.length;
    poles.instanceMatrix.needsUpdate = true;
    flames.instanceMatrix.needsUpdate = true;
  }, [torches]);

  useFrame(({ clock }) => {
    flameMat.color.setRGB(1, 0.62 + Math.sin(clock.elapsedTime * 3.1) * 0.08, 0.28);
  });

  return (
    <group>
      <instancedMesh
        ref={pyramidsRef}
        args={[pyramidGeo, stone, pyramids.length]}
        receiveShadow
        frustumCulled={false}
      />
      <instancedMesh
        ref={obelisksRef}
        args={[obeliskGeo, shaft, obelisks.length]}
        receiveShadow
        frustumCulled={false}
      />
      <instancedMesh ref={capsRef} args={[capGeo, gold, obelisks.length]} frustumCulled={false} />
      <instancedMesh ref={polesRef} args={[poleGeo, poleMat, torches.length]} frustumCulled={false} />
      <instancedMesh ref={flamesRef} args={[flameGeo, flameMat, torches.length]} frustumCulled={false} />
      <points frustumCulled={false}>
        <bufferGeometry>
          <bufferAttribute attach="attributes-position" count={torches.length} array={glowPositions} itemSize={3} />
        </bufferGeometry>
        <pointsMaterial
          map={glowTexture}
          color="#ff9a3c"
          size={7.2}
          transparent
          opacity={0.9}
          depthWrite={false}
          blending={THREE.AdditiveBlending}
          sizeAttenuation
          toneMapped={false}
          fog={false}
        />
      </points>
    </group>
  );
}

function DuneSea() {
  const geometry = useMemo(() => duneFieldGeometry(), []);
  useEffect(() => () => geometry.dispose(), [geometry]);
  return (
    <mesh geometry={geometry} receiveShadow frustumCulled={false}>
      <meshStandardMaterial vertexColors roughness={0.96} metalness={0.02} emissive="#2a160c" emissiveIntensity={0.22} />
    </mesh>
  );
}

/**
 * Sky, dunes and skyline for Pharaoh's Tomb only.
 * Mounted from MapAtmosphere so the court file and the frozen arena modules stay untouched.
 */
export function PharaohsTombBackdrop() {
  return (
    <group>
      <NightSky />
      {/* Near enough that the hieroglyph wall stays crisp; far dunes sink into the night. */}
      <fog attach="fog" args={["#2a1c30", 56, 148]} />
      <DuneSea />
      <HorizonProps />
    </group>
  );
}

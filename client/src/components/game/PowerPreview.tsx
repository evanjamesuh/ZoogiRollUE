import { useEffect, useLayoutEffect, useRef, useState } from "react";
import { Canvas, useFrame, useThree } from "@react-three/fiber";
import * as THREE from "three";
import { useAudio } from "@/lib/stores/useAudio";
import {
  ExplosionBlast,
  PowerUnlockFlash,
  StunBurst,
  StunnedIndicator,
  WolfCloneLook,
  type Vec3,
} from "./PowerEffects";

const ARENA_RADIUS = 18;

const MARBLES: { position: Vec3; color: string }[] = [
  { position: [0, 0.5, 0], color: "#F97316" },
  { position: [2.6, 0.5, -1.8], color: "#3B82F6" },
  { position: [-3.1, 0.5, 1.6], color: "#9CA3AF" },
  { position: [6.4, 0.5, 5.4], color: "#FBBF24" },
];

function PreviewCamera() {
  const camera = useThree((state) => state.camera);
  useLayoutEffect(() => {
    camera.position.set(0, 18, 22);
    camera.lookAt(0, 0.4, 0);
  }, [camera]);
  return null;
}

function Ground() {
  return (
    <group>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, -0.02, 0]}>
        <circleGeometry args={[ARENA_RADIUS + 8, 64]} />
        <meshStandardMaterial color="#163322" roughness={0.95} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]}>
        <circleGeometry args={[ARENA_RADIUS, 64]} />
        <meshStandardMaterial color="#2f6b3c" roughness={0.86} />
      </mesh>
      <mesh rotation={[-Math.PI / 2, 0, 0]} position={[0, 0.01, 0]}>
        <ringGeometry args={[ARENA_RADIUS - 0.18, ARENA_RADIUS, 72]} />
        <meshBasicMaterial color="#d7f5c4" toneMapped={false} />
      </mesh>
      {MARBLES.map((marble) => (
        <mesh key={marble.color} position={marble.position} castShadow>
          <sphereGeometry args={[0.5, 28, 28]} />
          <meshStandardMaterial color={marble.color} metalness={0.55} roughness={0.22} />
        </mesh>
      ))}
    </group>
  );
}

function MovingClone({ angle, startTime }: { angle: number; startTime: number }) {
  const ref = useRef<THREE.Group>(null);
  const velocity: Vec3 = [Math.cos(angle), 0, Math.sin(angle)];
  useFrame(() => {
    const travel = Math.min((Date.now() - startTime) / 1000, 1.25) * 3.4;
    ref.current?.position.set(Math.cos(angle) * travel, 0, Math.sin(angle) * travel);
  });
  return (
    <group ref={ref}>
      <WolfCloneLook position={[0, 0.5, 0]} velocity={velocity} />
    </group>
  );
}

interface Blast {
  id: number;
  kind: "explosion" | "stun" | "unlock";
  position: Vec3;
  startTime: number;
}

interface StunClock {
  id: number;
  position: Vec3;
  duration: number;
}

export function PowerPreview() {
  const isMuted = useAudio((state) => state.isMuted);
  const toggleMute = useAudio((state) => state.toggleMute);
  const playExplosion = useAudio((state) => state.playExplosion);
  const playStunZap = useAudio((state) => state.playStunZap);
  const playStunEnd = useAudio((state) => state.playStunEnd);
  const playWolfDash = useAudio((state) => state.playWolfDash);
  const playPowerUnlock = useAudio((state) => state.playPowerUnlock);
  const [blasts, setBlasts] = useState<Blast[]>([]);
  const [wolfStart, setWolfStart] = useState<number | null>(null);
  const [stun, setStun] = useState<StunClock | null>(null);
  const idRef = useRef(1);

  useEffect(() => {
    if (!stun) return;
    const timer = window.setTimeout(() => {
      playStunEnd();
      setStun(null);
    }, stun.duration * 1000);
    return () => window.clearTimeout(timer);
  }, [stun, playStunEnd]);

  const pushBlast = (kind: Blast["kind"], position: Vec3) => {
    const id = idRef.current++;
    const startTime = Date.now();
    setBlasts((prev) => [...prev, { id, kind, position, startTime }]);
    window.setTimeout(() => {
      setBlasts((prev) => prev.filter((blast) => blast.id !== id));
    }, 1200);
  };

  return (
    <div style={{ width: "100%", height: "100%", background: "#102033" }}>
      <Canvas
        shadows
        dpr={[1, 2]}
        camera={{ position: [0, 18, 22], fov: 50, near: 0.1, far: 200 }}
        gl={{
          antialias: true,
          preserveDrawingBuffer: true,
          toneMapping: THREE.ACESFilmicToneMapping,
          toneMappingExposure: 1,
        }}
      >
        <color attach="background" args={["#7eb6de"]} />
        <PreviewCamera />
        <ambientLight intensity={0.55} />
        <directionalLight position={[12, 18, 8]} intensity={1.15} />
        <hemisphereLight args={["#cfe8ff", "#2d4a32", 0.35]} />
        <Ground />
        {blasts.map((blast) => {
          if (blast.kind === "explosion") {
            return (
              <ExplosionBlast
                key={blast.id}
                position={blast.position}
                startTime={blast.startTime}
                radius={8}
              />
            );
          }
          if (blast.kind === "stun") {
            return (
              <StunBurst
                key={blast.id}
                position={blast.position}
                startTime={blast.startTime}
                radius={8}
              />
            );
          }
          return (
            <PowerUnlockFlash
              key={blast.id}
              position={blast.position}
              startTime={blast.startTime}
              radius={2.6}
              color="#ffd700"
            />
          );
        })}
        {stun && (
          <StunnedIndicator
            key={stun.id}
            position={stun.position}
            remaining={stun.duration}
            duration={stun.duration}
          />
        )}
        {wolfStart !== null && (
          <group key={wolfStart}>
            {[-Math.PI / 3, 0, Math.PI / 3].map((angle) => (
              <MovingClone key={angle} angle={angle} startTime={wolfStart} />
            ))}
          </group>
        )}
      </Canvas>

      <div
        style={{
          position: "absolute",
          top: 16,
          left: 16,
          display: "flex",
          flexDirection: "column",
          gap: 10,
          zIndex: 2,
          fontFamily: "Inter, system-ui, sans-serif",
        }}
      >
        <div style={{ color: "white", fontWeight: 700, textShadow: "0 1px 2px rgba(0,0,0,0.6)" }}>
          Power preview
        </div>
        <PreviewButton
          label={isMuted ? "Unmute" : "Mute"}
          color="#0f172a"
          onClick={() => toggleMute()}
        />
        <PreviewButton
          label="Explosion"
          color="#ea580c"
          onClick={() => {
            playExplosion();
            pushBlast("explosion", [0, 0, 0]);
          }}
        />
        <PreviewButton
          label="Stun"
          color="#0284c7"
          onClick={() => {
            playStunZap();
            pushBlast("stun", [0, 0, 0]);
            setStun({ id: idRef.current++, position: [2.6, 0.5, -1.8], duration: 2 });
          }}
        />
        <PreviewButton
          label="Wolf clones"
          color="#475569"
          onClick={() => {
            playWolfDash();
            setWolfStart(Date.now());
          }}
        />
        <PreviewButton
          label="Unlock"
          color="#ca8a04"
          onClick={() => {
            playPowerUnlock();
            pushBlast("unlock", [0, 0.2, 0]);
          }}
        />
      </div>
    </div>
  );
}

function PreviewButton({
  label,
  color,
  onClick,
}: {
  label: string;
  color: string;
  onClick: () => void;
}) {
  return (
    <button
      type="button"
      onClick={onClick}
      style={{
        background: color,
        color: "white",
        border: "2px solid rgba(255,255,255,0.85)",
        borderRadius: 10,
        padding: "12px 18px",
        fontSize: 18,
        fontWeight: 700,
        cursor: "pointer",
        minWidth: 160,
        textAlign: "left",
      }}
    >
      {label}
    </button>
  );
}

import { useEffect, useLayoutEffect, useMemo, useRef, useState } from "react";
import { Canvas, useFrame, useThree } from "@react-three/fiber";
import * as THREE from "three";
import { useAudio } from "@/lib/stores/useAudio";
import { useGameFeel } from "@/lib/stores/useGameFeel";
import { MoodyBloom } from "@/vfx/MoodyBloom";
import { SmokeBurstField } from "@/vfx/bursts";
import { emitImpact, ImpactField } from "@/vfx/impacts";
import { AimBeam, BindRibbons, RicochetShell } from "@/vfx/powerLooks";
import {
  ExplosionBlast,
  PowerUnlockFlash,
  ShadowPulse,
  ShadowWrap,
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
  const close = useMemo(
    () => typeof window !== "undefined" && new URLSearchParams(window.location.search).get("vfxClose") === "1",
    [],
  );
  const focus = useRef(new THREE.Vector3(0, 0.4, 0));
  const base = useRef(new THREE.Vector3());
  useLayoutEffect(() => {
    camera.position.set(0, close ? 4.6 : 18, close ? 6.6 : 16);
    camera.lookAt(0, 0.4, 0);
  }, [camera, close]);
  useFrame(() => {
    const aimed = close && typeof window !== "undefined"
      ? (window as Window & { __vfxFocus?: [number, number, number] }).__vfxFocus
      : undefined;
    if (aimed) focus.current.set(aimed[0], aimed[1], aimed[2]);
    else focus.current.set(0, 0.4, 0);
    // A focused close-up sits near the marble so a stun wrap reads; the wide close shot stays back.
    const tight = !!aimed;
    base.current.set(
      focus.current.x,
      focus.current.y + (close ? (tight ? 1.55 : 4.2) : 17.6),
      focus.current.z + (close ? (tight ? 2.35 : 6.6) : 15.6),
    );
    const shake = useGameFeel.getState().screenShake;
    let amp = 0;
    if (shake) {
      const elapsed = Date.now() - shake.startTime;
      const progress = elapsed / shake.duration;
      if (progress < 1) amp = shake.intensity * (1 - progress);
    }
    camera.position.set(
      base.current.x + (amp ? (Math.random() - 0.5) * amp * 2.4 : 0),
      base.current.y + (amp ? (Math.random() - 0.5) * amp : 0),
      base.current.z,
    );
    camera.lookAt(focus.current);
  });
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
    const travel = Math.min((Date.now() - startTime) / 1000, 1.15) * 6.2;
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
  kind: "explosion" | "stun" | "unlock" | "shadow";
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
  const playRicochetPing = useAudio((state) => state.playRicochetPing);
  const playBindWrap = useAudio((state) => state.playBindWrap);
  const playShadowPulse = useAudio((state) => state.playShadowPulse);
  const [blasts, setBlasts] = useState<Blast[]>([]);
  const [shadowBinds, setShadowBinds] = useState<{ id: number; position: Vec3 }[]>([]);
  const [wolfStart, setWolfStart] = useState<number | null>(null);
  const [stun, setStun] = useState<StunClock | null>(null);
  const [shellOn, setShellOn] = useState(false);
  const [bindOn, setBindOn] = useState(false);
  const [aimOn, setAimOn] = useState(false);
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
    const life = kind === "explosion" ? 16000 : kind === "shadow" ? 2800 : 1200;
    window.setTimeout(() => {
      setBlasts((prev) => prev.filter((blast) => blast.id !== id));
    }, life);
  };

  return (
    <div style={{ width: "100%", height: "100%", background: "#102033" }}>
      <Canvas
        shadows
        dpr={[1, 2]}
        camera={{ position: [0, 18, 16], fov: 50, near: 0.1, far: 200 }}
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
        <SmokeBurstField />
        <ImpactField />
        <MoodyBloom />
        {shellOn && (
          <group position={MARBLES[0].position}>
            <RicochetShell />
          </group>
        )}
        {bindOn && (
          <>
            <group position={MARBLES[1].position}>
              <BindRibbons />
            </group>
            <group position={MARBLES[2].position}>
              <BindRibbons />
            </group>
          </>
        )}
        {aimOn && (
          <AimBeam from={[0, 0.55, 0]} to={[2.6, 0.55, -1.8]} />
        )}
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
                targets={[[2.6, 0.5, -1.8]]}
              />
            );
          }
          if (blast.kind === "shadow") {
            return (
              <ShadowPulse
                key={blast.id}
                position={blast.position}
                startTime={blast.startTime}
                radius={4.5}
              />
            );
          }
          return (
            <PowerUnlockFlash
              key={blast.id}
              position={blast.position}
              startTime={blast.startTime}
              radius={5.2}
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
        {shadowBinds.map((bind) => (
          <group key={bind.id} position={bind.position}>
            <ShadowWrap />
          </group>
        ))}
        {wolfStart !== null && (
          <group key={wolfStart}>
            {[Math.PI / 2 - 0.9, Math.PI / 2, Math.PI / 2 + 0.9].map((angle) => (
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
          label="Shadow"
          color="#6B46C1"
          onClick={() => {
            playShadowPulse();
            pushBlast("shadow", [0, 0, 0]);
            const id = idRef.current;
            idRef.current += 2;
            setShadowBinds([
              { id, position: [2.6, 0.5, -1.8] },
              { id: id + 1, position: [-3.1, 0.5, 1.6] },
            ]);
            window.setTimeout(() => setShadowBinds([]), 8000);
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
          label="Ricochet"
          color="#1d4ed8"
          onClick={() => {
            playRicochetPing(shellOn ? "hit" : "arm");
            setShellOn(true);
            emitImpact("spark", [1.3, 0.45, -0.9], [0.86, 0.35, -0.55]);
          }}
        />
        <PreviewButton
          label="Bind"
          color="#a16207"
          onClick={() => {
            playBindWrap();
            setBindOn(true);
            emitImpact("dust", [2.6, 0.4, -1.8]);
          }}
        />
        <PreviewButton
          label="Pinpoint"
          color="#6d28d9"
          onClick={() => setAimOn(true)}
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

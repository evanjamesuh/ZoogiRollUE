import { memo, useEffect, useRef } from "react";
import { createRoot, type Root } from "react-dom/client";
import { useFrame, useThree } from "@react-three/fiber";
import * as THREE from "three";

/**
 * Screen-space marble badge. drei's Html rebuilds its DOM portal on every
 * React render and writes z-index while the marble moves, which reflows the
 * page during the sim. This badge is created once and only updates a CSS
 * transform when the rounded pixel position changes.
 */
interface MarbleNameplateProps {
  letter: string;
  color: string;
  border: string;
  size: number;
  fontSize: number;
  lift: number;
  glow?: boolean;
  slowed?: boolean;
  stunned?: boolean;
}

const worldPos = new THREE.Vector3();
const cameraPos = new THREE.Vector3();
const toCamera = new THREE.Vector3();
const cameraDir = new THREE.Vector3();

function BadgeFace({
  letter,
  color,
  border,
  size,
  fontSize,
  glow,
  slowed,
  stunned,
}: Omit<MarbleNameplateProps, "lift">) {
  return (
    <div style={{ display: "flex", flexDirection: "column", alignItems: "center" }}>
      <div
        style={{
          width: size,
          height: size,
          borderRadius: "50%",
          backgroundColor: color,
          border,
          boxShadow: glow ? "0 0 12px rgba(251, 191, 36, 0.6)" : "0 2px 8px rgba(0,0,0,0.4)",
          display: "flex",
          alignItems: "center",
          justifyContent: "center",
          color: "white",
          fontWeight: "bold",
          fontSize,
          textShadow: "0 1px 2px rgba(0,0,0,0.5)",
        }}
      >
        {letter}
      </div>
      {slowed && (
        <div style={{ marginTop: 4, background: "#D4C4B0", color: "#3f2e22", fontWeight: 700, fontSize: 11, padding: "2px 6px", borderRadius: 6 }}>
          Slowed
        </div>
      )}
      {stunned && (
        <div style={{ marginTop: 4, background: "#FDE047", color: "#3f2e22", fontWeight: 700, fontSize: 11, padding: "2px 6px", borderRadius: 6 }}>
          Stunned
        </div>
      )}
    </div>
  );
}

function MarbleNameplateInner({
  letter,
  color,
  border,
  size,
  fontSize,
  lift,
  glow = false,
  slowed = false,
  stunned = false,
}: MarbleNameplateProps) {
  const group = useRef<THREE.Group>(null);
  const node = useRef<HTMLDivElement | null>(null);
  const root = useRef<Root | null>(null);
  const placed = useRef("");
  const { camera, size: view, gl } = useThree();

  useEffect(() => {
    const parent = gl.domElement.parentElement;
    if (!parent) return;
    const el = document.createElement("div");
    el.style.position = "absolute";
    el.style.top = "0";
    el.style.left = "0";
    el.style.pointerEvents = "none";
    el.style.visibility = "hidden";
    el.style.willChange = "transform";
    parent.appendChild(el);
    node.current = el;
    const next = createRoot(el);
    root.current = next;
    next.render(
      <BadgeFace
        letter={letter}
        color={color}
        border={border}
        size={size}
        fontSize={fontSize}
        glow={glow}
        slowed={slowed}
        stunned={stunned}
      />,
    );
    return () => {
      next.unmount();
      el.remove();
      node.current = null;
      root.current = null;
      placed.current = "";
    };
    // The DOM node is created once per mount. Face updates are a separate effect.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [gl]);

  useEffect(() => {
    root.current?.render(
      <BadgeFace
        letter={letter}
        color={color}
        border={border}
        size={size}
        fontSize={fontSize}
        glow={glow}
        slowed={slowed}
        stunned={stunned}
      />,
    );
  }, [letter, color, border, size, fontSize, glow, slowed, stunned]);

  useFrame(() => {
    const anchor = group.current;
    const el = node.current;
    if (!anchor || !el) return;
    anchor.updateWorldMatrix(true, false);
    worldPos.setFromMatrixPosition(anchor.matrixWorld);
    cameraPos.setFromMatrixPosition(camera.matrixWorld);
    toCamera.copy(worldPos).sub(cameraPos);
    camera.getWorldDirection(cameraDir);
    const behind = toCamera.angleTo(cameraDir) > Math.PI / 2;
    const depthBucket = Math.round(toCamera.length() * 2);
    worldPos.project(camera);
    const x = Math.round(worldPos.x * view.width * 0.5 + view.width * 0.5);
    const y = Math.round(-worldPos.y * view.height * 0.5 + view.height * 0.5);
    const hidden = behind || worldPos.z > 1;
    const key = hidden ? `h${depthBucket}` : `${x},${y},${depthBucket}`;
    if (key === placed.current) return;
    placed.current = key;
    if (hidden) {
      el.style.visibility = "hidden";
      return;
    }
    el.style.visibility = "visible";
    el.style.zIndex = String(2000 - depthBucket);
    el.style.transform = `translate3d(${x}px,${y}px,0) translate(-50%,-50%)`;
  });

  return <group ref={group} position={[0, lift, 0]} />;
}

export const MarbleNameplate = memo(MarbleNameplateInner);

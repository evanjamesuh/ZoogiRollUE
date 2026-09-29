import { useEffect, useRef } from "react";
import * as THREE from "three";

export interface VfxTextures {
  smoke: THREE.Texture;
  ember: THREE.Texture;
  scorch: THREE.Texture;
}

let smokeTex: THREE.Texture | null = null;
let emberTex: THREE.Texture | null = null;
let scorchTex: THREE.Texture | null = null;
let users = 0;

function hash2(ix: number, iy: number): number {
  let n = Math.imul(ix, 374761393) + Math.imul(iy, 668265263);
  n = Math.imul(n ^ (n >>> 13), 1274126177);
  return ((n ^ (n >>> 16)) >>> 0) / 4294967296;
}

function valueNoise(x: number, y: number): number {
  const x0 = Math.floor(x);
  const y0 = Math.floor(y);
  const fx = x - x0;
  const fy = y - y0;
  const sx = fx * fx * (3 - 2 * fx);
  const sy = fy * fy * (3 - 2 * fy);
  const n00 = hash2(x0, y0);
  const n10 = hash2(x0 + 1, y0);
  const n01 = hash2(x0, y0 + 1);
  const n11 = hash2(x0 + 1, y0 + 1);
  return n00 * (1 - sx) * (1 - sy) + n10 * sx * (1 - sy) + n01 * (1 - sx) * sy + n11 * sx * sy;
}

function fbm(x: number, y: number): number {
  let v = 0;
  let a = 0.5;
  let f = 1;
  for (let o = 0; o < 4; o++) {
    v += a * valueNoise(x * f, y * f);
    f *= 2;
    a *= 0.5;
  }
  return v;
}

function canvasTexture(size: number, paint: (data: Uint8ClampedArray, size: number) => void): THREE.CanvasTexture {
  const canvas = document.createElement("canvas");
  canvas.width = size;
  canvas.height = size;
  const ctx = canvas.getContext("2d");
  if (!ctx) {
    const fallback = new THREE.DataTexture(new Uint8Array([255, 255, 255, 255]), 1, 1);
    fallback.needsUpdate = true;
    return fallback as unknown as THREE.CanvasTexture;
  }
  const image = ctx.createImageData(size, size);
  paint(image.data, size);
  ctx.putImageData(image, 0, 0);
  const texture = new THREE.CanvasTexture(canvas);
  texture.colorSpace = THREE.SRGBColorSpace;
  texture.wrapS = THREE.ClampToEdgeWrapping;
  texture.wrapT = THREE.ClampToEdgeWrapping;
  texture.needsUpdate = true;
  return texture;
}

function paintSmoke(data: Uint8ClampedArray, size: number): void {
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const u = (x + 0.5) / size - 0.5;
      const v = (y + 0.5) / size - 0.5;
      const d = Math.hypot(u, v) * 2;
      const radial = d >= 1 ? 0 : Math.pow(Math.max(0, 1 - d * d), 0.45);
      const n = fbm(u * 5.5 + 3.2, v * 5.5 + 1.4);
      const alpha = radial * (0.62 + 0.38 * n);
      const tone = 175 + n * 70;
      const i = (y * size + x) * 4;
      data[i] = tone;
      data[i + 1] = tone * 0.96;
      data[i + 2] = tone * 0.9;
      data[i + 3] = Math.max(0, Math.min(255, alpha * 255));
    }
  }
}

function paintEmber(data: Uint8ClampedArray, size: number): void {
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const u = (x + 0.5) / size - 0.5;
      const v = (y + 0.5) / size - 0.5;
      const d = Math.hypot(u, v) * 2;
      const glow = Math.exp(-d * d * 5.5);
      const core = Math.exp(-d * d * 22);
      const alpha = Math.max(0, Math.min(1, glow));
      const i = (y * size + x) * 4;
      const tone = 220 + core * 35;
      data[i] = tone;
      data[i + 1] = tone;
      data[i + 2] = tone;
      data[i + 3] = alpha * 255;
    }
  }
}

function paintScorch(data: Uint8ClampedArray, size: number): void {
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const u = (x + 0.5) / size - 0.5;
      const v = (y + 0.5) / size - 0.5;
      const d = Math.hypot(u, v) * 2;
      const n = fbm(u * 3.4 + 2.2, v * 3.4 + 8.1);
      const ragged = d < 0.62 ? 1 : Math.max(0, 1 - (d - 0.62) / 0.4);
      const alpha = ragged * (0.72 + 0.28 * n);
      const i = (y * size + x) * 4;
      const tone = 8 + n * 14;
      data[i] = tone;
      data[i + 1] = tone * 0.72;
      data[i + 2] = tone * 0.5;
      data[i + 3] = Math.max(0, Math.min(255, alpha * 255));
    }
  }
}

export function retainVfxTextures(): VfxTextures {
  users += 1;
  if (!smokeTex || !emberTex || !scorchTex) {
    smokeTex = canvasTexture(128, paintSmoke);
    emberTex = canvasTexture(64, paintEmber);
    scorchTex = canvasTexture(128, paintScorch);
  }
  return { smoke: smokeTex, ember: emberTex, scorch: scorchTex };
}

export function releaseVfxTextures(): void {
  users = Math.max(0, users - 1);
  if (users > 0) return;
  smokeTex?.dispose();
  emberTex?.dispose();
  scorchTex?.dispose();
  smokeTex = null;
  emberTex = null;
  scorchTex = null;
}

export function useVfxTextures(): VfxTextures {
  const ref = useRef<VfxTextures | null>(null);
  if (ref.current === null) ref.current = retainVfxTextures();
  useEffect(() => {
    return () => {
      releaseVfxTextures();
      ref.current = null;
    };
  }, []);
  return ref.current;
}

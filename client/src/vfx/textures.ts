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
  const cell = size / 2;
  for (let y = 0; y < size; y++) {
    for (let x = 0; x < size; x++) {
      const cx = Math.floor(x / cell);
      const cy = Math.floor(y / cell);
      const u = ((x % cell) + 0.5) / cell - 0.5;
      const v = ((y % cell) + 0.5) / cell - 0.5;
      const d = Math.hypot(u, v) * 2;
      const falloff = Math.exp(-d * d * 1.25);
      const n = fbm(u * 3.1 + cx * 5.7 + 1.3, v * 3.4 + cy * 4.9 + 2.8);
      const alpha = falloff * (0.22 + 0.78 * n);
      const tone = 168 + n * 60;
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
      const n = fbm(u * 5.4 + 1.7, v * 4.8 + 6.4);
      const lobe = fbm(u * 2.2 + 4.1, v * 2.6 + 0.7);
      const limit = 0.42 + lobe * 0.38;
      const fall = Math.max(0, 1 - d / limit);
      const ragged = Math.pow(fall, 1.35) * (0.35 + 0.65 * n);
      const alpha = d > limit ? 0 : ragged;
      const i = (y * size + x) * 4;
      const tone = 18 + n * 22;
      data[i] = tone;
      data[i + 1] = tone * 0.72;
      data[i + 2] = tone * 0.5;
      data[i + 3] = Math.max(0, Math.min(255, alpha * 180));
    }
  }
}

export function retainVfxTextures(): VfxTextures {
  users += 1;
  if (!smokeTex || !emberTex || !scorchTex) {
    smokeTex = canvasTexture(256, paintSmoke);
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

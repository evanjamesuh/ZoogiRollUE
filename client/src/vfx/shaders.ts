export const SPRITE_VERT = /* glsl */ `
attribute float aOpacity;
attribute float aSpin;
attribute float aSize;
attribute float aStretch;
attribute float aVariant;
attribute vec3 aColor;
attribute vec3 aVelocity;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;
varying float vVariant;
varying float vStretch;

void main() {
  vOpacity = aOpacity;
  vColor = aColor;
  vUv = uv;
  vVariant = aVariant;
  vStretch = max(aStretch, 1.0);
  #ifdef USE_INSTANCING
    vec3 center = vec3(instanceMatrix[3][0], instanceMatrix[3][1], instanceMatrix[3][2]);
  #else
    vec3 center = vec3(0.0);
  #endif
  float c = cos(aSpin);
  float s = sin(aSpin);
  vec2 spun = vec2(c * position.x - s * position.y, s * position.x + c * position.y);
  vec4 viewCenter = viewMatrix * modelMatrix * vec4(center, 1.0);
  vec3 viewVel = mat3(viewMatrix) * mat3(modelMatrix) * aVelocity;
  vec2 dir = viewVel.xy;
  float mag = length(dir);
  vec2 along = mag > 0.001 ? dir / mag : vec2(0.0, 1.0);
  vec2 side = vec2(-along.y, along.x);
  float stretch = max(aStretch, 1.0);
  vec2 offset = stretch > 1.08
    ? along * position.x * stretch + side * position.y
    : spun;
  viewCenter.xy += offset * aSize;
  gl_Position = projectionMatrix * viewCenter;
}
`;

const PUFF_NOISE = /* glsl */ `
float hash21(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash21(i);
  float b = hash21(i + vec2(1.0, 0.0));
  float c = hash21(i + vec2(0.0, 1.0));
  float d = hash21(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
  float v = 0.0;
  float a = 0.5;
  for (int i = 0; i < 4; i++) {
    v += a * noise(p);
    p = p * 2.05 + vec2(1.7, 9.2);
    a *= 0.5;
  }
  return v;
}
`;

export const SMOKE_FRAG = /* glsl */ `
uniform float uTime;
uniform float uWarm;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;
varying float vVariant;

void main() {
  vec2 uv = vUv - 0.5;
  float dist = length(uv);
  float roundMask = smoothstep(0.5, 0.15, dist);
  vec2 scroll = vec2(uTime * 0.06, uTime * 0.035) + vVariant * 2.1;
  float n = fbm(vUv * 3.2 + scroll);
  float lobes = fbm(vUv * 1.75 + n * 1.8 + scroll.yx);
  float interior = smoothstep(0.4, 0.16, dist);
  float lumps = smoothstep(0.3, 0.68, lobes);
  float shape = mix(lumps, 1.0, interior);
  float alpha = min(roundMask * shape * (0.78 + 0.22 * n) * vOpacity, 0.85);
  float lower = smoothstep(0.58, 0.14, vUv.y);
  float lit = mix(1.15, 0.46, lower);
  vec3 charcoal = vec3(0.12, 0.125, 0.135) * lit;
  vec3 warm = vec3(0.52, 0.18, 0.05);
  vec3 col = mix(charcoal, warm, lower * uWarm * 0.8);
  gl_FragColor = vec4(col, alpha);
}
`.replace("void main()", `${PUFF_NOISE}\nvoid main()`);

export const MIST_FRAG = /* glsl */ `
uniform float uTime;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;
varying float vVariant;

void main() {
  float dist = length(vUv - 0.5);
  float roundMask = smoothstep(0.5, 0.15, dist);
  float n = fbm(vUv * 3.1 + vec2(uTime * 0.09, vVariant * 2.4));
  float alpha = min(roundMask * (0.5 + 0.5 * n) * vOpacity, 0.7);
  vec3 col = vec3(0.46, 0.56, 0.64) * (0.78 + 0.32 * n);
  gl_FragColor = vec4(col, alpha);
}
`.replace("void main()", `${PUFF_NOISE}\nvoid main()`);

export const FIRE_FRAG = /* glsl */ `
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;

void main() {
  float dist = length(vUv - 0.5);
  float edge = smoothstep(0.5, 0.16, dist);
  float d = dist * 2.0;
  float glow = exp(-d * d * 2.2) * edge;
  float hot = exp(-d * d * 16.0);
  vec3 orange = vec3(1.0, 0.42, 0.05);
  vec3 white = vec3(1.35, 1.15, 0.82);
  vec3 col = mix(orange, white, hot) * (0.75 + vColor.g * 0.35);
  gl_FragColor = vec4(col * glow * vOpacity * 4.8, 1.0);
}
`;

export const EMBER_FRAG = /* glsl */ `
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;
varying float vStretch;

void main() {
  vec2 p = vUv - 0.5;
  float edge = smoothstep(0.5, 0.2, length(p));
  float mask;
  float hot;
  if (vStretch < 1.2) {
    float d = length(p) * 2.0;
    mask = exp(-d * d * 9.0) * edge;
    hot = exp(-d * d * 28.0);
  } else {
    float across = abs(p.y) * 2.0;
    float along = abs(p.x) * 2.0;
    mask = exp(-across * across * 36.0) * exp(-along * along * 2.4) * edge;
    hot = exp(-across * across * 90.0) * exp(-along * along * 6.0);
  }
  vec3 col = mix(vColor, vec3(1.0, 0.96, 0.75), hot);
  gl_FragColor = vec4(col * mask * vOpacity * 6.0, 1.0);
}
`;

export const RING_VERT = /* glsl */ `
varying vec2 vUv;
void main() {
  vUv = uv;
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}
`;

export const RING_FRAG = /* glsl */ `
uniform float uWave;
uniform float uOpacity;
varying vec2 vUv;

void main() {
  vec2 p = vUv * 2.0 - 1.0;
  float r = length(p);
  float width = 0.014;
  float d = r - uWave;
  float band = exp(-(d * d) / (width * width));
  float edge = 1.0 - smoothstep(0.96, 1.02, r);
  float alpha = band * uOpacity * edge;
  if (alpha < 0.015) discard;
  vec3 dust = vec3(0.55, 0.46, 0.34);
  gl_FragColor = vec4(dust, alpha);
}
`;

export const WOLF_VERT = /* glsl */ `
attribute float aLimb;
attribute vec3 aPivot;
uniform float uTime;
varying vec3 vNormal;
varying vec3 vWorld;
varying vec3 vLocal;

void main() {
  vec3 p = position;
  float bob = sin(uTime * 9.0) * 0.04;
  if (aLimb > 0.5 && aLimb < 4.5) {
    float phase = (aLimb < 1.5 || (aLimb > 3.5 && aLimb < 4.5)) ? 0.0 : 3.14159;
    float ang = sin(uTime * 11.0 + phase) * 0.6;
    vec3 q = p - aPivot;
    float c = cos(ang);
    float s = sin(ang);
    q = vec3(q.x, c * q.y - s * q.z, s * q.y + c * q.z);
    p = aPivot + q;
  } else if (aLimb > 4.5) {
    float sway = sin(uTime * 6.5) * 0.45;
    vec3 q = p - aPivot;
    float c = cos(sway);
    float s = sin(sway);
    q = vec3(c * q.x - s * q.z, q.y, s * q.x + c * q.z);
    p = aPivot + q;
  }
  p.y += bob;
  vLocal = p;
  vec4 world = modelMatrix * vec4(p, 1.0);
  vWorld = world.xyz;
  vNormal = normalize(mat3(modelMatrix) * normal);
  gl_Position = projectionMatrix * viewMatrix * world;
}
`;

const WOLF_NOISE = /* glsl */ `
float hash21w(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noiseW(vec2 p) {
  vec2 i = floor(p);
  vec2 f = fract(p);
  f = f * f * (3.0 - 2.0 * f);
  float a = hash21w(i);
  float b = hash21w(i + vec2(1.0, 0.0));
  float c = hash21w(i + vec2(0.0, 1.0));
  float d = hash21w(i + vec2(1.0, 1.0));
  return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}
`;

export const WOLF_FRAG = /* glsl */ `
uniform float uTime;
uniform float uFade;
varying vec3 vNormal;
varying vec3 vWorld;
varying vec3 vLocal;

void main() {
  vec3 n = normalize(vNormal);
  vec3 viewDir = normalize(cameraPosition - vWorld);
  float fres = pow(1.0 - clamp(abs(dot(n, viewDir)), 0.0, 1.0), 1.45);
  float nse = noiseW(vLocal.xy * 2.6 + vLocal.yz * 1.7 + vec2(uTime * 0.9, uTime * 0.45));
  float flick = 0.72 + 0.28 * sin(uTime * 17.0 + nse * 22.0);
  fres *= flick;
  float rear = smoothstep(0.12, -0.85, vLocal.z);
  float low = smoothstep(0.02, -0.62, vLocal.y);
  float breakUp = clamp(rear * 0.92 + low * 0.88, 0.0, 1.0);
  float wisp = smoothstep(0.2, 0.7, nse);
  float mask = mix(1.0, wisp, breakUp);
  vec3 col = vec3(0.92, 1.5, 1.82) * (1.15 + fres * 3.1);
  float alpha = (0.5 + fres * 0.78) * uFade * mask;
  gl_FragColor = vec4(col, alpha);
}
`.replace("void main()", `${WOLF_NOISE}\nvoid main()`);

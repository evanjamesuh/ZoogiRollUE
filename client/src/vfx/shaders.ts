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
varying float vWorldY;
varying float vCamDist;

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
  vWorldY = cameraPosition.y + dot(viewMatrix[1].xyz, viewCenter.xyz);
  vCamDist = length(viewCenter.xyz);
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
varying float vWorldY;
varying float vCamDist;

void main() {
  vec2 uv = vUv - 0.5;
  vec2 scroll = vec2(uTime * 0.06, uTime * 0.035) + vVariant * 2.1;
  float n = fbm(vUv * 3.2 + scroll);
  float dist = length(uv * (0.82 + n * 0.36));
  float roundMask = smoothstep(0.5, 0.0, length(uv));
  float lobes = fbm(vUv * 1.75 + n * 1.8 + scroll.yx);
  float interior = smoothstep(0.5, 0.18, dist);
  float lumps = smoothstep(0.28, 0.66, lobes);
  float shape = mix(lumps, 1.0, interior);
  float alpha = min(roundMask * shape * (0.88 + 0.12 * n) * vOpacity, 0.85);
  alpha *= smoothstep(0.0, 0.55, vWorldY + (n - 0.5) * 0.2);
  float lower = smoothstep(0.62, 0.14, vUv.y);
  float lit = mix(1.2, 0.58, lower);
  vec3 charcoal = max(vColor, vec3(0.04)) * lit;
  vec3 warm = vec3(0.32, 0.1, 0.025);
  vec3 col = mix(charcoal, warm, lower * uWarm * 0.9);
  gl_FragColor = vec4(col, alpha);
}
`.replace("void main()", `${PUFF_NOISE}\nvoid main()`);

export const MIST_FRAG = /* glsl */ `
uniform float uTime;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;
varying float vVariant;
varying float vWorldY;
varying float vCamDist;

void main() {
  vec2 uv = vUv - 0.5;
  float n = fbm(vUv * 1.6 + vec2(uTime * 0.03, vVariant));
  float n2 = fbm(vUv * 3.4 + vec2(4.2, vVariant * 2.0));
  vec2 warped = uv + vec2(n - 0.5, n2 - 0.5) * 0.42;
  float dist = length(warped);
  float alpha = exp(-dist * dist * 5.2);
  alpha *= smoothstep(0.48, 0.04, length(uv));
  alpha = min(alpha * (0.55 + 0.45 * n) * vOpacity, 0.25);
  alpha *= smoothstep(0.0, 0.45, vWorldY + (n2 - 0.5) * 0.16);
  alpha *= smoothstep(2.0, 3.1, vCamDist);
  vec3 col = vec3(0.32, 0.42, 0.5);
  gl_FragColor = vec4(col, alpha);
}
`.replace("void main()", `${PUFF_NOISE}\nvoid main()`);

export const DUST_FRAG = /* glsl */ `
uniform float uTime;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;
varying float vVariant;
varying float vWorldY;
varying float vCamDist;

void main() {
  float dist = length(vUv - 0.5);
  float roundMask = smoothstep(0.5, 0.02, dist);
  float n = fbm(vUv * 2.8 + vec2(uTime * 0.05, vVariant * 1.7));
  float alpha = min(roundMask * (0.42 + 0.58 * n) * vOpacity, 0.62);
  alpha *= smoothstep(0.0, 0.55, vWorldY + (n - 0.5) * 0.18);
  vec3 col = vec3(0.62, 0.48, 0.3) * (0.72 + 0.4 * n);
  gl_FragColor = vec4(col, alpha);
}
`.replace("void main()", `${PUFF_NOISE}\nvoid main()`);

export const RIBBON_VERT = /* glsl */ `
attribute float aSide;
attribute float aAlong;
varying float vSide;
varying float vAlong;
void main() {
  vSide = aSide;
  vAlong = aAlong;
  gl_Position = projectionMatrix * modelViewMatrix * vec4(position, 1.0);
}
`;

export const RIBBON_FRAG = /* glsl */ `
uniform vec3 uColor;
uniform float uFade;
uniform float uCloth;
uniform float uEndFade;
uniform float uHot;
varying float vSide;
varying float vAlong;

float clothHash(vec2 p) {
  return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

void main() {
  float edge = smoothstep(1.0, 0.22, abs(vSide));
  float core = exp(-vSide * vSide * 14.0);
  float along = smoothstep(0.0, 0.16, vAlong) * smoothstep(1.0, 0.72, vAlong);
  float tail = mix(1.0, smoothstep(1.0, 0.42, vAlong), uEndFade);
  along *= tail;
  vec3 col = uColor * mix(0.62, 1.0, core);
  if (uCloth > 0.5) {
    float stripe = 0.74 + 0.26 * sin(vAlong * 42.0);
    float grit = clothHash(vec2(floor(vAlong * 48.0), floor(vSide * 5.0)));
    float fray = mix(0.45 + 0.55 * grit, 1.0, smoothstep(0.0, 0.22, vAlong) * smoothstep(1.0, 0.78, vAlong));
    col *= stripe * (0.86 + 0.14 * grit);
    gl_FragColor = vec4(col, edge * along * fray * uFade);
  } else {
    float gain = uHot > 0.5 ? 4.8 : 1.35;
    float body = uHot > 0.5 ? 0.22 : 0.55;
    col = uColor * edge * body + uColor * core * gain;
    gl_FragColor = vec4(col * along * uFade, 1.0);
  }
}
`;

export const SHELL_VERT = /* glsl */ `
varying vec3 vNormal;
varying vec3 vWorld;
varying vec3 vLocal;
void main() {
  vLocal = position;
  vec4 world = modelMatrix * vec4(position, 1.0);
  vWorld = world.xyz;
  vNormal = normalize(mat3(modelMatrix) * normal);
  gl_Position = projectionMatrix * viewMatrix * world;
}
`;

export const SHELL_FRAG = /* glsl */ `
uniform float uTime;
varying vec3 vNormal;
varying vec3 vWorld;
varying vec3 vLocal;
void main() {
  vec3 n = normalize(vNormal);
  vec3 viewDir = normalize(cameraPosition - vWorld);
  float fres = pow(1.0 - clamp(abs(dot(n, viewDir)), 0.0, 1.0), 1.85);
  float band = smoothstep(0.62, 0.95, sin(vLocal.y * 8.0 + uTime * 1.3) * 0.5 + 0.5);
  vec3 col = vec3(0.62, 0.82, 1.15) * (0.55 + fres * 2.6) + vec3(1.7, 1.85, 2.1) * band * fres;
  float alpha = fres * (0.55 + band * 0.5);
  gl_FragColor = vec4(col, alpha);
}
`;

export const GLINT_VERT = /* glsl */ `
uniform float uSize;
varying vec2 vUv;
void main() {
  vUv = uv;
  vec4 viewCenter = viewMatrix * modelMatrix * vec4(0.0, 0.0, 0.0, 1.0);
  viewCenter.xy += position.xy * uSize;
  gl_Position = projectionMatrix * viewCenter;
}
`;

export const GLINT_FRAG = /* glsl */ `
uniform vec3 uColor;
uniform float uTime;
varying vec2 vUv;
void main() {
  float dist = length(vUv - 0.5);
  float edge = smoothstep(0.5, 0.16, dist);
  float core = exp(-dist * dist * 48.0);
  float pulse = 0.62 + 0.38 * sin(uTime * 6.5);
  vec3 col = uColor * (edge * 0.16 + core * 2.8) * pulse;
  gl_FragColor = vec4(col, 1.0);
}
`;

export const GROUND_GLOW_FRAG = /* glsl */ `
uniform vec3 uColor;
uniform float uOpacity;
uniform float uWave;
varying vec2 vUv;
void main() {
  float r = length(vUv * 2.0 - 1.0);
  float band = exp(-pow((r - uWave) / 0.18, 2.0));
  float pool = (1.0 - smoothstep(0.0, 0.9, r)) * 0.5;
  float edge = 1.0 - smoothstep(0.78, 1.0, r);
  float alpha = (band * 0.35 + pool) * edge * uOpacity;
  gl_FragColor = vec4(uColor * 1.25, alpha);
}
`;

export const STUN_RIM_FRAG = /* glsl */ `
uniform float uTime;
varying vec3 vNormal;
varying vec3 vWorld;
void main() {
  vec3 n = normalize(vNormal);
  vec3 viewDir = normalize(cameraPosition - vWorld);
  float fres = pow(1.0 - clamp(abs(dot(n, viewDir)), 0.0, 1.0), 2.0);
  float flick = 0.62 + 0.38 * sin(uTime * 19.0);
  vec3 col = vec3(0.45, 0.68, 1.05) * fres * flick * 1.05;
  gl_FragColor = vec4(col, 1.0);
}
`;

export const FIRE_FRAG = /* glsl */ `
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;
varying float vWorldY;
varying float vCamDist;

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
varying float vWorldY;
varying float vCamDist;

void main() {
  vec2 p = vUv - 0.5;
  float edge = smoothstep(0.5, 0.2, length(p));
  float mask;
  float hot;
  float gain;
  if (vStretch < 1.2) {
    float d = length(p) * 2.0;
    mask = exp(-d * d * 28.0) * edge;
    hot = exp(-d * d * 70.0);
    gain = 3.2;
  } else {
    float across = abs(p.y) * 2.0;
    float along = abs(p.x) * 2.0;
    mask = exp(-across * across * 110.0) * exp(-along * along * 1.35) * edge;
    hot = exp(-across * across * 280.0) * exp(-along * along * 7.0);
    gain = 2.2 + hot * 3.4;
  }
  vec3 col = mix(vColor, vec3(1.0, 0.97, 0.82), hot);
  gl_FragColor = vec4(col * mask * vOpacity * gain, 1.0);
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
  float fres = pow(1.0 - clamp(abs(dot(n, viewDir)), 0.0, 1.0), 2.6);
  float nse = noiseW(vLocal.xy * 2.6 + vLocal.yz * 1.7 + vec2(uTime * 0.9, uTime * 0.45));
  float flick = 0.86 + 0.14 * sin(uTime * 17.0 + nse * 22.0);
  float rear = smoothstep(0.02, -1.15, vLocal.z);
  float low = smoothstep(0.58, 0.05, vLocal.y);
  float breakUp = clamp(rear * 1.45 + low * 1.7, 0.0, 1.0);
  float wisp = smoothstep(0.28, 0.78, nse);
  float mask = mix(0.94, wisp * 0.22, breakUp);
  vec3 body = vec3(0.34, 0.7, 0.86) * (0.9 + nse * 0.22) * flick;
  vec3 col = body + vec3(0.25, 0.55, 0.7) * fres * (1.0 - breakUp) * 0.45;
  float alpha = (0.78 + fres * 0.06) * uFade * mask;
  gl_FragColor = vec4(col, alpha);
}
`.replace("void main()", `${WOLF_NOISE}\nvoid main()`);

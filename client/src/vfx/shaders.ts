export const SPRITE_VERT = /* glsl */ `
attribute float aOpacity;
attribute float aSpin;
attribute float aSize;
attribute float aStretch;
attribute float aVariant;
attribute vec3 aColor;
attribute vec3 aVelocity;
uniform float uAtlas;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;

void main() {
  vOpacity = aOpacity;
  vColor = aColor;
  vec2 baseUv = uv;
  if (uAtlas > 0.5) {
    float v = mod(aVariant, 4.0);
    vec2 cell = vec2(mod(v, 2.0), floor(v / 2.0));
    baseUv = uv * 0.5 + cell * 0.5;
  }
  vUv = baseUv;
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

export const SMOKE_FRAG = /* glsl */ `
uniform sampler2D uMap;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;

void main() {
  float mask = texture2D(uMap, vUv).a;
  float alpha = mask * vOpacity;
  if (alpha < 0.02) discard;
  float core = smoothstep(0.0, 0.9, mask);
  vec3 warmEdge = vec3(0.48, 0.24, 0.1);
  vec3 col = mix(warmEdge, vColor, core);
  gl_FragColor = vec4(col, alpha);
}
`;

export const MIST_FRAG = /* glsl */ `
uniform sampler2D uMap;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;

void main() {
  float mask = texture2D(uMap, vUv).a;
  float alpha = mask * vOpacity;
  if (alpha < 0.025) discard;
  vec3 col = vColor * (0.62 + 0.5 * mask);
  gl_FragColor = vec4(col, alpha);
}
`;

export const FIRE_FRAG = /* glsl */ `
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;

void main() {
  vec2 p = vUv * 2.0 - 1.0;
  float d = length(p);
  float glow = exp(-d * d * 2.4);
  float hot = exp(-d * d * 14.0);
  if (glow < 0.02) discard;
  vec3 orange = vec3(1.0, 0.42, 0.05);
  vec3 white = vec3(1.35, 1.15, 0.82);
  vec3 col = mix(orange, white, hot) * (0.75 + vColor.g * 0.35);
  gl_FragColor = vec4(col * glow * vOpacity * 4.8, 1.0);
}
`;

export const EMBER_FRAG = /* glsl */ `
uniform sampler2D uMap;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;

void main() {
  vec4 tex = texture2D(uMap, vUv);
  float alpha = tex.a * vOpacity;
  if (alpha < 0.02) discard;
  gl_FragColor = vec4(vColor * tex.rgb * alpha * 4.2, 1.0);
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
  vec4 world = modelMatrix * vec4(p, 1.0);
  vWorld = world.xyz;
  vNormal = normalize(mat3(modelMatrix) * normal);
  gl_Position = projectionMatrix * viewMatrix * world;
}
`;

export const WOLF_FRAG = /* glsl */ `
uniform float uFade;
varying vec3 vNormal;
varying vec3 vWorld;

void main() {
  vec3 n = normalize(vNormal);
  vec3 viewDir = normalize(cameraPosition - vWorld);
  float fres = pow(1.0 - clamp(abs(dot(n, viewDir)), 0.0, 1.0), 1.7);
  vec3 col = vec3(0.55, 0.9, 1.15) * (0.42 + fres * 1.7);
  float alpha = (0.3 + fres * 0.55) * uFade;
  gl_FragColor = vec4(col, alpha);
}
`;

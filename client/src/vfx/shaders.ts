export const SPRITE_VERT = /* glsl */ `
attribute float aOpacity;
attribute float aSpin;
attribute vec3 aColor;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;

void main() {
  vUv = uv;
  vOpacity = aOpacity;
  vColor = aColor;
  #ifdef USE_INSTANCING
    vec3 center = vec3(instanceMatrix[3][0], instanceMatrix[3][1], instanceMatrix[3][2]);
    float size = length(instanceMatrix[0].xyz);
  #else
    vec3 center = vec3(0.0);
    float size = 1.0;
  #endif
  float c = cos(aSpin);
  float s = sin(aSpin);
  vec2 spun = vec2(c * position.x - s * position.y, s * position.x + c * position.y);
  vec4 worldCenter = modelMatrix * vec4(center, 1.0);
  vec3 camRight = vec3(viewMatrix[0][0], viewMatrix[0][1], viewMatrix[0][2]);
  vec3 camUp = vec3(viewMatrix[1][0], viewMatrix[1][1], viewMatrix[1][2]);
  vec3 worldPos = worldCenter.xyz + (camRight * spun.x + camUp * spun.y) * size;
  gl_Position = projectionMatrix * viewMatrix * vec4(worldPos, 1.0);
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
  if (alpha < 0.03) discard;
  float core = smoothstep(0.18, 0.72, mask);
  vec3 warmEdge = vec3(0.62, 0.30, 0.13);
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

export const EMBER_FRAG = /* glsl */ `
uniform sampler2D uMap;
varying float vOpacity;
varying vec3 vColor;
varying vec2 vUv;

void main() {
  vec4 tex = texture2D(uMap, vUv);
  float alpha = tex.a * vOpacity;
  if (alpha < 0.02) discard;
  gl_FragColor = vec4(vColor * tex.rgb * alpha * 3.6, 1.0);
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
  float width = 0.075;
  float d = r - uWave;
  float band = exp(-(d * d) / (width * width));
  float edge = 1.0 - smoothstep(0.9, 1.04, r);
  float alpha = band * uOpacity * edge;
  if (alpha < 0.02) discard;
  vec3 dust = mix(vec3(0.62, 0.48, 0.32), vec3(0.34, 0.26, 0.18), smoothstep(0.35, 1.0, band));
  gl_FragColor = vec4(dust, alpha * 0.9);
}
`;

export const WOLF_VERT = /* glsl */ `
varying vec3 vNormal;
varying vec3 vWorld;
void main() {
  vec4 world = modelMatrix * vec4(position, 1.0);
  vWorld = world.xyz;
  vNormal = normalize(mat3(modelMatrix) * normal);
  gl_Position = projectionMatrix * viewMatrix * world;
}
`;

export const WOLF_FRAG = /* glsl */ `
uniform float uTime;
uniform float uFade;
uniform sampler2D uNoise;
varying vec3 vNormal;
varying vec3 vWorld;

void main() {
  vec3 n = normalize(vNormal);
  vec3 viewDir = normalize(cameraPosition - vWorld);
  float fres = pow(1.0 - clamp(abs(dot(n, viewDir)), 0.0, 1.0), 1.45);
  vec2 uv = vWorld.xz * 0.55 + vec2(uTime * 0.07, uTime * 0.04);
  float smoke = texture2D(uNoise, fract(uv)).a;
  float wisp = smoothstep(0.2, 0.85, smoke);
  vec3 deep = vec3(0.04, 0.09, 0.16);
  vec3 rim = vec3(0.62, 0.84, 1.2);
  vec3 col = mix(deep, rim, fres) * (0.75 + fres * 1.35);
  float alpha = (0.05 + wisp * 0.06 + fres * 0.78) * uFade;
  if (alpha < 0.02) discard;
  gl_FragColor = vec4(col, alpha);
}
`;

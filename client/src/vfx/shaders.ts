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
  vec4 tex = texture2D(uMap, vUv);
  float alpha = tex.a * vOpacity;
  if (alpha < 0.012) discard;
  gl_FragColor = vec4(vColor * tex.rgb * alpha, alpha);
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
  gl_FragColor = vec4(vColor * tex.rgb * alpha * 2.6, 1.0);
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
uniform float uTime;
varying vec2 vUv;

void main() {
  vec2 p = vUv * 2.0 - 1.0;
  float ang = atan(p.y, p.x);
  float r = length(p) + sin(ang * 7.0 + uTime * 5.0) * 0.012;
  float width = 0.035 + uWave * 0.02;
  float d = r - uWave;
  float band = exp(-(d * d) / (width * width));
  float alpha = band * uOpacity * (1.0 - smoothstep(0.92, 1.02, r));
  if (alpha < 0.01) discard;
  vec3 col = mix(vec3(0.85, 0.28, 0.08), vec3(1.15, 0.72, 0.38), clamp(band, 0.0, 1.0));
  gl_FragColor = vec4(col * alpha, alpha);
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
  float fres = pow(1.0 - clamp(abs(dot(n, viewDir)), 0.0, 1.0), 1.65);
  vec2 uv = vWorld.xz * 0.45 + vec2(uTime * 0.06, uTime * 0.035);
  float smoke = texture2D(uNoise, uv).a;
  float body = smoothstep(0.15, 0.8, smoke);
  vec3 deep = vec3(0.08, 0.14, 0.24);
  vec3 rim = vec3(0.75, 0.9, 1.35);
  vec3 col = mix(deep, rim, 0.22 + fres * 0.78) * (1.05 + fres * 1.15);
  float alpha = (0.34 + body * 0.22 + fres * 0.4) * uFade;
  if (alpha < 0.02) discard;
  gl_FragColor = vec4(col, alpha);
}
`;

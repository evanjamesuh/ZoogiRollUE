/**
 * Builds the iPhone icon and splash from Wolfgang's drawn portrait
 * (client/src/components/game/ZoogiPortrait.tsx) and the Zoogi Roll name.
 * Run: node script/ios-art.mjs
 */
import { mkdir } from "node:fs/promises";
import { dirname, join } from "node:path";
import sharp from "sharp";

const background = "#12081f";
const font = "Arimo";

/** Wolfgang, in the portrait's 64-unit space. Eyes and the smile are heavier so they survive a 60pt icon. */
function wolfgangMarkup() {
  return `
    <defs>
      <radialGradient id="fur" cx="38%" cy="32%" r="70%">
        <stop offset="0%" stop-color="#d1d5db"/>
        <stop offset="42%" stop-color="#9ca3af"/>
        <stop offset="78%" stop-color="#6b7280"/>
        <stop offset="100%" stop-color="#374151"/>
      </radialGradient>
    </defs>
    <polygon points="14,28 8,8 24,18" fill="#9ca3af"/>
    <polygon points="16.2,22.4 12.4,12.2 20.6,17.4" fill="#4b5563"/>
    <polygon points="50,28 56,8 40,18" fill="#9ca3af"/>
    <polygon points="47.8,22.4 51.6,12.2 43.4,17.4" fill="#4b5563"/>
    <circle cx="32" cy="32" r="30" fill="url(#fur)"/>
    <ellipse cx="24" cy="22" rx="8" ry="5" fill="#ffffff" opacity="0.28"/>
    <circle cx="24" cy="34" r="5.4" fill="#0b0614"/>
    <circle cx="40" cy="34" r="5.4" fill="#0b0614"/>
    <circle cx="25.6" cy="32.4" r="1.7" fill="#ffffff"/>
    <circle cx="41.6" cy="32.4" r="1.7" fill="#ffffff"/>
    <path d="M24 43c4 4.2 12 4.2 16 0" fill="none" stroke="#0b0614" stroke-width="3.2" stroke-linecap="round"/>
  `;
}

function iconSvg() {
  return `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="1024" height="1024" viewBox="0 0 1024 1024">
  <rect width="1024" height="1024" fill="${background}"/>
  <g transform="scale(16)">
    ${wolfgangMarkup()}
  </g>
</svg>`;
}

function splashSvg() {
  return `<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="2732" height="2732" viewBox="0 0 2732 2732">
  <rect width="2732" height="2732" fill="${background}"/>
  <g transform="translate(180 748) scale(18.5)">
    ${wolfgangMarkup()}
  </g>
  <defs>
    <linearGradient id="title" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0%" stop-color="#ffffff"/>
      <stop offset="22%" stop-color="#ffcc88"/>
      <stop offset="48%" stop-color="#ff7700"/>
      <stop offset="100%" stop-color="#cc3300"/>
    </linearGradient>
    <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="10" dy="10" stdDeviation="0" flood-color="#000000" flood-opacity="0.55"/>
    </filter>
  </defs>
  <text x="1560" y="1288" font-family="${font}" font-weight="700" font-size="210" fill="url(#title)" filter="url(#shadow)">Zoogi</text>
  <text x="1560" y="1528" font-family="${font}" font-weight="700" font-size="210" fill="url(#title)" filter="url(#shadow)">Roll</text>
</svg>`;
}

await mkdir("assets", { recursive: true });

const icon = await sharp(Buffer.from(iconSvg()))
  .resize(1024, 1024)
  .flatten({ background })
  .removeAlpha()
  .png()
  .toBuffer();

await sharp(icon).png().toFile("assets/icon.png");

const splash = await sharp(Buffer.from(splashSvg()))
  .resize(2732, 2732)
  .flatten({ background })
  .removeAlpha()
  .png()
  .toBuffer();

await sharp(splash).png().toFile("assets/splash.png");

const iconOut = "ios/App/App/Assets.xcassets/AppIcon.appiconset/AppIcon-512@2x.png";
const splashDir = "ios/App/App/Assets.xcassets/Splash.imageset";
const splashNames = [
  "Default@1x~universal~anyany.png",
  "Default@2x~universal~anyany.png",
  "Default@3x~universal~anyany.png",
  "Default@1x~universal~anyany-dark.png",
  "Default@2x~universal~anyany-dark.png",
  "Default@3x~universal~anyany-dark.png",
];

await mkdir(dirname(iconOut), { recursive: true });
await mkdir(splashDir, { recursive: true });
await sharp(icon).png().toFile(iconOut);
for (const name of splashNames) {
  await sharp(splash).png().toFile(join(splashDir, name));
}

console.log("Wrote assets/icon.png, assets/splash.png, and the iOS icon and splash sizes");

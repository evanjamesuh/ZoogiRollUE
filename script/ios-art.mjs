/**
 * Builds the iPhone icon and splash from Wolfgang's portrait.
 * Needs client/public/portraits/wolfgang.png (git lfs pull).
 * Run: node script/ios-art.mjs
 */
import { access } from "node:fs/promises";
import { mkdir } from "node:fs/promises";
import { dirname, join } from "node:path";
import sharp from "sharp";

const portraitPath = "client/public/portraits/wolfgang.png";
const background = "#12081f";
const font = "Arimo";

try {
  await access(portraitPath);
} catch {
  console.error("Missing Wolfgang's portrait. Run git lfs pull, then try again.");
  process.exit(1);
}

await mkdir("assets", { recursive: true });

const icon = await sharp(portraitPath)
  .resize(1024, 1024, { fit: "cover", position: "centre" })
  .flatten({ background })
  .removeAlpha()
  .png()
  .toBuffer();

await sharp(icon).png().toFile("assets/icon.png");

const face = await sharp(portraitPath)
  .resize(1180, 1180, { fit: "cover", position: "centre" })
  .flatten({ background })
  .removeAlpha()
  .png()
  .toBuffer();

const wordmark = Buffer.from(`<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="2732" height="2732" viewBox="0 0 2732 2732">
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
  <text x="1420" y="1288" font-family="${font}" font-weight="700" font-size="200" fill="url(#title)" filter="url(#shadow)">Zoogi</text>
  <text x="1420" y="1520" font-family="${font}" font-weight="700" font-size="200" fill="url(#title)" filter="url(#shadow)">Roll</text>
</svg>`);

const splash = await sharp({
  create: { width: 2732, height: 2732, channels: 3, background },
})
  .composite([
    { input: face, left: 90, top: 776 },
    { input: wordmark },
  ])
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

console.log("Wrote the iPhone icon and splash from Wolfgang's portrait");

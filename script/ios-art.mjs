/**
 * Builds the iPhone icon and splash from the Zoogi picture already in the repo.
 * Run: node script/ios-art.mjs
 */
import { mkdir } from "node:fs/promises";
import sharp from "sharp";

const source = "client/public/icons/zoogi-512.png";
const background = "#12081f";

await mkdir("assets", { recursive: true });

const iconInner = await sharp(source)
  .resize(860, 860, { fit: "contain", background })
  .png()
  .toBuffer();

await sharp({
  create: { width: 1024, height: 1024, channels: 4, background },
})
  .composite([{ input: iconInner, gravity: "center" }])
  .png()
  .toFile("assets/icon.png");

const splashLogo = await sharp(source)
  .resize(980, 980, { fit: "contain", background })
  .png()
  .toBuffer();

await sharp({
  create: { width: 2732, height: 2732, channels: 4, background },
})
  .composite([{ input: splashLogo, gravity: "center" }])
  .png()
  .toFile("assets/splash.png");

console.log("Wrote assets/icon.png and assets/splash.png");

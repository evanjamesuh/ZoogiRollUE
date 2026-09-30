/**
 * Drops the unused art listed in script/ios-exclude.txt from the iPhone pack.
 * The website build does not run this. Run: node script/trim-ios-bundle.mjs
 */
import { readFile, rm, stat } from "node:fs/promises";
import path from "node:path";

const root = path.resolve("dist/ios");
const listPath = path.resolve("script/ios-exclude.txt");
const text = await readFile(listPath, "utf8");
const paths = text
  .split("\n")
  .map((line) => line.replace(/#.*/, "").trim())
  .filter(Boolean);

let removed = 0;
let bytes = 0;
const missing = [];

for (const rel of paths) {
  if (rel.includes("..") || path.isAbsolute(rel)) {
    throw new Error(`Refusing to omit ${rel}`);
  }
  const full = path.join(root, rel);
  if (!full.startsWith(root + path.sep)) {
    throw new Error(`Refusing to omit ${rel}`);
  }
  try {
    const info = await stat(full);
    await rm(full);
    removed += 1;
    bytes += info.size;
  } catch (error) {
    if (error && error.code === "ENOENT") missing.push(rel);
    else throw error;
  }
}

const mb = (bytes / 1024 / 1024).toFixed(1);
console.log(`iPhone pack omitted ${removed} unused files (${mb} MB).`);
if (missing.length) {
  console.log(`${missing.length} listed files were not in this build.`);
}

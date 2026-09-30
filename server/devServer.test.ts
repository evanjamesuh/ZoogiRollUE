import assert from "node:assert/strict";
import path from "node:path";
import test from "node:test";
import { fileURLToPath } from "node:url";
import viteConfig from "../vite.config.ts";
import { resolveDevViteConfig } from "./devServer.ts";

const repoRoot = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

test("resolveDevViteConfig calls the vite.config function", async () => {
  const config = await resolveDevViteConfig(viteConfig);
  const alias = config.resolve?.alias;
  assert.ok(alias && typeof alias === "object" && !Array.isArray(alias));
  const aliases = alias as Record<string, string>;
  assert.equal(config.root, path.resolve(repoRoot, "client"));
  assert.equal(aliases["@"], path.resolve(repoRoot, "client", "src"));
  assert.equal(aliases["@shared"], path.resolve(repoRoot, "shared"));
  assert.ok(Array.isArray(config.plugins) && config.plugins.length > 0);
});

test("resolveDevViteConfig returns a plain object config unchanged", async () => {
  const plain = {
    root: path.resolve(repoRoot, "client"),
    plugins: [],
  };
  const resolved = await resolveDevViteConfig(plain);
  assert.equal(resolved, plain);
});

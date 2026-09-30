import assert from "node:assert/strict";
import test from "node:test";
import { shouldServeIndexHtml } from "./static.ts";

test("a missing portrait is not served as the game page", () => {
  assert.equal(shouldServeIndexHtml("/portraits/wolfgang.png"), false);
  assert.equal(shouldServeIndexHtml("/icons/zoogi-192.png"), false);
  assert.equal(shouldServeIndexHtml("/"), true);
  assert.equal(shouldServeIndexHtml("/play-login"), true);
});

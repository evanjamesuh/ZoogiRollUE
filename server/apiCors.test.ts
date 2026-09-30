import assert from "node:assert/strict";
import { createServer } from "node:http";
import test from "node:test";
import express from "express";
import { applyApiCors, readCorsOrigins, sessionCookieSameSite } from "./apiCors.ts";

test("cors stays off unless an origin is set", () => {
  assert.deepEqual(readCorsOrigins({}), []);
  assert.deepEqual(readCorsOrigins({ API_CORS_ORIGIN: "  " }), []);
  assert.deepEqual(readCorsOrigins({ API_CORS_ORIGIN: " capacitor://localhost " }), [
    "capacitor://localhost",
  ]);
  assert.deepEqual(
    readCorsOrigins({ API_CORS_ORIGIN: "capacitor://localhost, https://localhost" }),
    ["capacitor://localhost", "https://localhost"],
  );
});

test("login cookies stay lax unless production is opened to the app", () => {
  assert.equal(sessionCookieSameSite({}), "lax");
  assert.equal(sessionCookieSameSite({ NODE_ENV: "production" }), "lax");
  assert.equal(
    sessionCookieSameSite({ NODE_ENV: "development", API_CORS_ORIGIN: "capacitor://localhost" }),
    "lax",
  );
  assert.equal(
    sessionCookieSameSite({ NODE_ENV: "production", API_CORS_ORIGIN: "capacitor://localhost" }),
    "none",
  );
});

function listen(app: express.Express): Promise<{ port: number; close: () => Promise<void> }> {
  const server = createServer(app);
  return new Promise((resolve) => {
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      const port = typeof address === "object" && address ? address.port : 0;
      resolve({
        port,
        close: () => new Promise((done) => server.close(() => done())),
      });
    });
  });
}

test("the api answers the iPhone app and ignores other sites", async () => {
  const app = express();
  applyApiCors(app, ["capacitor://localhost"]);
  app.get("/api/auth/me", (_req, res) => {
    res.json({ user: null });
  });
  app.get("/", (_req, res) => {
    res.type("text/plain").send("game");
  });

  const server = await listen(app);
  try {
    const allowed = await fetch(`http://127.0.0.1:${server.port}/api/auth/me`, {
      headers: { Origin: "capacitor://localhost" },
    });
    assert.equal(allowed.status, 200);
    assert.equal(allowed.headers.get("access-control-allow-origin"), "capacitor://localhost");
    assert.equal(allowed.headers.get("access-control-allow-credentials"), "true");

    const preflight = await fetch(`http://127.0.0.1:${server.port}/api/auth/me`, {
      method: "OPTIONS",
      headers: { Origin: "capacitor://localhost" },
    });
    assert.equal(preflight.status, 204);

    const stranger = await fetch(`http://127.0.0.1:${server.port}/api/auth/me`, {
      headers: { Origin: "https://evil.example" },
    });
    assert.equal(stranger.headers.get("access-control-allow-origin"), null);

    const page = await fetch(`http://127.0.0.1:${server.port}/`, {
      headers: { Origin: "capacitor://localhost" },
    });
    assert.equal(page.headers.get("access-control-allow-origin"), null);
  } finally {
    await server.close();
  }
});

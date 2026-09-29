import assert from "node:assert/strict";
import { createServer } from "node:http";
import test from "node:test";
import express from "express";
import {
  applyPlayGate,
  passwordMatches,
  readPlayPassword,
  requestHasPlayAccess,
} from "./playGate.ts";

function appWithGate(password: string | null) {
  const app = express();
  app.set("trust proxy", 1);
  app.use(express.json());
  app.use(express.urlencoded({ extended: false }));
  applyPlayGate(app, { password, secret: "test-secret" });
  app.get("/", (_req, res) => {
    res.type("text/plain").send("game");
  });
  app.get("/api/leaderboard", (_req, res) => {
    res.json({ ok: true });
  });
  return app;
}

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

test("an empty play password leaves the gate off", () => {
  assert.equal(readPlayPassword({}), null);
  assert.equal(readPlayPassword({ PLAY_PASSWORD: "" }), null);
  assert.equal(readPlayPassword({ PLAY_PASSWORD: "   " }), null);
  assert.equal(readPlayPassword({ PLAY_PASSWORD: " marble " }), "marble");
});

test("password checks do not treat a nearby string as a match", () => {
  assert.equal(passwordMatches("secret", "secret", "test-secret"), true);
  assert.equal(passwordMatches("secret2", "secret", "test-secret"), false);
  assert.equal(passwordMatches("", "secret", "test-secret"), false);
});

test("the game is open when PLAY_PASSWORD is unset", async () => {
  const server = await listen(appWithGate(null));
  try {
    const page = await fetch(`http://127.0.0.1:${server.port}/`, {
      headers: { accept: "text/html" },
    });
    assert.equal(page.status, 200);
    assert.equal(await page.text(), "game");
    const health = await fetch(`http://127.0.0.1:${server.port}/healthz`);
    assert.equal(health.status, 200);
    assert.equal(await health.text(), "ok");
  } finally {
    await server.close();
  }
});

test("a play password blocks the game until the right password is sent", async () => {
  const server = await listen(appWithGate("secret"));
  try {
    const blocked = await fetch(`http://127.0.0.1:${server.port}/`, {
      redirect: "manual",
      headers: { accept: "text/html" },
    });
    assert.equal(blocked.status, 302);
    assert.equal(blocked.headers.get("location"), "/play-login");

    const health = await fetch(`http://127.0.0.1:${server.port}/healthz`);
    assert.equal(health.status, 200);

    const api = await fetch(`http://127.0.0.1:${server.port}/api/leaderboard`, {
      headers: { accept: "application/json" },
    });
    assert.equal(api.status, 401);
    assert.deepEqual(await api.json(), { error: "Play password required" });

    const wrong = await fetch(`http://127.0.0.1:${server.port}/play-login`, {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ password: "nope" }),
    });
    assert.equal(wrong.status, 401);
    assert.equal(wrong.headers.get("set-cookie"), null);
    assert.match(await wrong.text(), /Wrong password/);

    const login = await fetch(`http://127.0.0.1:${server.port}/play-login`, {
      method: "POST",
      redirect: "manual",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ password: "secret" }),
    });
    assert.equal(login.status, 303);
    assert.equal(login.headers.get("location"), "/");
    const setCookie = login.headers.get("set-cookie") || "";
    assert.match(setCookie, /zoogi_play=/);
    assert.match(setCookie, /HttpOnly/i);
    assert.doesNotMatch(setCookie, /Secure/i);

    const cookie = setCookie.split(";")[0];
    const page = await fetch(`http://127.0.0.1:${server.port}/`, {
      headers: { cookie, accept: "text/html" },
    });
    assert.equal(page.status, 200);
    assert.equal(await page.text(), "game");

    const basic = await fetch(`http://127.0.0.1:${server.port}/api/leaderboard`, {
      headers: {
        accept: "application/json",
        authorization: `Basic ${Buffer.from("evan:secret").toString("base64")}`,
      },
    });
    assert.equal(basic.status, 200);
    assert.deepEqual(await basic.json(), { ok: true });

    const secureLogin = await fetch(`http://127.0.0.1:${server.port}/play-login`, {
      method: "POST",
      redirect: "manual",
      headers: {
        "content-type": "application/json",
        "x-forwarded-proto": "https",
      },
      body: JSON.stringify({ password: "secret" }),
    });
    assert.match(secureLogin.headers.get("set-cookie") || "", /Secure/i);
  } finally {
    await server.close();
  }
});

test("websocket upgrades need the same play access", () => {
  assert.equal(requestHasPlayAccess({}, "secret", "test-secret"), false);
  const token = "not-the-token";
  assert.equal(
    requestHasPlayAccess({ cookie: `zoogi_play=${token}` }, "secret", "test-secret"),
    false,
  );
  assert.equal(
    requestHasPlayAccess(
      { authorization: `Basic ${Buffer.from("player:secret").toString("base64")}` },
      "secret",
      "test-secret",
    ),
    true,
  );
});

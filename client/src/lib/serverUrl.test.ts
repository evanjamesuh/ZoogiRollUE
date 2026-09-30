import assert from "node:assert/strict";
import test from "node:test";
import {
  classifyApiRequest,
  effectiveApiBaseUrl,
  initialServerStatus,
  messageForStatus,
  normalizeApiBaseUrl,
  serverEnvFrom,
  voiceChatWebSocketUrl,
} from "./serverUrl.ts";

const web = serverEnvFrom(false, "https://zoogi.example");
const phoneBare = serverEnvFrom(true, "");
const phoneHosted = serverEnvFrom(true, "https://zoogi.example/");

test("a blank server address is ignored, including a trailing slash", () => {
  assert.equal(normalizeApiBaseUrl(undefined), "");
  assert.equal(normalizeApiBaseUrl("  "), "");
  assert.equal(normalizeApiBaseUrl("https://zoogi.example/"), "https://zoogi.example");
});

test("the website ignores a server address and keeps relative URLs", () => {
  assert.equal(effectiveApiBaseUrl(web), "");
  assert.equal(initialServerStatus(web), "web");
  assert.equal(messageForStatus("web"), null);
  assert.deepEqual(classifyApiRequest("/api/leaderboard?limit=10", web), { action: "leave" });
  assert.deepEqual(classifyApiRequest("/sounds/hit.mp3", web), { action: "leave" });
  assert.equal(
    voiceChatWebSocketUrl(web, { protocol: "http:", host: "127.0.0.1:5000" }),
    "ws://127.0.0.1:5000/voice-chat",
  );
  assert.equal(
    voiceChatWebSocketUrl(web, { protocol: "https:", host: "zoogi.example" }),
    "wss://zoogi.example/voice-chat",
  );
});

test("the website leaves absolute URLs alone", () => {
  assert.deepEqual(
    classifyApiRequest("https://discord.com/api/webhooks/123", web),
    { action: "leave" },
  );
  assert.deepEqual(classifyApiRequest("//cdn.example/api/file", phoneHosted), { action: "leave" });
});

test("an iPhone build with no server blocks API calls and explains why", () => {
  assert.equal(initialServerStatus(phoneBare), "unconfigured");
  assert.match(messageForStatus("unconfigured") ?? "", /match on this phone/);
  assert.deepEqual(classifyApiRequest("/api/auth/me", phoneBare), { action: "block" });
  assert.deepEqual(classifyApiRequest("/models/lars.glb", phoneBare), { action: "leave" });
  assert.equal(voiceChatWebSocketUrl(phoneBare, { protocol: "https:", host: "localhost" }), null);
});

test("an iPhone build with a server points API and voice chat at that host", () => {
  assert.equal(initialServerStatus(phoneHosted), "checking");
  assert.equal(messageForStatus("checking"), null);
  assert.deepEqual(classifyApiRequest("/api/leaderboard?limit=10", phoneHosted), {
    action: "rewrite",
    url: "https://zoogi.example/api/leaderboard?limit=10",
  });
  assert.equal(
    voiceChatWebSocketUrl(phoneHosted, { protocol: "capacitor:", host: "localhost" }),
    "wss://zoogi.example/voice-chat",
  );
});

test("a server that does not answer has its own message", () => {
  assert.match(messageForStatus("offline") ?? "", /did not answer/);
  assert.match(messageForStatus("offline") ?? "", /match on this phone/);
});

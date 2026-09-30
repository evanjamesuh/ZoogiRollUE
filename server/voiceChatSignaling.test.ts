import assert from "node:assert/strict";
import { createServer } from "node:http";
import type { AddressInfo } from "node:net";
import test from "node:test";
import { WebSocket, WebSocketServer } from "ws";
import { setupVoiceChatSignaling, VOICE_CHAT_PATH } from "./voiceChatSignaling.ts";

function openSocket(url: string): Promise<WebSocket> {
  return new Promise((resolve, reject) => {
    const ws = new WebSocket(url);
    const timer = setTimeout(() => {
      ws.terminate();
      reject(new Error(`timed out opening ${url}`));
    }, 5000);
    ws.once("open", () => {
      clearTimeout(timer);
      resolve(ws);
    });
    ws.once("error", (error) => {
      clearTimeout(timer);
      reject(error);
    });
  });
}

function closeSocket(ws: WebSocket): Promise<void> {
  if (ws.readyState === WebSocket.CLOSED) return Promise.resolve();
  return new Promise((resolve) => {
    ws.once("close", () => resolve());
    ws.close();
  });
}

test("voice chat upgrade leaves other websocket paths open", async () => {
  const server = createServer();
  setupVoiceChatSignaling(server);

  const other = new WebSocketServer({ noServer: true });
  server.on("upgrade", (req, socket, head) => {
    const pathname = (req.url ?? "").split("?")[0];
    if (pathname !== "/other-ws") return;
    other.handleUpgrade(req, socket, head, (ws) => other.emit("connection", ws, req));
  });

  await new Promise<void>((resolve) => {
    server.listen(0, "127.0.0.1", () => resolve());
  });
  const { port } = server.address() as AddressInfo;

  const otherWs = await openSocket(`ws://127.0.0.1:${port}/other-ws`);
  const voiceWs = await openSocket(`ws://127.0.0.1:${port}${VOICE_CHAT_PATH}`);
  assert.equal(otherWs.readyState, WebSocket.OPEN);
  assert.equal(voiceWs.readyState, WebSocket.OPEN);

  await closeSocket(otherWs);
  await closeSocket(voiceWs);
  await new Promise<void>((resolve, reject) => {
    other.close((error) => (error ? reject(error) : resolve()));
  });
  await new Promise<void>((resolve, reject) => {
    server.close((error) => (error ? reject(error) : resolve()));
  });
});

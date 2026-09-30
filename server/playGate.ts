import { createHmac, timingSafeEqual } from "node:crypto";
import type { IncomingHttpHeaders, Server as HttpServer } from "node:http";
import type { Duplex } from "node:stream";
import type { Express, Request, Response } from "express";

export const PLAY_COOKIE = "zoogi_play";

export interface PlayGateOptions {
  password: string | null;
  secret: string;
}

export function readPlayPassword(env: NodeJS.ProcessEnv = process.env): string | null {
  const raw = env.PLAY_PASSWORD;
  if (typeof raw !== "string") return null;
  const trimmed = raw.trim();
  return trimmed.length > 0 ? trimmed : null;
}

export function playGateToken(password: string, secret: string): string {
  return createHmac("sha256", secret).update(`zoogi-play-v1\n${password}`).digest("base64url");
}

export function safeEqual(a: string, b: string): boolean {
  const left = Buffer.from(a);
  const right = Buffer.from(b);
  if (left.length !== right.length) return false;
  return timingSafeEqual(left, right);
}

export function passwordMatches(submitted: string, expected: string, secret: string): boolean {
  return safeEqual(playGateToken(submitted, secret), playGateToken(expected, secret));
}

export function readCookie(header: string | undefined, name: string): string | null {
  if (!header) return null;
  for (const part of header.split(";")) {
    const separator = part.indexOf("=");
    if (separator === -1) continue;
    const key = part.slice(0, separator).trim();
    if (key !== name) continue;
    const value = part.slice(separator + 1).trim();
    try {
      return decodeURIComponent(value);
    } catch {
      return null;
    }
  }
  return null;
}

export function basicPassword(authorization: string | undefined): string | null {
  if (!authorization) return null;
  const match = /^Basic\s+(.+)$/i.exec(authorization.trim());
  if (!match) return null;
  try {
    const decoded = Buffer.from(match[1], "base64").toString("utf8");
    const separator = decoded.indexOf(":");
    if (separator === -1) return null;
    return decoded.slice(separator + 1);
  } catch {
    return null;
  }
}

export function requestHasPlayAccess(
  headers: IncomingHttpHeaders,
  password: string,
  secret: string,
): boolean {
  const cookie = readCookie(headers.cookie, PLAY_COOKIE);
  const expected = playGateToken(password, secret);
  if (cookie && safeEqual(cookie, expected)) return true;
  const basic = basicPassword(headers.authorization);
  return basic !== null && passwordMatches(basic, password, secret);
}

function escapeHtml(value: string): string {
  return value
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

export function playLoginHtml(error?: string): string {
  const notice = error
    ? `<p class="err">${escapeHtml(error)}</p>`
    : `<p class="hint">This game is private. Enter the play password to continue.</p>`;
  return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1, minimum-scale=1, maximum-scale=1, user-scalable=no, viewport-fit=cover" />
  <title>Zoogi Roll</title>
  <style>
    :root { color-scheme: dark; }
    * { box-sizing: border-box; }
    html, body { margin: 0; height: 100%; background: #12081f; color: #fff; font-family: Inter, Segoe UI, sans-serif; }
    body { display: flex; align-items: center; justify-content: center; padding: 24px; }
    form { width: min(100%, 420px); background: #1c1030; border: 1px solid #6d28d9; border-radius: 20px; padding: 28px 22px; }
    h1 { margin: 0 0 8px; font-size: 28px; }
    .hint, .err { margin: 0 0 18px; line-height: 1.4; }
    .hint { color: #d8b4fe; }
    .err { color: #fecaca; }
    label { display: block; margin-bottom: 8px; font-size: 14px; color: #e9d5ff; }
    input { width: 100%; min-height: 48px; font-size: 16px; border-radius: 12px; border: 1px solid #7c3aed; background: #0b0614; color: white; padding: 0 14px; }
    button { width: 100%; min-height: 52px; margin-top: 16px; border: 0; border-radius: 999px; font-size: 18px; font-weight: 700; background: linear-gradient(90deg, #facc15, #f97316); color: #111; }
  </style>
</head>
<body>
  <form method="post" action="/play-login">
    <h1>Zoogi Roll</h1>
    ${notice}
    <label for="password">Play password</label>
    <input id="password" name="password" type="password" autocomplete="current-password" required />
    <button type="submit">Enter</button>
  </form>
</body>
</html>`;
}

function submittedPassword(req: Request): string {
  const body = req.body as { password?: unknown } | undefined;
  return typeof body?.password === "string" ? body.password : "";
}

function requestIsHttps(req: Request): boolean {
  if (req.secure) return true;
  const proto = req.headers["x-forwarded-proto"];
  const first = Array.isArray(proto) ? proto[0] : proto;
  return typeof first === "string" && first.split(",")[0].trim() === "https";
}

function wantsHtml(req: Request): boolean {
  if (req.path.startsWith("/api")) return false;
  if (req.method !== "GET" && req.method !== "HEAD") return false;
  const accept = req.get("accept") || "";
  if (accept.includes("application/json")) return false;
  return accept.includes("text/html") || accept.includes("*/*") || accept === "";
}

export function applyPlayGate(app: Express, options: PlayGateOptions): void {
  app.use((req, res, next) => {
    if (req.path === "/healthz" && (req.method === "GET" || req.method === "HEAD")) {
      res.status(200).type("text/plain").send("ok");
      return;
    }
    if (!options.password) {
      next();
      return;
    }

    if (req.path === "/play-login" && req.method === "GET") {
      res.status(200).type("html").send(playLoginHtml());
      return;
    }
    if (req.path === "/play-login" && req.method === "POST") {
      handleLogin(req, res, options.password, options.secret);
      return;
    }
    if (requestHasPlayAccess(req.headers, options.password, options.secret)) {
      next();
      return;
    }
    if (wantsHtml(req)) {
      res.redirect(302, "/play-login");
      return;
    }
    res.status(401).json({ error: "Play password required" });
  });
}

function handleLogin(req: Request, res: Response, password: string, secret: string): void {
  const submitted = submittedPassword(req);
  if (!passwordMatches(submitted, password, secret)) {
    res.status(401).type("html").send(playLoginHtml("Wrong password."));
    return;
  }
  const token = playGateToken(password, secret);
  res.cookie(PLAY_COOKIE, token, {
    httpOnly: true,
    sameSite: "lax",
    secure: requestIsHttps(req),
    path: "/",
    maxAge: 1000 * 60 * 60 * 24 * 30,
  });
  res.redirect(303, "/");
}

export function attachPlayGateUpgradeGuard(server: HttpServer, options: PlayGateOptions): void {
  if (!options.password) return;
  const password = options.password;
  server.prependListener("upgrade", (req, socket) => {
    if (requestHasPlayAccess(req.headers, password, options.secret)) return;
    const stream = socket as Duplex;
    stream.write("HTTP/1.1 401 Unauthorized\r\nConnection: close\r\n\r\n");
    stream.destroy();
  });
}

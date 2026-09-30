/**
 * Where the iPhone app sends accounts, scores, and other server calls.
 *
 * The website keeps using relative /api and websocket URLs. Only the iOS
 * build (VITE_NATIVE_APP=true) reads VITE_API_BASE_URL.
 */

export interface ServerEnv {
  /** True for the bundled iPhone app. False for the website and `npm run dev`. */
  nativeApp: boolean;
  /** Example: https://zoogi-roll.onrender.com  No trailing slash. */
  apiBaseUrl?: string;
}

export type ServerStatus = "web" | "checking" | "online" | "unconfigured" | "offline";

export const UNCONFIGURED_MESSAGE =
  "This phone does not have a game server saved yet. You can still play a match on this phone. Scores, accounts, and playing with people on the internet turn on after a server address is added.";

export const OFFLINE_MESSAGE =
  "The game server did not answer. You can still play a match on this phone. Check your internet connection, then try again.";

export function normalizeApiBaseUrl(raw: string | undefined | null): string {
  return (raw ?? "").trim().replace(/\/+$/, "");
}

export function serverEnvFrom(nativeApp: boolean, apiBaseUrl: string | undefined | null): ServerEnv {
  return {
    nativeApp,
    apiBaseUrl: normalizeApiBaseUrl(apiBaseUrl),
  };
}

/** Website builds ignore a server address so relative URLs stay as they are today. */
export function effectiveApiBaseUrl(env: ServerEnv): string {
  if (!env.nativeApp) return "";
  return normalizeApiBaseUrl(env.apiBaseUrl);
}

export function initialServerStatus(env: ServerEnv): ServerStatus {
  if (!env.nativeApp) return "web";
  return effectiveApiBaseUrl(env) ? "checking" : "unconfigured";
}

export function messageForStatus(status: ServerStatus): string | null {
  if (status === "unconfigured") return UNCONFIGURED_MESSAGE;
  if (status === "offline") return OFFLINE_MESSAGE;
  return null;
}

export function isApiPath(pathname: string): boolean {
  return pathname === "/api" || pathname.startsWith("/api/");
}

export type ApiRoute =
  | { action: "leave" }
  | { action: "block" }
  | { action: "rewrite"; url: string };

/**
 * Decide what to do with one fetch URL.
 * Absolute URLs (Discord, model CDNs, already-full server URLs) are left alone.
 * Relative /api URLs on the website are left alone.
 */
export function classifyApiRequest(rawUrl: string, env: ServerEnv): ApiRoute {
  if (!rawUrl.startsWith("/") || rawUrl.startsWith("//")) {
    return { action: "leave" };
  }

  let pathname = rawUrl;
  try {
    pathname = new URL(rawUrl, "http://local.invalid").pathname;
  } catch {
    return { action: "leave" };
  }
  if (!isApiPath(pathname)) return { action: "leave" };

  const base = effectiveApiBaseUrl(env);
  if (env.nativeApp && !base) return { action: "block" };
  if (env.nativeApp && base) return { action: "rewrite", url: `${base}${rawUrl}` };
  return { action: "leave" };
}

/** Same string the website builds today when this is not the iPhone app. */
export function voiceChatWebSocketUrl(
  env: ServerEnv,
  page: { protocol: string; host: string },
): string | null {
  const path = "/voice-chat";
  const base = effectiveApiBaseUrl(env);
  if (env.nativeApp && !base) return null;
  if (base) {
    const wsBase = base.replace(/^https:/i, "wss:").replace(/^http:/i, "ws:");
    return `${wsBase}${path}`;
  }
  const protocol = page.protocol === "https:" ? "wss:" : "ws:";
  return `${protocol}//${page.host}${path}`;
}

export function offlineResponseBody(message: string) {
  return {
    error: message,
    offline: true,
    user: null,
    data: [] as unknown[],
    messages: [] as unknown[],
    friendships: [] as unknown[],
    active: [] as unknown[],
    upcoming: [] as unknown[],
  };
}

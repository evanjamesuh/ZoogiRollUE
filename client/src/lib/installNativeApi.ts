import {
  classifyApiRequest,
  effectiveApiBaseUrl,
  offlineResponseBody,
  messageForStatus,
  type ServerEnv,
} from "./serverUrl";
import { getOnlineMessage, getServerEnv, setServerStatus } from "./serverStatus";

let patched = false;

const nativeEnv: ServerEnv = getServerEnv();

function offlineResponse(): Response {
  const message = getOnlineMessage() ?? messageForStatus("offline") ?? "The game server did not answer.";
  return new Response(JSON.stringify(offlineResponseBody(message)), {
    status: 503,
    headers: { "Content-Type": "application/json" },
  });
}

function requestUrl(input: RequestInfo | URL): string {
  if (typeof input === "string") return input;
  if (input instanceof URL) return input.toString();
  return input.url;
}

export function installNativeApi() {
  if (patched || typeof window === "undefined") return;
  patched = true;

  const originalFetch = window.fetch.bind(window);

  const zoogiFetch: typeof window.fetch = async (input, init) => {
    const route = classifyApiRequest(requestUrl(input), nativeEnv);
    if (route.action === "leave") return originalFetch(input, init);
    if (route.action === "block") return offlineResponse();

    try {
      const response = input instanceof Request
        ? await originalFetch(new Request(route.url, input))
        : await originalFetch(route.url, init);
      setServerStatus("online");
      return response;
    } catch (error) {
      console.warn("Game server did not answer.", error);
      setServerStatus("offline");
      return offlineResponse();
    }
  };

  window.fetch = zoogiFetch;

  if (!nativeEnv.nativeApp) return;

  void hideNativeChrome();
  if (effectiveApiBaseUrl(nativeEnv)) void probeServer(originalFetch);
}

async function probeServer(originalFetch: typeof window.fetch) {
  const base = effectiveApiBaseUrl(nativeEnv);
  const controller = new AbortController();
  const timer = window.setTimeout(() => controller.abort(), 4000);
  try {
    await originalFetch(`${base}/api/auth/me`, {
      credentials: "include",
      signal: controller.signal,
    });
    setServerStatus("online");
  } catch {
    setServerStatus("offline");
  } finally {
    window.clearTimeout(timer);
  }
}

async function hideNativeChrome() {
  try {
    const { Capacitor } = await import("@capacitor/core");
    if (!Capacitor.isNativePlatform()) return;
    const { StatusBar } = await import("@capacitor/status-bar");
    await StatusBar.hide();
    const { SplashScreen } = await import("@capacitor/splash-screen");
    await SplashScreen.hide();
  } catch (error) {
    console.warn("Native chrome stayed as the system left it.", error);
  }
}

import type { Server } from "node:http";
import type { UserConfig, UserConfigExport } from "vite";

export const VITE_HMR_PATH = "/vite-hmr";

/**
 * Vite middleware options for the dev server.
 * `hmr.host` is left unset on purpose. Vite then tells the browser to open
 * the websocket to the same hostname that loaded the page, so a phone on
 * the LAN does not try to reach 127.0.0.1 or 0.0.0.0.
 */
export function viteMiddlewareOptions(httpServer: Server, port: number) {
  return {
    middlewareMode: true as const,
    allowedHosts: true as const,
    hmr: {
      server: httpServer,
      path: VITE_HMR_PATH,
      clientPort: port,
    },
  };
}

/**
 * vite.config.ts exports `defineConfig(({ mode }) => ({ ... }))`, a function.
 * The dev server passes `configFile: false`, so it must call that function
 * itself. Spreading the function would drop root, plugins, and aliases.
 */
export async function resolveDevViteConfig(config: UserConfigExport): Promise<UserConfig> {
  return typeof config === "function"
    ? await config({ command: "serve", mode: "development" })
    : await config;
}

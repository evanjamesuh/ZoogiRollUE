import { type Express } from "express";
import { createServer as createViteServer, createLogger } from "vite";
import { type Server } from "http";
import viteConfig from "../vite.config";
import fs from "fs";
import path from "path";
import { nanoid } from "nanoid";
import { viteMiddlewareOptions, resolveDevViteConfig } from "./devServer";
import { shouldServeIndexHtml } from "./static";

const viteLogger = createLogger();

export async function setupVite(server: Server, app: Express, port: number) {
  const serverOptions = viteMiddlewareOptions(server, port);

  const vite = await createViteServer({
    ...(await resolveDevViteConfig(viteConfig)),
    configFile: false,
    customLogger: {
      ...viteLogger,
      error: (msg, options) => {
        viteLogger.error(msg, options);
        process.exit(1);
      },
    },
    server: serverOptions,
    appType: "custom",
  });

  app.use(vite.middlewares);

  const MISSING_ASSET_PREFIXES = ["/models", "/sounds", "/videos", "/textures"];

  app.use("*", async (req, res, next) => {
    const url = req.originalUrl;
    const pathname = url.split("?")[0];
    const isAssetRequest =
      !shouldServeIndexHtml(pathname) ||
      MISSING_ASSET_PREFIXES.some(
        (prefix) => pathname === prefix || pathname.startsWith(`${prefix}/`)
      );
    if (isAssetRequest) {
      res.status(404).type("text/plain").send("Not found");
      return;
    }

    try {
      const clientTemplate = path.resolve(
        import.meta.dirname,
        "..",
        "client",
        "index.html",
      );

      // always reload the index.html file from disk incase it changes
      let template = await fs.promises.readFile(clientTemplate, "utf-8");
      template = template.replace(
        `src="/src/main.tsx"`,
        `src="/src/main.tsx?v=${nanoid()}"`,
      );
      const page = await vite.transformIndexHtml(url, template);
      res.status(200).set({ "Content-Type": "text/html" }).end(page);
    } catch (e) {
      vite.ssrFixStacktrace(e as Error);
      next(e);
    }
  });
}

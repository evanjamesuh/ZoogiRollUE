import type { Express, Request, Response, NextFunction } from "express";

export function readCorsOrigins(env: NodeJS.ProcessEnv = process.env): string[] {
  const raw = env.API_CORS_ORIGIN?.trim() ?? "";
  if (!raw) return [];
  return raw.split(",").map((origin) => origin.trim()).filter(Boolean);
}

/** Same-site lax cookies unless a hosted server is explicitly opened to the app. */
export function sessionCookieSameSite(env: {
  NODE_ENV?: string;
  API_CORS_ORIGIN?: string;
}): "lax" | "none" {
  const origins = readCorsOrigins(env as NodeJS.ProcessEnv);
  if (origins.length > 0 && env.NODE_ENV === "production") return "none";
  return "lax";
}

export function applyApiCors(app: Express, origins: string[]) {
  if (origins.length === 0) return;

  app.use("/api", (req: Request, res: Response, next: NextFunction) => {
    const requestOrigin = req.header("origin");
    if (requestOrigin && origins.includes(requestOrigin)) {
      res.setHeader("Access-Control-Allow-Origin", requestOrigin);
      res.setHeader("Access-Control-Allow-Credentials", "true");
      res.setHeader("Vary", "Origin");
      res.setHeader("Access-Control-Allow-Headers", "Content-Type");
      res.setHeader("Access-Control-Allow-Methods", "GET,POST,PUT,PATCH,DELETE,OPTIONS");
    }
    if (req.method === "OPTIONS") {
      res.sendStatus(204);
      return;
    }
    next();
  });
}

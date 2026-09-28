import express, { type Request, Response, NextFunction } from "express";
import session from "express-session";
import connectPgSimple from "connect-pg-simple";
import pg from "pg";
import { registerRoutes } from "./routes";
import { serveStatic } from "./static";
import { createServer } from "http";
import { execSync } from "child_process";
import { storage } from "./storage";

const app = express();
const httpServer = createServer(app);

const THIRTY_DAYS_MS = 30 * 24 * 60 * 60 * 1000;

function createSessionStore() {
  if (process.env.DATABASE_URL) {
    const PgSession = connectPgSimple(session);
    const pgPool = new pg.Pool({
      connectionString: process.env.DATABASE_URL,
    });
    console.log("Using PostgreSQL session store for persistent sessions");
    return new PgSession({
      pool: pgPool,
      tableName: 'user_sessions',
      createTableIfMissing: true,
    });
  } else {
    console.log("No DATABASE_URL - using in-memory session store (sessions will not persist across restarts)");
    return undefined;
  }
}

app.use(
  session({
    store: createSessionStore(),
    secret: process.env.SESSION_SECRET || "zoogi-roll-dev-secret-change-in-prod",
    resave: false,
    saveUninitialized: false,
    rolling: true,
    cookie: {
      secure: process.env.NODE_ENV === "production",
      httpOnly: true,
      maxAge: THIRTY_DAYS_MS,
      sameSite: 'lax',
    },
  })
);

app.use(express.json({ limit: '50mb' }));
app.use(express.urlencoded({ extended: false, limit: '50mb' }));

export function log(message: string, source = "express") {
  const formattedTime = new Date().toLocaleTimeString("en-US", {
    hour: "numeric",
    minute: "2-digit",
    second: "2-digit",
    hour12: true,
  });

  console.log(`${formattedTime} [${source}] ${message}`);
}

app.use((req, res, next) => {
  const start = Date.now();
  const path = req.path;
  let capturedJsonResponse: Record<string, any> | undefined = undefined;

  const originalResJson = res.json;
  res.json = function (bodyJson, ...args) {
    capturedJsonResponse = bodyJson;
    return originalResJson.apply(res, [bodyJson, ...args]);
  };

  res.on("finish", () => {
    const duration = Date.now() - start;
    if (path.startsWith("/api")) {
      let logLine = `${req.method} ${path} ${res.statusCode} in ${duration}ms`;
      if (capturedJsonResponse) {
        logLine += ` :: ${JSON.stringify(capturedJsonResponse)}`;
      }

      log(logLine);
    }
  });

  next();
});

(async () => {
  await registerRoutes(httpServer, app);

  app.use((err: any, _req: Request, res: Response, _next: NextFunction) => {
    const status = err.status || err.statusCode || 500;
    const message = err.message || "Internal Server Error";

    res.status(status).json({ message });
    throw err;
  });

  // importantly only setup vite in development and after
  // setting up all the other routes so the catch-all route
  // doesn't interfere with the other routes
  if (process.env.NODE_ENV === "production") {
    serveStatic(app);
  } else {
    const { setupVite } = await import("./vite");
    await setupVite(httpServer, app);
  }

  // ALWAYS serve the app on the port specified in the environment variable PORT
  // Other ports are firewalled. Default to 5000 if not specified.
  // this serves both the API and the client.
  // It is the only port that is not firewalled.
  const port = parseInt(process.env.PORT || "5000", 10);

  // Proactively kill any process holding this port before we try to listen
  try {
    const portHex = port.toString(16).toUpperCase().padStart(4, "0");
    const tcpData = execSync(`cat /proc/net/tcp 2>/dev/null || true`, { encoding: "utf8" });
    const match = tcpData.split("\n").find((line) => line.includes(`:${portHex}`));
    if (match) {
      const inode = match.trim().split(/\s+/)[9];
      if (inode) {
        const fdSearch = execSync(
          `grep -rl "socket:\\[${inode}\\]" /proc/[0-9]*/fd 2>/dev/null | grep -oP '/proc/\\K[0-9]+' | head -1 || true`,
          { encoding: "utf8", shell: "/bin/sh" }
        ).trim();
        if (fdSearch) {
          log(`Killing PID ${fdSearch} holding port ${port}`);
          execSync(`kill -9 ${fdSearch} 2>/dev/null || true`, { shell: "/bin/sh" });
          // Wait for the port to be released
          execSync("sleep 1");
        }
      }
    }
  } catch (_) {}

  httpServer.listen({ port, host: "0.0.0.0" }, () => {
    log(`serving on port ${port}`);
  });
})();

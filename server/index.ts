import "./env";
import express, { type Request, Response, NextFunction } from "express";
import session from "express-session";
import connectPgSimple from "connect-pg-simple";
import { databaseConfigured, pool } from "./db";
import { registerRoutes } from "./routes";
import { serveStatic } from "./static";
import { createServer } from "http";
import { listenLogLines, resolveListenHost, resolveListenPort } from "./listen";
import { applyPlayGate, attachPlayGateUpgradeGuard, readPlayPassword } from "./playGate";

const app = express();
app.set("trust proxy", 1);
const httpServer = createServer(app);

const THIRTY_DAYS_MS = 30 * 24 * 60 * 60 * 1000;

const DATABASE_UNAVAILABLE_MESSAGE =
  "Database is not available. Set DATABASE_URL in .env and start Postgres to use accounts, leaderboards, and saved data.";

function createSessionStore() {
  if (databaseConfigured && pool) {
    const PgSession = connectPgSimple(session);
    console.log("Using PostgreSQL session store for persistent sessions");
    return new PgSession({
      pool,
      tableName: "user_sessions",
      createTableIfMissing: true,
    });
  }

  console.log("Using in-memory session store (sessions reset when the server stops)");
  return new session.MemoryStore();
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
      sameSite: "lax",
    },
  })
);

app.use(express.json({ limit: "50mb" }));
app.use(express.urlencoded({ extended: false, limit: "50mb" }));

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
  const playPassword = readPlayPassword();
  const playGate = {
    password: playPassword,
    secret: process.env.SESSION_SECRET || "zoogi-roll-dev-secret-change-in-prod",
  };
  applyPlayGate(app, playGate);
  attachPlayGateUpgradeGuard(httpServer, playGate);
  if (playPassword) {
    log("Play password is on. Visitors must enter it before the game loads.");
  }

  if (!databaseConfigured) {
    app.use("/api", (_req, res) => {
      res.status(503).json({ error: DATABASE_UNAVAILABLE_MESSAGE });
    });
  }

  await registerRoutes(httpServer, app);

  app.use((err: any, _req: Request, res: Response, _next: NextFunction) => {
    const status = err.status || err.statusCode || 500;
    const message = err.message || "Internal Server Error";

    res.status(status).json({ message });
    throw err;
  });

  const port = resolveListenPort();
  const host = resolveListenHost();

  if (process.env.NODE_ENV === "production") {
    serveStatic(app);
  } else {
    const { setupVite } = await import("./vite");
    await setupVite(httpServer, app, port);
  }

  httpServer.on("error", (err: NodeJS.ErrnoException) => {
    if (err.code === "EADDRINUSE") {
      log(`Port ${port} is already in use. Stop the other program using that port, or set PORT in .env to a free port.`);
      process.exit(1);
    }
    console.error(err);
    process.exit(1);
  });

  httpServer.listen({ port, host }, () => {
    for (const line of listenLogLines(host, port)) {
      log(line);
    }
  });
})();

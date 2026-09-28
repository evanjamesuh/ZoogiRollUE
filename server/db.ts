import "./env";
import { drizzle } from "drizzle-orm/node-postgres";
import pg from "pg";
import * as schema from "@shared/schema";

const connectionString = process.env.DATABASE_URL;

export const databaseConfigured = Boolean(connectionString);

export const pool = connectionString
  ? new pg.Pool({ connectionString })
  : null;

const realDb = pool ? drizzle(pool, { schema }) : null;

type Database = NonNullable<typeof realDb>;

if (!databaseConfigured) {
  console.warn(
    "[zoogi] DATABASE_URL is not set. The game will still start, but accounts, leaderboards, and saved data are unavailable. Copy .env.example to .env, start Postgres, and run npm run db:push."
  );
}

export const db: Database = new Proxy({} as Database, {
  get(_target, prop, _receiver) {
    if (!realDb) {
      throw new Error(
        "Database is not available. Set DATABASE_URL in .env and start Postgres."
      );
    }
    const value = Reflect.get(realDb, prop, realDb);
    return typeof value === "function" ? value.bind(realDb) : value;
  },
});

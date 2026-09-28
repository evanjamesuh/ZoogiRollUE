import "./server/env";
import { defineConfig } from "drizzle-kit";

if (!process.env.DATABASE_URL) {
  throw new Error(
    "DATABASE_URL is required. Copy .env.example to .env and set it to your local Postgres URL."
  );
}

export default defineConfig({
  out: "./migrations",
  schema: "./shared/schema.ts",
  dialect: "postgresql",
  dbCredentials: {
    url: process.env.DATABASE_URL,
  },
});

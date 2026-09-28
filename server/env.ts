import fs from "node:fs";
import path from "node:path";
import { loadEnvFile } from "node:process";

const envPath = path.resolve(process.cwd(), ".env");

if (fs.existsSync(envPath)) {
  loadEnvFile(envPath);
}

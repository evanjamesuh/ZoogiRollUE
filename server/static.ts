import express, { type Express } from "express";
import fs from "fs";
import path from "path";

const FILE_ASSET = /\.(png|jpe?g|gif|webp|svg|ico|glb|gltf|mp3|ogg|wav|mp4|webm|woff2?)$/i;

/** Missing pictures must 404. Falling through to index.html makes the browser paint alt text. */
export function shouldServeIndexHtml(pathname: string): boolean {
  return !FILE_ASSET.test(pathname);
}

export function serveStatic(app: Express) {
  const distPath = path.resolve(__dirname, "public");
  if (!fs.existsSync(distPath)) {
    throw new Error(
      `Could not find the build directory: ${distPath}, make sure to build the client first`,
    );
  }

  app.use(express.static(distPath));

  // fall through to index.html if the file doesn't exist
  app.use("*", (req, res) => {
    const pathname = req.originalUrl.split("?")[0];
    if (!shouldServeIndexHtml(pathname)) {
      res.status(404).type("text/plain").send("Not found");
      return;
    }
    res.sendFile(path.resolve(distPath, "index.html"));
  });
}

import os from "node:os";

export const DEFAULT_HOST = "127.0.0.1";
export const DEFAULT_PORT = 5000;

type InterfaceMap = ReturnType<typeof os.networkInterfaces>;

export function resolveListenHost(env: NodeJS.ProcessEnv = process.env): string {
  const host = env.HOST?.trim();
  return host ? host : DEFAULT_HOST;
}

export function resolveListenPort(env: NodeJS.ProcessEnv = process.env): number {
  const raw = env.PORT?.trim();
  if (!raw) return DEFAULT_PORT;
  const port = Number.parseInt(raw, 10);
  if (!Number.isInteger(port) || port <= 0 || port > 65535) return DEFAULT_PORT;
  return port;
}

export function isWildcardHost(host: string): boolean {
  return host === "0.0.0.0" || host === "::" || host === "::0";
}

export function isLoopbackHost(host: string): boolean {
  return host === "127.0.0.1" || host === "localhost" || host === "::1";
}

function isIpv4(family: string | number): boolean {
  return family === "IPv4" || family === 4;
}

function isLinkLocal(address: string): boolean {
  return address.startsWith("169.254.");
}

function lanRank(address: string): number {
  if (address.startsWith("192.168.")) return 0;
  if (address.startsWith("10.")) return 1;
  const match = /^172\.(\d+)\./.exec(address);
  if (match) {
    const second = Number(match[1]);
    if (second >= 16 && second <= 31) return 2;
  }
  return 3;
}

export function lanHttpUrls(port: number, interfaces: InterfaceMap = os.networkInterfaces()): string[] {
  const addresses: string[] = [];
  for (const entries of Object.values(interfaces)) {
    for (const entry of entries ?? []) {
      if (!isIpv4(entry.family) || entry.internal || isLinkLocal(entry.address)) continue;
      addresses.push(entry.address);
    }
  }
  const unique = [...new Set(addresses)];
  unique.sort((a, b) => lanRank(a) - lanRank(b) || a.localeCompare(b, undefined, { numeric: true }));
  return unique.map((address) => `http://${address}:${port}`);
}

function httpUrl(host: string, port: number): string {
  if (host.includes(":") && !host.startsWith("[")) return `http://[${host}]:${port}`;
  return `http://${host}:${port}`;
}

export function listenLogLines(host: string, port: number, interfaces?: InterfaceMap): string[] {
  if (isWildcardHost(host)) {
    const lines = [`serving on http://127.0.0.1:${port}`];
    const urls = lanHttpUrls(port, interfaces);
    if (urls.length === 0) {
      lines.push("LAN mode: no Wi-Fi address found. Use this PC's IPv4 address with this port on your phone.");
      return lines;
    }
    for (const url of urls) {
      lines.push(`Phone on this Wi-Fi: ${url}`);
    }
    return lines;
  }

  const url = httpUrl(host, port);
  if (isLoopbackHost(host)) return [`serving on ${url}`];
  return [`serving on ${url}`, `Phone on this Wi-Fi: ${url}`];
}

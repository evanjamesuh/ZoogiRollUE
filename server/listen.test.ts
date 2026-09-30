import assert from "node:assert/strict";
import test from "node:test";
import { viteMiddlewareOptions, VITE_HMR_PATH } from "./devServer.ts";
import {
  isWildcardHost,
  lanHttpUrls,
  listenLogLines,
  resolveListenHost,
  resolveListenPort,
} from "./listen.ts";

const iface = (address: string, family: string, internal = false) => ({
  address,
  family,
  internal,
  netmask: "255.255.255.0",
  mac: "00:00:00:00:00:00",
  cidr: null,
});

test("host defaults to localhost", () => {
  assert.equal(resolveListenHost({}), "127.0.0.1");
  assert.equal(resolveListenHost({ HOST: "" }), "127.0.0.1");
  assert.equal(resolveListenHost({ HOST: "   " }), "127.0.0.1");
});

test("HOST opts into a bind address", () => {
  assert.equal(resolveListenHost({ HOST: "0.0.0.0" }), "0.0.0.0");
  assert.equal(resolveListenHost({ HOST: " 192.168.1.20 " }), "192.168.1.20");
  assert.equal(isWildcardHost("0.0.0.0"), true);
  assert.equal(isWildcardHost("::"), true);
  assert.equal(isWildcardHost("127.0.0.1"), false);
});

test("port defaults and ignores junk", () => {
  assert.equal(resolveListenPort({}), 5000);
  assert.equal(resolveListenPort({ PORT: "8080" }), 8080);
  assert.equal(resolveListenPort({ PORT: "nope" }), 5000);
  assert.equal(resolveListenPort({ PORT: "0" }), 5000);
});

test("LAN urls prefer Wi-Fi addresses and skip internal adapters", () => {
  const urls = lanHttpUrls(5000, {
    lo: [iface("127.0.0.1", "IPv4", true)],
    vEthernet: [iface("172.22.0.1", "IPv4")],
    WiFi: [iface("192.168.1.42", "IPv4")],
    linkLocal: [iface("169.254.12.8", "IPv4")],
    v6: [iface("fe80::1", "IPv6")],
  });
  assert.deepEqual(urls, ["http://192.168.1.42:5000", "http://172.22.0.1:5000"]);
});

test("localhost startup does not print a phone url", () => {
  assert.deepEqual(listenLogLines("127.0.0.1", 5000, {}), ["serving on http://127.0.0.1:5000"]);
});

test("LAN mode prints phone urls", () => {
  const lines = listenLogLines("0.0.0.0", 5000, {
    WiFi: [iface("192.168.0.8", "IPv4")],
  });
  assert.deepEqual(lines, [
    "serving on http://127.0.0.1:5000",
    "Phone on this Wi-Fi: http://192.168.0.8:5000",
  ]);
});

test("LAN mode explains when no address is found", () => {
  const lines = listenLogLines("0.0.0.0", 5000, {
    lo: [iface("127.0.0.1", "IPv4", true)],
  });
  assert.equal(lines[0], "serving on http://127.0.0.1:5000");
  assert.match(lines[1], /no Wi-Fi address/i);
});

test("a specific LAN host is printed as the url", () => {
  assert.deepEqual(listenLogLines("192.168.1.20", 5000, {}), [
    "serving on http://192.168.1.20:5000",
    "Phone on this Wi-Fi: http://192.168.1.20:5000",
  ]);
});

test("vite HMR follows the page host instead of localhost", () => {
  const options = viteMiddlewareOptions({} as never, 5000);
  assert.equal(options.middlewareMode, true);
  assert.equal(options.allowedHosts, true);
  assert.equal(options.hmr.path, VITE_HMR_PATH);
  assert.equal(options.hmr.clientPort, 5000);
  assert.equal("host" in options.hmr, false);
  assert.equal("protocol" in options.hmr, false);
});

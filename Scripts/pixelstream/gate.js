// Zoogi Roll phone gate: a tiny auth gate + reverse proxy in front of the Pixel Streaming signalling/web server (Wilbur).
// Only this gate is exposed through the tunnel. Everything (pages, player.js, the signalling WebSocket) needs a session
// cookie, which you get only by opening https://<tunnel>/<token> and entering the password. No token -> plain 404.
// Usage: node gate.js --port 8090 --upstream 8089 --secrets C:\Users\<you>\ZoogiPhone\access.json
'use strict';
const http = require('http');
const net = require('net');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

function arg(name, def) { const i = process.argv.indexOf('--' + name); return i > 0 ? process.argv[i + 1] : def; }
const PORT = parseInt(arg('port', '8090'), 10);
const UPSTREAM = parseInt(arg('upstream', '8089'), 10);
const SECRETS = arg('secrets');
const S = JSON.parse(fs.readFileSync(SECRETS, 'utf8').replace(/^﻿/, '')); // { token, password, sessionKey }; PowerShell 5.1 writes a BOM
if (!S.token || !S.password || !S.sessionKey) throw new Error('secrets file incomplete');
const TOKEN_PATH = '/' + S.token;
const NO_PASSWORD = process.argv.includes('--no-password'); // the link alone signs in (Start-ZoogiPhone default; -RequirePassword turns the page back on)
const SESSION_DAYS = 30;
const MOBILE_JS = fs.readFileSync(path.join(__dirname, 'zoogi-mobile.js'));

const log = (...a) => console.log(new Date().toISOString(), ...a);
const sha = (s) => crypto.createHash('sha256').update(String(s)).digest();
const eq = (a, b) => crypto.timingSafeEqual(sha(a), sha(b));
const mac = (exp) => crypto.createHmac('sha256', S.sessionKey).update('zg1.' + exp).digest('base64url');

function cookies(req) {
  const out = {};
  (req.headers.cookie || '').split(';').forEach((c) => { const i = c.indexOf('='); if (i > 0) out[c.slice(0, i).trim()] = c.slice(i + 1).trim(); });
  return out;
}
function authed(req) {
  const v = cookies(req).zg_s || '';
  const [exp, sig] = v.split('.');
  if (!exp || !sig || !/^\d+$/.test(exp) || Number(exp) < Date.now()) return false;
  return eq(sig, mac(exp));
}
function clientKey(req) { return String(req.headers['cf-connecting-ip'] || req.headers['x-forwarded-for'] || req.socket.remoteAddress || '?').split(',')[0].trim(); }

// brute-force guard: 5 wrong passwords per client -> 15 min block; 40 wrong in an hour overall -> logins closed for an hour
const fails = new Map(); let globalFails = []; let globalBlockUntil = 0;
function blocked(k) { const f = fails.get(k); return Date.now() < globalBlockUntil || (f && f.until > Date.now()); }
function noteFail(k) {
  const now = Date.now();
  const f = fails.get(k) || { n: 0, until: 0 };
  f.n++; if (f.n >= 5) { f.until = now + 15 * 60e3; f.n = 0; log('gate: client blocked 15 min after 5 bad passwords', k); }
  fails.set(k, f);
  globalFails = globalFails.filter((t) => now - t < 3600e3); globalFails.push(now);
  if (globalFails.length >= 40) { globalBlockUntil = now + 3600e3; log('gate: too many bad passwords overall - logins closed for 1 hour'); }
}

const SEC_HEADERS = { 'Referrer-Policy': 'no-referrer', 'X-Frame-Options': 'DENY', 'X-Content-Type-Options': 'nosniff', 'Strict-Transport-Security': 'max-age=31536000' };
function send(res, code, type, body, extra) { res.writeHead(code, Object.assign({ 'Content-Type': type, 'Cache-Control': 'no-store' }, SEC_HEADERS, extra || {})); res.end(body); }
const notFound = (res) => send(res, 404, 'text/plain', 'Not found');

function loginPage(msg) {
  return `<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="apple-mobile-web-app-capable" content="yes"><meta name="mobile-web-app-capable" content="yes"><meta name="apple-mobile-web-app-title" content="Zoogi Roll">
<meta name="apple-mobile-web-app-status-bar-style" content="black-translucent"><title>Zoogi Roll</title>
<style>body{margin:0;height:100vh;display:flex;align-items:center;justify-content:center;background:#0b0f1e;color:#fff;font-family:-apple-system,system-ui,sans-serif}
form{display:flex;flex-direction:column;gap:14px;width:min(320px,86vw)}h1{font-size:28px;margin:0 0 8px;text-align:center}
input,button{font-size:18px;padding:14px;border-radius:12px;border:0}button{background:#ffb703;font-weight:700}p{color:#ff8a8a;margin:0;text-align:center}</style></head>
<body><form method="post" action="${TOKEN_PATH}/login"><h1>Zoogi Roll</h1>${msg ? `<p>${msg}</p>` : ''}
<input type="password" name="password" autocomplete="current-password" placeholder="Password" autofocus required><button type="submit">Play</button></form></body></html>`;
}

const MANIFEST = JSON.stringify({ name: 'Zoogi Roll', short_name: 'Zoogi Roll', start_url: TOKEN_PATH, scope: '/', display: 'fullscreen', orientation: 'any', background_color: '#000000', theme_color: '#000000' });
const INJECT = '<meta name="apple-mobile-web-app-capable" content="yes"><meta name="mobile-web-app-capable" content="yes">' +
  '<meta name="apple-mobile-web-app-title" content="Zoogi Roll"><meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">' +
  '<link rel="manifest" href="/__zoogi/manifest.webmanifest"><script src="/__zoogi/mobile.js"></script>';

function proxy(req, res) {
  const headers = Object.assign({}, req.headers);
  delete headers.cookie; delete headers['accept-encoding'];
  headers.host = '127.0.0.1:' + UPSTREAM;
  const up = http.request({ host: '127.0.0.1', port: UPSTREAM, method: req.method, path: req.url, headers }, (ur) => {
    const type = String(ur.headers['content-type'] || '');
    if (type.includes('text/html')) {
      const chunks = []; ur.on('data', (c) => chunks.push(c));
      ur.on('end', () => {
        let html = Buffer.concat(chunks).toString('utf8');
        html = html.replace('<head>', '<head>' + INJECT).replace('<title>Pixel Streaming</title>', '<title>Zoogi Roll</title>');
        const h = Object.assign({}, ur.headers); delete h['content-length']; delete h.etag; delete h['last-modified'];
        res.writeHead(ur.statusCode, Object.assign(h, SEC_HEADERS, { 'Cache-Control': 'no-store' })); res.end(html);
      });
    } else { res.writeHead(ur.statusCode, Object.assign({}, ur.headers, SEC_HEADERS)); ur.pipe(res); }
  });
  up.on('error', () => send(res, 502, 'text/html', '<meta name="viewport" content="width=device-width"><h2 style="font-family:sans-serif">Zoogi Roll server is starting or stopped on the PC. Try again in a moment.</h2>'));
  req.pipe(up);
}

const server = http.createServer((req, res) => {
  const url = req.url.split('?')[0];
  if (url === '/healthz') return send(res, 200, 'text/plain', 'ok');
  if (url === TOKEN_PATH || url === TOKEN_PATH + '/') {
    if (authed(req)) return send(res, 302, 'text/plain', '', { Location: '/' });
    if (NO_PASSWORD) {
      // --no-password (Evan, Oct 20): the secret link itself signs the phone in - no password page
      const exp = Date.now() + SESSION_DAYS * 86400e3;
      log('gate: link login (no password) from', clientKey(req));
      return send(res, 303, 'text/plain', '', { Location: '/', 'Set-Cookie': `zg_s=${exp}.${mac(exp)}; Path=/; Max-Age=${SESSION_DAYS * 86400}; HttpOnly; Secure; SameSite=Lax` });
    }
    return send(res, 200, 'text/html', loginPage(''));
  }
  if (url === TOKEN_PATH + '/login' && req.method === 'POST') {
    const k = clientKey(req);
    if (blocked(k)) return send(res, 429, 'text/html', loginPage('Too many tries. Wait 15 minutes.'));
    let body = ''; req.on('data', (c) => { body += c; if (body.length > 4096) req.destroy(); });
    req.on('end', () => {
      const pw = new URLSearchParams(body).get('password') || '';
      if (!eq(pw, S.password)) { noteFail(k); log('gate: bad password from', k); return send(res, 401, 'text/html', loginPage('Wrong password.')); }
      fails.delete(k);
      const exp = Date.now() + SESSION_DAYS * 86400e3;
      log('gate: login ok from', k);
      send(res, 303, 'text/plain', '', { Location: '/', 'Set-Cookie': `zg_s=${exp}.${mac(exp)}; Path=/; Max-Age=${SESSION_DAYS * 86400}; HttpOnly; Secure; SameSite=Lax` });
    });
    return;
  }
  if (!authed(req)) return notFound(res);
  if (url === '/__zoogi/mobile.js') return send(res, 200, 'application/javascript', MOBILE_JS);
  if (url === '/__zoogi/manifest.webmanifest') return send(res, 200, 'application/manifest+json', MANIFEST);
  if (url === '/' && !req.url.includes('?')) {
    // stock Epic frontend, tuned for the phone: auto connect + play, no Epic UI panels, real touch events, no idle kick
    return send(res, 302, 'text/plain', '', { Location: '/?AutoConnect=true&AutoPlayVideo=true&HideUI=true&TouchInput=true&FakeMouseWithTouches=false&MatchViewportRes=false&TimeoutIfIdle=false&HoveringMouse=false&StartVideoMuted=false' });
  }
  proxy(req, res);
});

server.on('upgrade', (req, socket, head) => {
  if (!authed(req)) { log('gate: refused unauthenticated websocket from', clientKey(req)); socket.end('HTTP/1.1 404 Not Found\r\nConnection: close\r\n\r\n'); return; }
  const up = net.connect(UPSTREAM, '127.0.0.1', () => {
    const lines = [`${req.method} ${req.url} HTTP/1.1`];
    for (let i = 0; i < req.rawHeaders.length; i += 2) {
      const n = req.rawHeaders[i].toLowerCase();
      if (n === 'cookie') continue;
      lines.push(n === 'host' ? `Host: 127.0.0.1:${UPSTREAM}` : `${req.rawHeaders[i]}: ${req.rawHeaders[i + 1]}`);
    }
    up.write(lines.join('\r\n') + '\r\n\r\n'); if (head && head.length) up.write(head);
    up.pipe(socket); socket.pipe(up);
  });
  const end = () => { up.destroy(); socket.destroy(); };
  up.on('error', end); socket.on('error', end); up.on('close', end); socket.on('close', end);
});

server.listen(PORT, '127.0.0.1', () => log(`gate: listening on http://127.0.0.1:${PORT} -> Wilbur 127.0.0.1:${UPSTREAM}`));

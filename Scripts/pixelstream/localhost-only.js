// Preloaded into Wilbur (node -r): every server it opens (player web port, streamer port) listens on 127.0.0.1 only,
// so nothing on the LAN or internet can reach the signalling server except through the password gate.
'use strict';
const net = require('net');
const orig = net.Server.prototype.listen;
net.Server.prototype.listen = function (...a) {
  if (typeof a[0] === 'number' || (typeof a[0] === 'string' && /^\d+$/.test(a[0]))) {
    if (typeof a[1] !== 'string') a.splice(1, 0, '127.0.0.1'); else a[1] = '127.0.0.1';
  } else if (a[0] && typeof a[0] === 'object' && 'port' in a[0]) { a[0] = Object.assign({}, a[0], { host: '127.0.0.1' }); }
  return orig.apply(this, a);
};

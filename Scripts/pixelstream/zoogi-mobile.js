// Zoogi Roll phone helper, injected into the stock Pixel Streaming player page by gate.js.
// 1) The game's render resolution follows the phone: portrait menus <-> landscape battle (sends Resolution.Width/Height,
//    which the UE streamer turns into r.SetRes), scaled by the screen's pixel ratio and capped for mobile bandwidth.
// 2) Stops Safari from zooming / scrolling / selecting the page, so every finger goes to the game as a real touch
//    (pinch zoom, pan, aim drag, tap-to-target all happen in the game, not in the browser).
(function () {
  'use strict';
  var MAX_LONG = 1600, MAX_SHORT = 900; // render cap (long / short side, px): sharp on a phone, sane on cellular

  var css = document.createElement('style');
  css.textContent = 'html,body{background:#000!important;overscroll-behavior:none;touch-action:none;-webkit-user-select:none;user-select:none;' +
    '-webkit-touch-callout:none;-webkit-tap-highlight-color:transparent;position:fixed;inset:0;overflow:hidden}' +
    'video,#streamingVideo,#videoElementParent{touch-action:none!important}';
  document.head.appendChild(css);

  ['gesturestart', 'gesturechange', 'gestureend'].forEach(function (t) { document.addEventListener(t, function (e) { e.preventDefault(); }, { passive: false }); });
  document.addEventListener('touchmove', function (e) { e.preventDefault(); }, { passive: false });
  document.addEventListener('dblclick', function (e) { e.preventDefault(); }, { passive: false });
  document.addEventListener('contextmenu', function (e) { e.preventDefault(); });

  function wantRes() {
    var vv = window.visualViewport;
    var w = (vv ? vv.width : window.innerWidth), h = (vv ? vv.height : window.innerHeight);
    var dpr = Math.min(window.devicePixelRatio || 1, 3);
    var W = w * dpr, H = h * dpr;
    var lng = Math.max(W, H), sht = Math.min(W, H);
    var k = Math.min(1, MAX_LONG / lng, MAX_SHORT / sht);
    W = Math.max(2, Math.round(W * k / 2) * 2); H = Math.max(2, Math.round(H * k / 2) * 2);
    return { w: W, h: H };
  }

  var last = '', timer = 0, connected = false;
  function sendRes(force) {
    var ps = window.pixelStreaming;
    if (!ps || !connected) return;
    var r = wantRes(), key = r.w + 'x' + r.h;
    if (!force && key === last) return;
    last = key;
    try { ps.emitCommand({ 'Resolution.Width': r.w, 'Resolution.Height': r.h }); console.log('[zoogi] stream resolution -> ' + key); }
    catch (e) { console.warn('[zoogi] resolution command failed', e); last = ''; }
  }
  function soon(force) { clearTimeout(timer); timer = setTimeout(function () { sendRes(force); }, 250); }
  window.addEventListener('resize', function () { soon(false); });
  window.addEventListener('orientationchange', function () { soon(false); setTimeout(function () { sendRes(false); }, 700); });
  if (window.visualViewport) window.visualViewport.addEventListener('resize', function () { soon(false); });
  // safety net: iOS sometimes reports the new size late after a rotation
  setInterval(function () { sendRes(false); }, 1500);

  var hooked = false;
  function hook() {
    var ps = window.pixelStreaming;
    if (!ps || hooked) return !!ps;
    hooked = true;
    var on = function () { connected = true; last = ''; soon(true); setTimeout(function () { sendRes(true); }, 1500); };
    ['webRtcConnected', 'videoInitialized', 'playStream', 'dataChannelOpen'].forEach(function (ev) { try { ps.addEventListener(ev, on); } catch (e) {} });
    ['webRtcDisconnected', 'webRtcFailed'].forEach(function (ev) { try { ps.addEventListener(ev, function () { connected = false; }); } catch (e) {} });
    return true;
  }
  var tries = 0, iv = setInterval(function () { if (hook() || ++tries > 600) clearInterval(iv); }, 100);
  window.zoogiStream = { sendRes: sendRes, wantRes: wantRes, isConnected: function () { return connected; } };
})();

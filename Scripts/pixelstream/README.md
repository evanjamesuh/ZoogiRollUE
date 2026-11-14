# Zoogi Roll on the iPhone (Pixel Streaming)

The game runs on the Windows PC and streams to the phone's browser (Unreal Pixel Streaming 2, UE 5.6). Fingers on the
phone are sent back to the game as real touch events, so aim drag, drag launch, tap-to-target, pinch zoom / pan (ZOOM),
the HUD show/hide button and the portrait menus all work the same way they do on a phone build.

## Start / stop (on the PC)
- Start: double-click `Scripts\pixelstream\Start-ZoogiPhone.bat` (desktop shortcut: "Zoogi Roll - Phone").
  It waits for `C:\dev\unreal.lock` if another Unreal job holds it, takes the lock for the session, starts the game
  (off-screen), the signalling server, the password gate and the HTTPS tunnel, then prints the phone link + a QR code.
- Stop: Ctrl+C in that window, or double-click `Stop-ZoogiPhone.bat`. The lock is released.
- Rebuild the phone build after game changes: `powershell -File Scripts\pixelstream\Build-ZoogiPhone.ps1`
  (packages to `C:\dev\ZoogiPhone\Game`, holds the lock only while UAT runs).

## On the iPhone
Open the printed link in Safari, enter the password once (stays logged in 30 days), play. Rotate the phone: menus are
portrait, battle landscape (or portrait with the HUD hidden) - the stream resolution follows the phone.
Optional: Share > Add to Home Screen (full-screen, no Safari bars; asks for the password once more).

## Access gate
- Link = `https://<tunnel>/<token>`; without the token every URL is a plain 404. Then a password page.
- Token, password and cookie key: `%USERPROFILE%\ZoogiPhone\access.json` (only the user's account can read it).
  Delete that file to rotate everything; a new one is generated on the next start.
- After login a signed HttpOnly/Secure cookie (30 days) unlocks the player page and the signalling WebSocket.
  5 wrong passwords -> 15 min block per client; 40 per hour -> logins closed for 1 hour.
- The signalling server and the game's streamer port listen on 127.0.0.1 only; the tunnel reaches only the gate.

## Network / cellular
- Default tunnel: free Cloudflare quick tunnel (no account). The watchdog restarts it if it dies, but the link CHANGES
  then (the start window prints the new one; also in `%USERPROFILE%\ZoogiPhone\phone-link.txt`).
- Video/audio/input go over WebRTC directly between phone and PC (STUN only, no TURN). Works on the home Wi-Fi; on
  cellular it works only if the carrier/home NATs allow a direct UDP path.
- For reliable cellular + a stable link: install Tailscale on the PC and the iPhone (free account), sign in on both,
  then start with `Start-ZoogiPhone.bat -Tunnel tailscale`.
- Own TURN server: put `{"iceServers":[{"urls":["stun:..."]},{"urls":["turn:host:3478?transport=udp","turns:host:443?transport=tcp"],"username":"...","credential":"..."}]}`
  into `%USERPROFILE%\ZoogiPhone\turn.json`.

## Files
`gate.js` auth gate + proxy, `zoogi-mobile.js` injected phone helper (resolution follows orientation, no Safari zoom),
`localhost-only.js` keeps Wilbur on 127.0.0.1, `tunnel-watchdog.ps1`, `ice-default.json`.
Infrastructure (Epic, UE5.6-0.1.1): `C:\dev\PixelStreaming\PixelStreamingInfrastructure-UE5.6-0.1.1`; cloudflared: `C:\dev\PixelStreaming\bin`.
Logs: `C:\dev\ZoogiPhone\logs`; game log: `C:\dev\ZoogiPhone\Game\Windows\ZoogiRoll\Saved\Logs\ZoogiPhone.log`.

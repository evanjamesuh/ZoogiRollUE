<#
  Zoogi Roll on the phone: starts the Pixel Streaming game, the signalling server, the password gate and an HTTPS tunnel,
  then prints the phone link. Leave this window open while playing; press Ctrl+C here (or run Stop-ZoogiPhone.bat) to stop.

  -Tunnel cloudflare  (default) free Cloudflare quick tunnel, no account. The link CHANGES every time the tunnel restarts.
  -Tunnel tailscale   uses 'tailscale serve' (needs Tailscale installed + signed in on the PC and on the iPhone). Stable link,
                      and video works on cellular through Tailscale. Recommended once Evan has a (free) Tailscale account.
  -Tunnel none        local only: http://127.0.0.1:8090/<token> (testing on the PC).
  -ShowWindow         render in a normal window instead of off-screen.
#>
param(
  [ValidateSet('cloudflare', 'tailscale', 'none')][string]$Tunnel = 'cloudflare',
  [switch]$ShowWindow,
  [switch]$RequirePassword,   # Oct 20: off by default - the secret link alone signs the phone in
  [int]$LockWaitMinutes = 240
)
$ErrorActionPreference = 'Stop'
$Here      = $PSScriptRoot
$Infra     = 'C:\dev\PixelStreaming\PixelStreamingInfrastructure-UE5.6-0.1.1'
$Cloudflared = 'C:\dev\PixelStreaming\bin\cloudflared.exe'
$GameExe   = 'C:\dev\ZoogiPhone\Game\Windows\ZoogiRoll.exe'
$LogDir    = 'C:\dev\ZoogiPhone\logs'
$SecretDir = Join-Path $env:USERPROFILE 'ZoogiPhone'
$Secrets   = Join-Path $SecretDir 'access.json'
$LinkFile  = Join-Path $SecretDir 'phone-link.txt'
$StateFile = Join-Path $SecretDir 'session.json'
$Lock      = 'C:\dev\unreal.lock'
$GatePort = 8090; $PlayerPort = 8089; $StreamerPort = 8888; $SfuPort = 8887
$LockTag   = 'mobile-pixel-streaming PIXEL-STREAMING-SESSION'

New-Item -ItemType Directory -Force $LogDir, $SecretDir | Out-Null
function Say($m, $c = 'Gray') { Write-Host ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $m) -ForegroundColor $c }

if (-not (Test-Path $GameExe)) { throw "Phone game build not found at $GameExe. Run Scripts\pixelstream\Build-ZoogiPhone.ps1 first." }
if (Test-Path $StateFile) {
  $old = Get-Content $StateFile -Raw | ConvertFrom-Json
  if ($old.game -and (Get-Process -Id $old.game -ErrorAction SilentlyContinue)) { throw "A phone session is already running (game pid $($old.game)). Run Stop-ZoogiPhone.bat first." }
}

# ---- access secrets (generated once; the password is in $Secrets, never printed in logs)
function RandStr([int]$n, [string]$alphabet) {
  $b = New-Object byte[] $n; [System.Security.Cryptography.RandomNumberGenerator]::Create().GetBytes($b)
  -join ($b | ForEach-Object { $alphabet[$_ % $alphabet.Length] })
}
if (-not (Test-Path $Secrets)) {
  $a = 'abcdefghjkmnpqrstuvwxyz23456789'  # 31 chars, no look-alikes
  $pw = (1..4 | ForEach-Object { RandStr 4 $a }) -join '-'          # ~79 bits
  $obj = [ordered]@{ token = (RandStr 32 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'); password = $pw; sessionKey = (RandStr 48 'abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789'); note = 'Zoogi Roll phone access. token = secret part of the link, password = typed on the phone. Delete this file to rotate everything (old links and logins stop working).' }
  $obj | ConvertTo-Json | Set-Content -Encoding UTF8 $Secrets
  & icacls $Secrets /inheritance:r /grant:r "$($env:USERNAME):F" | Out-Null
  Say "New access password generated and saved in $Secrets" Yellow
}
$S = Get-Content $Secrets -Raw | ConvertFrom-Json

# ---- ICE (STUN/TURN). Put your own TURN server in $SecretDir\turn.json to override (same format as ice-default.json).
$Ice = Join-Path $Here 'ice-default.json'
if (Test-Path (Join-Path $SecretDir 'turn.json')) { $Ice = Join-Path $SecretDir 'turn.json'; Say 'Using TURN/ICE settings from turn.json' }

# ---- the shared Unreal lock: one Unreal job at a time on this PC
function LockHolder { if (Test-Path $Lock) { (Get-Content $Lock -Raw -ErrorAction SilentlyContinue) } else { $null } }
$deadline = (Get-Date).AddMinutes($LockWaitMinutes)
while ($true) {
  $h = LockHolder
  if (-not $h) {
    try {
      $fs = [System.IO.File]::Open($Lock, 'CreateNew', 'Write'); $w = New-Object System.IO.StreamWriter($fs)
      $w.Write("$LockTag $(Get-Date -Format 'HHmmss') pid=$PID - Zoogi Roll Pixel Streaming game running for Evan's phone. Stop: Scripts\pixelstream\Stop-ZoogiPhone.bat"); $w.Close(); break
    } catch { Start-Sleep 2; continue }
  }
  if ((Get-Date) -gt $deadline) { throw "Gave up waiting for the Unreal lock (held by: $h)" }
  Say "Unreal lock is held by someone else ($($h.Trim())) - waiting, re-checking every 30 s..." Yellow
  Start-Sleep 30
}
Say 'Unreal lock taken for the Pixel Streaming session' Green

$procs = @{}
function SaveState { $procs | ConvertTo-Json | Set-Content $StateFile }
function Cleanup {
  Say 'Stopping the phone session...' Yellow
  foreach ($k in 'watchdog', 'cloudflared', 'game', 'gate', 'wilbur') { if ($procs[$k]) { Stop-Process -Id $procs[$k] -Force -ErrorAction SilentlyContinue } }
  if ($procs.watchdog) { Get-CimInstance Win32_Process -Filter "ParentProcessId=$($procs.watchdog)" -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
  if ($Tunnel -eq 'tailscale') { & tailscale serve --https=443 off 2>$null | Out-Null }
  $h = LockHolder; if ($h -and $h.StartsWith($LockTag)) { Remove-Item $Lock -Force; Say 'Unreal lock released' Green }
  Remove-Item $StateFile, $LinkFile -Force -ErrorAction SilentlyContinue
}

try {
  # ---- signalling + web server (Wilbur), forced onto 127.0.0.1 only
  $wilburArgs = @('-r', "`"$Here\localhost-only.js`"", "`"$Infra\SignallingWebServer\dist\index.js`"", '--no_config', '--serve',
    '--http_root', "`"$Infra\SignallingWebServer\www`"", '--homepage', 'player.html', '--player_port', $PlayerPort, '--streamer_port', $StreamerPort,
    '--sfu_port', $SfuPort, '--max_players', '2', '--peer_options_file', "`"$Ice`"", '--log_folder', "`"$LogDir\wilbur`"", '--console_messages', 'basic')
  $procs.wilbur = (Start-Process node -ArgumentList $wilburArgs -WorkingDirectory "$Infra\SignallingWebServer" -WindowStyle Hidden -PassThru -RedirectStandardOutput "$LogDir\wilbur.out.log" -RedirectStandardError "$LogDir\wilbur.err.log").Id
  # ---- password gate (the only thing the tunnel can reach)
  $gateArgs = @("`"$Here\gate.js`"", '--port', $GatePort, '--upstream', $PlayerPort, '--secrets', "`"$Secrets`"")
  if (-not $RequirePassword) { $gateArgs += '--no-password' }
  $procs.gate = (Start-Process node -ArgumentList $gateArgs -WindowStyle Hidden -PassThru -RedirectStandardOutput "$LogDir\gate.out.log" -RedirectStandardError "$LogDir\gate.err.log").Id
  SaveState
  Start-Sleep 3
  foreach ($k in 'wilbur', 'gate') { if (-not (Get-Process -Id $procs[$k] -ErrorAction SilentlyContinue)) { throw "$k failed to start - see $LogDir\$k.err.log" } }

  # ---- the game, streaming to Wilbur. -ZoogiNoWindowSwap: the phone (not the PC window) decides portrait / landscape;
  #      -ZoogiRotatePromptTest: show the rotate-phone prompt when leaving battle, as on a real phone.
  $gameArgs = @("-PixelStreamingConnectionURL=ws://127.0.0.1:$StreamerPort", "-PixelStreamingURL=ws://127.0.0.1:$StreamerPort",
    '-Windowed', '-ResX=720', '-ResY=1280', '-ForceRes', '-AudioMixer', '-ZoogiNoWindowSwap', '-ZoogiRotatePromptTest',
    '-PixelStreamingWebRTCMaxBitrate=12000000', '-PixelStreamingWebRTCStartBitrate=4000000', '-PixelStreamingWebRTCFps=60',
    '-ini:Engine:[Audio]:UnfocusedVolumeMultiplier=1.0', '-ini:Engine:[/Script/Engine.Engine]:bSmoothFrameRate=False', '-log=ZoogiPhone.log')
  if (-not $ShowWindow) { $gameArgs += '-RenderOffScreen' }
  # a leftover phone game from an earlier session would connect as a second streamer ("multiple streamers available")
  Get-CimInstance Win32_Process -Filter "Name='ZoogiRoll.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith('C:\dev\ZoogiPhone\Game', [StringComparison]::OrdinalIgnoreCase) } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue; Say "Stopped a leftover phone game ($($_.ProcessId))" Yellow }
  $procs.game = (Start-Process $GameExe -ArgumentList $gameArgs -PassThru).Id
  SaveState
  Say "Game started (pid $($procs.game)), streaming to the signalling server" Green

  # ---- tunnel
  $base = "http://127.0.0.1:$GatePort"
  if ($Tunnel -eq 'cloudflare') {
    Remove-Item $LinkFile -ErrorAction SilentlyContinue
    $procs.watchdog = (Start-Process powershell -ArgumentList @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', "`"$Here\tunnel-watchdog.ps1`"", '-Cloudflared', "`"$Cloudflared`"", '-Port', $GatePort, '-OutFile', "`"$LinkFile.base`"", '-LogDir', "`"$LogDir`"") -WindowStyle Hidden -PassThru).Id
    SaveState
    Say 'Starting the Cloudflare tunnel...'
    for ($i = 0; $i -lt 150 -and -not (Test-Path "$LinkFile.base"); $i++) { Start-Sleep 1 } # the watchdog waits up to ~60 s for the edge before publishing
    if (-not (Test-Path "$LinkFile.base")) { throw "Tunnel did not come up - see $LogDir\cloudflared.log" }
    $base = (Get-Content "$LinkFile.base" -Raw).Trim()
  } elseif ($Tunnel -eq 'tailscale') {
    & tailscale serve --bg --https=443 "http://127.0.0.1:$GatePort" | Out-Null
    $dns = ((& tailscale status --json) | ConvertFrom-Json).Self.DNSName.TrimEnd('.')
    $base = "https://$dns"
  }
  $link = "$base/$($S.token)"
  Set-Content -Encoding UTF8 $LinkFile $link
  Write-Host ''
  Write-Host '  Zoogi Roll is ready for your phone.' -ForegroundColor Cyan
  Write-Host "  Phone link:  $link" -ForegroundColor Cyan
  if ($RequirePassword) { Write-Host ('  Password:    in ' + $Secrets + ' (the "password" line)') -ForegroundColor Cyan }
  else { Write-Host '  No password: the link itself signs you in - share it only with people who should play.' -ForegroundColor Cyan }
  Write-Host "  (link also saved in $LinkFile)" -ForegroundColor DarkGray
  $qr = 'C:\dev\PixelStreaming\tools\node_modules\qrcode-terminal'
  if (Test-Path $qr) { & node -e "require(process.argv[1]).generate(process.argv[2],{small:true})" $qr $link }
  Write-Host '  Keep this window open while you play. Ctrl+C here stops everything.' -ForegroundColor DarkGray
  Write-Host ''

  # ---- babysit: if the tunnel watchdog brings up a new link, print it; stop when the game exits
  $shown = $base
  while (Get-Process -Id $procs.game -ErrorAction SilentlyContinue) {
    Start-Sleep 5
    if ($Tunnel -eq 'cloudflare' -and (Test-Path "$LinkFile.base")) {
      $nb = (Get-Content "$LinkFile.base" -Raw).Trim()
      if ($nb -and $nb -ne $shown) { $shown = $nb; $link = "$nb/$($S.token)"; Set-Content -Encoding UTF8 $LinkFile $link; Say "Tunnel restarted - NEW phone link: $link" Yellow }
    }
    foreach ($k in 'wilbur', 'gate') {
      if (-not (Get-Process -Id $procs[$k] -ErrorAction SilentlyContinue)) { Say "$k stopped unexpectedly - see $LogDir\$k.err.log" Red }
    }
  }
  Say 'The game exited.' Yellow
} finally {
  Cleanup
  Remove-Item "$LinkFile.base" -ErrorAction SilentlyContinue
}

# Stops a phone session started by Start-ZoogiPhone.ps1 (only the processes it recorded) and releases its Unreal lock.
$SecretDir = Join-Path $env:USERPROFILE 'ZoogiPhone'
$StateFile = Join-Path $SecretDir 'session.json'
$Lock = 'C:\dev\unreal.lock'
$LockTag = 'mobile-pixel-streaming PIXEL-STREAMING-SESSION'
if (Test-Path $StateFile) {
  $s = Get-Content $StateFile -Raw | ConvertFrom-Json
  if ($s.watchdog) { Get-CimInstance Win32_Process -Filter "ParentProcessId=$($s.watchdog)" -ErrorAction SilentlyContinue | ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue } }
  foreach ($k in 'watchdog', 'cloudflared', 'game', 'gate', 'wilbur') { if ($s.$k) { Stop-Process -Id $s.$k -Force -ErrorAction SilentlyContinue; "stopped $k ($($s.$k))" } }
  Remove-Item $StateFile -Force
} else { 'No running phone session recorded.' }
# the recorded game pid is the small launcher (ZoogiRoll.exe); the real game it starts (ZoogiRoll\Binaries\Win64\ZoogiRoll.exe)
# survives it and would stay connected as a second streamer ("multiple streamers available") - stop every phone-build game
Get-CimInstance Win32_Process -Filter "Name='ZoogiRoll.exe'" -ErrorAction SilentlyContinue |
  Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith('C:\dev\ZoogiPhone\Game', [StringComparison]::OrdinalIgnoreCase) } |
  ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue; "stopped phone game ($($_.ProcessId))" }
if (Get-Command tailscale -ErrorAction SilentlyContinue) { & tailscale serve --https=443 off 2>$null | Out-Null }
if ((Test-Path $Lock) -and (Get-Content $Lock -Raw).StartsWith($LockTag)) { Remove-Item $Lock -Force; 'Unreal lock released.' }
Remove-Item (Join-Path $SecretDir 'phone-link.txt'), (Join-Path $SecretDir 'phone-link.txt.base') -Force -ErrorAction SilentlyContinue

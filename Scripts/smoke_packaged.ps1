# Packaged-build smoke test: runs the packaged exe (CPU auto match) and checks its log.
# Screenshots come ONLY from the game's own viewport (the engine's own screenshot requests via
# -ZoogiEpisodeShots=; -ZoogiShots= is not used because it also starts the arena showcase camera tour). This script does no OS-level screen or window capture of any kind, so nothing outside
# the game window (browser, chat, mail, other apps) can ever end up in a screenshot.
# (An OS-level PrintWindow capture was tried on 2026-10-08 and hung against the D3D12 window; it was dropped.)
# Takes C:\dev\unreal.lock after Scripts\waitclear.ps1 reports READY (pass -NoLock if the caller already holds it).
# Usage: powershell -File Scripts\smoke_packaged.ps1 [-Quit 120] [-Arena 1] [-Extra "..."]
param(
  [string]$Exe = 'C:\dev\ZoogiRollUnreal\Packaged\Windows\ZoogiRoll.exe',
  [string]$Log = (Join-Path $PSScriptRoot '..\Saved\Logs\pkg_smoke.log'),
  [string]$ShotDir = (Join-Path $PSScriptRoot '..\Saved\Shots\pkg_smoke'),
  [int]$Arena = 1,
  [int]$Quit = 120,
  [string]$Extra = '',
  [string]$LockOwner = 'smoke',
  [switch]$NoLock
)
$ErrorActionPreference = 'Stop'
$Log = [IO.Path]::GetFullPath($Log); $ShotDir = [IO.Path]::GetFullPath($ShotDir)

$lock = 'C:\dev\unreal.lock'
$waitclear = 'C:\dev\ZoogiRollUnreal\Scripts\waitclear.ps1'
if (-not $NoLock) {
  while ($true) {
    $wc = if (Test-Path $waitclear) { powershell -NoProfile -ExecutionPolicy Bypass -File $waitclear 2>&1 | Out-String } else { 'READY' }
    if ($wc -match 'READY') {
      try { New-Item $lock -ItemType File -Value $LockOwner -ErrorAction Stop | Out-Null; break } catch { Start-Sleep 20 }
    } else { Start-Sleep 15 }
  }
  "LOCK_TAKEN $(Get-Date -Format HH:mm:ss)"
}
try {
  New-Item -ItemType Directory -Force (Split-Path $Log) | Out-Null
  Remove-Item -Recurse -Force $ShotDir -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force $ShotDir | Out-Null
  $since = (Get-Date).AddSeconds(-1)
  $argLine = "-windowed -ResX=1600 -ResY=900 -ZoogiArena=$Arena -ZoogiAuto -ZoogiQuit=$Quit -ZoogiEpisodeShots=`"$ShotDir`" -nosplash -NoSound -log -abslog=`"$Log`" $Extra"
  $p = Start-Process $Exe -ArgumentList $argLine -PassThru
  if (-not $p.WaitForExit(($Quit + 120) * 1000)) {
    "TIMEOUT - closing the game started by this run"
    # only processes started by this run (a copy of the game someone already has open is never touched)
    Get-Process ZoogiRoll -ErrorAction SilentlyContinue | Where-Object { $_.StartTime -ge $since } | Stop-Process -Force
  }
  Start-Sleep 2
  "EXIT_CODE=$($p.ExitCode)"
  $shots = @(Get-ChildItem $ShotDir -Filter *.png -ErrorAction SilentlyContinue)
  "VIEWPORT_SHOTS=$($shots.Count) dir=$ShotDir"
  if (Test-Path $Log) {
    $txt = Get-Content $Log
    "BATTLE_LINES=$(@($txt | Select-String 'BATTLE ').Count) STATE_LINES=$(@($txt | Select-String 'BATTLE state ').Count) ACTIONS=$(@($txt | Select-String 'PHYSICS_RESOLVE -> RESULT').Count)"
    "FLOW=$((@($txt | Select-String 'BATTLE engine: flow=(\w+)') | Select-Object -First 1).Matches.Groups[1].Value) GAME_RNG=$((@($txt | Select-String 'GAME rng seed=\S+ source=\S+') | Select-Object -First 1).Matches.Value)"
    "CRASH_LINES=$(@($txt | Select-String 'Fatal error|Unhandled Exception|Assertion failed').Count)"
  } else { "NO_LOG" }
}
finally {
  if (-not $NoLock) { Remove-Item $lock -Force -ErrorAction SilentlyContinue; "LOCK_RELEASED $(Get-Date -Format HH:mm:ss)" }
}

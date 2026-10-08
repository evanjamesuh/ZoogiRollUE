# Packaged-build smoke test: runs the packaged exe (CPU auto match) and captures screenshots of the GAME WINDOW ONLY.
# Capture = PrintWindow on the game's own window handle (client area). It never reads the desktop, so other
# windows (browser, chat, mail) can't end up in a screenshot, even if they overlap the game window.
# Takes C:\dev\unreal.lock after Scripts\waitclear.ps1 reports READY (pass -NoLock if the caller already holds it).
# Usage: powershell -File Scripts\smoke_packaged.ps1 [-Quit 120] [-Arena 1] [-ShotAt 60,95] [-Extra "..."]
param(
  [string]$Exe = 'C:\dev\ZoogiRollUnreal\Packaged\Windows\ZoogiRoll.exe',
  [string]$Log = (Join-Path $PSScriptRoot '..\Saved\Logs\pkg_smoke.log'),
  [string]$ShotDir = (Join-Path $PSScriptRoot '..\Saved\Shots\pkg_smoke'),
  [int]$Arena = 1,
  [int]$Quit = 120,
  [int[]]$ShotAt = @(60, 95),
  [string]$Extra = '',
  [string]$LockOwner = 'smoke',
  [switch]$NoLock
)
$ErrorActionPreference = 'Stop'
$Log = [IO.Path]::GetFullPath($Log); $ShotDir = [IO.Path]::GetFullPath($ShotDir)
Add-Type -AssemblyName System.Drawing
Add-Type @'
using System;
using System.Runtime.InteropServices;
public static class ZoogiWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
  [DllImport("user32.dll")] public static extern bool IsWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
'@
[void][ZoogiWin]::SetProcessDPIAware()

function Find-GameWindow([datetime]$Since) {
  # The launcher stub starts Binaries\Win64\ZoogiRoll.exe; only a game process started by THIS run counts
  # (a copy of the game Evan already has open is never captured).
  Get-Process ZoogiRoll -ErrorAction SilentlyContinue |
    Where-Object { $_.MainWindowHandle -ne [IntPtr]::Zero -and $_.StartTime -ge $Since } |
    Sort-Object StartTime | Select-Object -Last 1
}

function Save-GameWindowShot([IntPtr]$Hwnd, [string]$Path) {
  if (-not [ZoogiWin]::IsWindow($Hwnd)) { return 'NO_WINDOW' }
  $r = New-Object ZoogiWin+RECT
  [void][ZoogiWin]::GetClientRect($Hwnd, [ref]$r)
  $w = $r.R - $r.L; $h = $r.B - $r.T
  if ($w -lt 16 -or $h -lt 16) { return 'NO_CLIENT_AREA' }
  $bmp = New-Object System.Drawing.Bitmap $w, $h
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $hdc = $g.GetHdc()
  $ok = [ZoogiWin]::PrintWindow($Hwnd, $hdc, 3)   # PW_CLIENTONLY | PW_RENDERFULLCONTENT: the window's own pixels only
  $g.ReleaseHdc($hdc); $g.Dispose()
  if (-not $ok) { $bmp.Dispose(); return 'PRINTWINDOW_FAILED' }
  $max = 0
  for ($y = 0; $y -lt 8; $y++) { for ($x = 0; $x -lt 8; $x++) {
    $c = $bmp.GetPixel([int](($x + 0.5) * $w / 8), [int](($y + 0.5) * $h / 8)); $max = [Math]::Max($max, $c.R + $c.G + $c.B) } }
  $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
  if ($max -lt 24) { return "BLACK ${w}x${h}" }
  return "OK ${w}x${h}"
}

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
  $t0 = Get-Date
  foreach ($t in ($ShotAt | Sort-Object)) {
    $wait = $t - ((Get-Date) - $t0).TotalSeconds
    if ($wait -gt 0) { Start-Sleep -Seconds $wait }
    $game = Find-GameWindow $since
    if (-not $game) { "SHOT t=$t : NO_GAME_WINDOW"; continue }
    "SHOT t=$t : $(Save-GameWindowShot $game.MainWindowHandle (Join-Path $ShotDir "window_t$t.png"))"
  }
  if (-not $p.WaitForExit(($Quit + 120) * 1000)) {
    "TIMEOUT - closing the game started by this run"
    $game = Find-GameWindow $since
    if ($game) { $game.Kill() }; if (-not $p.HasExited) { $p.Kill() }
  }
  Start-Sleep 2
  "EXIT_CODE=$($p.ExitCode)"
}
finally {
  if (-not $NoLock) { Remove-Item $lock -Force -ErrorAction SilentlyContinue; "LOCK_RELEASED $(Get-Date -Format HH:mm:ss)" }
}

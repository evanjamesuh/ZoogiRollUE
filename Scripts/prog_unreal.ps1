# Progression worker: build + automation + cards-off roll parity under C:\dev\unreal.lock (one Unreal job at a time, 16 GB box).
#   .\Scripts\prog_unreal.ps1 -Tag p1 [-Tests "ZoogiRoll"] [-Parity] [-NoBuild]
param([string]$Tag = "run", [string]$Tests = "ZoogiRoll", [switch]$Parity, [switch]$NoBuild)
$UE = "C:\Program Files\Epic Games\UE_5.6"; $Root = Split-Path -Parent $PSScriptRoot; $Proj = Join-Path $Root "ZoogiRoll.uproject"
$lock = 'C:\dev\unreal.lock'; $Logs = "$Root\Saved\Logs"; New-Item -ItemType Directory -Force $Logs | Out-Null
while ($true) {
  & "$PSScriptRoot\waitclear.ps1" -Minutes 120 | Out-Null
  try { New-Item $lock -ItemType File -Value "progression (Grok Bot executor) $(Get-Date -Format s)" -ErrorAction Stop | Out-Null; break } catch { Start-Sleep 20 }
}
"LOCK_TAKEN $(Get-Date -Format HH:mm:ss)"
try {
  if (-not $NoBuild) {
    & "$UE\Engine\Build\BatchFiles\Build.bat" ZoogiRollEditor Win64 Development "-Project=$Proj" -WaitMutex -NoHotReloadFromIDE *> "$Logs\build_$Tag.log"
    "BUILD_EXIT=$LASTEXITCODE"
    if ($LASTEXITCODE -ne 0) { Select-String "$Logs\build_$Tag.log" -Pattern "error" | Select -First 30 | % { $_.Line }; return }
  }
  if ($Tests) {
    $a = "`"$Proj`" -ExecCmds=`"Automation RunTests $Tests;Quit`" -unattended -nullrhi -nosplash -NoSound -nop4 -log -abslog=$Logs\auto_$Tag.log"
    $p = Start-Process "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList $a -PassThru -WindowStyle Hidden; $p.WaitForExit()
    $r = Select-String "$Logs\auto_$Tag.log" -Pattern "Test Completed. Result=\{(\w+)\}"
    "TESTS total=$($r.Count) success=$(($r | ? { $_.Matches[0].Groups[1].Value -eq 'Success' }).Count)"
    $r | ? { $_.Matches[0].Groups[1].Value -ne 'Success' } | % { "FAIL: " + $_.Line }
    Select-String "$Logs\auto_$Tag.log" -Pattern "TEST COMPLETE. EXIT CODE" | % { $_.Line }
  }
  if ($Parity) {
    $a = "`"$Proj`" -game -windowed -ResX=1600 -ResY=900 -ZoogiAuto -ZoogiQuit=150 -ZoogiPlayers=4 -ZoogiArena=1 -ZoogiRounds=1 -nosplash -NoSound -log -abslog=$Logs\parity_$Tag.log -ZoogiBattleTest=roll -ZoogiNoForm -UseFixedTimeStep -FPS=60 -ZoogiEpisodeSeed=7"
    $p = Start-Process "$UE\Engine\Binaries\Win64\UnrealEditor.exe" -ArgumentList $a -PassThru; $p.WaitForExit()
    Select-String "$Logs\parity_$Tag.log" -Pattern "BATTLE_TEST case=roll result" | % { $_.Line -replace '.*(result=\w+).*(ledger_hash=\w+).*', 'PARITY $1 $2' }
  }
} finally { Remove-Item $lock -Force -ErrorAction SilentlyContinue; "LOCK_RELEASED $(Get-Date -Format HH:mm:ss)" }

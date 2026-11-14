# KFX finale iteration for this worktree: [kfx_build.py] -> [C++ build] -> [comparison capture]. Each Unreal job waits for waitclear.ps1.
param([switch]$Finale, [string]$Base = "0,0,12000", [switch]$KfxBuild, [switch]$Build, [switch]$Capture, [switch]$HudShot, [string[]]$Arts = @("Ember","Storm","Void"), [string]$Out = "$env:USERPROFILE\kfx_ap_xfer\shots", [string]$Plan = "")
$ErrorActionPreference = "Continue"
$UE = "C:\Program Files\Epic Games\UE_5.6"
$Root = Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $PSScriptRoot))
$Proj = Join-Path $Root "ZoogiRoll.uproject"
$Logs = "$Root\Saved\Logs"; New-Item -ItemType Directory -Force $Logs | Out-Null
if (-not $Plan) { $Plan = Join-Path $PSScriptRoot "plan_afx.txt" }
function Wait-Clear { & powershell -NoProfile -File "$Root\Scripts\waitclear.ps1" -Minutes 40 }
function Wait-NoGame {  # also keep clear of any other -game run (16 GB RAM)
  for ($i = 0; $i -lt 120; $i++) { $g = Get-CimInstance Win32_Process | Where-Object { $_.Name -match '^UnrealEditor(-Cmd)?\.exe$' -or $_.Name -match '^ZoogiRoll' }; if (-not $g) { return "NOGAME" }; Start-Sleep 15 }; return "GAME_STILL_RUNNING"
}
"ROOT $Root"
if ($Build -and -not (Test-Path "$Root\Binaries\Win64\UnrealEditor-ZoogiRoll.dll")) { $first = $true }
if ($Build -and $first) {
  "WAIT " + (Wait-Clear)
  & "$UE\Engine\Build\BatchFiles\Build.bat" ZoogiRollEditor Win64 Development "-Project=$Proj" -WaitMutex -NoHotReloadFromIDE *> "$Logs\ap_build0.log"
  "BUILD0_EXIT=$LASTEXITCODE"; Get-Content "$Logs\ap_build0.log" | Select-String -Pattern "error|Result:|Total execution" | Select-Object -Last 15 | ForEach-Object { $_.Line }
}
if ($KfxBuild) {
  "WAIT " + (Wait-Clear)
  & "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $Proj -run=pythonscript "-script=$Root\Scripts\kfx\kfx_build.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput *> "$Logs\ap_kfxbuild.log"
  "KFXBUILD_EXIT=$LASTEXITCODE"; Get-Content "$Logs\ap_kfxbuild.log" | Select-String -Pattern "KFX .*(compile=|FAILED|failed|done|extras)|Error:|Traceback" | Select-Object -Last 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(400, $_.Line.Length)) }
}
if ($Finale) {
  "WAIT " + (Wait-Clear)
  & "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $Proj -run=pythonscript "-script=$PSScriptRoot\kfx_finale.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput *> "$Logs\ap_finale.log"
  "FINALE_EXIT=$LASTEXITCODE"; Get-Content "$Logs\ap_finale.log" | Select-String -Pattern "ZAFX|Error:|Traceback|error " | Select-Object -Last 60 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(400, $_.Line.Length)) }
}
if ($Build) {
  "WAIT " + (Wait-Clear)
  & "$UE\Engine\Build\BatchFiles\Build.bat" ZoogiRollEditor Win64 Development "-Project=$Proj" -WaitMutex -NoHotReloadFromIDE *> "$Logs\ap_build.log"
  "BUILD_EXIT=$LASTEXITCODE"; Get-Content "$Logs\ap_build.log" | Select-String -Pattern "error|Result:|Total execution" | Select-Object -Last 15 | ForEach-Object { $_.Line }
}
if ($Capture) {
  "WAIT " + (Wait-Clear); "WAIT " + (Wait-NoGame)
  Remove-Item -Recurse -Force $Out -ErrorAction SilentlyContinue; New-Item -ItemType Directory -Force $Out | Out-Null
  $p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList @("`"$Proj`"", "/Game/Maps/Meadow", "-game", "-ZoogiKFXPreview", "-ZoogiKFXQuit", "-ZoogiNoHUD", "-ZoogiKFXBase=$Base", "`"-ZoogiKFXPlan=$Plan`"", "`"-ZoogiShots=$Out`"", "-RenderOffscreen", "-ResX=1280", "-ResY=720", "-benchmark", "-fps=30", "-unattended", "-nosplash", "-nop4", "-log=ap_capture.log") -PassThru
  if (-not $p.WaitForExit(1200000)) { "CAPTURE_TIMEOUT"; Stop-Process -Id $p.Id -Force }  # only our own process
  "CAPTURE_EXIT=$($p.ExitCode)"
  Get-Content "$Logs\ap_capture.log" -ErrorAction SilentlyContinue | Select-String -Pattern "KFX plan|Error|Fatal" | Select-Object -Last 30 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) }
  $n = (Get-ChildItem $Out -Filter *.png).Count; "SHOTS=$n"
  Compress-Archive -Force -Path "$Out\*" -DestinationPath "$Out.zip"
}
if ($HudShot) {
  # In-game check with the real HUD: -ZoogiUIPolish infuses (forced artifact), locks a target and saves traj_aim with the aim path drawn.
  foreach ($art in $Arts) {
    "WAIT " + (Wait-Clear); "WAIT " + (Wait-NoGame)
    $d = "$Out\hud_$art"; New-Item -ItemType Directory -Force $d | Out-Null
    $p = Start-Process -FilePath "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" -ArgumentList @("`"$Proj`"", "/Game/Maps/Meadow", "-game", "-ZoogiAuto", "-ZoogiUIPolish", "-ZoogiPolishArt=$art", "`"-ZoogiShots=$d`"", "-ResX=1280", "-ResY=720", "-RenderOffscreen", "-unattended", "-nosplash", "-nop4", "-log=ap_hud_$art.log") -PassThru
    $t0 = Get-Date; $got = $false
    while (((Get-Date) - $t0).TotalSeconds -lt 300 -and -not $p.HasExited) { if (Get-ChildItem $d -Recurse -Filter "*traj_aim*" -ErrorAction SilentlyContinue) { $got = $true; Start-Sleep 3; break }; Start-Sleep 2 }
    if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }  # only the process started here
    "HUD_$art=" + $(if ($got) { "OK" } else { "NO_SHOT" }) + " files=" + ((Get-ChildItem $d -Recurse -Filter *.png | % { $_.Name }) -join ',')
  }
}
"ITER_DONE $(Get-Date -Format 'HH:mm:ss')"

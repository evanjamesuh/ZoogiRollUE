# Builds ZoogiRollEditor Win64 Development for the checkout this script lives in (main or a worktree). Log: Saved\Logs\build.log
$ErrorActionPreference = "Continue"
$UE = "C:\Program Files\Epic Games\UE_5.6"
$Root = Split-Path -Parent $PSScriptRoot
$Proj = Join-Path $Root "ZoogiRoll.uproject"
New-Item -ItemType Directory -Force "$Root\Saved\Logs" | Out-Null
$log = "$Root\Saved\Logs\build.log"
& "$UE\Engine\Build\BatchFiles\Build.bat" ZoogiRollEditor Win64 Development "-Project=$Proj" -WaitMutex -NoHotReloadFromIDE *> $log
"BUILD_EXIT=$LASTEXITCODE" | Out-File -Append $log

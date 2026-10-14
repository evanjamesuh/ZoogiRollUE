# Like Scripts\run_python.ps1 but for the checkout this script lives in (main or a worktree).
#   .\Scripts\music\run_python_wt.ps1 Scripts\import_music.py [-LogName music_import]
param([string]$Script, [string]$LogName = "python")
$UE = "C:\Program Files\Epic Games\UE_5.6"
$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Proj = Join-Path $Root "ZoogiRoll.uproject"
$abs = Join-Path $Root $Script
New-Item -ItemType Directory -Force "$Root\Saved\Logs" | Out-Null
$log = "$Root\Saved\Logs\$LogName.log"
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $Proj -run=pythonscript "-script=$abs" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput *> $log
"PY_EXIT=$LASTEXITCODE" | Out-File -Append $log

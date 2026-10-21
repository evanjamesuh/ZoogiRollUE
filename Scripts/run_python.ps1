# Runs an editor Python script headlessly: .\run_python.ps1 Scripts\import_assets.py
param([string]$Script, [string]$LogName = "python")
$UE = "C:\Program Files\Epic Games\UE_5.6"
$Proj = "C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject"
$abs = Join-Path C:\dev\ZoogiRollUnreal $Script
$log = "C:\dev\ZoogiRollUnreal\Saved\Logs\$LogName.log"
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $Proj -run=pythonscript "-script=$abs" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput *> $log
"PY_EXIT=$LASTEXITCODE" | Out-File -Append $log

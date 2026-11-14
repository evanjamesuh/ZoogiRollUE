param([string]$Script, [string]$LogName = "kfx")
$UE = "C:\Program Files\Epic Games\UE_5.6"
$Root = "C:\dev\ZoogiRollUnreal-kfx"
$log = "$Root\Saved\Logs\$LogName.log"
New-Item -ItemType Directory -Force "$Root\Saved\Logs" | Out-Null
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Root\ZoogiRoll.uproject" -run=pythonscript "-script=$Root\Scripts\kfx\$Script" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput *> $log
"PY_EXIT=$LASTEXITCODE" | Out-File -Append $log

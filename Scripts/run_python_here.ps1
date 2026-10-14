param([string]$Script, [string]$LogName = "python")
$UE = "C:\Program Files\Epic Games\UE_5.6"; $Root = Split-Path -Parent $PSScriptRoot
& "$UE\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "$Root\ZoogiRoll.uproject" -run=pythonscript "-script=$Root\$Script" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput *> "$Root\Saved\Logs\$LogName.log"
"PY_EXIT=$LASTEXITCODE" | Out-File -Append "$Root\Saved\Logs\$LogName.log"

# Builds ZoogiRollEditor Win64 Development. Log: Saved\Logs\build.log
$ErrorActionPreference = "Continue"
$UE = "C:\Program Files\Epic Games\UE_5.6"
$Proj = "C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject"
New-Item -ItemType Directory -Force C:\dev\ZoogiRollUnreal\Saved\Logs | Out-Null
$log = "C:\dev\ZoogiRollUnreal\Saved\Logs\build.log"
& "$UE\Engine\Build\BatchFiles\Build.bat" ZoogiRollEditor Win64 Development "-Project=$Proj" -WaitMutex -NoHotReloadFromIDE *> $log
"BUILD_EXIT=$LASTEXITCODE" | Out-File -Append $log

# Packages a Win64 Development build to $Root\Packaged (log: Saved\Logs\package.log)
# -NoHotReloadFromIDE lets UBT build while another editor on this engine has Live Coding running.
param([string]$Root = (Split-Path $PSScriptRoot -Parent))
$UE = "C:\Program Files\Epic Games\UE_5.6"
$log = "$Root\Saved\Logs\package.log"
& "$UE\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\ZoogiRoll.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive "-archivedirectory=$Root\Packaged" -utf8output -unattended -ubtargs=-NoHotReloadFromIDE *> $log
"PACKAGE_EXIT=$LASTEXITCODE" | Out-File -Append $log

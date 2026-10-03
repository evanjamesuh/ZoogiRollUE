# Packages a Win64 Development build to C:\dev\ZoogiRollUnreal\Packaged (log: Saved\Logs\package.log)
# -NoHotReloadFromIDE lets UBT build while another editor on this engine has Live Coding running.
$UE = "C:\Program Files\Epic Games\UE_5.6"
$log = "C:\dev\ZoogiRollUnreal\Saved\Logs\package.log"
& "$UE\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun -project=C:\dev\ZoogiRollUnreal\ZoogiRoll.uproject -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive -archivedirectory=C:\dev\ZoogiRollUnreal\Packaged -utf8output -unattended -ubtargs=-NoHotReloadFromIDE *> $log
"PACKAGE_EXIT=$LASTEXITCODE" | Out-File -Append $log

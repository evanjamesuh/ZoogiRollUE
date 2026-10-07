# Waits until no UnrealBuildTool / normal or -game UnrealEditor / cook / shader worker is running (16 GB box). Prints READY or TIMEOUT.
param([int]$Minutes = 60)
$end = (Get-Date).AddMinutes($Minutes)
while ((Get-Date) -lt $end) {
  $busy = Get-CimInstance Win32_Process | Where-Object {
    ($_.Name -match '^UnrealEditor(-Cmd)?\.exe$') -or
    ($_.Name -match 'UnrealBuildTool|AutomationTool|ShaderCompileWorker|UnrealPak') -or
    ($_.Name -match '^dotnet\.exe$' -and $_.CommandLine -match 'UnrealBuildTool|AutomationTool') -or
    ($_.Name -match '^cmd\.exe$' -and $_.CommandLine -match 'Build\.bat|RunUAT')
  }
  if (-not $busy) { "READY $(Get-Date -Format 'HH:mm:ss')"; exit 0 }
  Start-Sleep -Seconds 15
}
"TIMEOUT $(Get-Date -Format 'HH:mm:ss') busy: " + (($busy | ForEach-Object { "$($_.ProcessId) $($_.Name)" }) -join ', ')
exit 1

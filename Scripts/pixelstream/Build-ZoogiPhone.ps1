# Packages the Pixel Streaming phone build (Win64 Development) of THIS checkout to C:\dev\ZoogiPhone\Game.
# Takes C:\dev\unreal.lock only while UAT runs (waits if someone else holds it) and always releases it.
param([int]$LockWaitMinutes = 240)
$Root = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$UE = 'C:\Program Files\Epic Games\UE_5.6'
$Out = 'C:\dev\ZoogiPhone\Game'
$Lock = 'C:\dev\unreal.lock'
$Tag = 'mobile-pixel-streaming packaging'
New-Item -ItemType Directory -Force "$Root\Saved\Logs", $Out | Out-Null
$log = "$Root\Saved\Logs\phone-package.log"
$deadline = (Get-Date).AddMinutes($LockWaitMinutes)
while ($true) {
  if (-not (Test-Path $Lock)) {
    try { $fs = [System.IO.File]::Open($Lock, 'CreateNew', 'Write'); $w = New-Object System.IO.StreamWriter($fs); $w.Write("$Tag $(Get-Date -Format 'HHmmss') pid=$PID"); $w.Close(); break } catch { Start-Sleep 2; continue }
  }
  if ((Get-Date) -gt $deadline) { "LOCK_TIMEOUT held by: $(Get-Content $Lock -Raw)"; exit 2 }
  "$(Get-Date -Format HH:mm:ss) waiting for lock held by: $((Get-Content $Lock -Raw -ErrorAction SilentlyContinue))"
  Start-Sleep 30
}
"$(Get-Date -Format HH:mm:ss) LOCK_TAKEN"
try {
  & "$UE\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\ZoogiRoll.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive "-archivedirectory=$Out" -utf8output -unattended -ubtargs=-NoHotReloadFromIDE *> $log
  $code = $LASTEXITCODE
  "PACKAGE_EXIT=$code" | Out-File -Append $log
  "$(Get-Date -Format HH:mm:ss) PACKAGE_EXIT=$code"
} finally {
  if ((Test-Path $Lock) -and (Get-Content $Lock -Raw).StartsWith($Tag)) { Remove-Item $Lock -Force; "$(Get-Date -Format HH:mm:ss) LOCK_RELEASED" }
}

# Packages the standalone Windows game (Shipping, Win64) of this checkout to C:\Games\ZoogiRoll and puts a "Zoogi Roll"
# shortcut on the desktop (portrait window). Plays locally: no streaming, no network. Log: Saved\Logs\app-package.log
# Takes C:\dev\unreal.lock only while UAT runs (waits if another Unreal job holds it).
param([string]$Out = 'C:\Games\ZoogiRoll', [string]$Config = 'Shipping', [int]$LockWaitMinutes = 240)
$Root = Split-Path $PSScriptRoot -Parent
$UE = 'C:\Program Files\Epic Games\UE_5.6'
$Lock = 'C:\dev\unreal.lock'
$Tag = 'standalone app packaging'
$log = "$Root\Saved\Logs\app-package.log"
New-Item -ItemType Directory -Force "$Root\Saved\Logs", $Out | Out-Null
$deadline = (Get-Date).AddMinutes($LockWaitMinutes)
while ($true) {
  if (-not (Test-Path $Lock)) {
    try { $fs = [System.IO.File]::Open($Lock, 'CreateNew', 'Write'); $w = New-Object System.IO.StreamWriter($fs); $w.Write("$Tag $(Get-Date -Format 'HHmmss') pid=$PID"); $w.Close(); break } catch { Start-Sleep 2; continue }
  }
  if ((Get-Date) -gt $deadline) { "LOCK_TIMEOUT held by: $(Get-Content $Lock -Raw)"; exit 2 }
  Start-Sleep 30
}
try {
  # a running copy of this app locks its exe and the copy step silently keeps the old one
  Get-CimInstance Win32_Process -Filter "Name='ZoogiRoll.exe'" | Where-Object { $_.ExecutablePath -and $_.ExecutablePath.StartsWith($Out, [StringComparison]::OrdinalIgnoreCase) } | ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
  & "$UE\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\ZoogiRoll.uproject" -noP4 -platform=Win64 "-clientconfig=$Config" -build -cook -stage -pak -archive "-archivedirectory=$Out" -prereqs -utf8output -unattended -ubtargs=-NoHotReloadFromIDE *> $log
  $code = $LASTEXITCODE
  "PACKAGE_EXIT=$code" | Out-File -Append $log
  "PACKAGE_EXIT=$code"
  $fails = (Select-String -Path $log -Pattern 'Failed to copy' | Measure-Object).Count
  if ($fails) { "WARNING: $fails files could not be copied (game running?) - see $log" }
  $exe = Join-Path $Out 'Windows\ZoogiRoll.exe'
  if ($code -eq 0 -and (Test-Path $exe)) {
    $lnk = Join-Path ([Environment]::GetFolderPath('Desktop')) 'Zoogi Roll.lnk'
    $sh = (New-Object -ComObject WScript.Shell).CreateShortcut($lnk)
    $sh.TargetPath = $exe
    $sh.Arguments = '-Windowed -ResX=720 -ResY=1280'
    $sh.WorkingDirectory = Split-Path $exe
    $sh.Description = 'Zoogi Roll (local, portrait)'
    $sh.Save()
    "SHORTCUT=$lnk"
  }
} finally {
  if ((Test-Path $Lock) -and (Get-Content $Lock -Raw).StartsWith($Tag)) { Remove-Item $Lock -Force }
}

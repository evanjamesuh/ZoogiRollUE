# Packages the Android tablet build (Shipping, arm64, ASTC) to C:\Games\ZoogiRollAndroid and copies the APK to
# C:\Games\ZoogiRoll.apk. Needs: Android target support in the Epic Games Launcher (Engine\Build\Android), the SDK / NDK
# at %ANDROID_HOME% (see [/Script/AndroidPlatformEditor.AndroidSDKSettings] in DefaultEngine.ini) and JDK 17.
# Optional -Install: adb install -r to the connected tablet (USB debugging on). Log: Saved\Logs\android-package.log
param([string]$Out = 'C:\Games\ZoogiRollAndroid', [string]$Config = 'Shipping', [switch]$Install, [int]$LockWaitMinutes = 240)
$Root = Split-Path $PSScriptRoot -Parent
$UE = 'C:\Program Files\Epic Games\UE_5.6'
$Lock = 'C:\dev\unreal.lock'
$Tag = 'android packaging'
$log = "$Root\Saved\Logs\android-package.log"
if (-not (Test-Path "$UE\Engine\Build\Android\Java")) { 'MISSING: Android target support. Epic Games Launcher > Library > UE 5.6 > Options > Target Platforms > Android.'; exit 3 }
$env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"; $env:ANDROID_SDK_ROOT = $env:ANDROID_HOME
$env:NDKROOT = "$env:ANDROID_HOME\ndk\25.1.8937393"; $env:NDK_ROOT = $env:NDKROOT
$env:JAVA_HOME = 'C:\Program Files\Microsoft\jdk-17.0.20.101-hotspot'; $env:Path = "$env:JAVA_HOME\bin;$env:ANDROID_HOME\platform-tools;$env:Path"
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
  & "$UE\Engine\Build\BatchFiles\RunUAT.bat" BuildCookRun "-project=$Root\ZoogiRoll.uproject" -noP4 -platform=Android -cookflavor=ASTC "-clientconfig=$Config" -build -cook -stage -pak -package -archive "-archivedirectory=$Out" -utf8output -unattended -ubtargs=-NoHotReloadFromIDE *> $log
  $code = $LASTEXITCODE
  "PACKAGE_EXIT=$code" | Out-File -Append $log
  "PACKAGE_EXIT=$code"
  $apk = Get-ChildItem $Out -Recurse -Filter *.apk -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
  if ($code -eq 0 -and $apk) {
    Copy-Item $apk.FullName 'C:\Games\ZoogiRoll.apk' -Force
    "APK=C:\Games\ZoogiRoll.apk ($([int]($apk.Length/1MB)) MB)"
    if ($Install) { & adb install -r 'C:\Games\ZoogiRoll.apk' }
  }
} finally {
  if ((Test-Path $Lock) -and (Get-Content $Lock -Raw).StartsWith($Tag)) { Remove-Item $Lock -Force }
}

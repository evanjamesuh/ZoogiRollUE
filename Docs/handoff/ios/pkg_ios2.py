# iOS package run #2: same as pkg_ios.py + live copy of the UAT log, then closes the editor (Live Coding blocks the editor build).
import subprocess, os
exec(open('C:/dev/ios/pkg_ios.py').read())
mon = r"while(Test-Path C:\dev\unreal.lock){ Copy-Item \"$env:APPDATA\Unreal Engine\AutomationTool\Logs\C+Program+Files+Epic+Games+UE_5.6\Log.txt\" C:\dev\ios\uat_live.txt -ErrorAction SilentlyContinue; Start-Sleep 20 }; Copy-Item \"$env:APPDATA\Unreal Engine\AutomationTool\Logs\C+Program+Files+Epic+Games+UE_5.6\Log.txt\" C:\dev\ios\uat_live.txt"
subprocess.Popen(['powershell', '-NoProfile', '-WindowStyle', 'Hidden', '-Command', 'Start-Sleep 5; ' + mon], creationflags=0x00000008 | 0x00000200, close_fds=True)
print('MONITOR_STARTED; quitting editor')
import unreal
unreal.SystemLibrary.quit_editor()

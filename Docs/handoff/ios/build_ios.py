# Compile ZoogiRoll for iOS on the Mac (UBT remote build). Detached; log -> C:/dev/ios/build_ios.log
import subprocess, os
open('C:/dev/unreal.lock','w').write('iOS remote compile (Claude, ZoogiRollUnreal-combined)')
cmd = r'"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" ZoogiRoll IOS Development -Project="C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject" -WaitMutex -NoHotReload > C:\dev\ios\build_ios.log 2>&1 && (echo BUILD_OK>> C:\dev\ios\build_ios.log) || (echo BUILD_FAIL>> C:\dev\ios\build_ios.log) & del C:\dev\unreal.lock'
subprocess.Popen('cmd /c "' + cmd + '"', creationflags=0x00000008 | 0x00000200, close_fds=True)
print('IOS_BUILD_STARTED')

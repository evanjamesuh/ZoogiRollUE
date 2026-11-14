# Full iOS package (Shipping, distribution signing) via UAT; remote compile on the MacinCloud Mac. Log -> C:/dev/ios/pkg_ios.log
import subprocess
open('C:/dev/unreal.lock','w').write('iOS package (Claude, ZoogiRollUnreal-combined)')
uat = r'"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat"'
args = r' BuildCookRun -project="C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject" -noP4 -platform=IOS -clientconfig=Shipping -distribution -build -cook -stage -pak -package -archive -archivedirectory="C:\dev\pkg-ios" -unattended -utf8output'
log = r'C:\dev\ios\pkg_ios.log'
cmd = uat + args + ' > ' + log + r' 2>&1 && (echo PKG_OK>> ' + log + r') || (echo PKG_FAIL>> ' + log + r') & del C:\dev\unreal.lock'
subprocess.Popen('cmd /c "' + cmd + '"', creationflags=0x00000008 | 0x00000200, close_fds=True)
print('IOS_PKG_STARTED')

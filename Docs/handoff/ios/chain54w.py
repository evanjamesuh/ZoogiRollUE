# Nov 1 (Claude): Wolf Pack 2.0 - editor build, S_Howl import, wolf test capture
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\p5run.py', 'none'])
st = open('C:/dev/ios/p5_status.txt').read()
open('C:/dev/ios/chain54w_status.txt', 'w').write('BUILD ' + ('OK' if 'BUILD_RC=0' in st else 'FAIL') + '\n')
if 'BUILD_RC=0' in st:
    subprocess.run([py, r'C:\dev\ios\sfx_import_run.py'])
    open('C:/dev/ios/chain54w_status.txt', 'a').write('SFX ' + open('C:/dev/ios/sfx_import_status.txt').read().replace('\n', ' ') + '\n')
    subprocess.run([py, r'C:\dev\ios\p5run.py', 'wolf_test'])
    open('C:/dev/ios/chain54w_status.txt', 'a').write('SHOTS DONE\n')

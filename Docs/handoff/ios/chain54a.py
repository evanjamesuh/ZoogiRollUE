# Nov 1 (Claude): Ancient Meadows v2 - editor build, kit import (new tiles, sky, M_AncientDecal), anc_shots capture
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\p5run.py', 'none'])
st = open('C:/dev/ios/p5_status.txt').read()
open('C:/dev/ios/chain54a_status.txt', 'w').write('BUILD ' + ('OK' if 'BUILD_RC=0' in st else 'FAIL') + '\n')
if 'BUILD_RC=0' in st:
    subprocess.run([py, r'C:\dev\ios\ancient_kit_run.py'])
    open('C:/dev/ios/chain54a_status.txt', 'a').write('KIT ' + open('C:/dev/ios/ancient_kit_status.txt').read().replace('\n', ' ') + '\n')
    subprocess.run([py, r'C:\dev\ios\p5run.py', 'anc_shots'])
    open('C:/dev/ios/chain54a_status.txt', 'a').write('SHOTS DONE\n')

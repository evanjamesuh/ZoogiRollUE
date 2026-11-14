# Nov 1 (Claude): editor build (compile check) then iOS build 50 -> TestFlight
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
r = subprocess.run([py, r'C:\dev\ios\p5run.py', 'anc_motion'])
st = open('C:/dev/ios/p5_status.txt').read()
if 'BUILD_RC=0' in st:
    subprocess.run([py, r'C:\dev\ios\b50ios.py'])
else:
    open('C:/dev/ios/b50ios_status.txt', 'w').write('SKIPPED: editor build failed\n')

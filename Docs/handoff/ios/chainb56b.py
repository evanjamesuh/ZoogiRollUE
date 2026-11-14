# Nov 1 b56 (Claude): captures (drag hint, v14 arena with compiled shaders) then TestFlight build 56
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\p5run.py', 'classic_clean,anc_shots'])
st = open('C:/dev/ios/p5_status.txt').read()
if 'BUILD_RC=0' in st:
    subprocess.run([py, r'C:\dev\ios\b56ios.py'])
else:
    open('C:/dev/ios/b56ios_status.txt', 'w').write('SKIPPED: editor build failed\n')

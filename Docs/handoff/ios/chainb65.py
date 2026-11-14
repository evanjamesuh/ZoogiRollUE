# Nov 2 b65 (Claude): build + inventory captures, then TestFlight build 65
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\p5run.py', 'front_iphone,wolf_front'])
st = open('C:/dev/ios/p5_status.txt').read()
if 'BUILD_RC=0' in st:
    subprocess.run([py, r'C:\dev\ios\b65ios.py'])
else:
    open('C:/dev/ios/b65ios_status.txt', 'w').write('SKIPPED: editor build failed\n')

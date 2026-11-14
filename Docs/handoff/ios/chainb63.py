# Nov 1 b63 (Claude): build + captures, then TestFlight build 63
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\p5run.py', 'none'])
st = open('C:/dev/ios/p5_status.txt').read()
if 'BUILD_RC=0' in st:
    subprocess.run([py, r'C:\dev\ios\b63ios.py'])
else:
    open('C:/dev/ios/b63ios_status.txt', 'w').write('SKIPPED: editor build failed\n')

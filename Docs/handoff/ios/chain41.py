# Oct 31 (Claude): build 43 - editor build check, then iOS cook + stage + Mac build + TestFlight upload (b48ios_HOLD.py).
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\p5run.py', 'map_volcanic,map_cosmic,map_neon,map_tomb'])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' in st: subprocess.run([py, r'C:\dev\ios\b48ios_HOLD.py'])
else: open(r'C:\dev\ios\b47ios_status.txt', 'w').write('SKIPPED: editor build failed\n')

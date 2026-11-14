# Nov 1 b57 (Claude): build + captures, then TestFlight build 57
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\p5run.py', 'crit_shatter,clash_ult,front_port,classic_clean'])
st = open('C:/dev/ios/p5_status.txt').read()
if 'BUILD_RC=0' in st:
    subprocess.run([py, r'C:\dev\ios\b57ios.py'])
else:
    open('C:/dev/ios/b57ios_status.txt', 'w').write('SKIPPED: editor build failed\n')

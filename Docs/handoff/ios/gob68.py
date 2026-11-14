# Nov 2 b68 (Claude): wait for C:\dev\unreal.lock to clear (Android package), compile + clash capture, then TestFlight build 68
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
t0 = time.time()
while os.path.exists(r'C:\dev\unreal.lock') and time.time() - t0 < 5400: time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "clash_ult,turn_iphone"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' in st:
    subprocess.run([py, r"C:\dev\ios\b68ios.py"])
else:
    open(r'C:\dev\ios\b68ios_status.txt', 'w').write('SKIPPED: editor build failed\n')

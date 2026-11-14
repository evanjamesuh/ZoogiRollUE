# Nov 2 b73 (Claude): compile + captures now, then (after the b72 Mac build is done) TestFlight build 70
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "turn_iphone"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b73ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
t0 = time.time()
while time.time() - t0 < 3600:
    s72 = open(r'C:\dev\ios\b72ios_status.txt').read()
    if s72.count('DONE') >= 1: break
    time.sleep(20)
subprocess.run([py, r"C:\dev\ios\b73ios.py"])

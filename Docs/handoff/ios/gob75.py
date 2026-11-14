# Nov 2 b75 (Claude): compile + captures now, then (after the b74 Mac build is done) TestFlight build 70
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "turn_iphone,turn_land"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b75ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
t0 = time.time()
while time.time() - t0 < 3600:
    s74 = open(r'C:\dev\ios\b74ios_status.txt').read()
    if s74.count('DONE') >= 1: break
    time.sleep(20)
subprocess.run([py, r"C:\dev\ios\b75ios.py"])

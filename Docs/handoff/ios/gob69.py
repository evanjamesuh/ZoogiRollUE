# Nov 2 b69 (Claude): compile + captures now, then (after the b68 Mac build is done) TestFlight build 69
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "turn_land,turn_iphone"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b69ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
t0 = time.time()
while time.time() - t0 < 3600:
    s68 = open(r'C:\dev\ios\b68ios_status.txt').read()
    if s68.count('DONE') >= 2: break
    time.sleep(20)
subprocess.run([py, r"C:\dev\ios\b69ios.py"])

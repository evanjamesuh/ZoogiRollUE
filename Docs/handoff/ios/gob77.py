# Nov 3 b77 (Claude): compile + captures, then TestFlight build 77
import subprocess, time, os
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "turn_iphone,turn_land"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b77ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
t0 = time.time()
while time.time() - t0 < 3600:
    s75 = open(r'C:\dev\ios\b76ios_status.txt').read()
    if 'DONE' in s75: break
    time.sleep(20)
subprocess.run([py, r"C:\dev\ios\b77ios.py"])

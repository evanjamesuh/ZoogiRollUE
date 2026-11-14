# Nov 4 b120 phase C (Claude): map select proofs with the new portrait art, then TestFlight build 120, then the Android package
import subprocess, time, os
open(r'C:\dev\ios\gob120_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'C start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "map_select_port,map_select,front_iphone"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b120ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
subprocess.run([py, r"C:\dev\ios\b120ios.py"])
subprocess.run([py, r"C:\dev\ios\androidrun.py"])

# Nov 4 b111 (Claude): compile + hand test, TestFlight build 111, then the Android tablet package
import subprocess, time, os
open(r'C:\dev\ios\gob111_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "hand_test"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b111ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
subprocess.run([py, r"C:\dev\ios\b111ios.py"])
subprocess.run([py, r"C:\dev\ios\androidrun.py"])

# Nov 3 b107 (Claude): compile + defend window soak, TestFlight build 105, then the Android tablet package for Grok's playtest
import subprocess, time, os
open(r'C:\dev\ios\gob107_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "duel_soak,skills_land"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b107ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
while not os.path.exists(r'C:\dev\ios\b106ios_status.txt') or 'DONE' not in open(r'C:\dev\ios\b106ios_status.txt').read(): time.sleep(30)
subprocess.run([py, r"C:\dev\ios\b107ios.py"])
subprocess.run([py, r"C:\dev\ios\androidrun.py"])

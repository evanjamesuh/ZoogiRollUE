# Nov 3 b104 (Claude): compile + duel soak + skills, then TestFlight build 104 (b103 was held for the field duels)
import subprocess, time, os
open(r'C:\dev\ios\gob104_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "duel_soak,skills_land"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b104ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
subprocess.run([py, r"C:\dev\ios\b104ios.py"])

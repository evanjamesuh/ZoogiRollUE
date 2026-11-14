# Nov 3 b101 (Claude): compile (no captures), then the MASTERCLASS film scenes
import subprocess, time, os
open(r'C:\dev\ios\gob101_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "none"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\film_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
subprocess.run([py, r"C:\dev\ios\film_run.py", open(r'C:\dev\ios\film_only.txt').read().strip() if os.path.exists(r'C:\dev\ios\film_only.txt') else ''])

# Nov 3 b103 (Claude): import the epic SFX, compile + captures, then TestFlight build 103
import subprocess, time, os
open(r'C:\dev\ios\gob103_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\epic_sfx_import_run.py"])
subprocess.run([py, r"C:\dev\ios\p5run.py", "clash_ult,skills_land"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b103ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
t0 = time.time()
while time.time() - t0 < 3600:
    s75 = open(r'C:\dev\ios\b102ios_status.txt').read()
    if 'DONE' in s75: break
    time.sleep(20)
subprocess.run([py, r"C:\dev\ios\b103ios.py"])

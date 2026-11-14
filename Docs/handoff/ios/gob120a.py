# Nov 4 b120 phase A (Claude): compile + inventory / weapons captures + portrait map shots (art made from these before phase B ships)
import subprocess, time, os
open(r'C:\dev\ios\gob120_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'A start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "front_port,weapons_royale,pmap_frozen,pmap_meadow,pmap_volcanic,pmap_cosmic,pmap_arabian,pmap_tomb,pmap_neon,pmap_teaser"])

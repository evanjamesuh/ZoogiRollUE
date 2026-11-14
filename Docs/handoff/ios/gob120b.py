# Nov 4 b120 phase B (Claude): compile the 17:50 fixes + portrait inventory check + portrait arena shots at 720x1280 (864x1536 does not fit this monitor)
import subprocess, time, os
open(r'C:\dev\ios\gob120_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'B start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
time.sleep(5)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
import shutil, glob
for d in glob.glob(r'C:\dev\proofs\p5_style\pmap_*'):   # keep phase A's 1920x1080 shots for the landscape cards
    dst = d.replace('pmap_', 'lmap_')
    shutil.rmtree(dst, ignore_errors=True); os.makedirs(dst, exist_ok=True)
    for f in glob.glob(os.path.join(d, 'arena_*.png')): shutil.copy2(f, dst)
open(r'C:\dev\ios\gob120_hb.txt','a').write(time.strftime('%H:%M:%S ') + 'B copied lmap\n')
subprocess.run([py, r"C:\dev\ios\p5run.py", "front_port,pmap_frozen,pmap_meadow,pmap_volcanic,pmap_cosmic,pmap_arabian,pmap_tomb,pmap_neon,pmap_teaser"])

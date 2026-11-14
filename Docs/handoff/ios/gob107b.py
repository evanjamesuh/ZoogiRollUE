# Nov 3 b107b (Claude): after the Android package (stale-obj fix), compile + duel/skills soak, then TestFlight build 107
import subprocess, time, os
HB = r'C:\dev\ios\gob107b_hb.txt'
open(HB,'a').write(time.strftime('%H:%M:%S ') + 'start\n')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
T0 = time.time()
AO = r'C:\dev\ios\android_pkg.out'
def android_done():
    try: return os.path.getmtime(AO) > T0 and 'PACKAGE_EXIT' in open(AO, encoding='utf-8', errors='replace').read()
    except Exception: return False
while not android_done(): time.sleep(20)
open(HB,'a').write(time.strftime('%H:%M:%S ') + 'android done\n')
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "duel_soak,skills_land"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b107ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
try: os.remove(r'C:\dev\ios\b107ios.started')
except Exception: pass
open(HB,'a').write(time.strftime('%H:%M:%S ') + 'ios start\n')
subprocess.run([py, r"C:\dev\ios\b107ios.py"])

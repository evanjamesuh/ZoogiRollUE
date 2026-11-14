# Nov 4 b109 (Claude): after the b108 Android package, compile + lab/duel captures, then TestFlight build 109
import subprocess, time, os
HB = r'C:\dev\ios\gob109_hb.txt'
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
subprocess.run([py, r"C:\dev\ios\p5run.py", "skill_lab,duel_soak"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b109ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
try: os.remove(r'C:\dev\ios\b109ios.started')
except Exception: pass
open(HB,'a').write(time.strftime('%H:%M:%S ') + 'ios start\n')
subprocess.run([py, r"C:\dev\ios\b109ios.py"])

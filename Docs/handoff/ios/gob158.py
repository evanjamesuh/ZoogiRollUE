# Nov 6 b158 (Claude): trick shot = start-up pop-up, trick shot plaza without the central drums, solid titan, Z flags, thinner ON FIRE -> TestFlight 158
import subprocess, time, os
open(r'C:\dev\ios\gob120_hb.txt','a').write(time.strftime('%H:%M:%S ') + '158 start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while 'DONE' not in (open(r'C:\dev\ios\b157ios_status.txt').read() if os.path.exists(r'C:\dev\ios\b157ios_status.txt') else 'DONE'): time.sleep(15)  # b157 upload first
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "trick_ancient,boss_meadow,raid_test,front_land,menu_land"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b158ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
subprocess.run([py, r"C:\dev\ios\b158ios.py"])

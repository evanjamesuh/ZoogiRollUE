# Nov 6 b160 (Claude): trick shot = start-up pop-up, trick shot plaza without the central drums, solid titan, Z flags, thinner ON FIRE -> TestFlight 160
import subprocess, time, os
open(r'C:\dev\ios\gob120_hb.txt','a').write(time.strftime('%H:%M:%S ') + '160 start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while 'DONE' not in (open(r'C:\dev\ios\b159ios_status.txt').read() if os.path.exists(r'C:\dev\ios\b159ios_status.txt') else 'DONE'): time.sleep(15)  # b159 upload first
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "boss_meadow,boss_frozen,boss_volcano,boss_arabian,boss_tomb,boss_neon,boss_test,trick_ancient"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b160ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
subprocess.run([py, r"C:\dev\ios\b160ios.py"])

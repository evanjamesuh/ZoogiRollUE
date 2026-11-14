# Nov 4 b120 b121 (Claude): REDIRECT tap fix + button routing, captures, TestFlight 122 (TestFlight only - Evan 20:19)
import subprocess, time, os
open(r'C:\dev\ios\gob120_hb.txt','a').write(time.strftime('%H:%M:%S ') + '147 start lock=%s\n' % os.path.exists(r'C:\dev\unreal.lock'))
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while 'DONE' not in (open(r'C:\dev\ios\b146ios_status.txt').read() if os.path.exists(r'C:\dev\ios\b146ios_status.txt') else 'DONE'): time.sleep(15)  # b146 upload first
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "boss_test"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b147ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
subprocess.run([py, r"C:\dev\ios\b147ios.py"])

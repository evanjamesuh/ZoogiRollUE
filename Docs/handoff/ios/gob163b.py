# Nov 6 (Claude): b163 retry = soundscape + league court + ANIME DIRECTOR (compile fixes: include order, CourtPieces, final-round check) -> TestFlight 163
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '163b ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "court_match,story_spar,story_ep3,boss_frozen,front_land"])
s = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in s:
    open(r'C:\dev\ios\b163ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('tests done -> ios')
subprocess.run([py, r"C:\dev\ios\b163ios.py"])

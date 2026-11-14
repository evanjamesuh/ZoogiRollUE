# Nov 6 (Claude): b164 retry - the PC ran out of memory for the compiler (C1076/C3859) at 15:06; patch_b164r is already applied
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '164b ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "pmap_ancient,trick_ancient,front_land"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): break
    hb('build failed (attempt %d) - waiting 3 min' % (attempt + 1)); time.sleep(180)
else:
    open(r'C:\dev\ios\b164ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED x3'); raise SystemExit
hb('waiting for map cards')
while not os.path.exists(r'C:\dev\ios\b164_cards_ok'): time.sleep(15)
hb('cards ok -> ios')
subprocess.run([py, r"C:\dev\ios\b164ios.py"])

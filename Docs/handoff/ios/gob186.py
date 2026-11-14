# Nov 8 b186 (Claude): SHOWTIME stuck fix (theme length), old awaken renamed AWAKEN
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '186 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "gattling_test"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b186ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('waiting for the dev check (b186_cards_ok)')
while not os.path.exists(r'C:\dev\ios\b186_cards_ok'): time.sleep(15)
hb('-> ios')
subprocess.run([py, r"C:\dev\ios\b186ios.py"])

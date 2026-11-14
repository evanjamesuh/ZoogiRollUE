# Nov 6 b166 (Claude): chest pop-ups open (Evan 17:10 "I want the chest to open", 18:08 "All chest pop up types"):
# common = classic burst, rare = card fan, epic = P5 slash reveal; lid/base PNGs in MenuArt/Front; NEWS B166 -> TestFlight 166
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '166 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "chest_land,chest_port"])
    s = open(r'C:\dev\ios\p5_status.txt').read()
    if 'BUILD_RC=0' in s: ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b166ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('waiting for the chest check (b166_cards_ok)')
while not os.path.exists(r'C:\dev\ios\b166_cards_ok'): time.sleep(15)
hb('-> ios')
subprocess.run([py, r"C:\dev\ios\b166ios.py"])

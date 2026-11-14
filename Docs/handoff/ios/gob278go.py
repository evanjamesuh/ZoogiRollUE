# Nov 12 (Claude): B278 - editor build + chest/menu/awaken/match previews, then iOS 278 (B253 configs, no Voyager plugins)
import subprocess, os, time
try: os.close(os.open(r'C:\dev\ios\b278go.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '278 ' + s + '\n')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
hb('start - waiting for B277')
while not os.path.exists(r'C:\dev\ios\b277ios_status.txt') or 'DONE' not in open(r'C:\dev\ios\b277ios_status.txt').read(): time.sleep(20)
hb('B277 done -> go')
ok = False
for attempt in range(2):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b278_react"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(60)
if not ok:
    open(r'C:\dev\ios\b278ios_status.txt', 'w').write('EDITOR BUILD FAILED\n'); hb('BUILD FAILED'); raise SystemExit
hb('preview done -> ios')
subprocess.run([py, r"C:\dev\ios\b278ios.py"])
hb('ios done')

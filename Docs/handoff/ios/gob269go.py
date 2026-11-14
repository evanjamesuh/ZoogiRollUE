# Nov 12 (Claude): B269 - editor build + chest/menu/awaken/match previews, then iOS 269 (B253 configs, no Voyager plugins)
import subprocess, os, time
try: os.close(os.open(r'C:\dev\ios\b269go.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '269 ' + s + '\n')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
hb('start - waiting for B268 to finish')
while 'DONE' not in open(r'C:\dev\ios\b268ios_status.txt').read(): time.sleep(20)
hb('B268 done -> go')
ok = False
for attempt in range(2):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b269_match,b269_raid"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(60)
if not ok:
    open(r'C:\dev\ios\b269ios_status.txt', 'w').write('EDITOR BUILD FAILED\n'); hb('BUILD FAILED'); raise SystemExit
hb('preview done -> ios')
subprocess.run([py, r"C:\dev\ios\b269ios.py"])
hb('ios done')

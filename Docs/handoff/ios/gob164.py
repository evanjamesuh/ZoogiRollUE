# Nov 6 b164 (Claude): Ancient Meadows back to normal size (v14 default, -ZoogiAncientV15 = the big ring) -> map card -> TestFlight 164
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '164 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
def st(n): p = r'C:\dev\ios\b%dios_status.txt' % n; return open(p).read() if os.path.exists(p) else ''
while 'SOURCES_UP' not in st(163) and 'DONE' not in st(163) and 'SKIPPED' not in st(163): time.sleep(15)
r = subprocess.run([py, r'C:\dev\ios\patch_b164r.py'], capture_output=True, text=True); hb('patch ' + (r.stdout + r.stderr).strip()[-160:])
if 'b164r ok' not in r.stdout: hb('PATCH FAILED'); raise SystemExit
while 'DONE' not in st(163) and 'SKIPPED' not in st(163): time.sleep(15)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "pmap_ancient,trick_ancient,front_land"])
s = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in s:
    open(r'C:\dev\ios\b164ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('waiting for map cards')
while not os.path.exists(r'C:\dev\ios\b164_cards_ok'): time.sleep(15)
hb('cards ok -> ios')
subprocess.run([py, r"C:\dev\ios\b164ios.py"])

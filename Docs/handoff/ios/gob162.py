# Nov 6 b162 (Claude): Ancient Collision Meadows v15 (plaza ring x2, map pushed out) - patch, import, see-through, tests, new map card, iOS
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '162 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
def st161(): return open(r'C:\dev\ios\b161ios_status.txt').read() if os.path.exists(r'C:\dev\ios\b161ios_status.txt') else ''
while 'SOURCES_UP' not in st161() and 'DONE' not in st161(): time.sleep(15)   # b161 sources are on the Mac before b162 edits
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
r = subprocess.run([py, r'C:\dev\ios\patch_b162a.py'], capture_output=True, text=True); hb('patch ' + (r.stdout + r.stderr).strip()[-200:])
if 'b162a ok' not in r.stdout: hb('PATCH FAILED'); raise SystemExit
while 'DONE' not in st161(): time.sleep(15)   # the b161 runner reopens the editor at the end
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('b162 v15 arena import (Claude)')
UE = r'C:\Program Files\Epic Games\UE_5.6'; P = r'C:\dev\ZoogiRollUnreal-p5style'
for nm, sc in (('import', r'Scripts\arenas\ancient\import_v15.py'), ('seethrough', r'Scripts\camera\make_seethrough.py')):
    with open(r'C:\dev\ios\%s162.log' % nm, 'w') as lf:
        rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s\\ZoogiRoll.uproject" -run=pythonscript -script="%s\\%s" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P, P, sc), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
    hb('%s UE_RC=%d' % (nm, rc))
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
subprocess.run([py, r"C:\dev\ios\p5run.py", "pmap_ancient,anc_classic,trick_ancient,royale_mix,front_land"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b162ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('waiting for map cards')
while not os.path.exists(r'C:\dev\ios\b162_cards_ok'): time.sleep(15)
hb('cards ok -> ios')
subprocess.run([py, r"C:\dev\ios\b162ios.py"])

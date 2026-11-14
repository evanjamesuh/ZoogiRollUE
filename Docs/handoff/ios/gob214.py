# Nov 9 b214 (Claude): Tripo stone golem (purple) = Meadow titan, Tripo crystal brute = Dark Realm titan
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '214 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject'
LOCK = r'C:\dev\unreal.lock'
while os.path.exists(LOCK): time.sleep(15)
open(LOCK, 'w').write('b214 Tripo boss import (Claude)')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(5)
hb('import')
with open(r'C:\dev\ios\golem_import_run.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s" -run=pythonscript -script="C:\\dev\\ios\\golem_import.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
hb('import rc=%d; reduce' % rc)
try:
    subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" -ExecutePythonScript="C:\\dev\\ios\\golem_reduce.py" -nosplash' % (UE, P), shell=True, timeout=1500)
except Exception as e: hb('reduce timeout %r' % e)
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(5)
try: os.remove(LOCK)
except Exception: pass
hb('reduce done')
ok = False
for attempt in range(3):
    subprocess.run([py, r"C:\dev\ios\p5run.py", "b214_meadow,b214_dark"])
    if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read(): ok = True; break
    hb('build failed, attempt %d' % (attempt + 1)); time.sleep(90)
if not ok:
    open(r'C:\dev\ios\b214ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
# standing rule: every build goes straight to TestFlight
open(r'C:\dev\ios\b214_cards_ok', 'w').write('auto')
hb('-> ios (background)')
subprocess.Popen([py, r"C:\dev\ios\b214ios.py"], creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
t0 = time.time()
while time.time() - t0 < 2700:
    st = open(r'C:\dev\ios\b214ios_status.txt').read() if os.path.exists(r'C:\dev\ios\b214ios_status.txt') else ''
    if 'SOURCES_UP' in st or 'FAIL' in st or 'SKIP' in st or 'RC=1' in st: break
    time.sleep(20)
time.sleep(20)
while os.path.exists(LOCK): time.sleep(15)
hb('trailer footage')
TR = 'tr_demo,tr_demo_p,tr_raid_meadow,tr_raid_volc,tr_raid_frozen,tr_raid_dark,tr_ballers,tr_aw_blue,tr_aw_vex,tr_aw_frosti,tr_aw_hotstreak,tr_aw_lumi,tr_howto,tr_skills'
subprocess.run([py, r"C:\dev\ios\p5run.py", TR])
open(r'C:\dev\ios\trailer_footage_done', 'w').write('ok')
hb('trailer footage done')

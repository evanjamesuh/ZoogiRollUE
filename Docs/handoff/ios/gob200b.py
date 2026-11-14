# Nov 9 b200 (Claude): PC compile check, then open the gate for the parked gob200 -> iOS / TestFlight
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '200b ' + s + '\n')
hb('start (compile check)')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "none"])
if 'BUILD_RC=0' in open(r'C:\dev\ios\p5_status.txt').read():
    open(r'C:\dev\ios\b200_cards_ok', 'w').write('ok\n'); hb('compile ok -> gate open')
else: hb('COMPILE FAILED')

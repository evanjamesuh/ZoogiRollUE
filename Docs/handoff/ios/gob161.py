# Nov 6 b161 (Claude): Ancient without the centre drums, see-through cut-out (MPC_ZoogiCam + Ancient materials), new Ancient map art
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '161 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while 'DONE' not in (open(r'C:\dev\ios\b160ios_status.txt').read() if os.path.exists(r'C:\dev\ios\b160ios_status.txt') else 'DONE'): time.sleep(15)
while 'DONE' not in open(r'C:\dev\ios\p5_status.txt').read(): time.sleep(15)   # the old shots run first
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('b161 see-through materials (Claude)')
UE = r'C:\Program Files\Epic Games\UE_5.6'; P = r'C:\dev\ZoogiRollUnreal-p5style'
with open(r'C:\dev\ios\seethrough161.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Binaries\\Win64\\UnrealEditor-Cmd.exe" "%s\\ZoogiRoll.uproject" -run=pythonscript -script="%s\\Scripts\\camera\\make_seethrough.py" -unattended -nop4 -nosplash -NullRHI -stdout -FullStdOutLogOutput' % (UE, P, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
hb('seethrough UE_RC=%d' % rc)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
subprocess.run([py, r"C:\dev\ios\p5run.py", "anc_shots,pmap_ancient,trick_ancient,anc_match"])
st = open(r'C:\dev\ios\p5_status.txt').read()
if 'BUILD_RC=0' not in st:
    open(r'C:\dev\ios\b161ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); raise SystemExit
hb('waiting for map cards')
while not os.path.exists(r'C:\dev\ios\b161_cards_ok'): time.sleep(15)   # Claude makes ms_map_ancient(.png/_p.jpg) from the shots
hb('cards ok -> ios')
subprocess.run([py, r"C:\dev\ios\b161ios.py"])

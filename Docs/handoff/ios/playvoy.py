# Nov 11 (Claude): open the Zoogi build straight into a Voyager shooter level for a played demo (no auto-kill; closes after 15 min)
import subprocess, time, os, sys
UE = r'C:\Program Files\Epic Games\UE_5.6'; P = r'C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject'
lvl = sys.argv[1] if len(sys.argv) > 1 else '1'
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(3)
open('C:/dev/unreal.lock', 'w').write('Voyager played demo (Claude)')
args = [UE + r'\Engine\Binaries\Win64\UnrealEditor.exe', P, '-game', '-windowed', '-ResX=1600', '-ResY=900', '-WinX=150', '-WinY=60', '-ZoogiNoSplash', '-ZoogiNoTutorial', '-ZoogiVoyLevel=' + lvl, '-log', r'-abslog=C:\dev\ios\playvoy.log', '-nosplash']
p = subprocess.Popen(args, creationflags=0x08000000)
try: p.wait(timeout=900)
except Exception: p.kill()
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
open(r'C:\dev\ios\playvoy_status.txt', 'w').write('DONE\n')

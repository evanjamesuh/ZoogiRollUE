# Nov 10 (Claude): run one Voyager demo level for a played + Game Bar recorded demo (Evan's Voyager TPS). Stops on ios\voyplay.stop or 15 min.
import subprocess, time, os, sys
ST = r'C:\dev\ios\voy_status.txt'
def st(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
L = open(r'C:\dev\ios\voyplay_level.txt').read().strip() if os.path.exists(r'C:\dev\ios\voyplay_level.txt') else 'LevelWeapons'
while os.path.exists(r'C:\dev\unreal.lock') and 'Voyager' not in open(r'C:\dev\unreal.lock').read(): time.sleep(10)
open(r'C:\dev\unreal.lock', 'w').write('Voyager TPS: played demo %s (Claude)' % L)
STOP = r'C:\dev\ios\voyplay.stop'
if os.path.exists(STOP): os.remove(STOP)
UE = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe'
p = subprocess.Popen([UE, r'C:\dev\Voyager\VoyagerThirdPersonShooter.uproject', '/Game/Voyager/Demo/Levels/' + L, '-game', '-windowed', '-ResX=1920', '-ResY=1080', '-WinX=0', '-WinY=0', '-nosplash', '-log', '-abslog=C:\\dev\\ios\\voy_play_%s.log' % L])
st('PLAY %s pid %d' % (L, p.pid))
t0 = time.time()
while p.poll() is None and not os.path.exists(STOP) and time.time() - t0 < 900: time.sleep(2)
if p.poll() is None: subprocess.run('taskkill /F /T /PID %d' % p.pid, shell=True, capture_output=True)
try: os.remove(r'C:\dev\unreal.lock')
except Exception: pass
st('PLAY_DONE %s' % L)

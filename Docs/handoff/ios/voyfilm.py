# Nov 10 (Claude): Voyager: Third Person Shooter v3 (Evan bought it on Fab, project in C:\dev\Voyager)
# 1) shader warm-up per demo level (-game until the ShaderCompileWorkers go quiet), 2) film each level with -benchmark -fps=30 -dumpmovie
import subprocess, time, os, glob, shutil, sys
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + 'voy ' + s + '\n')
ST = r'C:\dev\ios\voy_status.txt'
def st(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
hb('start'); st('START')
B = r'C:\dev\ios\b232ios_status.txt'
while not (os.path.exists(B) and 'DONE' in open(B).read()): time.sleep(20)
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
open(r'C:\dev\unreal.lock', 'w').write('Voyager TPS: shader warm-up + demo films (Claude, C:\\dev\\Voyager)')
UE = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealEditor.exe'
PRJ = r'C:\dev\Voyager\VoyagerThirdPersonShooter.uproject'
OUT = r'C:\dev\proofs\voyager'
os.makedirs(OUT, exist_ok=True)
LEVELS = sys.argv[1].split(',') if len(sys.argv) > 1 else ['LevelWeapons', 'LevelWaveAssault', 'LevelModularCharacter', 'LevelAdvancedAI', 'LevelCover']
def scw():
    r = subprocess.run('tasklist /FI "IMAGENAME eq ShaderCompileWorker.exe"', shell=True, capture_output=True, text=True)
    return r.stdout.count('ShaderCompileWorker.exe')
P = [None]
def kill():
    if P[0] is not None and P[0].poll() is None: subprocess.run('taskkill /F /T /PID %d' % P[0].pid, shell=True, capture_output=True); time.sleep(5)
def shots():
    return glob.glob(r'C:\dev\Voyager\Saved\Screenshots\**\*.png', recursive=True)
try:
    for L in LEVELS:
        m = '/Game/Voyager/Demo/Levels/' + L
        # 1) warm-up: run until no ShaderCompileWorker for 45 s (max 20 min)
        st('WARM ' + L)
        p = subprocess.Popen([UE, PRJ, m, '-game', '-windowed', '-ResX=1280', '-ResY=720', '-WinX=40', '-WinY=40', '-nosplash', '-log', '-abslog=C:\\dev\\ios\\voy_warm_%s.log' % L]); P[0] = p
        t0 = time.time(); quiet = None
        while time.time() - t0 < 1200 and p.poll() is None:
            time.sleep(10)
            if scw() == 0:
                quiet = quiet or time.time()
                if time.time() - quiet > 45 and time.time() - t0 > 90: break
            else: quiet = None
        st('WARM_DONE %s %.0f s' % (L, time.time() - t0))
        kill()
        for f in shots(): os.remove(f)
        # 2) film: fixed 30 fps, every frame dumped
        st('FILM ' + L)
        p = subprocess.Popen([UE, PRJ, m, '-game', '-windowed', '-ResX=1280', '-ResY=720', '-WinX=40', '-WinY=40', '-nosplash', '-benchmark', '-fps=30', '-dumpmovie', '-log', '-abslog=C:\\dev\\ios\\voy_film_%s.log' % L]); P[0] = p
        t0 = time.time()
        while time.time() - t0 < 240 and p.poll() is None:
            time.sleep(10)
            if len(shots()) >= 600: break  # 20 s of film
        kill()
        d = os.path.join(OUT, L); shutil.rmtree(d, ignore_errors=True); os.makedirs(d)
        fs = sorted(shots(), key=os.path.getmtime)
        for i, f in enumerate(fs): shutil.move(f, os.path.join(d, 'f_%05d.png' % i))
        st('FILMED %s %d frames' % (L, len(fs)))
finally:
    kill()
    try: os.remove(r'C:\dev\unreal.lock')
    except Exception: pass
    st('DONE'); hb('done')

# Nov 11 (Claude): Voyager REAL gameplay port - step 1 (PC proof). Copies the whole Voyager content into the Zoogi project,
# layers Voyager's config as Config/Custom/Voyager (only read with -CustomConfig=Voyager, the normal game is untouched),
# enables Voyager's plugins in the .uproject for the test run (restored after), runs a Voyager level and takes screenshots.
import subprocess, time, os, sys, shutil, json
ST = r'C:\dev\ios\voyport_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
UE = r'C:\Program Files\Epic Games\UE_5.6'
PD = r'C:\dev\ZoogiRollUnreal-p5style'; P = PD + r'\ZoogiRoll.uproject'
VD = r'C:\dev\Voyager'
LEVELS = sys.argv[1].split(',') if len(sys.argv) > 1 else ['LevelCover']
SECS = int(sys.argv[2]) if len(sys.argv) > 2 else 240
def editor_running():
    return 'UnrealEditor.exe' in subprocess.run('tasklist /FI "IMAGENAME eq UnrealEditor.exe"', shell=True, capture_output=True, text=True).stdout
status('UP')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(3)
t0 = time.time()
while editor_running() and time.time() - t0 < 300: time.sleep(5)
open('C:/dev/unreal.lock', 'w').write('Voyager port PC proof (Claude, ZoogiRollUnreal-p5style)')
bak = P + '.pre_voy'
try:
    # 1. content (skip files already there)
    r = subprocess.run(['robocopy', VD + r'\Content\Voyager', PD + r'\Content\Voyager', '/E', '/XC', '/XN', '/XO', '/NFL', '/NDL', '/NP', '/R:1', '/W:1', '/MT:8'], capture_output=True, text=True)
    status('COPY rc=%d %s' % (r.returncode, ' '.join(r.stdout.split()[-40:])[:300]))
    # 2. config layer
    cd = PD + r'\Config\Custom\Voyager'; os.makedirs(cd, exist_ok=True)
    for f in ('DefaultEngine.ini', 'DefaultInput.ini', 'DefaultGameplayTags.ini', 'DefaultGame.ini'):
        if os.path.exists(VD + '\\Config\\' + f): shutil.copy(VD + '\\Config\\' + f, cd + '\\' + f)
    status('CONFIG_LAYER')
    # 3. plugins (for this run)
    if not os.path.exists(bak): shutil.copy(P, bak)
    up = json.load(open(bak, encoding='utf-8-sig')); vp = json.load(open(VD + r'\VoyagerThirdPersonShooter.uproject', encoding='utf-8-sig'))
    have = {p['Name'] for p in up['Plugins']}
    for p in vp.get('Plugins', []):
        if p['Name'] not in have: up['Plugins'].append(p)
    json.dump(up, open(P, 'w', encoding='utf-8'), indent='\t')
    status('PLUGINS ' + ','.join(p['Name'] for p in up['Plugins']))
    # 4. run
    for lv in LEVELS:
        d = r'C:\dev\proofs\voyager_port\%s' % lv; os.makedirs(d, exist_ok=True)
        args = '"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" /Game/Voyager/Demo/Levels/%s -game -CustomConfig=Voyager -windowed -ResX=1600 -ResY=900 -WinX=200 -WinY=40 -log -abslog=C:\\dev\\ios\\voyport_%s.log -nosplash' % (UE, P, lv, lv)
        status('RUN ' + lv)
        subprocess.Popen(args, shell=True)
        t1 = time.time(); n = 0
        while time.time() - t1 < SECS:
            time.sleep(15); n += 1
            ps = ("Add-Type -AssemblyName System.Drawing; $b = New-Object System.Drawing.Bitmap 1616,940; $g = [System.Drawing.Graphics]::FromImage($b); "
                  "$g.CopyFromScreen(196,40,0,0,$b.Size); $b.Save('%s\\s_%03d.png'); $g.Dispose(); $b.Dispose()" % (d, n))
            subprocess.run(['powershell', '-NoProfile', '-Command', ps], capture_output=True)
            if not editor_running(): status('EXITED_EARLY %s after %ds' % (lv, time.time() - t1)); break
        subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(6)
    status('RUNS_DONE')
except Exception as e:
    status('ERROR %r' % (e,))
finally:
    if os.path.exists(bak): shutil.copy(bak, P); status('UPROJECT_RESTORED')
    try: os.remove('C:/dev/unreal.lock')
    except Exception: pass
    status('DONE')

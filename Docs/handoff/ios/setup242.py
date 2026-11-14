# Nov 2 (Claude): set up TX242 (monthly MacInCloud, user303094) from the PC's own copies, then ship build 65 there.
# 1) ~/zr: dist.p12, App Store profile, API key, fin.sh/build3.sh, AppIcon catalog  2) Epic BuildRules (MarketplaceRules.dll)
# 3) UBT remote build of the combined project (uploads engine + project mirror, ~16 GB first time) with the PC Binaries backed up/restored
# 4) chain b65up242.py (stage upload + p5style sources + build3.sh 65 + TestFlight)
import subprocess, time, os, glob
ST = 'C:/dev/ios/tx242_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'StrictHostKeyChecking=accept-new', '-o', 'ServerAliveInterval=30', 'user303094@TX242.macincloud.com']
def ssh(cmd, inp=None, to=300):
    try:
        r = subprocess.run(SSH + [cmd], input=inp, capture_output=True, timeout=to)
        return r.returncode, (r.stdout + r.stderr).decode(errors='replace').strip()
    except Exception as e: return -1, repr(e)
rc, out = ssh('whoami; sw_vers -productVersion; xcodebuild -version | head -1; xcodebuild -checkFirstLaunchStatus; echo FL=$?; xcodebuild -license check; echo LIC=$?; xcrun --sdk iphoneos --show-sdk-version; df -h ~ | tail -1')
status('TX242 rc=%d %s' % (rc, out.replace('\n', ' | ')[:400]))
if rc != 0: status('NEED_KEY_TX242'); status('DONE'); raise SystemExit
CB = r'C:\dev\ZoogiRollUnreal-combined'
up = [(CB + r'\Intermediate\Remote\ZoogiRoll\IOS\Shipping\Certificate.p12', 'zr/dist.p12'),
      (r'C:\dev\ios\ZoogiRoll_AppStore.mobileprovision', 'zr/appstore.mobileprovision'),
      (r'C:\dev\ios\keys\AuthKey_VY8Z87BCDX.p8', 'zr/AuthKey_VY8Z87BCDX.p8'),
      (CB + r'\Build\IOS\Resources\Assets.xcassets\Contents.json', 'zr/AppIcon.xcassets/Contents.json'),
      (CB + r'\Build\IOS\Resources\Assets.xcassets\AppIcon.appiconset\Contents.json', 'zr/AppIcon.xcassets/AppIcon.appiconset/Contents.json'),
      (CB + r'\Build\IOS\Resources\Assets.xcassets\AppIcon.appiconset\Icon1024.png', 'zr/AppIcon.xcassets/AppIcon.appiconset/Icon1024.png')]
for sh in ['fin.sh', 'build3.sh']:
    up.append((r'C:\dev\ios' + '\\' + sh, 'zr/' + sh))
br = os.path.expandvars(r'%LOCALAPPDATA%\UnrealEngine\Intermediate\Build\BuildRules')
for f in glob.glob(br + r'\*'):
    if os.path.isfile(f): up.append((f, 'Library/Application Support/Epic/UnrealEngine/Intermediate/Build/BuildRules/' + os.path.basename(f)))
for src, dst in up:
    data = open(src, 'rb').read()
    if dst.endswith('.sh'): data = data.replace(b'\r\n', b'\n')
    d = os.path.dirname(dst)
    rc, out = ssh('mkdir -p "$HOME/%s" && cat > "$HOME/%s" && chmod %s "$HOME/%s" && echo OK' % (d, dst, '755' if dst.endswith('.sh') else '600', dst), inp=data)
    status('UP %s rc=%d %s' % (dst, rc, out[-60:]))
# DefaultEngine.ini -> TX242 (backup once)
for proj in [CB, r'C:\dev\ZoogiRollUnreal-p5style']:
    ini = proj + r'\Config\DefaultEngine.ini'
    s = open(ini, encoding='utf-8-sig').read()
    if not os.path.exists(ini + '.tx544'): open(ini + '.tx544', 'w', encoding='utf-8').write(s)
    s = s.replace('RemoteServerName=TX544.macincloud.com', 'RemoteServerName=TX242.macincloud.com').replace('RSyncUsername=user294974', 'RSyncUsername=user303094')
    open(ini, 'w', encoding='utf-8').write(s)
status('INI_TX242')
AJ = r'C:\Program Files\Epic Games\UE_5.6\Engine\Config\Apple\Apple_SDK.json'
try:
    j = open(AJ, encoding='utf-8').read()
    if '"MaxVersion": "27.9.0"' not in j:
        open(AJ + '.bak-tx242', 'w', encoding='utf-8').write(j)
        j = j.replace('"MaxVersion": "26.9.0"', '"MaxVersion": "27.9.0"')
        open(AJ, 'w', encoding='utf-8').write(j)
    status('APPLE_SDK MaxVersion 27.9.0 ' + str('"MaxVersion": "27.9.0"' in j))
except Exception as e: status('APPLE_SDK_FAIL %r' % e)
while os.path.exists('C:/dev/unreal.lock'):
    status('WAIT_LOCK ' + open('C:/dev/unreal.lock').read()[:80]); time.sleep(60)
open('C:/dev/unreal.lock', 'w').write('iOS remote build to TX242 (Claude, ZoogiRollUnreal-combined)')
BK = r'C:\dev\backups\combined_Binaries_tx242'
rc = subprocess.run(r'robocopy "%s\Binaries" "%s" /E /NFL /NDL /NJH /NJS /NP' % (CB, BK), shell=True).returncode
status('BIN_BACKUP_RC=%d' % rc)
status('REMOTE_BUILD_START')
with open(r'C:\dev\ios\build242.log', 'w') as lf:
    rc = subprocess.run(r'"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\Build.bat" ZoogiRoll IOS Shipping -Project="%s\ZoogiRoll.uproject" -distribution -WaitMutex -NoHotReload' % CB, shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('REMOTE_BUILD_RC=%d' % rc)
r2 = subprocess.run(r'robocopy "%s" "%s\Binaries" /E /XC /XN /XO /NFL /NDL /NJH /NJS /NP' % (BK, CB), shell=True).returncode
status('BIN_RESTORE_RC=%d' % r2)
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
rc2, out = ssh('ls ~/UE5/Builds/*/C/dev/ZoogiRollUnreal-combined/Intermediate/Build/XmlConfigCache.bin ~/UE5/Builds/*/C/Program_Files/Epic_Games/UE_5.6/Engine/Build/BatchFiles/Mac/Build.sh; du -sh ~/UE5')
status('MIRROR %s' % out.replace('\n', ' | '))
if 'XmlConfigCache.bin' in out and 'No such' not in out:
    status('CHAIN b65up242')
    subprocess.run([r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe', r'C:\dev\ios\b65up242.py'])
status('DONE')

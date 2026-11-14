# Oct 30 (Claude): build 29 iOS - full route (stage includes the build 28 avatars).
# 1) RunUAT BuildCookRun IOS Shipping -skipbuild -cook -stage -pak (-iterate) -> Saved\StagedBuilds\IOS
# 2) stream the stage to the Mac (~/zr/stage)   3) ios29.py (sources + build3.sh 29)   4) poll tail29 until upload.
# status -> C:/dev/ios/b29ios_status.txt ; logs cook29.log / upstage29.log
import subprocess, time, os, sys, tarfile
ST = 'C:/dev/ios/b29ios_status.txt'
def status(s):
    open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
status('RUNNER_UP')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(4)
open('C:/dev/unreal.lock', 'w').write('iOS cook + stage for build 29 (Claude, ZoogiRollUnreal-combined)')
uat = r'"C:\Program Files\Epic Games\UE_5.6\Engine\Build\BatchFiles\RunUAT.bat"'
args = r' BuildCookRun -project="C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject" -noP4 -platform=IOS -clientconfig=Shipping -distribution -skipbuild -cook -iterate -stage -pak -unattended -utf8output'
status('COOK_START')
with open(r'C:\dev\ios\cook29.log', 'w') as lf:
    rc = subprocess.run(uat + args, shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('COOK_RC=%d' % rc)
# Oct 29: -iterate makes staging write "-iterative" into uecommandline.txt (loose-file deploy) -> "Failed to open descriptor file
# ../../../ZoogiRoll/ZoogiRoll.uproject" on the phone (build 21). Always write the plain command line.
open(r'C:\dev\ZoogiRollUnreal-combined\Saved\StagedBuilds\IOS\uecommandline.txt', 'w', newline='').write('-project="../../../ZoogiRoll/ZoogiRoll.uproject"')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
if rc != 0:
    status('DONE'); raise SystemExit
# upload the stage
status('UPSTAGE_START')
t0 = time.time()
log = open(r'C:\dev\ios\upstage29.log', 'w')
p = subprocess.Popen(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com',
                      'rm -rf ~/zr/stage_new && mkdir -p ~/zr/stage_new && tar -xf - -C ~/zr/stage_new && rm -rf ~/zr/stage && mv ~/zr/stage_new ~/zr/stage && du -sh ~/zr/stage'],
                     stdin=subprocess.PIPE, stdout=log, stderr=log)
with tarfile.open(fileobj=p.stdin, mode='w|') as tf:
    tf.add(r'C:\dev\ZoogiRollUnreal-combined\Saved\StagedBuilds\IOS', arcname='.')
p.stdin.close()
urc = p.wait()
log.write('\nrc=%d %.0fs\n' % (urc, time.time() - t0)); log.close()
status('UPSTAGE_RC=%d %.0fs' % (urc, time.time() - t0))
if urc != 0:
    status('DONE'); raise SystemExit
status('IOS29_START')
r = subprocess.run([sys.executable, r'C:\dev\ios\ios29.py'], capture_output=True, text=True, timeout=900)
status('IOS29_RC=%d' % r.returncode)
for k in range(40):
    time.sleep(90)
    subprocess.run([sys.executable, r'C:\dev\ios\tail29.py'], capture_output=True, timeout=120)
    t = open(r'C:\dev\ios\tail29.txt', errors='replace').read()
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or 'ERROR: ' in t:
        status('IOS29_FINISHED'); break
status('IOS29_POLL_END')
status('DONE')

# Nov 2 (Claude): move the iOS build Mac from TX544 (weekly plan) to TX242 (monthly plan, user303094).
# TX242 must already have our public key (C:\dev\mac_add_key.txt pasted in its Terminal by Evan).
# TX242 pulls ~/UE5 (engine + project mirror), ~/zr (certs, profile, API key, scripts) and Epic BuildRules straight from TX544
# with a temporary copy of our key, rewrites /Users/user294974 -> /Users/user303094, then removes the temp key.
import subprocess, time
ST = 'C:/dev/ios/tx242_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
O = ['-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'StrictHostKeyChecking=accept-new', '-o', 'ServerAliveInterval=30']
N = ['ssh'] + O + ['user303094@TX242.macincloud.com']
OLD = ['ssh'] + O + ['user294974@TX544.macincloud.com']
def run(cmd, rc_only=False, inp=None, to=300):
    try:
        r = subprocess.run(cmd, input=inp, capture_output=True, timeout=to)
        return r.returncode, (r.stdout + r.stderr).decode(errors='replace').strip()
    except Exception as e:
        return -1, repr(e)
rc, out = run(N + ['whoami; echo $HOME; sw_vers -productVersion; xcodebuild -version | head -1; df -h ~ | tail -1; ls ~'])
status('TX242 rc=%d %s' % (rc, out.replace('\n', ' | ')[:400]))
if rc != 0:
    status('NEED_KEY_TX242: paste C:\\dev\\mac_add_key.txt into Terminal on TX242'); status('DONE'); raise SystemExit
rc, out = run(OLD + ['du -sh ~/UE5 ~/zr 2>/dev/null; ls ~/zr | head -40'])
status('TX544 rc=%d %s' % (rc, out.replace('\n', ' | ')[:600]))
if rc != 0:
    status('TX544_DOWN: cannot copy certs/mirror'); status('DONE'); raise SystemExit
key = open('C:/dev/k_t.key', 'rb').read().replace(b'\r\n', b'\n')
rc, out = run(N + ['mkdir -p ~/.ssh && chmod 700 ~/.ssh && cat > ~/.ssh/k544 && chmod 600 ~/.ssh/k544 && echo KEY_OK'], inp=key)
status('TEMPKEY rc=%d %s' % (rc, out))
mig = r'''#!/bin/bash
E="ssh -i $HOME/.ssh/k544 -o BatchMode=yes -o StrictHostKeyChecking=accept-new"
S=user294974@TX544.macincloud.com
echo START $(date)
mkdir -p ~/UE5 ~/zr "$HOME/Library/Application Support/Epic"
rsync -a -e "$E" --exclude stage --exclude 'stage_new' $S:zr/ ~/zr/ ; echo ZR_RC=$?
rsync -a -e "$E" "$S:Library/Application Support/Epic/" "$HOME/Library/Application Support/Epic/" ; echo EPIC_RC=$?
rsync -a -e "$E" $S:UE5/ ~/UE5/ ; echo UE5_RC=$?
for f in ~/zr/*.sh; do sed -i '' 's#/Users/user294974#/Users/user303094#g' "$f"; done
P=~/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined
grep -rlI '/Users/user294974' $P/Intermediate 2>/dev/null | while read f; do sed -i '' 's#/Users/user294974#/Users/user303094#g' "$f"; done
rm -f $P/Intermediate/Build/XmlConfigCache.bin
rm -f ~/.ssh/k544
du -sh ~/UE5 ~/zr
echo MIGRATE_DONE $(date)
'''
rc, out = run(N + ['cat > ~/migrate.sh && chmod +x ~/migrate.sh && (nohup ~/migrate.sh > ~/migrate.log 2>&1 < /dev/null &) && echo started'], inp=mig.encode())
status('MIGRATE ' + out)
for k in range(120):
    time.sleep(30)
    rc, out = run(N + ['tail -5 ~/migrate.log; du -sh ~/UE5 2>/dev/null'], to=120)
    open('C:/dev/ios/tx242_tail.txt', 'w').write(out)
    if 'MIGRATE_DONE' in out:
        status('MIGRATE_DONE ' + out.replace('\n', ' | ')[:500]); break
rc, out = run(N + ['grep -E "_RC=" ~/migrate.log'])
status('RCS ' + out.replace('\n', ' '))
if 'ZR_RC=0' in out and 'UE5_RC=0' in out:
    status('CHAIN b65up242')
    subprocess.run([r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe', r'C:\dev\ios\b65up242.py'])
status('DONE')

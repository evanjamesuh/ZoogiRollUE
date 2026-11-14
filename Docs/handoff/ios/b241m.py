# Nov 11 (Claude): B241 Mac-only rerun - build3.sh without -SkipRulesCompile
import subprocess, time, os, sys, tarfile, filecmp
ST = 'C:/dev/ios/b241m_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'ServerAliveInterval=30', 'user294974@TX544.macincloud.com']
P5 = r'C:\dev\ZoogiRollUnreal-p5style'
CB = r'C:\dev\ZoogiRollUnreal-combined'
status('MAC_ONLY rerun of 241 (stage + sources already on the Mac)')
b3 = open(r'C:\dev\ios\build3.sh', 'rb').read().replace(b'\r\n', b'\n')
r = subprocess.run(SSH + ['cat > ~/zr/build3.sh; chmod +x ~/zr/build3.sh; nohup ~/zr/build3.sh 241 > ~/zr/build241.log 2>&1 < /dev/null & echo started'], input=b3, capture_output=True, timeout=120)
status('IOS241 ' + r.stdout.decode(errors='replace').strip() + r.stderr.decode(errors='replace').strip())
for k in range(40):
    time.sleep(64)
    try: r = subprocess.run(SSH + ['tail -c 3000 ~/zr/build241.log; echo; echo ===ERR; grep -n "error:\|UBT_RC" ~/zr/build241.log | head -40; ls -la $HOME/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined/Binaries/IOS/ZoogiRoll-IOS-Shipping'], capture_output=True, timeout=120)
    except Exception as e: status('POLL_EXC %r' % e); continue
    t = r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
    open(r'C:\dev\ios\tail241.txt', 'w', encoding='utf-8', errors='replace').write(t)
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or ('UBT_RC=' in t and 'UBT_RC=0' not in t):
        status('IOS241_FINISHED'); break
try:
    UE = r'C:\Program Files\Epic Games\UE_5.6'
    0 and subprocess.Popen([UE + r'\Engine\Binaries\Win64\UnrealEditor.exe', CB + r'\ZoogiRoll.uproject'], creationflags=0x00000008 | 0x00000200, close_fds=True)
    status('EDITOR_REOPENED')
except Exception as e: status('EDITOR_REOPEN_FAIL %r' % e)
status('DONE')

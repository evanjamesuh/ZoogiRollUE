# Nov 5 12:45 (Claude): the b142ios poller stopped writing at 12:32 (process gone); the Mac build runs on by itself (nohup) -
# this re-polls it and writes DONE so gob143 carries on.
import subprocess, time, os
ST = 'C:/dev/ios/b142ios_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
status('REPOLL_START')
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'ServerAliveInterval=30', 'user294974@TX544.macincloud.com']
for k in range(40):
    time.sleep(20)
    try: r = subprocess.run(SSH + ['tail -c 3000 ~/zr/build142.log; echo; echo ===ERR; grep -n "error:\|UBT_RC" ~/zr/build142.log | head -40; ls -la $HOME/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined/Binaries/IOS/ZoogiRoll-IOS-Shipping'], capture_output=True, timeout=120)
    except Exception as e: status('POLL_EXC %r' % e); continue
    t = r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
    open(r'C:\dev\ios\tail142.txt', 'w').write(t)
    if 'EXPORT_UPLOAD_RC' in t or 'BUILD FAILED' in t or ('UBT_RC=' in t and 'UBT_RC=0' not in t):
        status('IOS142_FINISHED'); break
try:
    UE = r'C:\Program Files\Epic Games\UE_5.6'
    0 and subprocess.Popen([UE + r'\Engine\Binaries\Win64\UnrealEditor.exe', CB + r'\ZoogiRoll.uproject'], creationflags=0x00000008 | 0x00000200, close_fds=True)
    status('EDITOR_REOPENED')
except Exception as e: status('EDITOR_REOPEN_FAIL %r' % e)
status('DONE')

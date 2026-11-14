import subprocess, time
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=25', 'user294974@TX544.macincloud.com', 'echo MAC_OK; uptime; df -h /System/Volumes/Data | tail -1'], capture_output=True, timeout=90)
open('C:/dev/ios/mactest.txt', 'w').write(time.strftime('%H:%M:%S ') + 'rc=%d\n' % r.returncode + r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))

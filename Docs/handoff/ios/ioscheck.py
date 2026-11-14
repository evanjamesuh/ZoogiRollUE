import subprocess, time
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', 'user294974@TX544.macincloud.com']
try:
    r = subprocess.run(SSH + ['date; for n in 233 234; do echo == $n; tail -c 900 ~/zr/build$n.log; echo; grep -n "UBT_RC\\|EXPORT_UPLOAD_RC\\|error:\\|BUILD FAILED" ~/zr/build$n.log | head -12; done; pgrep -fl "build3|xcodebuild|UnrealBuildTool|altool" | head -5; df -h /System/Volumes/Data | tail -1'], capture_output=True, timeout=90)
    t = r.stdout.decode(errors='replace') + '\nERR:' + r.stderr.decode(errors='replace')
except Exception as e: t = 'EXC %r' % e
open(r'C:\dev\ios\ioscheck.txt', 'w').write(time.strftime('%H:%M:%S\n') + t)

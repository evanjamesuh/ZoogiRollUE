import subprocess
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com']
cmd = 'df -h / /System/Volumes/Data $TMPDIR 2>&1; echo TMPDIR=$TMPDIR; du -sh $TMPDIR 2>/dev/null; ls -la $TMPDIR | head -30; du -sh ~/zr/* 2>/dev/null | sort -h | tail -8; du -sh ~/Library/Developer/Xcode/* 2>/dev/null; du -sh ~/Library/Caches 2>/dev/null; quota 2>&1 | head -3'
r = subprocess.run(SSH + [cmd], capture_output=True, timeout=180)
open(r'C:\dev\ios\diag31.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))

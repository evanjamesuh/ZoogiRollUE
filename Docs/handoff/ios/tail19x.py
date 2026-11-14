import subprocess
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', 'grep -n "Compile\\|Module.ZoogiRoll\\|\\[[0-9]*/[0-9]*\\]\\|EXPORT\\|UPLOAD\\|error" ~/zr/build19.log | head -40; ls -la ~/zr/build19.log'], capture_output=True, timeout=60)
open(r'C:\dev\ios\tail19x.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))

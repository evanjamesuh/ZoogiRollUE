import subprocess
def ssh(cmd):
    r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], capture_output=True, timeout=120)
    return r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')
out = []
out.append('== upload lines\n' + ssh('grep -n -i "itms\\|warning\\|error\\|upload\\|export\\|invalid\\|icon" ~/zr/build21.log | tail -60'))
out.append('== tail\n' + ssh('tail -c 6000 ~/zr/build21.log'))
out.append('== plist\n' + ssh('cd ~/zr/work && A=$(ls -d Payload/*.app) && /usr/libexec/PlistBuddy -c "Print" "$A/Info.plist" | head -120; ls -la "$A" | head -60; codesign -dv --verbose=2 "$A" 2>&1 | head -20; codesign --verify --deep --strict "$A" 2>&1 | head; echo VERIFY_RC=$?'))
out.append('== actool\n' + ssh('cat ~/zr/work/icons/actool.txt; cat ~/zr/work/icons/partial.plist'))
open(r'C:\dev\ios\diag21.txt', 'w').write('\n'.join(out))
print('DIAG21_DONE')

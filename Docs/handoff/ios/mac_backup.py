# Evan Nov 9 "Back files incase the Mac cloud deletes": pull everything that lives only on the Mac into C:\dev\mac_backup\mac_pull
import subprocess, os, time
K = 'C:/dev/k_t.key'
SSH = ['ssh', '-i', K, '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'StrictHostKeyChecking=no', 'user294974@TX544.macincloud.com']
OUT = r'C:\dev\mac_backup\mac_pull'
os.makedirs(OUT, exist_ok=True)
log = open(os.path.join(OUT, 'pull_log.txt'), 'a')
log.write('\n%s start\n' % time.ctime())
# the small, irreplaceable things: ~/zr (scripts, p12s, provisioning, plists, logs), the Mac's provisioning profiles, keychain list, Xcode account plist
cmd = ('cd ~ && tar -czf - --exclude="zr/stage" --exclude="zr/stage_new" '
       'zr "Library/MobileDevice/Provisioning Profiles" Library/Keychains/login.keychain-db .ssh/authorized_keys 2>/dev/null')
r = subprocess.run(SSH + [cmd], capture_output=True, timeout=1200)
open(os.path.join(OUT, 'mac_home_small.tgz'), 'wb').write(r.stdout)
log.write('tar rc=%d bytes=%d err=%s\n' % (r.returncode, len(r.stdout), r.stderr.decode(errors='replace')[:400]))
r = subprocess.run(SSH + ['ls -la ~ ~/zr; du -sh ~/UE5/Builds 2>/dev/null; security find-identity -v -p codesigning'], capture_output=True, timeout=120)
open(os.path.join(OUT, 'mac_listing.txt'), 'wb').write(r.stdout + r.stderr)
log.write('%s done\n' % time.ctime()); log.close()
print('MAC_BACKUP_DONE')

import subprocess
data = open(r'C:\dev\ios\ZoogiRoll_AppStore.mobileprovision', 'rb').read()
cmd = 'for D in "$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles" "$HOME/Library/MobileDevice/Provisioning Profiles"; do mkdir -p "$D"; cat > "$D/4eef96ba-b81a-4086-8fb2-52d50289b2cd.mobileprovision.tmp"; done'
# write once then copy
cmd = 'D1="$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles"; D2="$HOME/Library/MobileDevice/Provisioning Profiles"; mkdir -p "$D1" "$D2"; cat > "$D1/4eef96ba-b81a-4086-8fb2-52d50289b2cd.mobileprovision"; cp "$D1/4eef96ba-b81a-4086-8fb2-52d50289b2cd.mobileprovision" "$D2/"; ls -la "$D1" "$D2"'
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com', cmd], input=data, capture_output=True, timeout=60)
open(r'C:\dev\ios\upprov.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
print('UPPROV', r.returncode)
exec(open('C:/dev/ios/pkg_ios.py').read())

# Nov 11 (Claude): B240 failed on the Mac (UHT: no UCommonGameViewportClient) - the Mac .uproject never got the Voyager plugins
# (the iOS scripts upload Source only). Upload ZoogiRoll.uproject now so B241's Mac compile sees CommonUI etc.
import subprocess, time
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', '-o', 'ServerAliveInterval=30', 'user294974@TX544.macincloud.com']
MAC = '$HOME/UE5/Builds/EVAN/C/dev/ZoogiRollUnreal-combined'
data = open(r'C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject', 'rb').read()
out = []
for a in range(4):
    r = subprocess.run(SSH + ['cp "%s/ZoogiRoll.uproject" "%s/ZoogiRoll.uproject.pre_voy" 2>/dev/null; cat > "%s/ZoogiRoll.uproject" && wc -c < "%s/ZoogiRoll.uproject" && grep -c CommonUI "%s/ZoogiRoll.uproject" && ls $HOME/UE5 | head; ls "/Users/Shared/Epic Games" 2>/dev/null' % (MAC, MAC, MAC, MAC, MAC)], input=data, capture_output=True, timeout=120)
    out.append('rc=%d len=%d %s %s' % (r.returncode, len(data), r.stdout.decode(errors='replace'), r.stderr.decode(errors='replace')[:300]))
    if r.returncode == 0: break
    time.sleep(5)
r = subprocess.run(SSH + ['for p in CommonUI EnhancedInput PoseSearch Mover Chooser HairStrands RigLogic LiveLink MotionWarping AnimationWarping AnimationLocomotionLibrary FieldSystemPlugin; do f=$(find "/Users/Shared/Epic Games/UE_5.6/Engine/Plugins" -maxdepth 4 -name "$p.uplugin" 2>/dev/null | head -1); echo "$p: $f"; done'], capture_output=True, timeout=180)
out.append(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace')[:300])
open(r'C:\dev\ios\upuproj_status.txt', 'w').write('\n'.join(out) + '\nDONE\n')

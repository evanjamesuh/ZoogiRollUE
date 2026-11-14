import subprocess
SSH = ['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', '-o', 'ConnectTimeout=30', 'user294974@TX544.macincloud.com']
E = '$HOME/UE5/Builds/EVAN/C/Program_Files/Epic_Games/UE_5.6/Engine'
cmd = 'for p in CommonUI EnhancedInput PoseSearch Mover Chooser HairStrands RigLogic LiveLink LiveLinkControlRig MotionWarping AnimationWarping AnimationLocomotionLibrary FieldSystemPlugin Niagara ProceduralMeshComponent; do f=$(find "%s/Plugins" -maxdepth 5 -name "$p.uplugin" 2>/dev/null | head -1); n=0; [ -n "$f" ] && n=$(find "$(dirname "$f")" -name "*.cpp" | wc -l); echo "$p: $f cpp=$n"; done; ls "%s/Plugins" | head -30; du -sh "%s" ' % (E, E, E)
r = subprocess.run(SSH + [cmd], capture_output=True, timeout=300)
open(r'C:\dev\ios\macplug.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace') + '\nDONE\n')

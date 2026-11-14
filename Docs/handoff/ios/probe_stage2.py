# Oct 29 (Claude): read-only - how the staged iOS manifest names MenuArt files + pak listing -> C:/dev/ios/probe_stage2.txt
import subprocess
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com',
    'grep -i "ms_card_classic\\|ModeSelect" ~/zr/stage/Manifest_UFSFiles_IOS.txt | head -5; grep -ic menuart ~/zr/stage/Manifest_UFSFiles_IOS.txt; head -3 ~/zr/stage/Manifest_UFSFiles_IOS.txt; ls -la ~/zr/stage/cookeddata/zoogiroll/content/paks/; ls ~/UE5/Builds/EVAN/C/Program_Files/Epic_Games/UE_5.6/Engine/Binaries/Mac/ | head -30'], capture_output=True, timeout=60)
open(r'C:\dev\ios\probe_stage2.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
print('PROBE2_OK')

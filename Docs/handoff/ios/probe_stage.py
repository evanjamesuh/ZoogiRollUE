# Oct 29 (Claude): read-only look at the Mac's staged iOS data (where MenuArt PNGs live) -> C:/dev/ios/probe_stage.txt
import subprocess
r = subprocess.run(['ssh', '-i', 'C:/dev/k_t.key', '-o', 'BatchMode=yes', 'user294974@TX544.macincloud.com',
    'find ~/zr/stage -iname "ms_card_*" | head; find ~/zr/stage -iname "*.pak" | head; ls ~/zr/stage | head -30'], capture_output=True, timeout=60)
open(r'C:\dev\ios\probe_stage.txt', 'w').write(r.stdout.decode(errors='replace') + r.stderr.decode(errors='replace'))
print('PROBE_OK')

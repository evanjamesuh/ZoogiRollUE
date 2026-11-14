# Oct 29 (Claude): patch pak with the new MODE SELECTION map art for the iOS fast path (the staged art lives in the main pak).
# -> C:/dev/ios/zoogiroll-ios_1_P.pak (+ log mkpak14.txt). Paths inside = the main pak's (mount ../../../).
import subprocess, os
ART = r'C:\dev\ZoogiRollUnreal-combined\Content\Data\MenuArt\ModeSelect'
names = ['ms_map_frozen.png', 'ms_map_meadow.png', 'ms_map_volcanic.png', 'ms_map_cosmic.png', 'ms_map_arabian.png', 'ms_map_tomb.png', 'ms_map_neon.png', 'ms_map_teaser.png', 'ms_title_map.png']
resp = r'C:\dev\ios\pak14_resp.txt'
with open(resp, 'w') as f:
    for n in names:
        f.write('"%s" "../../../ZoogiRoll/Content/Data/MenuArt/ModeSelect/%s"\n' % (os.path.join(ART, n), n))
out = r'C:\dev\ios\zoogiroll-ios_1_P.pak'
up = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\Win64\UnrealPak.exe'
r = subprocess.run([up, out, '-create=' + resp], capture_output=True, text=True)
r2 = subprocess.run([up, out, '-list'], capture_output=True, text=True)
open(r'C:\dev\ios\mkpak14.txt', 'w').write('rc=%d\n%s\n%s\n---list---\n%s' % (r.returncode, r.stdout[-3000:], r.stderr[-2000:], r2.stdout[-3000:]))
print('MKPAK14', r.returncode)

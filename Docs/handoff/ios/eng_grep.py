# Oct 28 (Claude): read-only grep of the installed UE 5.6 engine source (orientation API on iOS). Output -> C:/dev/ios/eng_grep.txt
import os, re
root = r'C:\Program Files\Epic Games\UE_5.6\Engine\Source\Runtime'
pat = re.compile(r'SetAllowedDeviceOrientation|AllowedDeviceOrientation|supportedInterfaceOrientations|setNeedsUpdateOfSupportedInterfaceOrientations|requestGeometryUpdate|enum class EDeviceScreenOrientation|FullSensor|LandscapeSensor')
out = []
for d, _, fs in os.walk(root):
    for f in fs:
        if not f.endswith(('.h', '.cpp', '.mm')): continue
        p = os.path.join(d, f)
        try:
            for i, l in enumerate(open(p, encoding='utf-8', errors='replace')):
                if pat.search(l): out.append('%s:%d: %s' % (p[len(root):], i + 1, l.rstrip()[:220]))
        except Exception as e: out.append('ERR %s %s' % (p, e))
open(r'C:\dev\ios\eng_grep.txt', 'w').write('\n'.join(out))
print('ENG_GREP_OK', len(out))

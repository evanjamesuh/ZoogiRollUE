import io, os, re, sys
S = os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/Private')
files = ['ZoogiP5Command', 'ZoogiClassicPad', 'ZoogiP5Style', 'ZoogiAnimeCues', 'ZoogiFrameTrail']
for f in files:
    p = os.path.join(S, f + '.cpp'); lines = io.open(p, encoding='utf-8-sig').read().split('\n')
    out = []; i = 0; n = 0
    while i < len(lines):
        ln = lines[i]; out.append(ln)
        m = re.search(r'OnPaint\(.*FSlateWindowElementList& \w+, int32 (\w+),', ln)
        if m and 'override' in ln:
            assert lines[i + 1].strip() == '{', (f, i, lines[i + 1])
            out.append(lines[i + 1]); i += 1
            ind = re.match(r'\s*', lines[i]).group(0) + '\t'
            extra = 'Hits.Reset(); ' if f == 'ZoogiP5Command' else ''
            out.append(ind + 'if (ZoogiWallCine::Active()) { %sreturn %s; } // b232: the wall-smash cinematic plays on a clean screen' % (extra, m.group(1)))
            n += 1
        i += 1
    t = '\n'.join(out)
    if '#include "ZoogiWallCine.h"' not in t:
        k = t.index('#include', t.index('#include') + 1)
        t = t[:k] + '#include "ZoogiWallCine.h" // b232\n' + t[k:]
    io.open(p, 'w', encoding='utf-8', newline='').write(t); print('ok', f, n)
p = os.path.join(S, 'ZoogiFront.cpp'); t = io.open(p, encoding='utf-8-sig').read()
a = '			{ TEXT("B231"),'
assert t.count(a) == 1
t = t.replace(a, '			{ TEXT("B232"), TEXT("CLEAN CINEMATIC"), TEXT("Behind the scenes: the action menu, pad buttons and speech bubbles stay off the screen while the wall-smash cinematic plays.") },\n' + a)
io.open(p, 'w', encoding='utf-8', newline='').write(t); print('news ok')

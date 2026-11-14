# Oct 28 (Claude): read-only grep of engine iOS headers -> C:/dev/ios/eng_lines.txt
import re
R = r'C:\Program Files\Epic Games\UE_5.6\Engine\Source\Runtime\ApplicationCore\Public\IOS'
out = []
for f, pat in (('IOSAppDelegate.h', r'IOSView|IOSController|GetDelegate|^@interface|^#import|^#include|InterfaceOrientation'), ('IOSView.h', r'SupportedInterfaceOrientations|@public|@private|@protected|^@interface|^@end|^#import|^#include|^class|^struct')):
    for i, l in enumerate(open(R + '\\' + f, encoding='utf-8', errors='replace')):
        if re.search(pat, l): out.append('%s:%d: %s' % (f, i + 1, l.rstrip()[:200]))
open(r'C:\dev\ios\eng_lines.txt', 'w').write('\n'.join(out))
print('ENG_LINES_OK')

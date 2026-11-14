# Nov 11 (Claude): Evan 18:17 "yes delete" - delete the 20 old ZoogiRollUnreal-* working copies he approved (keep p5style, combined, tripo15).
import os, shutil, stat, time
L = open(r'C:\dev\ios\freespace.txt', 'w')
def log(s): L.write(time.strftime('%H:%M:%S ') + s + '\n'); L.flush()
def free(): return shutil.disk_usage('C:\\').free / 1e9
NAMES = ['hudfix', 'kfx-ap', 'launch', 'launchpads', 'menu-realistic', 'menutx', 'phone-controls', 'phone-orbs', 'physics', 'polish',
         'portrait-ui', 'quality', 'realistic', 'scoring-ui', 'starnet', 'starnet-destr', 'ui22', 'vibair', 'vibration', 'volcanic']
def onerr(f, p, e):
    try: os.chmod(p, stat.S_IWRITE); f(p)
    except Exception: pass
log('start, C: free %.1f GB' % free())
for n in NAMES:
    p = os.path.join(r'C:\dev', 'ZoogiRollUnreal-' + n)
    if not os.path.isdir(p): log('missing ' + p); continue
    b = free(); shutil.rmtree('\\\\?\\' + p, onerror=onerr)
    log('deleted %s  (+%.1f GB, free %.1f GB)%s' % (p, free() - b, free(), ' PARTLY LEFT' if os.path.exists(p) else ''))
log('DONE, C: free %.1f GB' % free())

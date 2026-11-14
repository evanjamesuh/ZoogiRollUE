# Nov 11 (Claude): B259 (PoseSearch, AnimationLocomotionLibrary, MotionWarping, Chooser, Mover) crashes at launch on iPhone.
# Two test builds back to back: B260 = without Mover; B261 = without PoseSearch + Chooser (Mover back).
import subprocess, os, time, json, io, sys
if not os.environ.get('G262_CHILD'):
    try: os.close(os.open(r'C:\dev\ios\b262go.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
    except FileExistsError: raise SystemExit
    e = dict(os.environ); e['G262_CHILD'] = '1'
    subprocess.Popen([sys.executable, __file__], env=e, creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True); raise SystemExit
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
U = r'C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject'
BASE = ['PoseSearch', 'AnimationLocomotionLibrary', 'MotionWarping', 'Chooser', 'Mover']
def setp(keep):
    u = json.load(io.open(U, encoding='utf-8-sig'))
    u['Plugins'] = [p for p in u['Plugins'] if p['Name'] not in BASE] + [{'Name': n, 'Enabled': True} for n in keep]
    io.open(U, 'w', encoding='utf-8', newline='').write(json.dumps(u, indent='\t'))
py = sys.executable
while '261 done' not in open(H).read(): time.sleep(20)
for n, keep in ((262, ['AnimationLocomotionLibrary', 'MotionWarping']), (263, ['Chooser'])):
    setp(keep); hb('%d start plugins %s' % (n, keep))
    subprocess.run([py, r'C:\dev\ios\b%dios.py' % n])
    hb('%d done' % n)

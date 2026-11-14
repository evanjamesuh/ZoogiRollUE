# Nov 6 (Claude): League Training Court pieces (Blender headless) -> pieces_court + renders_court
import subprocess, time, glob
ST = r'C:\dev\ios\court_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
B = sorted(glob.glob(r'C:\Program Files\Blender Foundation\Blender*\blender.exe'))
A = r'C:\dev\art\ancient_meadows\grok_design\blender\arena'
r = subprocess.run([B[-1], '-b', '--factory-startup', '--python', A + r'\build_court.py', '--', A], capture_output=True, text=True, timeout=3600)
open(r'C:\dev\ios\court_blender_stdout.txt', 'w').write(r.stdout[-20000:] + '\n---\n' + r.stderr[-8000:])
status('BLENDER_RC=%d' % r.returncode); status('DONE')

# Nov 6 (Claude): Ancient Collision Meadows v15 (2x plaza ring) - Blender headless build (pieces_v15 + renders_v15), no Unreal
import subprocess, time, glob
ST = r'C:\dev\ios\v15_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
B = sorted(glob.glob(r'C:\Program Files\Blender Foundation\Blender*\blender.exe'))
status('BLENDER %r' % B)
A = r'C:\dev\art\ancient_meadows\grok_design\blender\arena'
r = subprocess.run([B[-1], '-b', '--factory-startup', '--python', A + r'\build_arena_v15.py', '--', A], capture_output=True, text=True, timeout=3600)
open(r'C:\dev\ios\v15_blender_stdout.txt', 'w').write(r.stdout[-20000:] + '\n---\n' + r.stderr[-8000:])
status('BLENDER_RC=%d' % r.returncode)
status('DONE')

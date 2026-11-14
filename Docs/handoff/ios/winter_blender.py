# Nov 6 (Claude): run winter_inspect.py headless in Blender
import subprocess, time, glob
ST = r'C:\dev\ios\winter_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
B = sorted(glob.glob(r'C:\Program Files\Blender Foundation\Blender*\blender.exe'))
status('BLENDER ' + B[-1])
r = subprocess.run([B[-1], '-b', '--factory-startup', '--python', r'C:\dev\ios\winter_inspect.py', '--', r'C:\dev\art\ithappy_winter'], capture_output=True, text=True, timeout=3600)
open(r'C:\dev\ios\winter_blender_stdout.txt', 'w').write(r.stdout[-30000:] + '\n---\n' + r.stderr[-8000:])
status('BLENDER_RC=%d' % r.returncode); status('DONE')

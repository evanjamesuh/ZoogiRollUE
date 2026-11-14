# Nov 12 (Claude): skill review PC checks after B270 (no iOS build)
import subprocess, os, time
try: os.close(os.open(r'C:\dev\ios\b270s.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
except FileExistsError: raise SystemExit
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '270s ' + s + '\n')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
hb('waiting for B270')
while not os.path.exists(r'C:\dev\ios\b270ios_status.txt') or 'DONE' not in open(r'C:\dev\ios\b270ios_status.txt').read(): time.sleep(20)
hb('go')
subprocess.run([py, r"C:\dev\ios\p5run.py", "b270s_sword_slash,b270s_frost_spikes,b270s_cross_slash,b270s_glacier_lance,b270s_ice_wall,b270s_ice_shards"])
hb('done')

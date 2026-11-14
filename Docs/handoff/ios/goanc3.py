import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
cmd = '"%s" C:\\dev\\ios\\relics_run.py && "%s" C:\\dev\\ios\\p5run.py anc_shots,anc_royale' % (py, py)
subprocess.Popen(cmd, shell=True, creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
print('GOANC3_STARTED')

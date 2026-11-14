import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
# kit import (Grok tiles + sky) -> editor build + Ancient captures (sequential, one Unreal job at a time)
cmd = '"%s" C:\\dev\\ios\\ancient_kit_run.py && "%s" C:\\dev\\ios\\p5run.py anc_shots,anc_royale,anc_test' % (py, py)
subprocess.Popen(cmd, shell=True, creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
print('GOANCV2_STARTED')

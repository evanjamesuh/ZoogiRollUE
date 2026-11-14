# Nov 1 (Claude): Micro Werewolf re-import (material BaseLit) -> build -> wolf test
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\werewolf_import_run.py'])
subprocess.run([py, r'C:\dev\ios\p5run.py', 'wolf_test'])

# Nov 1 b56 (Claude): import Grok's v14 arena pieces, then build + captures (v14 arena, menus, drag hint)
import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
subprocess.run([py, r'C:\dev\ios\v14_import_run.py'])
subprocess.run([py, r'C:\dev\ios\p5run.py', 'anc_shots,front_port,front_land,classic_clean'])

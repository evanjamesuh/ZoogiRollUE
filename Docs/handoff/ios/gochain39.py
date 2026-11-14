import subprocess
py = r'C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe'
# editor build + map-select captures, then the build 39 iOS runner (sequential: one Unreal job at a time)
cmd = '"%s" C:\\dev\\ios\\p5run.py map_select,map_select_port && "%s" C:\\dev\\ios\\b39ios.py' % (py, py)
subprocess.Popen(cmd, shell=True, creationflags=0x00000008 | 0x00000200 | 0x01000000, close_fds=True)
print('CHAIN39_STARTED')

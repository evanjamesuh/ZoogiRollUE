import subprocess, time
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
def hb(m):
    with open(r'C:\dev\ios\gob120_hb.txt','a') as f: f.write(time.strftime('%H:%M:%S') + ' 177v ' + m + '\n')
hb('verify start')
subprocess.run([py, r"C:\dev\ios\p5run.py", "skill_lab,golem_attack,pk_wood_shield"])
hb('verify done')

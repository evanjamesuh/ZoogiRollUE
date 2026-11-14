# Nov 6 b165 (Claude): story EP1 stop 1 = ZOOGI LEAGUE COURT (Spots.csv: label, sub, env Training) + new training_map.png tile from a
# top-down shot of the court -> TestFlight 165 (chest opening comes after Evan picks an option)
import subprocess, time, os
H = r'C:\dev\ios\gob120_hb.txt'
def hb(s): open(H, 'a').write(time.strftime('%H:%M:%S ') + '165 ' + s + '\n')
hb('start')
py = r"C:\Program Files\Epic Games\UE_5.6\Engine\Binaries\ThirdParty\Python3\Win64\python.exe"
while os.path.exists(r'C:\dev\unreal.lock'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "mapshot_training"])
hb('waiting for the tile (b165_cards_ok)')
while not os.path.exists(r'C:\dev\ios\b165_cards_ok'): time.sleep(15)
subprocess.run([py, r"C:\dev\ios\p5run.py", "story_spar"])
if 'BUILD_RC=0' not in open(r'C:\dev\ios\p5_status.txt').read():
    open(r'C:\dev\ios\b165ios_status.txt', 'w').write('SKIPPED: editor build failed\n'); hb('BUILD FAILED'); raise SystemExit
hb('-> ios')
subprocess.run([py, r"C:\dev\ios\b165ios.py"])

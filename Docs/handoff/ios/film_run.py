# Nov 3 b101 (Claude): MASTERCLASS film - one game launch per scene, frames -> C:/dev/proofs/film/<scene>/<NN_clip>/f_*.jpg + times.txt
import subprocess, time, os, sys
ST = 'C:/dev/ios/film_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
ONLY = sys.argv[1] if len(sys.argv) > 1 else ''
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject'
OUT = r'C:\dev\proofs\film'
B = '-ZoogiAuto -ZoogiHumanTest -ZoogiFilm -ZoogiNoScreenShatter '
AW = B + '-ZoogiAwakenTest -ZoogiFilmMaxClips=1 -ZoogiPlayers=4 -ZoogiRounds=3 '
SK = B + '-ZoogiSkillTest -ZoogiFilmMaxClips=2 -ZoogiPlayers=4 -ZoogiRounds=3 '
runs = (
    ('a_open_meteor', 55, AW + '-ZoogiAwakenBall=Hotstreak -ZoogiArena=2'),
    ('b_basics', 60, B + '-ZoogiFunTest -ZoogiFilmRoll=7 -ZoogiFilmMaxClips=2 -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3'),
    ('c_redirect', 70, B + '-ZoogiRedirectTest -ZoogiFilmRoll=8 -ZoogiFilmMaxClips=2 -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=3'),
    ('d_shockwave', 30, SK + '-ZoogiSkillOnly=energy_shockwave -ZoogiArena=1'),
    ('d_watercannon', 30, SK + '-ZoogiSkillOnly=water_cannon -ZoogiArena=4'),
    ('d_icewall', 30, SK + '-ZoogiSkillOnly=ice_wall -ZoogiArena=1'),
    ('d_woodshield', 30, SK + '-ZoogiSkillOnly=wood_shield -ZoogiArena=0'),
    ('d_boltballs', 30, SK + '-ZoogiSkillOnly=bolt_balls -ZoogiArena=6'),
    ('d_meteorrain', 30, SK + '-ZoogiSkillOnly=meteor_rain -ZoogiArena=3'),
    ('d_magicrays', 30, SK + '-ZoogiSkillOnly=magic_rays -ZoogiArena=5'),
    ('e_wolves', 75, B + '-ZoogiWolfBankTest -ZoogiFilmWolfTaps -ZoogiFilmMaxClips=1 -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2'),
    ('f_combo_shock_cannon', 32, SK + '-ZoogiSkillOnly=energy_shockwave+water_cannon -ZoogiArena=4'),
    ('f_combo_shock_meteor', 32, SK + '-ZoogiSkillOnly=energy_shockwave+meteor_rain -ZoogiArena=3'),
    ('f_combo_wall_cannon', 32, SK + '-ZoogiSkillOnly=ice_wall+water_cannon -ZoogiArena=1'),
    ('g_duel', 130, B + '-ZoogiClashTest -ZoogiFilmDuels -ZoogiFilmMaxClips=3 -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=3'),
    ('h_aw_vex', 60, AW + '-ZoogiAwakenBall=Vex -ZoogiFilmWolfTaps -ZoogiArena=3'),
    ('h_aw_blue', 50, AW + '-ZoogiAwakenBall=Blue -ZoogiArena=0'),
    ('h_aw_frosti', 50, AW + '-ZoogiAwakenBall=Frosti -ZoogiArena=1'),
    ('h_aw_lumi', 50, AW + '-ZoogiAwakenBall=Lumi -ZoogiArena=5'),
    ('h_aw_terra', 50, AW + '-ZoogiAwakenBall=Terra -ZoogiArena=0'),
    ('h_aw_zephyr', 50, AW + '-ZoogiAwakenBall=Zephyr -ZoogiArena=4'),
    ('h_aw_ember', 50, AW + '-ZoogiAwakenBall=Ember -ZoogiArena=6'),
    ('h_aw_hotstreak', 55, AW + '-ZoogiAwakenBall=Hotstreak -ZoogiArena=3'),
)
while os.path.exists('C:/dev/unreal.lock'): time.sleep(15)
open('C:/dev/unreal.lock', 'w').write('MASTERCLASS film captures (Claude, ZoogiRollUnreal-p5style)')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); time.sleep(3)
for name, secs, extra in runs:
    if ONLY and name not in ONLY.split(','): continue
    d = os.path.join(OUT, name); os.makedirs(d, exist_ok=True)
    args = '"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" -game -windowed -ResX=1600 -ResY=900 -WinX=100 -WinY=40 -ZoogiShots=%s %s -log -abslog=C:\\dev\\ios\\film_%s.log -nosplash' % (UE, P, d, extra, name)
    status('RUN_' + name)
    subprocess.Popen(args, shell=True)
    time.sleep(secs)
    subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True)
    time.sleep(5)
status('RUNS_DONE')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
status('DONE')

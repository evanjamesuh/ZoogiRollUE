# Oct 30 (Claude): P5-style branch (C:\dev\ZoogiRollUnreal-p5style, feat/p5-style): Win64 editor build + menu capture runs.
# argv: [runs (comma list or '' = all, 'none')] ; status -> C:/dev/ios/p5_status.txt, frames -> C:/dev/proofs/p5_style/<run>/
import subprocess, time, os, sys
if len(sys.argv) > 1 and sys.argv[1] == 'b202_turret,front_land':  # b202: two launcher instances - one test run, after the import
    if not os.path.exists(r'C:\dev\ios\b202_import_done'): sys.exit(0)
    try: os.close(os.open(r'C:\dev\ios\b202_tests.excl', os.O_CREAT | os.O_EXCL | os.O_WRONLY))
    except FileExistsError: sys.exit(0)
ST = 'C:/dev/ios/p5_status.txt'
def status(s): open(ST, 'a').write(time.strftime('%H:%M:%S ') + s + '\n')
open(ST, 'w').write('')
def editor_running():
    return 'UnrealEditor.exe' in subprocess.run('tasklist /FI "IMAGENAME eq UnrealEditor.exe"', shell=True, capture_output=True, text=True).stdout
ONLY = sys.argv[1] if len(sys.argv) > 1 else ''
status('RUNNER_UP')
subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True); subprocess.run('taskkill /F /IM CrashReportClientEditor.exe', shell=True, capture_output=True); time.sleep(3)
t0 = time.time()
while editor_running() and time.time() - t0 < 300: time.sleep(5)
open('C:/dev/unreal.lock', 'w').write('Win64 editor build + P5-style menu captures (Claude, ZoogiRollUnreal-p5style)')
UE = r'C:\Program Files\Epic Games\UE_5.6'
P = r'C:\dev\ZoogiRollUnreal-p5style\ZoogiRoll.uproject'
status('BUILD_START')
with open('C:/dev/ios/p5_build.log', 'w') as lf:
    rc = subprocess.run('"%s\\Engine\\Build\\BatchFiles\\Build.bat" ZoogiRollEditor Win64 Development -Project="%s" -WaitMutex -NoHotReload' % (UE, P), shell=True, stdout=lf, stderr=subprocess.STDOUT).returncode
status('BUILD_RC=%d' % rc)
if rc == 0 and ONLY != 'none':
    shots = r'C:\dev\proofs\p5_style'
    runs = (('menu_land', 1600, 900, 40, '-ZoogiP5MenuTest'),
            ('menu_port', 720, 1280, 32, '-ZoogiP5MenuTest'),
            ('match_frozen', 1600, 900, 70, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiFunTest -ZoogiArcTest -ZoogiHudPeekTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2'),
            ('crit_land', 1600, 900, 65, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiP5CritTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2'),
            ('crit_shatter', 1600, 900, 65, '-ZoogiAuto -ZoogiHumanTest -ZoogiP5CritTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2'),
            ('crit_port', 720, 1280, 65, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiP5CritTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2'),
            ('results_land', 1600, 900, 75, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiP5ResultsTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=3'),
            ('results_port', 720, 1280, 75, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiP5ResultsTest -ZoogiP5ResultsLose -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=3'),
            ('outer_frozen', 1600, 900, 85, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiOuterTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=3'),
            ('clash_ult', 1600, 900, 85, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiFunTest -ZoogiClashTest -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=2'),
            ('story_land', 1600, 900, 150, '-ZoogiStoryTest -ZoogiNoScreenShatter'),
            ('story_port', 720, 1280, 150, '-ZoogiStoryTest -ZoogiStoryLose -ZoogiNoScreenShatter'),
            ('story_film', 1600, 900, 75, '-ZoogiStoryTest -ZoogiStoryFilm -ZoogiNoScreenShatter'),
            ('stall_volc1', 1600, 900, 300, '-ZoogiStoryTest -ZoogiStoryMission=m2_boss -ZoogiStoryNoFinish -ZoogiNoScreenShatter'),
            ('stall_volc2', 1600, 900, 300, '-ZoogiStoryTest -ZoogiStoryMission=m2_rival -ZoogiStoryNoFinish -ZoogiNoScreenShatter'),
            ('map_select', 1600, 900, 30, '-ZoogiP5MenuTest -ZoogiMapSelectTest'),
            ('map_select_port', 720, 1280, 30, '-ZoogiP5MenuTest -ZoogiMapSelectTest'),
            ('wolf_test', 1600, 900, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiWolfTest -ZoogiBloodMoon -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('b87_check', 1600, 900, 170, '-ZoogiAuto -ZoogiHumanTest -ZoogiWolfBankTest -ZoogiSPOrbTest -ZoogiArena=3 -ZoogiPlayers=2 -ZoogiRounds=4 -ZoogiNoScreenShatter'),
            ('b88_check', 1600, 900, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiWolfBankTest -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('pk_frost_spikes', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=frost_spikes -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pk_glacier_lance', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=glacier_lance -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pk_ice_shards', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=ice_shards -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pk_stone_pillar', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=stone_pillar -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pk_earthquake', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=earthquake -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pk_rock_rampart', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=rock_rampart -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pk_crescent_slash', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=crescent_slash -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pk_cross_slash', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=cross_slash -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pk_ground_slash', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=ground_slash -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b176a
            ('pkf_frost_spikes', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=frost_spikes -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pkf_glacier_lance', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=glacier_lance -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pkf_ice_shards', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=ice_shards -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pkf_stone_pillar', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=stone_pillar -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pkf_earthquake', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=earthquake -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pkf_rock_rampart', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=rock_rampart -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pkf_crescent_slash', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=crescent_slash -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pkf_cross_slash', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=cross_slash -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pkf_ground_slash', 1280, 720, 45, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiFilm -ZoogiSkillOnly=ground_slash -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b177 film
            ('pk_wood_shield', 1600, 900, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=wood_shield -ZoogiArena=4 -ZoogiPlayers=3 -ZoogiNoScreenShatter'),  # b177k locked shield
            ('meadow_v3', 1600, 900, 75, '-ZoogiAuto -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=1 -ZoogiNoScreenShatter'),  # b178d
            ('underrealm', 1600, 900, 120, '-ZoogiAuto -ZoogiUnderrealm -ZoogiArena=3 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b178e
            ('golem_attack', 1600, 900, 90, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=2 -ZoogiPlayers=2 -ZoogiRounds=2'),  # b177g hammer
            ('dragon_peek', 1600, 900, 100, '-ZoogiAuto -ZoogiBossAlways -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b177 Dracoria
            ('dragon_breath', 1600, 900, 90, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=1'),  # b177 ICE BREATH
            ('skills_meteor', 1600, 900, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=meteor_rain -ZoogiArena=4 -ZoogiNoScreenShatter'),
            ('duel_soak', 1600, 900, 240, '-ZoogiAuto -ZoogiClashTest -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=4 -ZoogiNoScreenShatter'),
            ('wolf_front', 1600, 900, 60, '-ZoogiFrontTest -ZoogiNoScreenShatter'),
    ('crash_test', 1600, 900, 85, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiFunTest -ZoogiClashTest -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=2'),
    ('hang_test', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiHangTest -ZoogiReactShots -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter'),
    ('hang_port', 720, 1280, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiHangTest -ZoogiReactShots -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter'),
    ('volc_vfx', 1600, 900, 95, '-ZoogiAuto -ZoogiHumanTest -ZoogiWolfTest -ZoogiStyTest -ZoogiReactShots -ZoogiNoBloodMoon -ZoogiArena=2 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
    ('wolf_look', 1600, 900, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiWolfTest -ZoogiWolfLook -ZoogiNoBloodMoon -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('anc_shots', 1600, 900, 140, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),
            ('anc_test', 1600, 900, 190, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiAncientTest'),
            ('anc_match', 1600, 900, 150, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiRounds=2'),
            ('anc_royale', 1600, 900, 170, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiRoyaleBotHuman -ZoogiRoyaleFrames -ZoogiRoyaleFrameHz=1 -ZoogiRelicTest -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter'),
            ('weapons_royale', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiRoyaleBotHuman -ZoogiRoyaleFrames -ZoogiRoyaleFrameHz=1 -ZoogiWeaponCycle -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter'),  # b120 weapons
            ('royale_pause', 1600, 900, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiRoyaleBotHuman -ZoogiRoyalePauseTest -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter'),
            ('classic_clean', 720, 1280, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiFunTest -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('skills_land', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiArena=4 -ZoogiNoScreenShatter'),
            ('precision_test', 1600, 900, 90, '-ZoogiAuto -ZoogiForceCard=precision -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter'),
            ('frozen_open', 1600, 900, 70, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('skill_lab', 1600, 900, 560, '-ZoogiAuto -ZoogiSoloTrain -ZoogiSkillLabTest -ZoogiArena=4 -ZoogiNoScreenShatter'),
            ('wc_test', 1600, 900, 45, '-ZoogiAuto -ZoogiSoloTrain -ZoogiSkillLabTest -ZoogiWcYawTest -ZoogiArena=4 -ZoogiNoScreenShatter'),
            ('train_bots', 1600, 900, 40, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiTrainBotsTest -ZoogiArena=4 -ZoogiNoScreenShatter'),
            ('showtime_test', 1600, 900, 150, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Hotstreak -ZoogiArena=4 -ZoogiPlayers=2 -ZoogiRounds=3'),
            ('bolt_air', 1600, 900, 60, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiSkyDiveTest -ZoogiBoltAirTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2'),
            ('trick_test', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiTrickShot -ZoogiNoCosmosBoss -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=1 -ZoogiNoScreenShatter'),
            ('trick_ancient', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiTrickShot -ZoogiAncientClassic -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiRounds=1 -ZoogiNoScreenShatter'),
            ('dragon_poster', 1600, 900, 70, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiDragonPoster -ZoogiNoHUD -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2'),  # b179 frontal map art
            ('earth_meadow', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthEventsTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b180 earth events
            ('tr_blastopts', 1280, 720, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoGuide -ZoogiFilm -ZoogiFilmNoUI -ZoogiDemoReel -ZoogiDemoBlasts -ZoogiFilmMaxClips=10 -ZoogiNoLoadScreen -ZoogiNoEarthEvents -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b215 rock-impact blast options 1-8 (demo rig, fixed camera)
            ('tr_rockblast', 1280, 720, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthSlamTest -ZoogiRockBlastTour -ZoogiFilm -ZoogiFilmNoUI -ZoogiFilmRock=2.6 -ZoogiFilmMaxClips=10 -ZoogiNoLoadScreen -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b215 rock-impact blast options 1-8
            ('tr_rockframes', 1280, 720, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthSlamTest -ZoogiImpactFrameTour -ZoogiFilm -ZoogiFilmNoUI -ZoogiFilmRock=2.6 -ZoogiFilmMaxClips=10 -ZoogiNoLoadScreen -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b215 rock impact-frame options 1-10
            ('tr_raid_meadow', 1280, 720, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiBossAttackTest -ZoogiFilm -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilmNoUI -ZoogiFilmRoll=7 -ZoogiFilmBoss=7 -ZoogiFilmMaxClips=6 -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=4'),  # b214 trailer footage (film + audio)
            ('tr_raid_volc', 1280, 720, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiBossAttackTest -ZoogiFilm -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilmNoUI -ZoogiFilmRoll=7 -ZoogiFilmBoss=7 -ZoogiFilmMaxClips=6 -ZoogiArena=2 -ZoogiPlayers=3 -ZoogiRounds=4'),  # b214 trailer footage (film + audio)
            ('tr_raid_frozen', 1280, 720, 160, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiBossAttackTest -ZoogiFilm -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilmNoUI -ZoogiFilmAt=0.5 -ZoogiFilmLen=12 -ZoogiFilmName=intro -ZoogiFilmRoll=7 -ZoogiFilmBoss=7 -ZoogiFilmMaxClips=6 -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=4'),  # b214 trailer footage (film + audio)
            ('tr_raid_dark', 1280, 720, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiBossAttackTest -ZoogiDarkRealm -ZoogiFilm -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilmNoUI -ZoogiFilmRoll=7 -ZoogiFilmBoss=7 -ZoogiFilmMaxClips=6 -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=4'),  # b214 trailer footage (film + audio)
            ('tr_ballers', 1280, 720, 75, '-ZoogiFrontTest -ZoogiBallerTour -ZoogiFilm -ZoogiFilmNoUI -ZoogiFilmAt=3 -ZoogiFilmLen=45 -ZoogiFilmName=ballers -ZoogiNoScreenShatter'),  # b214 trailer footage (film + audio)
            ('tr_aw_blue', 1280, 720, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Blue -ZoogiFilm -ZoogiFilmNoUI -ZoogiFilmAwaken=11 -ZoogiFilmMaxClips=1 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b215 Awaken film (SHOWTIME path) + audio
            ('tr_aw_vex', 1280, 720, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Vex -ZoogiFilm -ZoogiFilmNoUI -ZoogiFilmAwaken=11 -ZoogiFilmMaxClips=1 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b215 Awaken film (SHOWTIME path) + audio
            ('tr_aw_frosti', 1280, 720, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Frosti -ZoogiFilm -ZoogiFilmNoUI -ZoogiFilmAwaken=11 -ZoogiFilmMaxClips=1 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b215 Awaken film (SHOWTIME path) + audio
            ('tr_aw_hotstreak', 1280, 720, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Hotstreak -ZoogiFilm -ZoogiFilmNoUI -ZoogiFilmAwaken=11 -ZoogiFilmMaxClips=1 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=2 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b215 Awaken film (SHOWTIME path) + audio
            ('tr_aw_lumi', 1280, 720, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Lumi -ZoogiFilm -ZoogiFilmNoUI -ZoogiFilmAwaken=11 -ZoogiFilmMaxClips=1 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=4 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b215 Awaken film (SHOWTIME path) + audio
            ('tr_howto', 1280, 720, 110, '-ZoogiTutorialTest -ZoogiHumanTest -ZoogiGuideAuto -ZoogiFilm -ZoogiFilmAt=4 -ZoogiFilmLen=70 -ZoogiFilmName=tutorial -ZoogiNoScreenShatter'),  # b214 trailer footage (film + audio)
            ('tr_skills', 1280, 720, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiFilmRoll=7 -ZoogiFilmMaxClips=4 -ZoogiFilm -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=4 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b214 trailer footage (film + audio)
            ('tr_demo', 1280, 720, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoGuide -ZoogiFilm -ZoogiFilmNoUI -ZoogiDemoReel -ZoogiNoLoadScreen -ZoogiNoEarthEvents -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b214 trailer footage (film + audio)
            ('tr_demo_p', 720, 1280, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoGuide -ZoogiFilm -ZoogiFilmNoUI -ZoogiDemoReel -ZoogiNoLoadScreen -ZoogiNoEarthEvents -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b214 trailer footage (film + audio)
            ('b270s_sword_slash', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=sword_slash -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b270 skill review check
            ('b270s_frost_spikes', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=frost_spikes -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b270 skill review check
            ('b270s_cross_slash', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=cross_slash -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b270 skill review check
            ('b270s_glacier_lance', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=glacier_lance -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b270 skill review check
            ('b270s_ice_wall', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=ice_wall -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b270 skill review check
            ('b270s_ice_shards', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=ice_shards -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b270 skill review check
            ('b276_menu', 1948, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiP5CmdTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b276 menu at SUPERPOWER
            ('b276_react', 1948, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiReactComboTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=50 -ZoogiFilmName=b276_react'),  # b276 combo plates
            ('b277_react', 1948, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiReactComboTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=50 -ZoogiFilmName=b277_react'),  # b277 combo plates right of SUPERPOWER
            ('b278_react', 1948, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiReactComboTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=50 -ZoogiFilmName=b278_react'),  # b278 combo plates beside REACTION
            ('b279_react', 1948, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiReactComboTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=50 -ZoogiFilmName=b279_react'),  # b279 skill then REACTION keeps the roll
            ('b280_desc', 1948, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiP5CmdTest -ZoogiDescTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b280 skill how-to band
            ('b281_port', 720, 1280, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiP5CmdTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b281 portrait menu at the Zoogi
            ('b282_react', 1948, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiReactComboTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=50 -ZoogiFilmName=b282_react'),  # b282 camera glides back on reaction
            ('b283_koth', 1948, 900, 40, '-ZoogiAuto -ZoogiHumanTest -ZoogiLoadScreen -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiParty=1'),  # b283 loading screens
            ('b283_royale', 720, 1280, 40, '-ZoogiAuto -ZoogiHumanTest -ZoogiLoadScreen -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiLoadMode=royale'),
            ('b283_trick', 1948, 900, 40, '-ZoogiAuto -ZoogiHumanTest -ZoogiLoadScreen -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiLoadMode=trickcourse'),
            ('b283_train', 1948, 900, 40, '-ZoogiAuto -ZoogiHumanTest -ZoogiLoadScreen -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiLoadMode=training'),
            ('b284_royale', 720, 1280, 40, '-ZoogiAuto -ZoogiHumanTest -ZoogiLoadScreen -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiLoadMode=royale'),  # b284 portrait tip strip
            ('b285_fall', 720, 1280, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiArena=4 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiLoadScreen -ZoogiFallTest'),  # b285 raid tips + fall guard
            ('b286_koth', 1948, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiParty=1 -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=3 -ZoogiFilmLen=60 -ZoogiFilmName=b286_koth'),  # b286 fort KOTH
            ('b275_match', 1948, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=60 -ZoogiFilmName=b275_match'),  # b275 CPU bubble position
            ('b274_hotorb', 1948, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiParty=2 -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=120 -ZoogiFilmName=b274_hotorb'),  # b274 hot orb toucher
            ('b274_rush', 1948, 900, 200, '-ZoogiAuto -ZoogiHumanTest -ZoogiParty=4 -ZoogiRaid -ZoogiRaidHP=25 -ZoogiArena=2 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b274 boss rush chain (weak titans)
            ('b273_koth', 1948, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiParty=1 -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=120 -ZoogiFilmName=b273_koth'),  # b273 party mode
            ('b273_hotorb', 1948, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiParty=2 -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=120 -ZoogiFilmName=b273_hotorb'),  # b273 party mode
            ('b273_storm', 1948, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiParty=3 -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=120 -ZoogiFilmName=b273_storm'),  # b273 party mode
            ('b273_rush', 1948, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiParty=4 -ZoogiRaid -ZoogiArena=2 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=120 -ZoogiFilmName=b273_rush'),  # b273 party mode
            ('b273_trick', 1948, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiParty=5 -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=120 -ZoogiFilmName=b273_trick'),  # b273 party mode
            ('b273_front', 1948, 900, 90, '-ZoogiFrontTest -ZoogiNoScreenShatter'),  # b273 play styles
            ('b272s_grapple_hook', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=grapple_hook -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b272 new skill
            ('b272s_bounce_pad', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=bounce_pad -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b272 new skill
            ('b272s_hex', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=hex -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b272 new skill
            ('b272s_tether', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=tether -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b272 new skill
            ('b272s_decoy', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=decoy -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b272 new skill
            ('b272s_edge_save', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=edge_save -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoLoadScreen'),  # b272 new skill
            ('b272_front', 1948, 900, 90, '-ZoogiFrontTest -ZoogiNoScreenShatter'),  # b272 skills tab
            ('b271_raid', 1948, 900, 190, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiNoLoadScreen -ZoogiArena=2 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=160 -ZoogiFilmName=b271_raid'),  # b271 rescue, combo, MVP
            ('b271_match', 1948, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b271 element bonus check
            ('b270_match', 1948, 900, 170, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=140 -ZoogiFilmName=b270_match'),  # b270 KO cam framing
            ('b269_match', 1948, 900, 170, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=140 -ZoogiFilmName=b269_match'),  # b269 KO cam, crystals, combo badge
            ('b269_raid', 1948, 900, 190, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiNoLoadScreen -ZoogiArena=2 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=160 -ZoogiFilmName=b269_raid'),  # b269 raid orb volley
            ('b268_match', 1948, 900, 170, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=140 -ZoogiFilmName=b268_match'),  # b268 spectacle: KO moment, shot cam, chain rings, final round
            ('b268_raid', 1948, 900, 190, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiNoLoadScreen -ZoogiArena=2 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=5 -ZoogiFilmLen=160 -ZoogiFilmName=b268_raid'),  # b268 raid playtest (Evan: learn how to increase combat)
            ('b267_awaken', 1948, 900, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Blue -ZoogiFilm -ZoogiFilmAwaken=11 -ZoogiFilmMaxClips=1 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b267 Awaken camera
            ('b267_match', 1948, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiReactShots -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=6 -ZoogiFilmLen=40 -ZoogiFilmName=b267_match'),  # b267 crowd + HUD
            ('b266_chest', 1948, 900, 80, '-ZoogiFrontTest -ZoogiChestTest -ZoogiNoScreenShatter'),  # b266 straight chest rows
            ('b266_front', 1948, 900, 90, '-ZoogiFrontTest -ZoogiNoScreenShatter'),  # b266 menus overlap check
            ('b266_awaken', 1948, 900, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Blue -ZoogiFilm -ZoogiFilmAwaken=11 -ZoogiFilmMaxClips=1 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b266 Awaken cinematic via SHOWTIME
            ('b266_match', 1948, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiReactShots -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=6 -ZoogiFilmLen=50 -ZoogiFilmName=b266_match'),  # b266 meadow crowd + HUD
            ('b265_chest', 1948, 900, 80, '-ZoogiFrontTest -ZoogiChestTest -ZoogiNoScreenShatter'),  # b265 chest cards, iPhone aspect
            ('b265_front', 1948, 900, 90, '-ZoogiFrontTest -ZoogiNoScreenShatter'),  # b265 menu tour, iPhone aspect (overlap check)
            ('b265_awaken', 1948, 900, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiShowtimeTest -ZoogiAwakenBall=Blue -ZoogiFilm -ZoogiFilmAwaken=11 -ZoogiFilmMaxClips=1 -ZoogiNoScreenShatter -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3'),  # b265 Awaken cinematic with HUD
            ('b265_match', 1948, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiReactShots -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=6 -ZoogiFilmLen=60 -ZoogiFilmName=b265_match'),  # b265 gameplay HUD + banners
            ('b264_htl', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=1 -ZoogiRoyaleBotHuman -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=10 -ZoogiFilmLen=18 -ZoogiFilmName=b264_shooter'),  # b264 aim zoom + recoil
            ('b249_hero', 1600, 900, 120, '-ZoogiNoSplash -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiVoyLevel=1 -ZoogiVoyShots=C:/dev/proofs/p5_style/b249_hero'),
            ('win_demo2', 1600, 900, 480, '-ZoogiAuto -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiNoSplash -ZoogiFilm -ZoogiFilmMaxClips=14'),  # b247 Evan: demo of Claude winning
            ('b247_cover', 1600, 900, 120, '-ZoogiNoSplash -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiVoyLevel=3 -ZoogiVoyPadTest -ZoogiVoyShots=C:/dev/proofs/p5_style/b247_cover'),
            ('win_demo', 1600, 900, 300, '-ZoogiAuto -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiNoSplash'),  # Evan: demo of Claude winning (P1 = CPU brain)
            ('b246_padui', 1600, 900, 90, '-ZoogiNoSplash -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiVoyLevel=1 -ZoogiVoyPadTest'),
            ('b246_pad', 1600, 900, 130, '-ZoogiNoSplash -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiVoyLevel=1 -ZoogiVoyPadTest -ZoogiVoyShots=C:/dev/proofs/p5_style/b246_pad'),  # b246 touch pad + Zoogi hero
            ('voy_wave', 1600, 900, 120, '-ZoogiNoSplash -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiVoyLevel=1 -ZoogiVoyShots=C:/dev/proofs/p5_style/voy_wave'),  # Nov 11 voyager port
            ('voy_ai', 1600, 900, 120, '-ZoogiNoSplash -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiVoyLevel=2 -ZoogiVoyShots=C:/dev/proofs/p5_style/voy_ai'),  # Nov 11 voyager port
            ('voy_city', 1600, 900, 120, '-ZoogiNoSplash -ZoogiNoTutorial -ZoogiNoScreenShatter -ZoogiVoyLevel=3 -ZoogiVoyShots=C:/dev/proofs/p5_style/voy_city'),  # Nov 11 voyager port
            ('b245_pip', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiWallPipTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=4'),
            ('b245_pip_port', 720, 1280, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiWallPipTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=4'),
            ('b244_cpu', 1600, 900, 150, '-ZoogiAuto -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiAllSkills -ZoogiFilm -ZoogiFilmMaxClips=6'),  # b244 CPU earth block
            ('b243_dark', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiDarkArena -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiNoTutorial'),  # b243 dark arena + no arches
            ('b241_menu', 1600, 900, 100, '-ZoogiP5MenuTest'),
            ('b241_voy', 1600, 900, 200, '-ZoogiP5MenuTest -ZoogiVoySelTest'),
            ('b239_front', 1600, 900, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),
            ('b239_live', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiAwardBannerTest -ZoogiWallCineAlways -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=8'),
            ('b238_front', 1600, 900, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),  # b238 Grok-style inventory
            ('b238_front_port', 720, 1280, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),
            ('b238_live', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiAwardBannerTest -ZoogiWallCineAlways -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=8'),  # b238 live cine triggers + award banner
            ('b237_meadow', 1600, 900, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiArenaShots -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll=8 -ZoogiFilmMaxClips=2'),  # b237 Meadow villages
            ('b237_htl', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=1 -ZoogiShooterDashTest -ZoogiRoyaleBotHuman -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=10 -ZoogiFilmLen=18 -ZoogiFilmName=hold_the_line'),  # b237 shooter on Meadow
            ('b236_htl', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=1 -ZoogiShooterDashTest -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=10 -ZoogiFilmLen=18 -ZoogiFilmName=hold_the_line'),  # b236 Tripo head + crystals + ball dash
            ('b236_tdm', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=3 -ZoogiTdmGoal=6 -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=12 -ZoogiFilmLen=14 -ZoogiFilmName=team_deathmatch'),  # b236 Tripo heads on the squads
            ('b236_story', 1600, 900, 150, '-ZoogiStoryTest -ZoogiStoryMission=m5_line -ZoogiNoScreenShatter'),  # b236 smaller head
            ('b235_htl', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=1 -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=12 -ZoogiFilmLen=16 -ZoogiFilmName=hold_the_line'),  # b235 Zoogi-look hero vs Void Hollows
            ('b235_tdm', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=3 -ZoogiTdmGoal=6 -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=12 -ZoogiFilmLen=14 -ZoogiFilmName=team_deathmatch'),  # b235 Zoogi-look squads
            ('b235_cine', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiWallCineTour -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=2'),  # b235 Zoogi-look fighters close up
            ('b235_story', 1600, 900, 150, '-ZoogiStoryTest -ZoogiStoryMission=m5_line -ZoogiNoScreenShatter'),  # b235 EPISODE 5 mission
            ('b234_htl', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=1 -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=12 -ZoogiFilmLen=16 -ZoogiFilmName=hold_the_line'),  # b234 HOLD THE LINE
            ('b234_sroyale', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=2 -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=12 -ZoogiFilmLen=14 -ZoogiFilmName=shooter_royale'),  # b234 SHOOTER ROYALE
            ('b234_tdm', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiShooter=3 -ZoogiTdmGoal=6 -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=12 -ZoogiFilmLen=14 -ZoogiFilmName=team_deathmatch'),  # b234 TEAM DEATHMATCH
            ('b233_royale2', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiRoyaleOld -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=18 -ZoogiFilmLen=14 -ZoogiFilmName=royale_hero'),  # b233 Voyager royale hero (pure royale)
            ('b233_cine', 1600, 900, 130, '-ZoogiAuto -ZoogiHumanTest -ZoogiWallCineTour -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=8'),  # b233 cine with Voyager fighters
            ('b233_royale', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiRoyaleBotHuman -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=25 -ZoogiFilmLen=14 -ZoogiFilmName=royale_hero'),  # b233 Voyager royale hero
            ('b233_raid', 1600, 900, 170, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_turret -ZoogiTurretShot=5 -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiArena=5 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll=9 -ZoogiFilmBoss=7 -ZoogiFilmMaxClips=6'),  # b233 tomb spider + raid crew + rocket turret FX
            ('b232_cine2', 1600, 900, 130, '-ZoogiAuto -ZoogiHumanTest -ZoogiWallCineTour -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=8'),  # b232 cine re-film
            ('b232_cine', 1600, 900, 130, '-ZoogiAuto -ZoogiHumanTest -ZoogiWallCineTour -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=8'),  # b232 cine on a clean screen
            ('b231_cine', 1600, 900, 130, '-ZoogiAuto -ZoogiHumanTest -ZoogiWallCineTour -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=8'),  # b231 cine v2
            ('b230_cine', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiWallCineTour -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiNoTutorial -ZoogiFilm -ZoogiFilmMaxClips=8'),  # b230 wall-smash cinematic, trigger options 1-6
            ('b230_crowd', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiCrowdCam -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=10 -ZoogiFilmLen=7 -ZoogiFilmName=crowd_signs'),  # b230 signs the right way up
            ('b229_rock', 1600, 900, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthSlamTest -ZoogiFilm -ZoogiFilmRock=2.6 -ZoogiFilmMaxClips=3 -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b229 FIRE BLAST default (option 4)
            ('b229_crowd', 1600, 900, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiCrowdCam -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=10 -ZoogiFilmLen=7 -ZoogiFilmName=crowd_signs'),  # b229 sign close-up
            ('b229_relay', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiRapidDashTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll=9 -ZoogiFilmMaxClips=1'),  # b229 relay refunds
            ('b229_front', 1600, 900, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),  # b229 gold pill + chest items
            ('b228_crowd', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll=8 -ZoogiFilmMaxClips=3'),  # b228 toon crowd + signs
            ('b228_relay', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiRapidDashTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll=9 -ZoogiFilmMaxClips=1'),  # b228 ORB RELAY combo + slow-mo
            ('b228_front', 1600, 900, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),  # b228 chest items + ITEM tab
            ('b227_front', 1600, 900, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),  # b227 item counts (option A)
            ('b226_relay', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiRapidDashTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll=9 -ZoogiFilmMaxClips=1'),  # b226 ORB RELAY (was GATTLING RAPID)
            ('b225_lab', 1600, 900, 50, '-ZoogiAuto -ZoogiSoloTrain -ZoogiSkillLabTest -ZoogiLabOnly=gattling_turret -ZoogiArena=0 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmAt=12 -ZoogiFilmLen=9 -ZoogiFilmName=lab_turret'),  # b225 turret in the Skill Lab
            ('b224_dashv', 1600, 900, 100, '-ZoogiAuto -ZoogiHumanTest -ZoogiRapidDashTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll=9 -ZoogiFilmMaxClips=2'),  # b224 dash film
            ('b224_dash', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRapidDashTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll -ZoogiFilmMaxClips=4'),  # b224 GATTLING RAPID dashes (test run starts on the human's roll)
            ('b223_turret', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_turret -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll -ZoogiFilmMaxClips=6'),  # b223 turret: 10 shots
            ('b223_raidturret', 1600, 900, 170, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_turret -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiArena=2 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll -ZoogiFilmMaxClips=6'),  # b223 turret shoots the raid boss
            ('b223_dash', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_beat -ZoogiRapidDashTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll -ZoogiFilmMaxClips=6'),  # b223 GATTLING RAPID dashes
            ('b223_ammo', 1600, 900, 60, '-ZoogiFrontTest -ZoogiAmmoTour -ZoogiNoScreenShatter'),  # b223 inventory AMMO row
            ('b222_turret', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_turret -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmRoll -ZoogiFilmMaxClips=6'),  # b222 rocket turret in a match
            ('tr_turretopts2', 1280, 720, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoGuide -ZoogiFilm -ZoogiFilmNoUI -ZoogiDemoReel -ZoogiDemoTurret -ZoogiDemoTurretCamH=2000 -ZoogiFilmMaxClips=8 -ZoogiNoLoadScreen -ZoogiNoEarthEvents -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b221 turret options, wider top view
            ('b221_itemko2', 1600, 900, 240, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiItemKOTest'),  # b221 item-KO crystals, long enough for P1's 2nd roll
            ('b221_itemko', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiItemKOTest'),  # b221 item-KO crystals (test throws P2 off on P1's roll)
            ('tr_turretopts', 1280, 720, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoGuide -ZoogiFilm -ZoogiFilmNoUI -ZoogiDemoReel -ZoogiDemoTurret -ZoogiFilmMaxClips=8 -ZoogiNoLoadScreen -ZoogiNoEarthEvents -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b221 turret shot options 1-6, top view
            ('b220_itemko', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=4 -ZoogiNoScreenShatter -ZoogiItemKOTest'),  # b220 item knockouts pay +2 crystals (test: every KO counts)
            ('b219_meadow', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b219 Meadow robot titan back, Z medallion centre, no hub pillar
            ('b218_meadow', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilm -ZoogiFilmBoss=7 -ZoogiFilmMaxClips=6 -ZoogiFilmName=b218_golem'),  # b218 rigged golem stone material
            ('b217_meadow', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter -ZoogiFilmBoss=7 -ZoogiFilmMaxClips=5'),  # b217 rigged + animated Meadow golem, banners hidden on the boss cam
            ('b216_meadow', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b216 Meadow golem original colour, whole body
            ('b215_meadow', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b215 Meadow titan faces the centre
            ('b215_dark', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiDarkRealm -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b215 Dark Realm titan faces the centre
            ('b214_meadow', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b214 purple stone golem = Meadow titan
            ('b214_dark', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiNoLoadScreen -ZoogiDarkRealm -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b214 crystal brute = Dark Realm titan
            ('b213_demo', 1280, 720, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiNoGuide -ZoogiFilm -ZoogiFilmNoUI -ZoogiDemoReel -ZoogiNoLoadScreen -ZoogiNoEarthEvents -ZoogiNoTutorial -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b213 previs reel: burnout roll, model fight, fly, rock smash
            ('b213_slam', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthSlamTest -ZoogiNoLoadScreen -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b213 solid rock bends + side blast
            ('b213_wall', 1600, 900, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiSkillTest -ZoogiSkillOnly=sapphire_wall -ZoogiShowEarthCol -ZoogiNoLoadScreen -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b213 skill wall solid (pink = the solid)
            ('b211_buddy', 1600, 900, 60, '-ZoogiFrontTest -ZoogiBuddyTest -ZoogiNoScreenShatter'),  # b211 Blue buddy on the home screen
            ('b211_first', 1600, 900, 230, '-ZoogiSplashTest -ZoogiTutorialTest -ZoogiHumanTest -ZoogiGuideAuto -ZoogiNoScreenShatter'),  # b211 splash -> guided first match -> home + Blue buddy
            ('home_clean', 1600, 900, 70, '-ZoogiFrontTest -ZoogiFrontClean -ZoogiNoScreenShatter'),  # b209 clean home background
            ('home_clean_p', 720, 1280, 30, '-ZoogiFrontTest -ZoogiFrontClean -ZoogiNoScreenShatter'),
            ('b208_ballers', 1600, 900, 70, '-ZoogiFrontTest -ZoogiBallerTour -ZoogiNoScreenShatter'),  # b208 Tripo Ballers 3D view
            ('b207_raid', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiLoadScreen -ZoogiRaid -ZoogiRaidHP=200 -ZoogiBossAttackTest -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter'),  # b207 raid HUD A + D + loading E
            ('b207_classic', 1600, 900, 60, '-ZoogiAuto -ZoogiHumanTest -ZoogiLoadScreen -ZoogiArena=3 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b207 loading F
            ('b206_meadow', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b206 Tally Golem
            ('b203_dark', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiDarkRealm -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b203 dark realm map
            ('b202_turret', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_turret -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b202 Tripo turret
            ('raid_ch_armor', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiRaidChallenge=ARMOR -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter'),  # b201
            ('raid_ch_bank', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=200 -ZoogiRaidChallenge=BANK -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter'),  # b201
            ('b200_t', 1600, 900, 300, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthEventsTest -ZoogiDumpTall -ZoogiSkillTest -ZoogiSkillOnly=gattling_beat -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter'),
            ('b200_d', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthEventsTest -ZoogiDumpTall -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b200 poles dump + rapid circles
            ('b196_t', 1600, 900, 240, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthEventsTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_turret+gattling_burst -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b196 turret + bolts + earth colliders
            ('b193_frozen', 1600, 900, 330, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=4 -ZoogiNoScreenShatter'),  # b193 dracoria vs snowman + old rocks
            ('b191_t2', 1600, 900, 170, '-ZoogiAuto -ZoogiHumanTest -ZoogiBossGrabTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_beat+gattling_turret -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b191 grab + beat + turret
            ('b190_boss', 1600, 900, 330, '-ZoogiAuto -ZoogiHumanTest -ZoogiBossAngerTest -ZoogiSkillTest -ZoogiSkillOnly=gattling_burst,gattling_beat,gattling_turret -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=5 -ZoogiNoScreenShatter'),  # b190 boss + gattling
            ('gattling_test', 1600, 900, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiGattlingTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b182 SHOWTIME
            ('gattling_port', 720, 1280, 70, '-ZoogiAuto -ZoogiHumanTest -ZoogiGattlingTest -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b182 SHOWTIME portrait
            ('react_closeup', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiFunTest -ZoogiReactShots -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b181 attacker close-ups
            ('meadow_build', 1600, 900, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiBuildingTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b180 building blast
            ('meadow_stands', 1600, 900, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiStandsTest -ZoogiArena=0 -ZoogiPlayers=3 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b180 bleacher crowd
            ('earth_frozen', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiEarthEventsTest -ZoogiNoDragonIntro -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b180 earth events
            ('raid_frozen', 1600, 900, 150, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=60 -ZoogiArena=1 -ZoogiPlayers=3 -ZoogiRounds=4 -ZoogiNoScreenShatter'),  # b179 heart crystal
            ('raid_test', 1600, 900, 360, '-ZoogiAuto -ZoogiHumanTest -ZoogiRaid -ZoogiRaidHP=90 -ZoogiArena=3 -ZoogiPlayers=3 -ZoogiRounds=10 -ZoogiNoScreenShatter'),
            ('royale_mix', 1600, 900, 420, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiRoyaleBotHuman -ZoogiRounds=4 -ZoogiMixActionSec=20 -ZoogiPlayers=4 -ZoogiNoScreenShatter'),
            ('boss_meadow', 1600, 900, 55, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=1'),
            ('boss_frozen', 1600, 900, 55, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=1'),
            ('boss_volcano', 1600, 900, 55, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=2 -ZoogiPlayers=2 -ZoogiRounds=1'),
            ('boss_arabian', 1600, 900, 55, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=4 -ZoogiPlayers=2 -ZoogiRounds=1'),
            ('boss_tomb', 1600, 900, 55, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=5 -ZoogiPlayers=2 -ZoogiRounds=1'),
            ('boss_neon', 1600, 900, 55, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=6 -ZoogiPlayers=2 -ZoogiRounds=1'),
            ('boss_test', 1600, 900, 150, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiBossAttackTest -ZoogiArena=3 -ZoogiPlayers=2 -ZoogiRounds=3'),
            ('skydive_test', 1600, 900, 70, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiSkyDiveTest -ZoogiArena=3 -ZoogiPlayers=2 -ZoogiRounds=2'),
            ('item_test', 1600, 900, 70, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiItemTest -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2'),
            ('stands_test', 1600, 900, 60, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiStandsTest -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2'),
            ('hand_test', 1600, 900, 60, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiTestHand=water_cannon,ice_wall,wolf_pack -ZoogiArena=4 -ZoogiNoScreenShatter'),
            ('hand_wide', 1792, 828, 60, '-ZoogiAuto -ZoogiHumanTest -ZoogiSoloTrain -ZoogiTestHand=water_cannon,ice_wall,wolf_pack -ZoogiArena=4 -ZoogiNoScreenShatter'),  # b119b: iPhone landscape aspect
            ('cpu_smart', 1600, 900, 130, '-ZoogiAuto -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=3 -ZoogiNoScreenShatter'),  # b124 CPU brain
            ('cine_land', 1600, 900, 50, '-ZoogiAuto -ZoogiHumanTest -ZoogiCineDragTest -ZoogiArena=4 -ZoogiPlayers=2 -ZoogiNoScreenShatter'),  # b125 drag-back push-in (b19c: dist 1150 -> 681, fov 84 -> 72)
            ('card_art', 1200, 1000, 75, '-ZoogiAuto -ZoogiSoloTrain -ZoogiCardArtShoot -ZoogiNoHUD -ZoogiArena=7 -ZoogiNoScreenShatter'),  # b138 card art studio
            ('cosmos_boss', 1792, 828, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=3 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b136 the cosmos boss watcher
            ('volcano_meteors', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiArena=2 -ZoogiPlayers=2 -ZoogiRounds=3 -ZoogiVolcanoMeteorsTest -ZoogiNoScreenShatter'),  # b139 volcano meteor event
            ('cosmic_markers', 1600, 900, 170, '-ZoogiAuto -ZoogiArena=3 -ZoogiPlayers=2 -ZoogiRounds=4 -ZoogiNoScreenShatter'),  # b129 purple markers from round 2, growing
            ('anc_classic', 1600, 900, 150, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiAncientClassic -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiRounds=2'),  # b162 v15 ring
            ('court_match', 1600, 900, 120, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiArena=7 -ZoogiPlayers=2 -ZoogiRounds=2'),  # b163 league court
            ('mapshot_training', 1600, 900, 80, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=7 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiMapShot -ZoogiNoHUD -ZoogiShowcaseWait=12'),  # b165 story stop 1 tile
            ('chest_land', 1600, 900, 80, '-ZoogiFrontTest -ZoogiChestTest -ZoogiNoScreenShatter'),  # b166 chest shows
            ('chest_port', 720, 1280, 80, '-ZoogiFrontTest -ZoogiChestTest -ZoogiNoScreenShatter'),  # b166 chest shows (portrait)
            ('story_final', 1600, 900, 170, '-ZoogiStoryTest -ZoogiStoryMission=m1_final -ZoogiNoScreenShatter'),  # b166 2v2 story on classic
            ('story_dunes', 1600, 900, 170, '-ZoogiStoryTest -ZoogiStoryMission=m2_dunes -ZoogiNoScreenShatter'),  # b166 2v2 story on classic
            ('crush_test', 1600, 900, 80, '-ZoogiAuto -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=1 -ZoogiFeelLog -ZoogiNoScreenShatter'),  # b166 crush hit sounds (log)
            ('dev_land', 1600, 900, 70, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=4 -ZoogiDevAuto -ZoogiNoScreenShatter'),  # dev mode check
            ('dev_iphone', 592, 1280, 70, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=4 -ZoogiDevAuto -ZoogiNoScreenShatter'),  # dev mode check (phone)
            ('dev_menu', 1600, 900, 70, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=4 -ZoogiDevAuto -ZoogiDevMenuTest -ZoogiNoScreenShatter'),  # b167 dev mode via the pause menu path (unlocked)
            ('dev_menu_pin', 592, 1280, 60, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=4 -ZoogiDevAuto -ZoogiDevMenuTest -ZoogiNoScreenShatter -ZoogiDevLockedTest'),  # b167 locked device -> PIN pad
            ('dev_nopin', 592, 1280, 60, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=4 -ZoogiDevAuto -ZoogiDevMenuTest -ZoogiNoScreenShatter'),  # b168 no PIN
            ('modes_land', 1600, 900, 40, '-ZoogiP5MenuTest -ZoogiP5AllModes'),  # b170 every mode card
            ('modes_port', 720, 1280, 40, '-ZoogiP5MenuTest -ZoogiP5AllModes'),  # b170 every mode card (phone)
            ('ko_now', 1600, 900, 120, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiKOShow -ZoogiKOLine -ZoogiNoScreenShatter'),  # b171 KO example: today
            ('ko_new', 1600, 900, 120, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiKOShow -ZoogiKOLine -ZoogiFlightCap=300 -ZoogiFallOut=300 -ZoogiRollPitch=60 -ExecCmds="zoogi.Cam.AimPitch 32" -ZoogiNoScreenShatter'),  # b171 KO example: 3 m + tilted camera
            ('ko_new_p', 592, 1280, 120, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiKOShow -ZoogiKOLine -ZoogiFlightCap=300 -ZoogiFallOut=300 -ZoogiRollPitch=60 -ExecCmds="zoogi.Cam.AimPitch 32" -ZoogiNoScreenShatter'),  # b171 KO example phone
            ('trick_iphone', 592, 1280, 40, '-ZoogiTrickTest -ZoogiNoScreenShatter'),  # b171 trick pop fill

            ('trick_land', 1600, 900, 40, '-ZoogiTrickTest -ZoogiNoScreenShatter'),

            ('ko_shake', 1600, 900, 120, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiKOShow -ZoogiKOLine -ZoogiNoScreenShatter'),  # b171 KO at edge + blast shake
            ('meadow_shots', 1600, 900, 120, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b171e meadow ref rim + stands
            ('hud_land', 1600, 900, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiArenaHUDTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b171d big badges
            ('hud_iphone', 592, 1280, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiArenaHUDTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('hud_iphone2', 592, 1280, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiArenaHUDTest -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('weather_meadow', 1600, 900, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiWeather -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b172 weather + marble finder
            ('weather_frozen', 592, 1280, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiWeather -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('weather_volc', 1600, 900, 110, '-ZoogiAuto -ZoogiHumanTest -ZoogiWeather -ZoogiArena=2 -ZoogiPlayers=2 -ZoogiRounds=3 -ZoogiNoScreenShatter'),
            ('guide_tap', 592, 1280, 40, '-ZoogiAuto -ZoogiHumanTest -ZoogiGuideTest -ZoogiGuideTap -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b176d waits for a tap
            ('guide_port', 592, 1280, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiGuideTest -ZoogiArena=0 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b172e trainer guide
            ('guide_land', 1600, 900, 90, '-ZoogiAuto -ZoogiHumanTest -ZoogiGuideTest -ZoogiArena=1 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('crash_port', 592, 1280, 85, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiFunTest -ZoogiClashTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2'),  # clash loser smash
            ('boss_volc', 1600, 900, 100, '-ZoogiAuto -ZoogiBossAlways -ZoogiArena=2 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),  # b175b stone golem magma titan
            ('boss_volc_p', 592, 1280, 100, '-ZoogiAuto -ZoogiBossAlways -ZoogiArena=2 -ZoogiPlayers=2 -ZoogiRounds=2 -ZoogiNoScreenShatter'),

            ('story_spar', 1600, 900, 150, '-ZoogiStoryTest -ZoogiStoryMission=m1_spar -ZoogiNoScreenShatter'),  # Evan 12:57 ep1 practice field review
            ('story_neon', 1600, 900, 170, '-ZoogiStoryTest -ZoogiStoryMission=m3_neon -ZoogiNoScreenShatter'),  # b151 classic Blackout Brawl + EP3 film P2
            ('story_ep3', 1600, 900, 170, '-ZoogiStoryTest -ZoogiStoryMission=m3_tavi -ZoogiNoScreenShatter'),  # b132 EP3 films
            ('skill_lab_frozen', 900, 1600, 75, '-ZoogiAuto -ZoogiSoloTrain -ZoogiSkillLabTest -ZoogiArena=1 -ZoogiNoScreenShatter'),
            ('parry_land', 1600, 900, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiFunTest -ZoogiParryShieldTest -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('shift_frozen', 1600, 900, 120, '-ZoogiAuto -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=4 -ZoogiStageShiftTest -ZoogiVortexTest -ZoogiNoScreenShatter'),
            ('front_land', 1600, 900, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),
            ('front_port', 720, 1280, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),
            ('front_iphone', 592, 1280, 75, '-ZoogiFrontTest -ZoogiNoScreenShatter'),
            ('turn_iphone', 592, 1280, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiGuideTest -ZoogiRedirectTest -ZoogiDuelTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('turn_land', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiGuideTest -ZoogiRedirectTest -ZoogiDuelTest -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('classic_iphone', 592, 1280, 80, '-ZoogiAuto -ZoogiHumanTest -ZoogiFunTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiNoScreenShatter'),
            ('anc_motion', 1600, 900, 120, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiRoyaleBotHuman -ZoogiRoyaleFrames -ZoogiRoyaleFrameHz=10 -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter'),
            ('shift_classic', 1600, 900, 110, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiStageShiftTest -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=4'),
            ('shift_royale', 1600, 900, 75, '-ZoogiAuto -ZoogiHumanTest -ZoogiRoyale -ZoogiRoyaleBotHuman -ZoogiRoyaleFrames -ZoogiRoyaleFrameHz=1 -ZoogiStageShiftTest -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiNoScreenShatter'),
            ('menu_tour', 1600, 900, 90, '-ZoogiMenuTour=C:/dev/proofs/p5_style/menu_tour_shots'),
            ('map_meadow', 1600, 900, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),
            ('map_frozen', 1600, 900, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),
            ('map_volcanic', 1600, 900, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=2 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),
            ('map_cosmic', 1600, 900, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=3 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),
            ('map_arabian', 1600, 900, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),
            ('map_tomb', 1600, 900, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=5 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),
            ('map_neon', 1600, 900, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=6 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),
            ('pmap_meadow', 720, 1280, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=0 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b120 portrait map art
            ('pmap_frozen', 720, 1280, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b120 portrait map art
            ('pmap_volcanic', 720, 1280, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=2 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b120 portrait map art
            ('pmap_cosmic', 720, 1280, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=3 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b120 portrait map art
            ('pmap_arabian', 720, 1280, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=4 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b120 portrait map art
            ('pmap_tomb', 720, 1280, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=5 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b120 portrait map art
            ('pmap_neon', 720, 1280, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=6 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b120 portrait map art
            ('pmap_ancient', 720, 1280, 140, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=9 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b161 Evan 12:43 'update map image'
            ('pmap_teaser', 720, 1280, 95, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiArena=8 -ZoogiPlayers=4 -ZoogiRounds=2 -ZoogiArenaShots -ZoogiNoHUD -ZoogiShowcaseWait=10'),  # b120 portrait map art
            ('p5cmd_land', 1600, 900, 75, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiP5CmdTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2'),
            ('p5cmd_port', 720, 1280, 75, '-ZoogiAuto -ZoogiNoScreenShatter -ZoogiHumanTest -ZoogiP5CmdTest -ZoogiArena=1 -ZoogiPlayers=4 -ZoogiRounds=2'))
    for runname, rx, ry, secs, extra in runs:
        if ONLY and runname not in ONLY.split(','): continue
        d = os.path.join(shots, runname); os.makedirs(d, exist_ok=True)
        for f in os.listdir(d):
            try: os.remove(os.path.join(d, f))
            except Exception: pass
        args = '"%s\\Engine\\Binaries\\Win64\\UnrealEditor.exe" "%s" -game -windowed -ResX=%d -ResY=%d -WinX=200 -WinY=40 -ZoogiShots=%s %s -log -abslog=C:\\dev\\ios\\p5_%s.log -nosplash' % (UE, P, rx, ry, d, extra, runname)
        status('RUN_' + runname)
        subprocess.Popen(args, shell=True)
        time.sleep(secs)
        subprocess.run('taskkill /F /IM UnrealEditor.exe', shell=True, capture_output=True)
        time.sleep(5)
    status('RUNS_DONE')
try: os.remove('C:/dev/unreal.lock')
except Exception: pass
try:
    0 and subprocess.Popen([UE + r'\Engine\Binaries\Win64\UnrealEditor.exe', r'C:\dev\ZoogiRollUnreal-combined\ZoogiRoll.uproject'], creationflags=0x00000008 | 0x00000200, close_fds=True)
    status('EDITOR_REOPENED')
except Exception as e: status('EDITOR_REOPEN_FAIL %r' % e)
status('DONE')

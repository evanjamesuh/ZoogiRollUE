# Nov 2 (Claude): local git backup of ZoogiRollUnreal-p5style (branch feat/p5-style). Never pushes.
import subprocess
R = r'C:\dev\ZoogiRollUnreal-p5style'
LOG = r'C:\dev\ios\gitbackup.log'
out = open(LOG, 'w', encoding='utf-8')
def g(*a):
    r = subprocess.run(['git', '-C', R] + list(a), capture_output=True)
    t = (r.stdout + r.stderr).decode('utf-8', 'replace')
    out.write('$ git %s  (rc=%d)\n%s\n' % (' '.join(a), r.returncode, t[-6000:])); out.flush()
    return r.returncode, t
g('branch', '--show-current')
rc, t = g('ls-files', '--others', '--exclude-standard')
import os, collections
sz = collections.Counter()
for f in t.splitlines():
    fp = os.path.join(R, f)
    if os.path.isfile(fp): sz['/'.join(f.split('/')[:2])] += os.path.getsize(fp)
out.write('NEW FILES BY FOLDER (MB):\n' + '\n'.join('%8.1f %s' % (v / 1e6, k) for k, v in sz.most_common(40)) + '\nTOTAL %.1f MB\n' % (sum(sz.values()) / 1e6))
g('add', '-A')
rc, t = g('diff', '--cached', '--stat')
msg = ('b73-b80: skill list sticky + SKILL button, CPU avatar icons, 3D celebration PiP, drag hint after first launch, Wolf Pack on SUPERPOWER (wolf badges + profile wolf heads), mode bar AUTO/ARC (AUTO = DRIVE force), anime + skill summons in the top card, no pizza on Arabian, spire + centre props fade, P5 menu input capture, comments below avatars, P5 MENU button (CAMERA / DEV inside), nothing over pause, sky taps ignored, FIRE folds to SKILL, Golden Apple rework, banners left, notes aiming only, lower landscape aim cam, Bolt Balls missiles, Awaken lasts 3 more turns, ultimate-ready under the meter\n\n'
       'Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\n'
       'Claude-Session: https://claude.ai/code/session_018D3XxQBD5s6jwdbtsQrk4L\n')
rc, t = g('commit', '-q', '-m', msg)
if rc != 0 and 'identity' in t:
    g('-c', 'user.name=Evan James', '-c', 'user.email=ejames@trcoa.com', 'commit', '-q', '-m', msg)
g('log', '--oneline', '-3')
g('status', '--short')
out.write('GIT_BACKUP_DONE\n'); out.close()

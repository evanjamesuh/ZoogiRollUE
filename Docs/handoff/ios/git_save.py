# Nov 13 (Claude, Evan: "save our progress on git so we can later copy work to a laptop"):
# commit everything in the p5-style worktree (branch feat/p5-style) and push it to GitHub in batches under GitHub's limits.
# Files over 95 MB are left out (GitHub refuses them) and listed in Docs/handoff/LARGE_FILES_NOT_IN_GIT.txt.
# The build notes / scripts outside the repo (C:\dev\HANDOFF.md, C:\dev\ios\*.py, run_anc.bat) are copied into Docs/handoff first.
import subprocess, os, sys, time, shutil, glob
W = r'C:\dev\ZoogiRollUnreal-p5style'
LOG = r'C:\dev\ios\git_save_log.txt'
def log(s):
    with open(LOG, 'a', encoding='utf-8') as f: f.write(time.strftime('%H:%M:%S ') + s + '\n')
def git(*a, check=True):
    r = subprocess.run(['git', '-C', W] + list(a), capture_output=True, text=True, encoding='utf-8', errors='replace', env=dict(os.environ, GIT_LITERAL_PATHSPECS='1'))
    log('git %s -> %d %s %s' % (' '.join(a[:3]), r.returncode, r.stdout[-600:].strip(), r.stderr[-900:].strip()))
    if check and r.returncode != 0: raise SystemExit('git failed: ' + ' '.join(a))
    return r
open(LOG, 'w').write('')
log('START')
# 1) the handoff copies
H = os.path.join(W, 'Docs', 'handoff'); os.makedirs(os.path.join(H, 'ios'), exist_ok=True)
shutil.copy2(r'C:\dev\HANDOFF.md', os.path.join(H, 'HANDOFF.md'))
shutil.copy2(r'C:\dev\run_anc.bat', os.path.join(H, 'run_anc.bat'))
for p in glob.glob(r'C:\dev\ios\*.py'): shutil.copy2(p, os.path.join(H, 'ios', os.path.basename(p)))
open(os.path.join(H, 'README_LAPTOP.md'), 'w', encoding='utf-8').write(
"""# Zoogi Roll - moving to a new laptop
Branch: feat/p5-style (this worktree was C:\\dev\\ZoogiRollUnreal-p5style on the desktop PC).
1. Install Unreal Engine 5.6, Visual Studio 2022 (Game dev with C++), Git (+ Git LFS).
2. git clone https://github.com/evanjamesuh/ZoogiRollUE.git C:\\dev\\ZoogiRollUnreal-p5style  then  git checkout feat/p5-style
3. Copy the files listed in LARGE_FILES_NOT_IN_GIT.txt from the desktop PC (same paths) - GitHub refuses files over 100 MB.
4. Right-click ZoogiRoll.uproject > Generate Visual Studio project files, then open it (it builds the C++ the first time).
5. Build notes: Docs/handoff/HANDOFF.md. The PC build scripts (p5run.py, bNNNios.py, gobNNNgo.py) are in Docs/handoff/ios -
   they expect C:\\dev\\ios and the MacinCloud Mac for the iOS / TestFlight step.
""")
# 2) what changed
r = git('status', '--porcelain', '-z', '-uall')
ent = [e for e in r.stdout.split('\0') if e]
files, big, i = [], [], 0
while i < len(ent):
    e = ent[i]; st, path = e[:2], e[3:]
    if st[0] == 'R': i += 1  # rename: next entry is the old path
    i += 1
    full = os.path.join(W, path)
    if 'D' in st: files.append((path, 0)); continue
    if os.path.isdir(full): continue
    sz = os.path.getsize(full) if os.path.exists(full) else 0
    if sz > 95 * 1024 * 1024: big.append((path, sz)); continue
    files.append((path, sz))
log('changed files: %d (%.1f GB), too big for GitHub: %d' % (len(files), sum(s for _, s in files) / 1e9, len(big)))
with open(os.path.join(H, 'LARGE_FILES_NOT_IN_GIT.txt'), 'w', encoding='utf-8') as f:
    f.write('Files over 95 MB - copy these from the desktop PC by hand (same path under the project folder):\n')
    for p, s in big: f.write('%8.1f MB  %s\n' % (s / 1048576, p))
files.append(('Docs/handoff/LARGE_FILES_NOT_IN_GIT.txt', 1))
# 3) batches: code / config / docs first, then Content in ~1 GB pushes
code = [f for f in files if not f[0].startswith('Content/')]
cont = [f for f in files if f[0].startswith('Content/')]
batches, cur, acc = [code], [], 0
for f in cont:
    if acc + f[1] > 1.0e9 and cur: batches.append(cur); cur, acc = [], 0
    cur.append(f); acc += f[1]
if cur: batches.append(cur)
TAIL = '\n\nCo-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>\nClaude-Session: https://claude.ai/code/session_018D3XxQBD5s6jwdbtsQrk4L'
for n, b in enumerate(batches):
    if not b: continue
    lst = os.path.join(r'C:\dev\ios', 'git_save_batch.txt')
    open(lst, 'w', encoding='utf-8').write('\n'.join(p for p, _ in b) + '\n')
    git('add', '-A', '--pathspec-from-file=' + lst)
    msg = ('B81-B286 progress save %d/%d: %s' % (n + 1, len(batches), 'code, config, docs, build notes' if n == 0 else 'content'))
    c = git('commit', '-m', msg + TAIL, check=False)
    if c.returncode != 0 and 'nothing to commit' in (c.stdout + c.stderr): log('batch %d: nothing to commit' % (n + 1)); continue
    if c.returncode != 0: raise SystemExit('commit failed')
    for attempt in range(3):
        p = git('push', 'github', 'feat/p5-style', check=False)
        if p.returncode == 0: break
        time.sleep(20)
    if p.returncode != 0: log('PUSH FAILED at batch %d' % (n + 1)); raise SystemExit(1)
    log('batch %d/%d pushed (%d files, %.2f GB)' % (n + 1, len(batches), len(b), sum(s for _, s in b) / 1e9))
log('DONE')

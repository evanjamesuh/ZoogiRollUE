# Oct 30 (Claude, Evan's P5-style brief): fresh copy of feat/battle-royale = new worktree on branch feat/p5-style,
# then today's uncommitted work from ZoogiRollUnreal-combined copied in (feat/battle-royale itself untouched, nothing pushed).
import subprocess, os, shutil, time
SRC = r'C:\dev\ZoogiRollUnreal-combined'
DST = r'C:\dev\ZoogiRollUnreal-p5style'
LOG = r'C:\dev\ios\p5_worktree.txt'
log = open(LOG, 'w', encoding='utf-8')
def say(s): log.write(time.strftime('%H:%M:%S ') + s + '\n'); log.flush()
def g(*a, cwd=SRC):
    r = subprocess.run(['git'] + list(a), cwd=cwd, capture_output=True)
    return r.returncode, r.stdout, r.stderr.decode(errors='replace')
say('START')
if not os.path.exists(DST):
    rc, o, e = g('worktree', 'add', '-b', 'feat/p5-style', DST, 'feat/battle-royale')
    say('worktree add rc=%d %s' % (rc, e.strip()[-400:]))
    if rc != 0: raise SystemExit
else:
    say('worktree exists, refreshing the copied work only')
rc, o, e = g('status', '--porcelain=v1', '-uall', '-z')
ents = [x for x in o.decode('utf-8', errors='replace').split('\0') if x]
copied = deleted = skipped = 0; bytes_ = 0
i = 0
while i < len(ents):
    ent = ents[i]; i += 1
    code, path = ent[:2], ent[3:]
    if code[0] == 'R': i += 1  # rename: next entry is the old path
    s = os.path.join(SRC, path.replace('/', '\\')); d = os.path.join(DST, path.replace('/', '\\'))
    if 'D' in code:
        if os.path.isfile(d): os.remove(d); deleted += 1
        continue
    if os.path.isdir(s): skipped += 1; continue
    if not os.path.isfile(s): skipped += 1; continue
    os.makedirs(os.path.dirname(d), exist_ok=True)
    shutil.copy2(s, d); copied += 1; bytes_ += os.path.getsize(s)
say('copied %d files (%.0f MB), deleted %d, skipped %d' % (copied, bytes_ / 1e6, deleted, skipped))
rc, o, e = g('status', '--porcelain=v1', '-uno', cwd=DST)
say('p5style dirty files after copy: %d' % len([l for l in o.decode(errors='replace').splitlines() if l.strip()]))
rc, o, e = g('branch', '--show-current', cwd=DST); say('branch ' + o.decode().strip())
say('DONE')

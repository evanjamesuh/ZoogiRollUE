import os, sys, re
R = 'C:/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/' if os.name == 'nt' else os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/Source/ZoogiRoll/')
DRY = len(sys.argv) > 1 and sys.argv[1] == 'dry'
EDITS = []
def ed1(p, o, n): EDITS.append((p, o, n))
def run():
    files = {}
    for p, o, n in EDITS:
        if p not in files: files[p] = open(R + p, 'rb').read()
        b = files[p]; ob = o.encode(); nb = n.encode()
        if b'\r\n' in b: ob = ob.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'); nb = nb.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
        if b.count(ob) != 1:
            o2 = ob.replace(b'\r\n', b'\n'); n2 = nb.replace(b'\r\n', b'\n')
            if b.count(o2) == 1: ob, nb = o2, n2
            else:
                rx = re.compile(re.escape(o2).replace(b'\\\n', b'\r?\n'))
                m = list(rx.finditer(b)); assert len(m) == 1, (p, len(m), o[:120])
                files[p] = b[:m[0].start()] + n2.replace(b'\n', b'\r\n') + b[m[0].end():]; continue
        files[p] = b.replace(ob, nb)
    if DRY: print('dry ok', len(EDITS)); return
    for p, b in files.items(): open(R + p, 'wb').write(b)
    print('b166e ok', len(EDITS))
# b166e (Evan Nov 6 18:34 "Use this head for void calling"): EPISODE 4 VOID CALLING uses the void-crystal Frosti head -
#  the chapter cover face (Chapters.csv hero = void_frosti) and the player token on the EP4 map (story_void_frosti bust).
ST = 'Private/ZoogiStoryMode.inl'; FR = 'Private/ZoogiFront.cpp'
ed1(ST, 'TexCover(Out, G, L + 3, ZoogiStoryData::StoryArt(TEXT("blue")), P, FVector2D(100.f, 100.f), -6.f, Wht, FVector2D(0.5f, 0.4f));',
        'TexCover(Out, G, L + 3, ZoogiStoryData::StoryArt(Ch.Element == TEXT("Void") ? TEXT("void_frosti") : TEXT("blue")), P, FVector2D(100.f, 100.f), -6.f, Wht, FVector2D(0.5f, 0.4f)); // b166: EP4 = void Frosti')
ed1(FR, '\t\t\t{ TEXT("B166"), TEXT("TRAINING: LEAGUE COURT"),', '\t\t\t{ TEXT("B166"), TEXT("VOID CALLING: NEW LOOK"), TEXT("Episode 4 now shows Frosti\'s void-crystal form on its cover and on the story map.") },\n\t\t\t{ TEXT("B166"), TEXT("TRAINING: LEAGUE COURT"),')
run()

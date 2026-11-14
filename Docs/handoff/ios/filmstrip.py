import os,sys,glob
from PIL import Image, ImageDraw, ImageFont
P=os.path.expanduser('~/mnt/dev/proofs/p5_style/'); O=os.path.expanduser('~/mnt/dev/proofs/b176/')
F=ImageFont.truetype('/usr/share/fonts/truetype/lato/Lato-Medium.ttf',20)
ids=sys.argv[1:]
for i in ids:
    clips=sorted(glob.glob(P+'pkf_%s/0*_skill_%s'%(i,i)))
    if not clips: print('no clip',i); continue
    c=clips[1] if len(clips)>1 else clips[0]
    T=[(int(a),float(b)) for a,b in (l.split() for l in open(c+'/times.txt') if l.strip())]
    t0=T[0][1]; sel=[]
    for k in range(12):
        tt=t0+k*0.3; best=min(T,key=lambda x:abs(x[1]-tt)); sel.append((k*0.3,best[0]))
    W,H=400,225; S=Image.new('RGB',(4*W,3*H+30),(14,12,30)); d=ImageDraw.Draw(S)
    d.text((8,4),i.upper()+'  (every 0.3 s from the cast)',font=F,fill=(255,214,0))
    for n,(tt,fi) in enumerate(sel):
        im=Image.open(c+'/f_%05d.jpg'%fi).convert('RGB').resize((W,H)); S.paste(im,((n%4)*W,30+(n//4)*H)); d.text(((n%4)*W+6,30+(n//4)*H+4),'%.1fs'%tt,font=F,fill=(255,255,255))
    S.save(O+'film_%s.jpg'%i,quality=80); print(O+'film_%s.jpg'%i)

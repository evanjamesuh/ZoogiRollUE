import os,sys,glob
from PIL import Image, ImageDraw, ImageFont
P=os.path.expanduser('~/mnt/dev/proofs/p5_style/'); O=os.path.expanduser('~/mnt/dev/proofs/b176/')
F=ImageFont.truetype('/usr/share/fonts/truetype/lato/Lato-Black.ttf' if os.path.exists('/usr/share/fonts/truetype/lato/Lato-Black.ttf') else '/usr/share/fonts/truetype/lato/Lato-Medium.ttf',30)
S=[('frost_spikes','FROST SPIKES  area'),('glacier_lance','GLACIER LANCE  tap'),('ice_shards','ICE SHARDS  3 spots'),('stone_pillar','STONE PILLAR  tap'),('earthquake','EARTHQUAKE  area'),('rock_rampart','ROCK RAMPART  swipe'),('crescent_slash','CRESCENT SLASH  swipe'),('cross_slash','CROSS SLASH  tap'),('ground_slash','GROUND SLASH  swipe')]
W,H=480,270
def frames(i):
    for c in ('1','2','3'):
        fs=[glob.glob(P+'pk_%s/*only_%s_%s_%s.png'%(i,i,c,t)) for t in ('060','115','170')]
        if all(fs): return [f[0] for f in fs]
    fs=sorted(glob.glob(P+'pk_%s/*only_%s_*.png'%(i,i)))[:3]
    return fs
for part,rows in (('a',S[:5]),('b',S[5:])):
    sh=Image.new('RGB',(W*3,(H+44)*len(rows)),(14,12,30)); d=ImageDraw.Draw(sh)
    for r,(i,lab) in enumerate(rows):
        y=r*(H+44); d.text((12,y+5),lab,font=F,fill=(255,214,0))
        for k,f in enumerate(frames(i)): sh.paste(Image.open(f).convert('RGB').resize((W,H)),(k*W,y+44))
    sh.save(O+'skills_%s.jpg'%part,quality=82); print(O+'skills_%s.jpg'%part)

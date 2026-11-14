import sys,glob,os
from PIL import Image, ImageDraw
d=sys.argv[1]; out=sys.argv[2]
fs=sorted(glob.glob(d+'/arena_*.png'), key=lambda f:int(os.path.basename(f).split('_')[2]))
th=[]
for f in fs:
    im=Image.open(f).convert('RGB'); im.thumbnail((180,320)); c=Image.new('RGB',(180,340),'black'); c.paste(im,(0,0))
    ImageDraw.Draw(c).text((4,322),os.path.basename(f).split('_',2)[2][:26],fill='white'); th.append(c)
cols=10; rows=(len(th)+cols-1)//cols
s=Image.new('RGB',(cols*180,rows*340))
for i,t in enumerate(th): s.paste(t,((i%cols)*180,(i//cols)*340))
s.save(out,quality=80); print(len(fs))

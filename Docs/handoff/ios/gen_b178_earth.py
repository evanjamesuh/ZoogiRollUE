# b178a (Evan: "add all earth bending effects as skills"): the 47 Earthbender effects not used yet -> skills (C++ tables, Cards.csv,
# card art from each effect's own preview). Kinds: 0 area, 1 tap, 2 three spots, 3 swipe wall (side push), 4 swipe fault (forward).
import os,sys
from PIL import Image, ImageFilter, ImageDraw, ImageEnhance
R=os.path.expanduser('~/mnt/dev/ZoogiRollUnreal-p5style/')
TH=os.path.expanduser('~/mnt/dev/proofs/fxthumbs/eb/')
FAM={'slate':((0.55,0.6,0.8),'Neutral'),'sand':((0.95,0.78,0.5),'Neutral'),'jade':((0.4,0.9,0.75),'Frost'),'snow':((0.85,0.95,1.0),'Frost'),
     'granite':((0.65,0.7,0.55),'Neutral'),'obsidian':((0.55,0.4,0.75),'Void'),'sapphire':((0.25,0.45,1.0),'Shock'),'gold':((1.0,0.8,0.25),'Fire'),
     'rune':((0.3,0.75,1.0),'Shock'),'flash':((1.0,1.0,1.0),'Shock')}
#   V, id, name, kind, cost, family, line of rule
S=[(2,'boulder_block','BOULDER BLOCK',1,2,'slate','a stone block bursts up under it and hurls it away from you'),
 (3,'sandstone_spire','SANDSTONE SPIRE',1,2,'sand','a sandstone spire shoots up under it and launches it'),
 (4,'stone_barricade','STONE BARRICADE',3,2,'slate','a stone barricade erupts along the line and throws everything beside it aside'),
 (5,'desert_rubble','DESERT RUBBLE',2,2,'sand','sandstone rubble bursts up at each spot'),
 (6,'jade_boulder','JADE BOULDER',1,2,'jade','a jade boulder blasts out under it'),
 (7,'rock_burst','ROCK BURST',0,1,'slate','rocks burst out of the floor and scatter everything there'),
 (8,'boulder_fountain','BOULDER FOUNTAIN',0,2,'slate','a fountain of boulders erupts and throws everything up and out'),
 (9,'slab_row','SLAB ROW',3,2,'slate','a row of stone slabs slams up along the line'),
 (10,'jade_wall','JADE WALL',3,2,'jade','a jade wall bursts up along the line'),
 (11,'snowcap_pillar','SNOWCAP PILLAR',1,2,'snow','a snow-capped pillar spears up under it'),
 (12,'granite_block','GRANITE BLOCK',1,2,'granite','a granite block heaves up under it'),
 (13,'moss_tower','MOSS TOWER',1,2,'granite','a mossy stone tower rises under it and tips it away'),
 (14,'frostrock_grove','FROSTROCK GROVE',0,3,'snow','a grove of frosted pillars bursts up and shoves everything out'),
 (15,'obsidian_spike','OBSIDIAN SPIKE',1,2,'obsidian','a black obsidian spike stabs up under it'),
 (16,'obsidian_wall','OBSIDIAN WALL',3,2,'obsidian','an obsidian wall erupts along the line'),
 (17,'obsidian_cluster','OBSIDIAN CLUSTER',0,2,'obsidian','obsidian shards cluster up and blast everything out'),
 (18,'obsidian_burst','OBSIDIAN BURST',0,3,'obsidian','an obsidian slab bed explodes and throws everything away'),
 (19,'sapphire_pillar','SAPPHIRE PILLAR',1,2,'sapphire','a sapphire pillar launches it'),
 (20,'sapphire_burst','SAPPHIRE BURST',0,2,'sapphire','sapphire stones burst out and scatter everything'),
 (21,'sapphire_block','SAPPHIRE BLOCK',1,2,'sapphire','a sapphire block slams up under it'),
 (22,'gold_rush','GOLD RUSH',0,3,'gold','golden rocks erupt and blast everything out'),
 (23,'gold_nugget','GOLD NUGGET',1,1,'gold','a golden crystal pops up under it'),
 (24,'gold_wall','GOLD WALL',3,2,'gold','a golden wall rises along the line'),
 (25,'rune_pillar','RUNE PILLAR',1,2,'rune','a glowing rune pillar fires it away'),
 (26,'rune_wall','RUNE WALL',3,2,'rune','a glowing rune wall bursts up along the line'),
 (27,'rune_monolith','RUNE MONOLITH',1,3,'rune','a rune monolith slams up under it'),
 (28,'stone_teeth','STONE TEETH',2,2,'slate','jagged stone teeth bite up at each spot'),
 (29,'toppled_columns','TOPPLED COLUMNS',2,2,'slate','columns burst up and topple at each spot'),
 (31,'landslide','LANDSLIDE',4,3,'slate','great slabs tip along the line and shove everything forward'),
 (32,'starburst_spikes','STARBURST SPIKES',0,2,'slate','spikes shoot out in a star and throw everything away'),
 (33,'rock_geyser','ROCK GEYSER',1,1,'slate','a geyser of rocks blows it upward'),
 (34,'crater_ring','CRATER RING',0,2,'slate','the floor cracks into a crater and everything is thrown out'),
 (35,'gold_column','GOLD COLUMN',1,2,'gold','a golden column rises under it'),
 (36,'jade_columns','JADE COLUMNS',2,2,'jade','jade columns spear up at each spot'),
 (37,'sapphire_wall','SAPPHIRE WALL',3,2,'sapphire','a sapphire wall rises along the line'),
 (38,'sandstone_ruins','SANDSTONE RUINS',3,2,'sand','sandstone ruins burst up along the line'),
 (39,'obsidian_star','OBSIDIAN STAR',0,2,'obsidian','an obsidian star bursts and throws everything out'),
 (40,'earth_flash','EARTH FLASH',0,2,'flash','a blinding stone blast throws everything away'),
 (42,'rubble_storm','RUBBLE STORM',0,3,'slate','a storm of rubble explodes over the area'),
 (43,'stone_cage','STONE CAGE',0,2,'slate','a stone cage slams up and bursts everything outward'),
 (44,'pillar_field','PILLAR FIELD',2,3,'slate','pillars rise at each spot'),
 (45,'fault_line','FAULT LINE',4,2,'slate','the ground splits along the line and shoves everything forward'),
 (46,'shatter_star','SHATTER STAR',0,2,'slate','slabs shatter outward in a star'),
 (47,'slab_slam','SLAB SLAM',1,2,'slate','a giant slab slams up under it'),
 (48,'rift_crack','RIFT CRACK',4,2,'slate','a branching rift tears along the line and launches everything forward'),
 (49,'long_rampart','LONG RAMPART',3,2,'slate','a long low rampart rises along the line'),
 (50,'boulder_pile','BOULDER PILE',1,2,'slate','a pile of boulders bursts up under it')]
KP={0:(1.3,380.,1000.,520.,0.3),1:(1.2,190.,1300.,800.,0.25),2:(1.0,220.,760.,450.,0.25),3:(0.9,170.,900.,360.,0.25),4:(0.9,170.,1000.,450.,0.15)}
AIM={0:('Area','DRAG THE CIRCLE - THE EARTH ERUPTS THERE'),1:('Tap','TAP A RIVAL OR AN ORB'),2:('Spots','TAP 3 SPOTS'),3:('Line','SWIPE WHERE THE WALL RISES'),4:('Line','SWIPE THE FAULT LINE')}
RADEF={0:None,1:'0.f',2:'0.f',3:'0.f',4:'0.f'}
defs=[];pks=[];csv=[];lines=[]
for v,i,n,k,c,fam,rule in S:
    sc,r,sp,up,dl=KP[k]; col,el=FAM[fam]
    aim,hint=AIM[k]
    defs.append('\t\t\t{ TEXT("%s"), EAim::%s, %s, %d, TEXT("%s") }'%(i,aim,('%.0f.f'%r) if k==0 else '0.f',3 if k==2 else 1,hint))
    pks.append('\t\t\t{ TEXT("%s"), TEXT("/Game/EarthbenderV1/Particles/NiagaraSystems/NS_Earthbender_V%02d.NS_Earthbender_V%02d"), %.2ff, %d, %.0f.f, %.0f.f, %.0f.f, %.2ff, FLinearColor(%.2ff, %.2ff, %.2ff) }'%(i,v,v,sc,k,r,sp,up,dl,col[0],col[1],col[2]))
    typ='Terrain' if k in (2,3) else 'Impact'
    how={0:'Drag the circle inside your arc range: ',1:'Tap a rival or an orb inside your arc range: ',2:'Tap 3 spots inside your arc range: ',3:'Swipe a line inside your arc range: ',4:'Swipe a line inside your arc range: '}[k]
    pa={0:'%d'%r,1:'0',2:'3',3:'0',4:'0'}[k]
    csv.append('%s,%s,%d,%s,ActingZoogi,LeadActionEnd,%s,%d,0,true,%s,%s%s.'%(i,n,c,typ,pa,sp,el,how,rule))
    if k>=3: lines.append(i)
    # card art from the effect's preview
    src=TH+'NS_Earthbender_V%02d.png'%v
    if os.path.exists(src):
        im=Image.open(src).convert('RGB'); w,h=im.size
        cw=int(h*245/386); x0=(w-cw)//2; card=im.crop((x0,0,x0+cw,h)).resize((245,386),Image.LANCZOS)
        ic=im.crop((0,int(h*0.08),w,int(h*0.08)+int(w*250/300))).resize((300,250),Image.LANCZOS)
        def vig(a,tint):
            W,H=a.size; m=Image.new('L',(W,H),0); d=ImageDraw.Draw(m); d.ellipse((-W*0.25,-H*0.15,W*1.25,H*1.15),fill=255); m=m.filter(ImageFilter.GaussianBlur(W*0.12))
            bg=Image.new('RGB',(W,H),tuple(int(t*90) for t in tint)); return Image.composite(ImageEnhance.Contrast(a).enhance(1.15),bg,m)
        vig(card,col).save(R+'Content/Data/CardArt/%s.png'%i); vig(ic,col).save(R+'Content/Data/CardArt/Icons/%s.png'%i)
open(os.path.expanduser('~/mnt/dev/ios/b178_defs.txt'),'w').write(',\n'.join(defs))
open(os.path.expanduser('~/mnt/dev/ios/b178_pks.txt'),'w').write(',\n'.join(pks))
open(os.path.expanduser('~/mnt/dev/ios/b178_csv.txt'),'w').write('\n'.join(csv))
open(os.path.expanduser('~/mnt/dev/ios/b178_lines.txt'),'w').write(' '.join(lines))
open(os.path.expanduser('~/mnt/dev/ios/b178_ids.txt'),'w').write(' '.join(x[1] for x in S))
print(len(S),'skills,',len(lines),'lines')

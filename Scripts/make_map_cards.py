# Oct 29 (Claude): classic MAP SELECTION art (same look as Scripts/make_mode_select.py cards). usage: make_maps.py <shots_dir> <out_dir>
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
import make_mode_select as ms
from PIL import Image
shots, out = sys.argv[1], sys.argv[2]
ms.OUT = out; ms.FONT = os.path.join(os.path.dirname(__file__), '..', 'Content', 'Data', 'Fonts', 'LilitaOne-Regular.ttf')
os.makedirs(out, exist_ok=True)
# MAP SELECTION title
W, H = 520, 80
img = Image.new('RGBA', (ms.S(W), ms.S(H)), (0, 0, 0, 0))
ms.text_outlined(img, (6, 40), 'MAP SELECTION', 50, (255, 255, 255, 255), outline=ms.hexc('151a1f'), ow=3, shadow=4, anchor='lm', shadow_col=ms.hexc('151a1f'))
img.save(out + '/ms_title_map.png')
# key, title, frame colour, picture file, crop centre x / y, zoom
import json
spec = json.load(open(os.path.join(shots, 'pick.json')))
for key, title, col, pic, cx, cy, zoom in spec:
    p = os.path.join(shots, pic)
    if not os.path.exists(p): print('missing', p); continue
    ms.make_card('tmp', title, col, (Image.open(p).convert('RGB'), cx, cy, zoom), 4, False)
    os.replace(out + '/ms_card_tmp.png', out + f'/ms_map_{key}.png')
    print('card', key)

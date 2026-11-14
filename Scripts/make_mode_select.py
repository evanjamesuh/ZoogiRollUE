# Mode Selection art (Marble Clash style, landscape). Renders PNGs for Content/Data/MenuArt/ModeSelect.
# Design units: 1080 tall canvas. SS = pixels per design unit in the PNGs.
import os, math
from PIL import Image, ImageDraw, ImageFont, ImageFilter

SS = 1.5
OUT = '/home/claude/ms/out'
os.makedirs(OUT, exist_ok=True)
FONT = '/home/claude/ms/fonts/LilitaOne-Regular.ttf'
UP = '/mnt/user-data/uploads/dev'
ART = UP + '/ZoogiRollUnreal-combined/Content/Data/MenuArt'

def hexc(h, a=255):
    h = h.lstrip('#'); return (int(h[0:2], 16), int(h[2:4], 16), int(h[4:6], 16), a)

def font(px):
    return ImageFont.truetype(FONT, int(px))

def S(v):
    return int(round(v * SS))

def text_outlined(img, xy, txt, size, fill, outline=(0, 0, 0, 255), ow=4, shadow=0, anchor='mm', shadow_col=(0, 0, 0, 255)):
    d = ImageDraw.Draw(img)
    f = font(S(size))
    x, y = S(xy[0]), S(xy[1])
    if shadow:
        d.text((x, y + S(shadow)), txt, font=f, fill=shadow_col, anchor=anchor, stroke_width=S(ow), stroke_fill=shadow_col)
    d.text((x, y), txt, font=f, fill=fill, anchor=anchor, stroke_width=S(ow), stroke_fill=outline)

def crop_fit(im, w, h, cx=0.5, cy=0.5, zoom=1.0):
    """crop to aspect w/h around (cx, cy) (fractions), zoom >1 crops tighter"""
    W, H = im.size
    a = w / h
    cw = min(W, H * a) / zoom
    ch = cw / a
    x0 = max(0, min(W - cw, cx * W - cw / 2))
    y0 = max(0, min(H - ch, cy * H - ch / 2))
    return im.crop((int(x0), int(y0), int(x0 + cw), int(y0 + ch))).resize((w, h), Image.LANCZOS)

# ------------------------------------------------------------------ background (2400 x 1080, ScaleToFill)
def make_bg():
    W, H = 2400, 1080
    img = Image.new('RGBA', (S(W), S(H)), (0, 0, 0, 255))
    px = img.load()
    d = ImageDraw.Draw(img)
    # vertical gradient bands sampled from the reference (ref 1179 px tall -> 1080)
    def band(y0, y1, c0, c1):
        for y in range(S(y0), S(y1)):
            t = (y - S(y0)) / max(1, S(y1) - S(y0))
            c = tuple(int(c0[i] + (c1[i] - c0[i]) * t) for i in range(3)) + (255,)
            d.line([(0, y), (S(W), y)], fill=c)
    band(0, 330, hexc('0065b8'), hexc('028ed5'))
    band(330, 505, hexc('2b76d4'), hexc('2b76d4'))
    band(505, 1080, hexc('03a3e6'), hexc('05c0ff'))
    # faint tilted watermark logos
    wm = Image.new('RGBA', img.size, (0, 0, 0, 0))
    wd = ImageDraw.Draw(wm)
    f1, f2 = font(S(60)), font(S(44))
    for row in range(-1, 8):
        for col in range(-1, 9):
            x = S(col * 330 + (row % 2) * 165)
            y = S(row * 170)
            wd.text((x, y), 'ZOOGI', font=f1, fill=(255, 255, 255, 11))
            wd.text((x + S(30), y + S(58)), 'ROLL', font=f2, fill=(255, 255, 255, 11))
    wm = wm.rotate(14, resample=Image.BICUBIC, center=(img.size[0] / 2, img.size[1] / 2))
    img = Image.alpha_composite(img, wm)
    # dark top strip
    ImageDraw.Draw(img).rectangle([0, 0, S(W), S(41)], fill=hexc('1f262b'))
    img.convert('RGB').save(OUT + '/ms_bg.jpg', quality=92)

# ------------------------------------------------------------------ top bar pieces
def make_back():
    W, H = 262, 82
    img = Image.new('RGBA', (S(W), S(H)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon([(0, 0), (S(W), 0), (S(W - 8), S(H)), (0, S(H))], fill=hexc('2b2e35'))
    # chevron
    cx, cy = 72, 40
    d.line([(S(cx + 10), S(cy - 17)), (S(cx - 6), S(cy)), (S(cx + 10), S(cy + 17))], fill=hexc('d9dce6'), width=S(10), joint='curve')
    text_outlined(img, (150, 41), 'BACK', 38, (255, 255, 255, 255), outline=hexc('2b2e35'), ow=0)
    img.save(OUT + '/ms_back.png')

def make_title():
    W, H = 520, 80
    img = Image.new('RGBA', (S(W), S(H)), (0, 0, 0, 0))
    text_outlined(img, (6, 40), 'MODE SELECTION', 50, (255, 255, 255, 255), outline=hexc('151a1f'), ow=3, shadow=4, anchor='lm', shadow_col=hexc('151a1f'))
    img.save(OUT + '/ms_title.png')

def make_pill(kind):
    W, H = 332, 76
    img = Image.new('RGBA', (S(W), S(H)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.polygon([(S(10), 0), (S(W), 0), (S(W - 10), S(H)), (0, S(H))], fill=hexc('1f262b'))
    if kind == 'gem':
        cx, cy, r = 46, 36, 30
        pts = [(cx, cy - r), (cx + r * 0.78, cy - r * 0.15), (cx, cy + r), (cx - r * 0.78, cy - r * 0.15)]
        d.polygon([(S(x), S(y)) for x, y in pts], fill=hexc('e23bd6'))
        d.polygon([(S(cx), S(cy - r)), (S(cx + r * 0.78), S(cy - r * 0.15)), (S(cx), S(cy + 2))], fill=hexc('f78cf0'))
        d.polygon([(S(cx - r * 0.78), S(cy - r * 0.15)), (S(cx), S(cy + r)), (S(cx), S(cy + 2))], fill=hexc('b51cb0'))
        d.polygon([(S(cx - 12), S(cy - 6)), (S(cx), S(cy - r + 8)), (S(cx + 4), S(cy - 6))], fill=(255, 255, 255, 170))
    else:
        cx, cy, r = 46, 38, 30
        d.ellipse([S(cx - r + 3), S(cy - r + 3), S(cx + r + 3), S(cy + r + 3)], fill=hexc('c77700'))
        d.ellipse([S(cx - r), S(cy - r), S(cx + r), S(cy + r)], fill=hexc('ffb300'))
        d.ellipse([S(cx - r + 7), S(cy - r + 7), S(cx + r - 7), S(cy + r - 7)], fill=hexc('ffd34d'))
        d.ellipse([S(cx - r + 13), S(cy - r + 9), S(cx - 2), S(cy + 4)], fill=(255, 245, 190, 200))
    # green plus
    px, py, a, t = 290, 38, 21, 9
    d.rectangle([S(px - a), S(py - t / 2), S(px + a), S(py + t / 2)], fill=hexc('6fcf3a'))
    d.rectangle([S(px - t / 2), S(py - a), S(px + t / 2), S(py + a)], fill=hexc('6fcf3a'))
    d.rectangle([S(px - a + 2), S(py - t / 2), S(px + a - 2), S(py - 1)], fill=hexc('a6ea6c'))
    img.save(OUT + f'/ms_pill_{kind}.png')

# ------------------------------------------------------------------ cards
K = 47.0 / 647.0       # horizontal shear of the reference cards
CW, CH = 552, 593      # card (unsheared) in design units
TABH = 42
PW, PH = int(CW + K * CH + 8), TABH + CH + 6

def card_poly(x0, y0, w, h):
    """sheared rectangle: (x0, y0) is the bottom-left corner's x and the top y"""
    return [(x0 + K * h, y0), (x0 + K * h + w, y0), (x0 + w, y0 + h), (x0, y0 + h)]

def xs(pts):
    return [(S(x), S(y)) for x, y in pts]

def make_card(key, title, col, art, players, featured=False):
    img = Image.new('RGBA', (S(PW), S(PH)), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    y0 = TABH
    ox = 3
    frame = hexc(col)
    dark = tuple(int(c * 0.72) for c in frame[:3]) + (255,)
    # player tab (above the top-right corner)
    tw = 112
    tx1 = ox + K * CH + CW - 6
    tab = [(tx1 - tw, 2), (tx1, 2), (tx1, y0 + 6), (tx1 - tw, y0 + 6)]
    d.polygon(xs(tab), fill=hexc('2a2470'), outline=(0, 0, 0, 255))
    d.line(xs(tab + [tab[0]]), fill=(0, 0, 0, 255), width=S(4))
    # person icon + count
    ix, iy = tx1 - tw + 30, 21
    d.ellipse([S(ix - 7), S(iy - 12), S(ix + 7), S(iy + 2)], fill=hexc('c9ccd6'))
    d.pieslice([S(ix - 14), S(iy + 3), S(ix + 14), S(iy + 27)], 180, 360, fill=hexc('c9ccd6'))
    text_outlined(img, (tx1 - tw + 74, 22), str(players), 26, (255, 255, 255, 255), ow=0)
    # outer card + outline
    outer = card_poly(ox, y0, CW, CH)
    d.polygon(xs(outer), fill=frame)
    # inner image window (sheared, inset)
    top_band, cap_band, side = 80, 92, 20
    ih = CH - top_band - cap_band
    ix0 = ox + side + K * cap_band  # bottom-left x of the window (shear measured from the card bottom)
    win = card_poly(ix0, y0 + top_band, CW - 2 * side, ih)
    ww = int(CW - 2 * side + K * ih) + 2
    pic = crop_fit(art[0], S(ww), S(ih), *art[1:])
    mask = Image.new('L', img.size, 0)
    ImageDraw.Draw(mask).polygon(xs(win), fill=255)
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    layer.paste(pic, (S(ix0), S(y0 + top_band)))
    img.paste(layer, (0, 0), mask)
    d = ImageDraw.Draw(img)
    d.line(xs(win + [win[0]]), fill=dark, width=S(5))
    d.line(xs(outer + [outer[0]]), fill=(0, 0, 0, 255), width=S(5), joint='curve')
    # caption
    capx = ox + CW / 2 + K * cap_band / 2
    text_outlined(img, (capx, y0 + CH - cap_band / 2 - 2), title, 50, hexc('fff1a8'), outline=(0, 0, 0, 255), ow=4, shadow=3)
    if featured:
        # trophy bar + crown hex badge
        by0, by1 = y0 + 18, y0 + 61
        bar = [(ox + 128 + K * (CH - 18), by0), (ox + CW - 34 + K * (CH - 18), by0), (ox + CW - 34 + K * (CH - 61), by1), (ox + 128 + K * (CH - 61), by1)]
        d.polygon(xs(bar), fill=hexc('291f8a'))
        bx = (bar[0][0] + bar[1][0]) / 2
        # trophy
        tx, ty = bx - 24, (by0 + by1) / 2
        d.pieslice([S(tx - 11), S(ty - 15), S(tx + 11), S(ty + 7)], 0, 180, fill=hexc('ffc531'))
        d.rectangle([S(tx - 11), S(ty - 15), S(tx + 11), S(ty - 4)], fill=hexc('ffc531'))
        d.rectangle([S(tx - 3), S(ty + 4), S(tx + 3), S(ty + 11)], fill=hexc('ffc531'))
        d.rectangle([S(tx - 9), S(ty + 11), S(tx + 9), S(ty + 15)], fill=hexc('ffc531'))
        text_outlined(img, (bx + 14, ty + 1), '0', 30, (255, 255, 255, 255), ow=0)
        # hex badge
        hx, hy, hr = ox + 72 + K * (CH - 40), y0 + 38, 48
        hexp = [(hx + hr * math.cos(math.radians(a)), hy + hr * math.sin(math.radians(a))) for a in range(-90, 270, 60)]
        # "1" tab under the badge
        d.rectangle([S(hx - 28), S(hy + 30), S(hx + 28), S(hy + 82)], fill=hexc('2b2e35'), outline=(0, 0, 0, 255), width=S(3))
        text_outlined(img, (hx, hy + 62), '1', 30, (255, 255, 255, 255), ow=0)
        d.polygon(xs(hexp), fill=(0, 0, 0, 255))
        hexi = [(hx + (hr - 6) * math.cos(math.radians(a)), hy + (hr - 6) * math.sin(math.radians(a))) for a in range(-90, 270, 60)]
        d.polygon(xs(hexi), fill=hexc('f2a07a'))
        hexj = [(hx + (hr - 13) * math.cos(math.radians(a)), hy + (hr - 13) * math.sin(math.radians(a))) for a in range(-90, 270, 60)]
        d.polygon(xs(hexj), fill=hexc('e2541c'))
        crown = [(hx - 20, hy + 12), (hx - 22, hy - 10), (hx - 9, hy + 0), (hx, hy - 16), (hx + 9, hy + 0), (hx + 22, hy - 10), (hx + 20, hy + 12)]
        d.polygon(xs(crown), fill=(255, 255, 255, 255))
    img.save(OUT + f'/ms_card_{key}.png')
    return img

def story_art():
    bg = Image.open(ART + '/frozen_background.jpg').convert('RGB')
    bg = crop_fit(bg, 1100, 820, 0.5, 0.55)
    luma = Image.open(ART + '/luma.jpg').convert('RGBA')
    # white background -> transparent
    px = luma.load()
    for y in range(luma.size[1]):
        for x in range(luma.size[0]):
            r, g, b, a = px[x, y]
            if r > 240 and g > 240 and b > 240: px[x, y] = (r, g, b, 0)
    luma = luma.resize((620, int(620 * luma.size[1] / luma.size[0])), Image.LANCZOS)
    bg = bg.convert('RGBA')
    bg.alpha_composite(luma, (240, 200))
    return bg.convert('RGB')

if __name__ == '__main__':
    make_bg(); make_back(); make_title(); make_pill('gem'); make_pill('coin')
    P = UP + '/proofs'
    cards = [
        ('royale', 'BATTLE ROYALE', '6206d3', (Image.open(P + '/royale_robot_human/frame_00085.png').convert('RGB'), 0.47, 0.40, 1.75), 4, True),
        ('battle', '2V2 BATTLE', '00c2d3', (Image.open(P + '/blue_ball_classic/17_s2_5_rolling.png').convert('RGB'), 0.5, 0.6, 1.6), 4, False),
        ('classic', 'CLASSIC', 'b60e65', (Image.open(P + '/merge42/frozen1v1/eng/18_s2_5_rolling.png').convert('RGB'), 0.5, 0.52, 1.25), 4, False),
        ('training', 'TRAINING', '302986', (Image.open(P + '/merge42/training/eng/00_s1_1_draw.png').convert('RGB'), 0.5, 0.33, 1.15), 2, False),
        ('story', 'STORY', 'f07c12', (story_art(), 0.5, 0.5, 1.0), 1, False),
    ]
    for c in cards: make_card(*c)
    print('cards', PW, PH)

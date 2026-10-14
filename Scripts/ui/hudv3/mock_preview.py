# BOX MOCK of the hud-v3 overlay as ZoogiFramedHUDWidget/ZoogiTeamColumnWidget draw it (positions copied from the C++).
import sys
from PIL import Image, ImageDraw, ImageFont
F = '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
ARENA_SRC = '/workspace/ui-polish/arena-viewport/shots/v2/v2_16x9_awaken_card_tray.png'
def font(sz): return ImageFont.truetype(F, max(9, round(sz)))
def tex(n, d='tex'): return Image.open(f'{d}/{n}.png').convert('RGBA')
def mock(W, H, out):
    T = H / 768.0
    if W / T < 1180: T = W / 1180
    X0, X1, Y0, Y1 = 0, W, 0, H
    ColT = 220.0  # fixed in ZoogiFramedLayout for the overlay
    TT = Y1 - 202 * T
    LeftX = X0 + 8 * T; RightX = X1 - (8 + ColT) * T
    A0 = (round(LeftX + ColT * T + 14 * T), round(Y0 + 68 * T)); A1 = (round(RightX - 14 * T), round(TT - 6 * T))
    im = Image.new('RGBA', (W, H), (9, 9, 10, 255))
    # arena: crop the real framed screenshot's arena centre to the hole aspect
    a = Image.open(ARENA_SRC).convert('RGBA'); aw, ah = A1[0] - A0[0], A1[1] - A0[1]
    sx0, sy0, sx1, sy1 = 330, 95, 1590, 785; cw, ch = sx1 - sx0, sy1 - sy0
    if cw / ch > aw / ah: nw = ch * aw / ah; sx0 += (cw - nw) / 2; sx1 = sx0 + nw
    else: nh = cw * ah / aw; sy0 += (ch - nh) / 2; sy1 = sy0 + nh
    im.paste(a.crop((int(sx0), int(sy0), int(sx1), int(sy1))).resize((aw, ah), Image.LANCZOS), A0)
    def put(n, x, y, w=None, h=None):
        t = tex(n); w = w or t.width / 2 * T; h = h or t.height / 2 * T
        im.alpha_composite(t.resize((max(1, int(w)), max(1, int(h))), Image.LANCZOS), (int(x), int(y)))
    Xc = (A0[0] + A1[0]) / 2
    # pieces under the holes (pills, plate)
    put('name_plate', Xc - 90 * T, Y0 + 26 * T); put('pills_bar', Xc - 270 * T, Y0)
    pp = (Xc - 270 * T + (14 + 105) * T, Y0 + 6 * T)
    put('pill_done', Xc - 270 * T + 14 * T, Y0 + 6 * T, 95 * T, 24 * T)
    g = Image.new('RGBA', (W, H)); gd = ImageDraw.Draw(g); oc = (pp[0] + 47.5 * T, pp[1] + 12 * T)
    gd.rounded_rectangle([oc[0] - 70 * T, oc[1] - 22 * T, oc[0] + 70 * T, oc[1] + 22 * T], int(22 * T), fill=(90, 220, 255, 90))
    from PIL import ImageFilter; im.alpha_composite(g.filter(ImageFilter.GaussianBlur(6 * T)))
    put('pill_on', pp[0] + 47.5 * T - 78 * T, pp[1] + 12 * T - 25 * T + T); put('pill_on', pp[0] + 47.5 * T - 78 * T, pp[1] + 12 * T - 25 * T + T)
    # the overlay itself, stretched to the frame exactly as PaintBack does
    ov = Image.open('tex3/overlay.png').convert('RGBA'); OH = Y1 - Y0; LW, RW = int(380 * T), int(410 * T)
    # split at the arena edges, exactly as PaintBack: fixed left / right strips, stretched middle
    im.alpha_composite(ov.crop((0, 0, 380, 768)).resize((LW, OH), Image.LANCZOS), (X0, Y0))
    im.alpha_composite(ov.crop((380, 0, 966, 768)).resize((X1 - X0 - LW - RW, OH), Image.LANCZOS), (X0 + LW, Y0))
    im.alpha_composite(ov.crop((966, 0, 1376, 768)).resize((RW, OH), Image.LANCZOS), (X1 - RW, Y0))
    d = ImageDraw.Draw(im)
    def txt(x, y, s, sz, c, ax=0.0, ay=0.5):
        f = font(sz * T); b = d.textbbox((0, 0), s, font=f)
        d.text((x - (b[2] - b[0]) * ax, y - (b[3] + b[1]) * ay), s, font=f, fill=c)
    W_ = (255, 255, 255)
    for i, lb in enumerate(['DRAW >', 'AWAKEN', 'BATTLE', 'RESOLVE', 'END']):
        txt(Xc - 270 * T + (14 + 105 * i + 47.5) * T, Y0 + 18 * T, lb, 10.5 if i == 1 else 9.5, W_ if i == 1 else (247, 245, 237), 0.5)
    txt(X0 + 182.5 * T, Y0 + 34.5 * T, '1 / 8', 11, W_, 0.5); txt(X0 + 226 * T, Y0 + 34.5 * T, '24', 11, W_)
    txt(Xc - 90 * T + 98.5 * T, Y0 + 39 * T, 'P1 BLUE (CPU)', 8.5, W_, 0.5)
    txt(Xc - 90 * T + 94 * T, Y0 + 58 * T, 'FROZEN RING', 13.5, (240, 200, 110), 0.5)
    # columns
    ports = {'P1 BLUE': 'swirlie', 'P2 TAVI': 'hotstreak', 'P3 MOSSBACK': 'mossback', 'P4 GOURDON': 'gourdon'}
    cols = [(LeftX, [('P1 BLUE', 'blue', True), ('P3 ORIN', 'orin', False)], (140, 215, 255)),
            (RightX, [('P2 TAVI', 'tavi', False), ('P4 HOTSTREAK', 'hotstreak', False)], (255, 115, 100))]
    for cx, mine, sc in cols:
        for gi, (nm, pt, act) in enumerate(mine):
            Y = Y0 + 62 * T + gi * 255 * T
            CP = (cx + 10 * T, Y + 38 * T); CSx = (ColT - 20) * T
            p = Image.open(f'por/T_Portrait_{pt}.png').convert('RGBA').resize((int(54 * T), int(54 * T)), Image.LANCZOS)
            im.alpha_composite(p, (int(CP[0] + 36 * T - 27 * T), int(CP[1] + 35 * T - 27 * T)))
            TX = CP[0] + 72 * T
            txt(TX, CP[1] + 17 * T, nm, 13, (255, 242, 190) if act else (242, 237, 224))
            cxx = TX
            for art, lb, w in [('chip_purple', 'CPU', 34)] + ([('chip_gold', 'TURN', 41)] if act else []):
                put(art, cxx, CP[1] + 28 * T, w * T, 16 * T); txt(cxx + w * T / 2, CP[1] + 36 * T, lb, 8, W_, 0.5); cxx += (w + 5) * T
            txt(CP[0] + CSx - 12 * T, CP[1] + 19 * T, '0', 27, sc, 1.0)
            txt(TX, CP[1] + 54 * T, 'ORBS 0  KOS 0  HAND 5', 8.5, (209, 224, 242))
            txt(CP[0] + 9 * T, CP[1] + 98 * T, 'EPISODE 0/100', 8, (90, 220, 255))
            d.rounded_rectangle([CP[0] + 6 * T, CP[1] + 108 * T, CP[0] + 6 * T + (CSx - 12 * T) * 0.08, CP[1] + 118 * T], 3, fill=(60, 170, 255))
            BY = CP[1] + 138 * T; BH = Y + 240 * T - BY
            if not act:
                e = tex('emblem'); EW = (ColT - 20) * T; EH = min(BH, EW * e.height / e.width); put('emblem', cx + 10 * T, BY + (BH - EH) / 2, EW, EH)
            if act:
                AP = (cx + 10 * T, CP[1] + 138 * T)
                put('action_box', AP[0], AP[1], (ColT - 20) * T, min(BH, 64 * T))
                put('chip_blue', AP[0] + 10 * T, AP[1] + 6 * T, 66 * T, 15 * T); txt(AP[0] + 43 * T, AP[1] + 13.5 * T, 'STANDBY', 8, W_, 0.5)
                txt(AP[0] + 82 * T, AP[1] + 13.5 * T, 'CPU', 7.5, (191, 199, 209)); txt(AP[0] + 10 * T, AP[1] + 31 * T, 'CPU is awakening...', 9, W_)
    # tray values
    TL = (X0, TT + 12 * T); TR = (X1 - 398 * T, TT + 12 * T)
    sw = tex('orb_swirl').resize((int(128 * T), int(128 * T)), Image.LANCZOS).rotate(37); a_ = sw.getchannel('A').point(lambda v: int(v * 0.75)); sw.putalpha(a_)
    TRt = (X1 - 398 * T, TT + 12 * T); im.alpha_composite(sw, (int(TRt[0] + 252 * T - 64 * T), int(TRt[1] + 50 * T - 64 * T)))
    g = Image.new('RGBA', (W, H)); gd = ImageDraw.Draw(g)
    gd.rounded_rectangle([TRt[0] + 28 * T, TRt[1] + 20 * T, TRt[0] + 118 * T, TRt[1] + 130 * T], int(30 * T), fill=(100, 240, 255, 30)); im.alpha_composite(g)
    d = ImageDraw.Draw(im)
    txt(TL[0] + 71.5 * T, TL[1] + 139 * T, '10', 19, W_, 0.5)
    txt(TR[0] + 155 * T, TR[1] + 184 * T, 'DISCARD 0', 8, W_, 0.5)
    txt(TR[0] + 328 * T, TR[1] + 108 * T, '5/5', 22, W_, 0.5)
    # hand fan in L.Hand (scale to recess height, 3 deg tilt, light arc)
    HX0, HX1 = X0 + 382 * T, X1 - 428 * T; HT, HB = TT + 22 * T, Y1
    names = ['precision', 'heavy_roll', 'guard', 'orb_relay', 'anchor_guard']
    c0 = Image.open('cardart/guard.png'); ch = HB - HT - 14 * T; cw = ch * c0.width / c0.height
    sp = min(cw * 1.02, (HX1 - HX0 - 20 * T - cw) / 4); fcx = (HX0 + HX1) / 2
    for i, n in enumerate(names):
        k = i - 2; c = Image.open(f'cardart/{n}.png').convert('RGBA').resize((int(cw), int(ch)), Image.LANCZOS).rotate(-k * 3, expand=True, resample=Image.BICUBIC)
        x = fcx + k * sp - c.width / 2; y = HB + 4 * T - ch + k * k * 1.5 * T - (c.height - ch) / 2
        im.alpha_composite(c, (int(x), int(y)))
    d = ImageDraw.Draw(im)
    d.text((10, H - 22), 'BOX MOCK - not an engine shot', font=font(14), fill=(255, 80, 80))
    im.convert('RGB').save(out); return im
import os; os.makedirs('shots/v2', exist_ok=True)
mock(1920, 1080, 'shots/v2/preview_1920x1080.png'); mock(2400, 1080, 'shots/v2/preview_2400x1080.png')
tg = Image.open('/workspace/ui-polish/stone-ref/hud_v3_target.png').convert('RGB').resize((1920, 1072), Image.LANCZOS)
pv = Image.open('shots/v2/preview_1920x1080.png'); c = Image.new('RGB', (3840, 1080), (0, 0, 0)); c.paste(tg, (0, 4)); c.paste(pv, (1920, 0)); c.save('shots/v2/compare_preview.png'); print('ok')

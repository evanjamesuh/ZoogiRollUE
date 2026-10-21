"""Rewrite a GLB so it has no EXT_texture_webp: webp images become PNG, textures point at them."""
import io, json, struct, sys
from PIL import Image

def read_glb(path):
    d = open(path, 'rb').read()
    magic, ver, length = struct.unpack('<III', d[:12])
    assert magic == 0x46546C67
    off = 12; js = None; bin_ = b''
    while off < length:
        clen, ctype = struct.unpack('<II', d[off:off+8]); chunk = d[off+8:off+8+clen]
        if ctype == 0x4E4F534A: js = json.loads(chunk)
        elif ctype == 0x004E4942: bin_ = chunk
        off += 8 + clen
    return js, bin_

def write_glb(path, js, bin_):
    jb = json.dumps(js, separators=(',', ':')).encode()
    jb += b' ' * ((4 - len(jb) % 4) % 4)
    bin_ += b'\0' * ((4 - len(bin_) % 4) % 4)
    total = 12 + 8 + len(jb) + 8 + len(bin_)
    with open(path, 'wb') as f:
        f.write(struct.pack('<III', 0x46546C67, 2, total))
        f.write(struct.pack('<II', len(jb), 0x4E4F534A)); f.write(jb)
        f.write(struct.pack('<II', len(bin_), 0x004E4942)); f.write(bin_)

def convert(src, dst):
    js, bin_ = read_glb(src)
    views = js.get('bufferViews', [])
    replace = {}
    for img in js.get('images', []):
        if img.get('mimeType') == 'image/webp' and 'bufferView' in img:
            bv = views[img['bufferView']]
            raw = bin_[bv.get('byteOffset', 0): bv.get('byteOffset', 0) + bv['byteLength']]
            im = Image.open(io.BytesIO(raw)); out = io.BytesIO()
            im.save(out, 'PNG', optimize=False)
            replace[img['bufferView']] = out.getvalue()
            img['mimeType'] = 'image/png'
    new = bytearray()
    for i, bv in enumerate(views):
        data = replace.get(i)
        if data is None:
            o = bv.get('byteOffset', 0); data = bin_[o:o + bv['byteLength']]
        while len(new) % 4: new.append(0)
        bv['byteOffset'] = len(new); bv['byteLength'] = len(data); bv['buffer'] = 0
        new += data
    js['buffers'] = [{'byteLength': len(new)}]
    for tex in js.get('textures', []):
        ext = tex.get('extensions', {}).pop('EXT_texture_webp', None)
        if ext is not None:
            tex['source'] = ext['source']
        if 'extensions' in tex and not tex['extensions']:
            del tex['extensions']
    for key in ('extensionsUsed', 'extensionsRequired'):
        if key in js:
            js[key] = [e for e in js[key] if e != 'EXT_texture_webp']
            if not js[key]: del js[key]
    write_glb(dst, js, bytes(new))
    return len(replace)

if __name__ == '__main__':
    for s in sys.argv[1:]:
        d = s.replace('.glb', '.png.glb') if not s.endswith('.png.glb') else s
        print(s, '->', d, convert(s, d), 'images converted')

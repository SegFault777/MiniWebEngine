#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Image decoder regression test: generates PNG/BMP/GIF/JPEG files with Pillow, decodes them with
src/mwe_img.h (tests/img/dec.c) and compares every pixel with Pillow's own decoding.
Lossless formats must match exactly; JPEG must stay within +-4 per channel (mean < 0.5).
Progressive JPEG must be reported as unsupported. Run: python3 tests/img/run.py  (needs Pillow, a C compiler)."""
import os, subprocess, sys, tempfile, zlib, struct, random
from PIL import Image

here = os.path.dirname(os.path.abspath(__file__))
tmp = tempfile.mkdtemp()
exe = os.path.join(tmp, 'dec')
subprocess.check_call(['cc', '-std=gnu11', '-O1', '-Wall', '-Wextra', '-o', exe, os.path.join(here, 'dec.c')])
random.seed(3)

def noise(w, h):
    im = Image.new('RGB', (w, h)); px = []
    for y in range(h):
        for x in range(w):
            px.append(((x * 7 + y * 3) % 256, (x * 2 + y * 11) % 256, (x * y + random.randint(0, 40)) % 256))
    im.putdata(px); return im

def smooth(w, h):
    im = Image.new('RGB', (w, h)); px = im.load()
    for y in range(h):
        for x in range(w):
            px[x, y] = (255 * x // w, 255 * y // h, 128 + 63 * ((x // 16 + y // 16) % 2))
    return im

def adam7(path, im):
    w, h = im.size; px = im.load(); raw = b''
    xs = [0,4,0,2,0,1,0]; ys = [0,0,4,0,2,0,1]; dx = [8,8,4,4,2,2,1]; dy = [8,8,8,4,4,2,2]
    for p in range(7):
        pw = (w - xs[p] + dx[p] - 1) // dx[p]; ph = (h - ys[p] + dy[p] - 1) // dy[p]
        if pw <= 0 or ph <= 0: continue
        prev = bytes(pw * 3)
        for yy in range(ph):
            row = bytearray()
            for xx in range(pw): row += bytes(px[xs[p] + xx * dx[p], ys[p] + yy * dy[p]])
            ft = (yy + p) % 5; out = bytearray()
            for i, v in enumerate(row):
                a = row[i-3] if i >= 3 else 0; b = prev[i]; c = prev[i-3] if i >= 3 else 0
                if ft == 0: x = v
                elif ft == 1: x = v - a
                elif ft == 2: x = v - b
                elif ft == 3: x = v - ((a + b) >> 1)
                else:
                    pp = a + b - c; pa = abs(pp-a); pb = abs(pp-b); pc = abs(pp-c)
                    x = v - (a if pa <= pb and pa <= pc else (b if pb <= pc else c))
                out.append(x & 255)
            raw += bytes([ft]) + bytes(out); prev = bytes(row)
    def ch(t, d): return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    open(path, 'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 1)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))

base = noise(67, 45); sm = smooth(131, 97)
cases = []   # (name, tolerance_max, tolerance_mean)
def save(name, im, tol=(0, 0), **kw):
    im.save(os.path.join(tmp, name), **kw); cases.append((name, tol))
a = base.convert('RGBA'); a.putalpha(Image.linear_gradient('L').resize(base.size))
save('rgb.png', base); save('rgba.png', a); save('gray.png', base.convert('L')); save('la.png', base.convert('LA'))
save('pal.png', base.convert('P', palette=Image.ADAPTIVE, colors=40))
save('pal_trans.png', base.convert('P', palette=Image.ADAPTIVE, colors=16), transparency=3)
save('bw.png', base.convert('1')); save('stored.png', base, compress_level=0)
save('rgb.bmp', base); save('pal.bmp', base.convert('P', palette=Image.ADAPTIVE, colors=200)); save('bw.bmp', base.convert('1')); save('gray.bmp', base.convert('L'))
save('a.gif', base.convert('P', palette=Image.ADAPTIVE, colors=64)); save('i.gif', base.convert('P', palette=Image.ADAPTIVE, colors=128), interlace=True)
save('t.gif', base.convert('P', palette=Image.ADAPTIVE, colors=8), transparency=2)
adam7(os.path.join(tmp, 'adam7.png'), base); cases.append(('adam7.png', (0, 0)))
for n, sub in (('s444.jpg', 0), ('s420.jpg', 2), ('s422.jpg', 1)): save(n, sm, (4, 0.5), quality=92, subsampling=sub)
save('sg.jpg', sm.convert('L'), (4, 0.5), quality=92)
save('big.jpg', smooth(300, 200), (4, 0.5), quality=80, subsampling=2)
sm.save(os.path.join(tmp, 'prog.jpg'), progressive=True, quality=85)

fail = 0
env = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
r = subprocess.run([exe, os.path.join(tmp, 'prog.jpg'), os.path.join(tmp, 'o.raw')], capture_output=True, text=True)
if 'UNSUPPORTED' not in r.stdout: print('FAIL prog.jpg should be reported unsupported'); fail += 1
for name, (tmax, tmean) in cases:
    r = subprocess.run([exe, os.path.join(tmp, name), os.path.join(tmp, 'o.raw')], capture_output=True, text=True)
    first = r.stdout.split('\n')[0]
    if not first.startswith('OK'): print('FAIL', name, first); fail += 1; continue
    got = open(os.path.join(tmp, 'o.raw'), 'rb').read()
    ref = Image.open(os.path.join(tmp, name)).convert('RGBA'); exp = ref.tobytes()
    gb = bytearray(len(got))
    for i in range(0, len(got), 4): gb[i], gb[i+1], gb[i+2], gb[i+3] = got[i+2], got[i+1], got[i], got[i+3]
    mx = tot = n = 0
    for k in range(0, len(exp), 4):
        if exp[k+3] == 0 and gb[k+3] == 0: continue          # RGB of fully transparent pixels is not defined
        for c in range(4):
            d = abs(gb[k+c] - exp[k+c]); mx = max(mx, d); tot += d; n += 1
    mean = tot / max(n, 1)
    ok = mx <= tmax and mean <= (tmean if tmean else 0)
    print('%-12s max=%d mean=%.3f %s' % (name, mx, mean, 'ok' if ok else 'FAIL'))
    fail += 0 if ok else 1
sys.exit(1 if fail else 0)

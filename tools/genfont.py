#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Convert fonts into mwe_font.h for MiniWebEngine v0.4.

    pip install fonttools
    python3 tools/genfont.py Inter-Regular.otf Inter-Bold.otf \
        --cjk NotoSansCJK-Regular.ttc --cjk-index 2 -o mwe_font.h

Latin: Regular + Bold (ASCII, Latin-1, Latin Extended-A, Greek, Cyrillic, typographic symbols).
CJK (optional): one weight, the GB2312 repertoire (6763 hanzi + symbols/kana/pinyin) plus CJK
punctuation and full-width forms. Bold CJK text is synthesized by double-striking.
Outlines are quantized to a 1024-unit em, converted to quadratic curves and delta/varint encoded.
Use static fonts, not variable fonts.
"""
import argparse
from fontTools.ttLib import TTFont
from fontTools.pens.basePen import BasePen
from fontTools.pens.cu2quPen import Cu2QuPen

LATIN_EXTRA = (list(range(0xA0, 0x180)) + list(range(0x391, 0x3CA)) + list(range(0x401, 0x460)) +
               [0x2013, 0x2014, 0x2015, 0x2018, 0x2019, 0x201A, 0x201C, 0x201D, 0x201E, 0x2020, 0x2021,
                0x2022, 0x2026, 0x2030, 0x2032, 0x2033, 0x2039, 0x203A, 0x20AC, 0x2116, 0x2122, 0x2190,
                0x2191, 0x2192, 0x2193, 0x2212, 0x2215, 0x221E, 0x2248, 0x2260, 0x2264, 0x2265,
                0x25CF, 0x25CB, 0x25A0, 0x25A1, 0x2713, 0x2605])
UNITS = 1024


class Rec(BasePen):
    def __init__(self, gs, k):
        super().__init__(gs)
        self.k, self.ops = k, []

    def q(self, p):
        return (round(p[0] * self.k), round(p[1] * self.k))

    def _moveTo(self, p): self.ops.append(('m', self.q(p)))
    def _lineTo(self, p): self.ops.append(('l', self.q(p)))
    def _qCurveToOne(self, c, p): self.ops.append(('q', self.q(c), self.q(p)))
    def _curveToOne(self, a, b, c): raise RuntimeError('cubic reached pen')
    def _closePath(self): self.ops.append(('z',))
    def _endPath(self): self.ops.append(('z',))


def vi(out, v):
    z = v * 2 if v >= 0 else -v * 2 - 1
    while z >= 128:
        out.append((z & 127) | 128)
        z >>= 7
    out.append(z)


def enc(ops):
    out = bytearray()
    cx = cy = sx = sy = 0
    for o in ops:
        if o[0] == 'm':
            out.append(1); x, y = o[1]; vi(out, x - cx); vi(out, y - cy)
            cx, cy, sx, sy = x, y, x, y
        elif o[0] == 'l':
            out.append(2); x, y = o[1]; vi(out, x - cx); vi(out, y - cy); cx, cy = x, y
        elif o[0] == 'q':
            out.append(3); (qx, qy), (x, y) = o[1], o[2]
            vi(out, qx - cx); vi(out, qy - cy); vi(out, x - qx); vi(out, y - qy)
            cx, cy = x, y
        else:
            out.append(4); cx, cy = sx, sy
    out.append(0)
    return bytes(out)


def openfont(path, index):
    return TTFont(path, fontNumber=index) if index is not None else TTFont(path)


def load(f, cps, err):
    k = UNITS / f['head'].unitsPerEm
    cmap, gs, hm = f.getBestCmap(), f.getGlyphSet(), f['hmtx']
    data, adv = [], []
    for cp in cps:
        g = cmap[cp]
        rec = Rec(gs, k)
        gs[g].draw(Cu2QuPen(rec, max_err=err / k, reverse_direction=False))
        data.append(enc(rec.ops))
        adv.append(round(hm[g][0] * k))
    return data, adv


def gb2312_set(level):
    out = set()
    rows = range(0xA1, 0xAA) if level >= 0 else []
    for b0 in list(range(0xA1, 0xAA)) + list(range(0xB0, 0xD8 if level == 1 else 0xF8)):
        for b1 in range(0xA1, 0xFF):
            try:
                ch = bytes([b0, b1]).decode('gb2312')
            except UnicodeDecodeError:
                continue
            if len(ch) == 1: out.add(ord(ch))
    out.update(range(0x3000, 0x3040)); out.update(range(0xFF01, 0xFF5F)); out.update([0xFFE0, 0xFFE1, 0xFFE5])
    return out


def ksx1001_set():
    """Hangul of KS X 1001 (2350 most used syllables) + compatibility jamo + the standard's symbols."""
    out = set()
    for b0 in list(range(0xA1, 0xAD)) + list(range(0xB0, 0xC9)):
        for b1 in range(0xA1, 0xFF):
            try:
                ch = bytes([b0, b1]).decode('euc-kr')
            except UnicodeDecodeError:
                continue
            if len(ch) == 1: out.add(ord(ch))
    out.update(range(0x3131, 0x3164))
    return out


def arr(vals, w=20):
    return ',\n'.join(','.join(str(v) for v in vals[i:i + w]) for i in range(0, len(vals), w))


def offsets(data):
    o, t = [], 0
    for d in data: o.append(t); t += len(d)
    o.append(t)
    return o


def cstr(s):
    return '"' + s.replace('\\', '\\\\').replace('"', '\\"').replace('\n', '\\n') + '"'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('regular'); ap.add_argument('bold')
    ap.add_argument('-o', '--out', default='mwe_font.h')
    ap.add_argument('--latin-index', type=int, default=None, help='face index when the Latin fonts are .ttc files')
    ap.add_argument('--name', help='font label embedded in the header')
    ap.add_argument('--cjk', help='CJK font file (OTF/TTF/TTC); omit for a Latin-only build')
    ap.add_argument('--cjk-index', type=int, default=None, help='face index inside a .ttc')
    ap.add_argument('--hangul', help='Korean font file (OTF/TTF/TTC) supplying KS X 1001 Hangul + jamo')
    ap.add_argument('--hangul-index', type=int, default=None)
    ap.add_argument('--cjk-level', type=int, default=2, choices=(1, 2), help='GB2312 level 1 (3755) or 1+2 (6763 hanzi)')
    ap.add_argument('--cjk-name', help='CJK font label')
    ap.add_argument("--cjk-err", type=float, default=3.0, help="curve simplification tolerance (1024-unit em)")
    ap.add_argument('--notice', action='append', default=[], help='copyright/licence line (repeatable)')
    a = ap.parse_args()
    fr, fb = openfont(a.regular, a.latin_index), openfont(a.bold, a.latin_index)
    probe = [fr.getBestCmap(), fb.getBestCmap()]
    cps = list(range(32, 127))
    LATIN = None
    for cp in LATIN_EXTRA:
        if all(cp in c for c in probe): cps.append(cp)
    cps = sorted(set(cps))
    miss = [cp for cp in LATIN_EXTRA if cp not in cps]
    if miss: print('note: %d Latin code points missing in the font and skipped' % len(miss))
    rd, ra = load(fr, cps, 0.5); bd, ba = load(fb, cps, 0.5)
    hh = fr['hhea']; k = UNITS / fr['head'].unitsPerEm
    asc, desc = round(hh.ascent * k), round(-hh.descent * k)
    label = a.name or '%s Regular+Bold' % fr['name'].getDebugName(1)
    ro, bo = offsets(rd), offsets(bd)
    with open(a.out, 'w') as o:
        o.write('/* GENERATED by tools/genfont.py -- do not edit. Source: %s */\n' % label)
        o.write('/* Font outlines are licensed under the SIL Open Font License 1.1 (see fonts/ and NOTICE.md). */\n')
        o.write('#define MF_NAME %s\n#define MF_UPM %d\n#define MF_ASC %d\n#define MF_DESC %d\n#define MF_N %d\n'
                % (cstr(label), UNITS, asc, desc, len(cps)))
        o.write('static const unsigned short MF_CP[MF_N]={%s};\n' % ','.join(map(str, cps)))
        o.write('static const unsigned short MF_ADV[2][MF_N]={{%s},{%s}};\n' % (arr(ra), arr(ba)))
        o.write('static const unsigned MF_OFF[2][MF_N+1]={{%s},{%s}};\n' % (arr(ro), arr(bo)))
        o.write('static const unsigned char MF_D0[]={\n%s};\n' % arr(list(b''.join(rd)), 24))
        o.write('static const unsigned char MF_D1[]={\n%s};\n' % arr(list(b''.join(bd)), 24))
        tot = ro[-1] + bo[-1]
        cjn = 0
        if a.cjk:
            fc = openfont(a.cjk, a.cjk_index)
            cm = fc.getBestCmap()
            have = set(cps)
            want = sorted(cp for cp in gb2312_set(a.cjk_level) if cp in cm and cp >= 0x80 and cp not in have)
            cd, ca = load(fc, want, a.cjk_err)
            if a.hangul:                      # merge Hangul (and KS X 1001 symbols the SC font lacks) from the KR face
                fk = openfont(a.hangul, a.hangul_index)
                km = fk.getBestCmap()
                taken = have | set(want)
                kw = sorted(cp for cp in ksx1001_set() if cp in km and cp >= 0x80 and cp not in taken)
                kd, ka = load(fk, kw, a.cjk_err)
                merged = sorted(zip(want + kw, cd + kd, ca + ka), key=lambda t: t[0])
                want = [m[0] for m in merged]; cd = [m[1] for m in merged]; ca = [m[2] for m in merged]
                print('hangul/KS X 1001: %d glyphs merged from %s' % (len(kw), fk['name'].getDebugName(1)))
            co = offsets(cd); cjn = len(want); tot += co[-1]
            cl = a.cjk_name or fc['name'].getDebugName(1)
            co = offsets(cd); cjn = len(want); tot = ro[-1] + bo[-1] + co[-1]
            o.write('#define MF_CJ_NAME %s\n#define MF_CJ_N %d\n' % (cstr(cl), cjn))
            o.write('static const unsigned short MF_CJ_CP[MF_CJ_N]={%s};\n' % ','.join(map(str, want)))
            o.write('static const unsigned short MF_CJ_ADV[MF_CJ_N]={%s};\n' % arr(ca))
            o.write('static const unsigned MF_CJ_OFF[MF_CJ_N+1]={%s};\n' % arr(co))
            o.write('static const unsigned char MF_CJ_D[]={\n%s};\n' % arr(list(b''.join(cd)), 24))
        else:
            o.write('#define MF_CJ_N 0\n')
        o.write('#define MF_NOTICE %s\n' % (' '.join(cstr(n + '\n') for n in a.notice) or '""'))
    print('%s: %d latin glyphs (%d+%d bytes), %d cjk glyphs, total outline data %d bytes' %
          (a.out, len(cps), ro[-1], bo[-1], cjn, tot))


if __name__ == '__main__':
    main()

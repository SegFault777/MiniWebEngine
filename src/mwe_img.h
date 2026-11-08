/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.7 image decoders (no dependencies): PNG (all colour types and bit depths, Adam7),
 * JPEG (baseline and extended sequential Huffman, 8-bit, grey/YCbCr, restart markers), GIF (first frame,
 * transparency, interlace) and BMP (1/4/8/24/32-bit uncompressed). Not supported: progressive JPEG,
 * arithmetic-coded JPEG, CMYK, RLE bitmaps, animation (the first GIF frame is shown), WebP, SVG.
 * Output pixels are 0xAARRGGBB (not premultiplied). Every size field of the input is range-checked;
 * decoding fails (returns 0) instead of reading or writing out of bounds. */
#ifndef MWE_IMG_H
#define MWE_IMG_H

typedef unsigned char mu8;
#define MI_MAXDIM 8192

static int mi_be32(const mu8 *p) { return (int)(((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3]); }
static int mi_le16(const mu8 *p) { return p[0] | (p[1] << 8); }
static int mi_le32(const mu8 *p) { return (int)((unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24)); }

/* ---------------------------------------------------------------- inflate (RFC 1951) */
typedef struct { const mu8 *s; int sn, si; unsigned tag; int bc; mu8 *d; int dn, di; int err; } MInf;
typedef struct { unsigned short cnt[16], sym[288]; } MTree;

static int mi_bit(MInf *z) {
    int b;
    if (!z->bc--) {
        if (z->si >= z->sn) { z->err = 1; z->bc = 0; return 0; }
        z->tag = z->s[z->si++]; z->bc = 7;
    }
    b = (int)(z->tag & 1); z->tag >>= 1; return b;
}
static unsigned mi_bits(MInf *z, int n) { unsigned v = 0; int i; for (i = 0; i < n; i++) v |= (unsigned)mi_bit(z) << i; return v; }
static void mi_tree(MTree *t, const mu8 *len, int num) {
    unsigned short off[16]; int i; unsigned sum = 0;
    for (i = 0; i < 16; i++) t->cnt[i] = 0;
    for (i = 0; i < num; i++) t->cnt[len[i]]++;
    t->cnt[0] = 0;
    for (i = 0; i < 16; i++) { off[i] = (unsigned short)sum; sum += t->cnt[i]; }
    for (i = 0; i < num; i++) if (len[i]) t->sym[off[len[i]]++] = (unsigned short)i;
}
static int mi_sym(MInf *z, const MTree *t) {
    int sum = 0, cur = 0, len = 0;
    do {
        cur = 2 * cur + mi_bit(z); ++len;
        if (len > 15) { z->err = 1; return 0; }
        sum += t->cnt[len]; cur -= t->cnt[len];
    } while (cur >= 0);
    return t->sym[sum + cur];
}
static const unsigned short mi_lbase[29] = { 3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,67,83,99,115,131,163,195,227,258 };
static const mu8 mi_lext[29] = { 0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const unsigned short mi_dbase[30] = { 1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const mu8 mi_dext[30] = { 0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };
static int mi_inflate_codes(MInf *z, const MTree *lt, const MTree *dt) {
    for (;;) {
        int s = mi_sym(z, lt), len, dist, i;
        if (z->err) return 0;
        if (s < 256) { if (z->di >= z->dn) return 0; z->d[z->di++] = (mu8)s; continue; }
        if (s == 256) return 1;
        s -= 257; if (s >= 29) return 0;
        len = mi_lbase[s] + (int)mi_bits(z, mi_lext[s]);
        s = mi_sym(z, dt); if (z->err || s >= 30) return 0;
        dist = mi_dbase[s] + (int)mi_bits(z, mi_dext[s]);
        if (dist > z->di || z->di + len > z->dn) return 0;
        for (i = 0; i < len; i++) { z->d[z->di] = z->d[z->di - dist]; z->di++; }
        if (z->err) return 0;
    }
}
/* zlib stream -> d[0..dn); returns the number of bytes produced, or -1 */
static int mi_zlib(const mu8 *s, int sn, mu8 *d, int dn) {
    static MTree fl, fd, lt, dt;
    static int fixed_ready;
    MInf z; int last;
    if (sn < 2 || (s[0] & 15) != 8 || ((s[0] << 8) | s[1]) % 31 || (s[1] & 32)) return -1;
    z.s = s + 2; z.sn = sn - 2; z.si = 0; z.tag = 0; z.bc = 0; z.d = d; z.dn = dn; z.di = 0; z.err = 0;
    if (!fixed_ready) {
        mu8 l[288]; int i;
        for (i = 0; i < 144; i++) l[i] = 8;
        for (; i < 256; i++) l[i] = 9;
        for (; i < 280; i++) l[i] = 7;
        for (; i < 288; i++) l[i] = 8;
        mi_tree(&fl, l, 288);
        for (i = 0; i < 30; i++) l[i] = 5;
        mi_tree(&fd, l, 30);
        fixed_ready = 1;
    }
    do {
        int type;
        last = mi_bit(&z); type = (int)mi_bits(&z, 2);
        if (z.err) return -1;
        if (type == 0) {
            int len, i;
            z.bc = 0;
            if (z.si + 4 > z.sn) return -1;
            len = z.s[z.si] | (z.s[z.si + 1] << 8);
            if ((len ^ (z.s[z.si + 2] | (z.s[z.si + 3] << 8))) != 0xFFFF) return -1;
            z.si += 4;
            if (z.si + len > z.sn || z.di + len > z.dn) return -1;
            for (i = 0; i < len; i++) z.d[z.di++] = z.s[z.si++];
        } else if (type == 1) {
            if (!mi_inflate_codes(&z, &fl, &fd)) return -1;
        } else if (type == 2) {
            static const mu8 order[19] = { 16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };
            mu8 l[320]; int hlit = (int)mi_bits(&z, 5) + 257, hdist = (int)mi_bits(&z, 5) + 1, hclen = (int)mi_bits(&z, 4) + 4, i, n;
            MTree ct;
            for (i = 0; i < 19; i++) l[i] = 0;
            for (i = 0; i < hclen; i++) l[order[i]] = (mu8)mi_bits(&z, 3);
            mi_tree(&ct, l, 19);
            if (hlit > 286 || hdist > 30) return -1;
            for (n = 0; n < hlit + hdist;) {
                int s2 = mi_sym(&z, &ct), rep, v = 0;
                if (z.err) return -1;
                if (s2 < 16) { l[n++] = (mu8)s2; continue; }
                if (s2 == 16) { if (!n) return -1; v = l[n - 1]; rep = 3 + (int)mi_bits(&z, 2); }
                else if (s2 == 17) rep = 3 + (int)mi_bits(&z, 3);
                else rep = 11 + (int)mi_bits(&z, 7);
                if (n + rep > hlit + hdist) return -1;
                while (rep--) l[n++] = (mu8)v;
            }
            mi_tree(&lt, l, hlit); mi_tree(&dt, l + hlit, hdist);
            if (!mi_inflate_codes(&z, &lt, &dt)) return -1;
        } else return -1;
        if (z.err) return -1;
    } while (!last);
    return z.di;
}

/* ---------------------------------------------------------------- scratch + PNG */
#define MI_SCRATCH (24 << 20)
static mu8 mi_scr[MI_SCRATCH];

static int mi_paeth(int a, int b, int c) {
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    return (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
}
/* undo the PNG filter of one scanline in place; prev may be NULL (all zero) */
static int mi_unfilter(mu8 *cur, const mu8 *prev, int stride, int bpp, int ft) {
    int i;
    for (i = 0; i < stride; i++) {
        int a = i >= bpp ? cur[i - bpp] : 0, b = prev ? prev[i] : 0, c = (prev && i >= bpp) ? prev[i - bpp] : 0, x = cur[i];
        switch (ft) {
        case 0: break;
        case 1: x += a; break;
        case 2: x += b; break;
        case 3: x += (a + b) >> 1; break;
        case 4: x += mi_paeth(a, b, c); break;
        default: return 0;
        }
        cur[i] = (mu8)x;
    }
    return 1;
}
static int mi_png_hdr(const mu8 *d, int n, int *w, int *h, int *depth, int *ct, int *il) {
    static const mu8 sig[8] = { 137,80,78,71,13,10,26,10 };
    int i;
    if (n < 33) return 0;
    for (i = 0; i < 8; i++) if (d[i] != sig[i]) return 0;
    if (mi_be32(d + 8) != 13 || d[12] != 'I' || d[13] != 'H' || d[14] != 'D' || d[15] != 'R') return 0;
    *w = mi_be32(d + 16); *h = mi_be32(d + 20); *depth = d[24]; *ct = d[25]; *il = d[28];
    if (*w <= 0 || *h <= 0 || *w > MI_MAXDIM || *h > MI_MAXDIM || d[26] != 0 || d[27] != 0 || *il > 1) return 0;
    switch (*ct) {
    case 0: if (*depth != 1 && *depth != 2 && *depth != 4 && *depth != 8 && *depth != 16) return 0; break;
    case 3: if (*depth != 1 && *depth != 2 && *depth != 4 && *depth != 8) return 0; break;
    case 2: case 4: case 6: if (*depth != 8 && *depth != 16) return 0; break;
    default: return 0;
    }
    return 1;
}
static int mi_png(const mu8 *d, int n, unsigned *out, int W, int H) {
    int w, h, depth, ct, il, chan, bitspp, bpp, pos = 8, comp = 0, rawsz, i, plte_n = 0, have_key = 0, key[3] = { 0, 0, 0 };
    unsigned pal[256];
    mu8 *raw, *cbuf;
    if (!mi_png_hdr(d, n, &w, &h, &depth, &ct, &il) || w != W || h != H) return 0;
    chan = ct == 0 ? 1 : ct == 2 ? 3 : ct == 3 ? 1 : ct == 4 ? 2 : 4;
    bitspp = chan * depth; bpp = (bitspp + 7) / 8;
    { long long sz = 0; int pass;   /* raw size = sum of passes (each row has a filter byte) */
      if (!il) sz = (long long)h * (1 + ((long long)w * bitspp + 7) / 8);
      else for (pass = 0; pass < 7; pass++) {
          static const mu8 xs[7] = { 0,4,0,2,0,1,0 }, ys[7] = { 0,0,4,0,2,0,1 }, dx[7] = { 8,8,4,4,2,2,1 }, dy[7] = { 8,8,8,4,4,2,2 };
          int pw = (w - xs[pass] + dx[pass] - 1) / dx[pass], ph = (h - ys[pass] + dy[pass] - 1) / dy[pass];
          if (pw > 0 && ph > 0) sz += (long long)ph * (1 + ((long long)pw * bitspp + 7) / 8);
      }
      if (sz <= 0 || sz + n + 64 > MI_SCRATCH) return 0;
      rawsz = (int)sz; }
    raw = mi_scr; cbuf = mi_scr + rawsz;          /* compressed IDAT bytes are gathered after the raw area */
    for (i = 0; i < 256; i++) pal[i] = 0xFF000000u;
    while (pos + 12 <= n) {
        int len = mi_be32(d + pos); const mu8 *tp = d + pos + 4, *cd = d + pos + 8;
        if (len < 0 || pos + 12 + len > n) return 0;
        if (tp[0] == 'I' && tp[1] == 'D' && tp[2] == 'A' && tp[3] == 'T') { if ((long long)comp + len > n) return 0; for (i = 0; i < len; i++) cbuf[comp + i] = cd[i]; comp += len; }
        else if (tp[0] == 'P' && tp[1] == 'L' && tp[2] == 'T' && tp[3] == 'E') {
            plte_n = len / 3; if (plte_n > 256) plte_n = 256;
            for (i = 0; i < plte_n; i++) pal[i] = 0xFF000000u | ((unsigned)cd[i * 3] << 16) | ((unsigned)cd[i * 3 + 1] << 8) | cd[i * 3 + 2];
        } else if (tp[0] == 't' && tp[1] == 'R' && tp[2] == 'N' && tp[3] == 'S') {
            if (ct == 3) { for (i = 0; i < len && i < 256; i++) pal[i] = (pal[i] & 0x00FFFFFFu) | ((unsigned)cd[i] << 24); }
            else if (ct == 0 && len >= 2) { have_key = 1; key[0] = (cd[0] << 8) | cd[1]; }
            else if (ct == 2 && len >= 6) { have_key = 1; key[0] = (cd[0] << 8) | cd[1]; key[1] = (cd[2] << 8) | cd[3]; key[2] = (cd[4] << 8) | cd[5]; }
        } else if (tp[0] == 'I' && tp[1] == 'E' && tp[2] == 'N' && tp[3] == 'D') break;
        pos += 12 + len;
    }
    if (!comp || mi_zlib(cbuf, comp, raw, rawsz) != rawsz) return 0;
    if (ct == 3 && !plte_n) return 0;
    {   /* walk the passes, unfilter, expand to ARGB */
        int pass, npass = il ? 7 : 1, off = 0;
        static const mu8 xs[7] = { 0,4,0,2,0,1,0 }, ys[7] = { 0,0,4,0,2,0,1 }, dx[7] = { 8,8,4,4,2,2,1 }, dy[7] = { 8,8,8,4,4,2,2 };
        for (pass = 0; pass < npass; pass++) {
            int pw = il ? (w - xs[pass] + dx[pass] - 1) / dx[pass] : w, ph = il ? (h - ys[pass] + dy[pass] - 1) / dy[pass] : h, stride, y, x;
            if (pw <= 0 || ph <= 0) continue;
            stride = (pw * bitspp + 7) / 8;
            for (y = 0; y < ph; y++) {
                mu8 *row = raw + off + (long long)y * (stride + 1);
                const mu8 *prev = y ? row - (stride + 1) + 1 : 0;
                if (!mi_unfilter(row + 1, prev, stride, bpp, row[0])) return 0;
            }
            for (y = 0; y < ph; y++) {
                const mu8 *r = raw + off + (long long)y * (stride + 1) + 1;
                int oy = il ? ys[pass] + y * dy[pass] : y;
                for (x = 0; x < pw; x++) {
                    int ox = il ? xs[pass] + x * dx[pass] : x; unsigned px;
                    int v, rr, gg, bb, aa = 255;
                    if (ct == 3 || ct == 0) {
                        int bit = x * depth;
                        if (depth == 16) v = (r[x * 2] << 8) | r[x * 2 + 1];
                        else v = (r[bit >> 3] >> (8 - depth - (bit & 7))) & ((1 << depth) - 1);
                        if (ct == 3) px = pal[v];
                        else {
                            int g8 = depth == 16 ? v >> 8 : depth == 8 ? v : depth == 4 ? v * 17 : depth == 2 ? v * 85 : v * 255;
                            px = 0xFF000000u | ((unsigned)g8 << 16) | ((unsigned)g8 << 8) | (unsigned)g8;
                            if (have_key && v == key[0]) px = 0;
                        }
                    } else if (ct == 4) {
                        if (depth == 8) { rr = r[x * 2]; aa = r[x * 2 + 1]; } else { rr = r[x * 4]; aa = r[x * 4 + 2]; }
                        px = ((unsigned)aa << 24) | ((unsigned)rr << 16) | ((unsigned)rr << 8) | (unsigned)rr;
                    } else {
                        int st = depth == 8 ? 1 : 2, k = chan * st;
                        rr = r[x * k]; gg = r[x * k + st]; bb = r[x * k + 2 * st];
                        if (ct == 6) aa = r[x * k + 3 * st];
                        px = ((unsigned)aa << 24) | ((unsigned)rr << 16) | ((unsigned)gg << 8) | (unsigned)bb;
                        if (ct == 2 && have_key) {
                            int r16 = depth == 8 ? r[x * k] : (r[x * k] << 8) | r[x * k + 1], g16 = depth == 8 ? r[x * k + 1] : (r[x * k + 2] << 8) | r[x * k + 3], b16 = depth == 8 ? r[x * k + 2] : (r[x * k + 4] << 8) | r[x * k + 5];
                            if (r16 == key[0] && g16 == key[1] && b16 == key[2]) px = 0;
                        }
                    }
                    out[(long long)oy * w + ox] = px;
                }
            }
            off += ph * (stride + 1);
        }
    }
    return 1;
}

/* ---------------------------------------------------------------- BMP */
static int mi_bmp_hdr(const mu8 *d, int n, int *w, int *h) {
    int hs, bpp, comp;
    if (n < 30 || d[0] != 'B' || d[1] != 'M') return 0;
    hs = mi_le32(d + 14);
    if (hs < 40 || n < 14 + hs) return 0;
    *w = mi_le32(d + 18); *h = mi_le32(d + 22); bpp = mi_le16(d + 28); comp = mi_le32(d + 30);
    if (*h < 0) *h = -*h;
    if (*w <= 0 || *h <= 0 || *w > MI_MAXDIM || *h > MI_MAXDIM) return 0;
    if (comp != 0 && !(comp == 3 && (bpp == 32 || bpp == 16))) return 0;
    return bpp == 1 || bpp == 4 || bpp == 8 || bpp == 24 || bpp == 32;
}
static int mi_bmp(const mu8 *d, int n, unsigned *out, int W, int H) {
    int w, h, bpp, hs, off, topdown, ncol, stride, x, y, comp;
    unsigned pal[256];
    if (!mi_bmp_hdr(d, n, &w, &h) || w != W || h != H) return 0;
    hs = mi_le32(d + 14); off = mi_le32(d + 10); bpp = mi_le16(d + 28); comp = mi_le32(d + 30);
    topdown = mi_le32(d + 22) < 0;
    ncol = mi_le32(d + 46); if (bpp <= 8 && (ncol <= 0 || ncol > (1 << bpp))) ncol = 1 << bpp;
    if (bpp <= 8) {
        int p0 = 14 + hs, i;
        if (comp == 3) p0 += 12;
        if (p0 + ncol * 4 > n) return 0;
        for (i = 0; i < 256; i++) pal[i] = 0xFF000000u;
        for (i = 0; i < ncol; i++) pal[i] = 0xFF000000u | ((unsigned)d[p0 + i * 4 + 2] << 16) | ((unsigned)d[p0 + i * 4 + 1] << 8) | d[p0 + i * 4];
    }
    stride = (int)(((long long)w * bpp + 31) / 32 * 4);
    if (off < 14 + hs || (long long)off + (long long)stride * h > n) return 0;
    for (y = 0; y < h; y++) {
        const mu8 *r = d + off + (long long)(topdown ? y : h - 1 - y) * stride;
        for (x = 0; x < w; x++) {
            unsigned px;
            if (bpp == 24) px = 0xFF000000u | ((unsigned)r[x * 3 + 2] << 16) | ((unsigned)r[x * 3 + 1] << 8) | r[x * 3];
            else if (bpp == 32) px = 0xFF000000u | ((unsigned)r[x * 4 + 2] << 16) | ((unsigned)r[x * 4 + 1] << 8) | r[x * 4];
            else { int bit = x * bpp; px = pal[(r[bit >> 3] >> (8 - bpp - (bit & 7))) & ((1 << bpp) - 1)]; }
            out[(long long)y * w + x] = px;
        }
    }
    return 1;
}

/* ---------------------------------------------------------------- GIF (first frame) */
static int mi_gif_hdr(const mu8 *d, int n, int *w, int *h) {
    if (n < 13 || d[0] != 'G' || d[1] != 'I' || d[2] != 'F' || d[3] != '8') return 0;
    *w = mi_le16(d + 6); *h = mi_le16(d + 8);
    return *w > 0 && *h > 0 && *w <= MI_MAXDIM && *h <= MI_MAXDIM;
}
static int mi_gif(const mu8 *d, int n, unsigned *out, int W, int H) {
    unsigned gpal[256], lpal[256], *pal; int w, h, pos = 13, i, trans = -1;
    if (!mi_gif_hdr(d, n, &w, &h) || w != W || h != H) return 0;
    for (i = 0; i < w * h; i++) out[i] = 0;
    for (i = 0; i < 256; i++) gpal[i] = 0xFF000000u;
    if (d[10] & 0x80) {
        int cn = 2 << (d[10] & 7);
        if (pos + cn * 3 > n) return 0;
        for (i = 0; i < cn; i++) gpal[i] = 0xFF000000u | ((unsigned)d[pos + i * 3] << 16) | ((unsigned)d[pos + i * 3 + 1] << 8) | d[pos + i * 3 + 2];
        pos += cn * 3;
    }
    while (pos < n) {
        int b = d[pos++];
        if (b == 0x3B) return 0;
        if (b == 0x21) {                               /* extension */
            int lab, sz;
            if (pos >= n) return 0;
            lab = d[pos++];
            if (lab == 0xF9 && pos + 5 < n && d[pos] >= 4) { trans = (d[pos + 1] & 1) ? d[pos + 4] : -1; }
            while (pos < n && (sz = d[pos++]) != 0) pos += sz;
            continue;
        }
        if (b != 0x2C) return 0;
        {   /* image descriptor */
            int fx, fy, fw, fh, flags, minc, clear, eoi, nxt, csz, mask, prev, avail, nb, ipass = 0, row = 0, col = 0, done = 0;
            static unsigned short prefix[4096]; static mu8 suffix[4096], stk[4097];
            mu8 blk[256]; int bl = 0, bi = 0, sp, first = 0;
            if (pos + 9 > n) return 0;
            fx = mi_le16(d + pos); fy = mi_le16(d + pos + 2); fw = mi_le16(d + pos + 4); fh = mi_le16(d + pos + 6); flags = d[pos + 8]; pos += 9;
            if (fw <= 0 || fh <= 0 || fx + fw > w || fy + fh > h) return 0;
            pal = gpal;
            if (flags & 0x80) {
                int cn = 2 << (flags & 7);
                if (pos + cn * 3 > n) return 0;
                for (i = 0; i < 256; i++) lpal[i] = 0xFF000000u;
                for (i = 0; i < cn; i++) lpal[i] = 0xFF000000u | ((unsigned)d[pos + i * 3] << 16) | ((unsigned)d[pos + i * 3 + 1] << 8) | d[pos + i * 3 + 2];
                pos += cn * 3; pal = lpal;
            }
            if (pos >= n) return 0;
            minc = d[pos++]; if (minc < 2 || minc > 8) return 0;
            clear = 1 << minc; eoi = clear + 1; nxt = clear + 2; csz = minc + 1; mask = (1 << csz) - 1; prev = -1; avail = 0; nb = 0;
            for (i = 0; i < clear; i++) { prefix[i] = 0; suffix[i] = (mu8)i; }
            while (!done) {
                int code;
                while (nb < csz) {
                    if (bi >= bl) { if (pos >= n) return 0; bl = d[pos++]; bi = 0; if (!bl) { done = 1; break; } if (pos + bl > n) return 0; for (i = 0; i < bl; i++) blk[i] = d[pos + i]; pos += bl; }
                    avail |= blk[bi++] << nb; nb += 8;
                }
                if (done) break;
                code = avail & mask; avail >>= csz; nb -= csz;
                if (code == clear) { nxt = clear + 2; csz = minc + 1; mask = (1 << csz) - 1; prev = -1; continue; }
                if (code == eoi) break;
                if (prev < 0) { first = code; if (code >= clear) return 0; prev = code; sp = 0; stk[sp++] = (mu8)code; }
                else {
                    int cur = code; sp = 0;
                    if (code >= nxt) { if (code > nxt) return 0; stk[sp++] = (mu8)first; cur = prev; }
                    while (cur >= clear) { if (sp >= 4096) return 0; stk[sp++] = suffix[cur]; cur = prefix[cur]; }
                    stk[sp++] = (mu8)cur; first = cur;
                    if (nxt < 4096) { prefix[nxt] = (unsigned short)prev; suffix[nxt] = (mu8)first; nxt++; if (nxt > mask && csz < 12) { csz++; mask = (1 << csz) - 1; } }
                    prev = code;
                }
                while (sp > 0) {
                    int idx = stk[--sp], oy;
                    if (row < fh) {
                        oy = row;
                        if (flags & 0x40) oy = ipass == 0 ? row * 8 : ipass == 1 ? 4 + row * 8 : ipass == 2 ? 2 + row * 4 : 1 + row * 2;
                        if (oy < fh && idx != trans) out[(long long)(fy + oy) * w + fx + col] = pal[idx];
                    }
                    if (++col >= fw) {
                        col = 0; row++;
                        if (flags & 0x40) {
                            int lim = ipass == 0 ? (fh + 7) / 8 : ipass == 1 ? (fh + 3) / 8 : ipass == 2 ? (fh + 1) / 4 : fh / 2;
                            if (row >= lim && ipass < 3) { ipass++; row = 0; }
                        }
                    }
                }
            }
            return 1;                                   /* first frame only */
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- JPEG (baseline / extended sequential) */
typedef struct { mu8 bits[17]; mu8 val[256]; int mincode[17], maxcode[18], valptr[17]; int ok; } MHuff;
typedef struct { const mu8 *p; int n, i; unsigned buf; int cnt; int eof; } MJBits;
static void mi_hbuild(MHuff *h) {
    int code = 0, k = 0, l;
    for (l = 1; l <= 16; l++) {
        h->valptr[l] = k; h->mincode[l] = code;
        code += h->bits[l]; k += h->bits[l];
        h->maxcode[l] = h->bits[l] ? code - 1 : -1;
        code <<= 1;
    }
    h->maxcode[17] = 0x7FFFFFFF; h->ok = 1;
}
static int mi_jbit(MJBits *b) {
    if (!b->cnt) {
        int c;
        if (b->i >= b->n) { b->eof = 1; return 0; }
        c = b->p[b->i++];
        if (c == 0xFF) {
            int c2 = b->i < b->n ? b->p[b->i] : 0xD9;
            if (c2 == 0) b->i++;
            else { b->i--; b->eof = 1; return 0; }     /* marker: stop feeding bits */
        }
        b->buf = (unsigned)c; b->cnt = 8;
    }
    b->cnt--; return (int)((b->buf >> b->cnt) & 1);
}
static int mi_jdec(MJBits *b, const MHuff *h) {
    int code = 0, l;
    for (l = 1; l <= 16; l++) {
        code = (code << 1) | mi_jbit(b);
        if (b->eof) return -1;
        if (h->maxcode[l] >= 0 && code <= h->maxcode[l] && code >= h->mincode[l]) return h->val[h->valptr[l] + code - h->mincode[l]];
    }
    return -1;
}
static int mi_jrecv(MJBits *b, int s) { int v = 0; while (s--) v = (v << 1) | mi_jbit(b); return v; }
static int mi_jext(int v, int s) { return (s && v < (1 << (s - 1))) ? v - (1 << s) + 1 : v; }
static const mu8 mi_zz[64] = { 0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63 };
static void mi_idct(const int *in, mu8 *out, int stride) {
    static float c[8][8]; static int init;
    float tmp[64]; int x, y, u, v;
    if (!init) {
        for (x = 0; x < 8; x++) for (u = 0; u < 8; u++) {
            /* cos((2x+1) u pi / 16) via a small table-free recurrence is overkill; use a polynomial-free closed form */
            static const float ct[16] = { 1.0f, 0.98078528f, 0.92387953f, 0.83146961f, 0.70710678f, 0.55557023f, 0.38268343f, 0.19509032f, 0.0f, -0.19509032f, -0.38268343f, -0.55557023f, -0.70710678f, -0.83146961f, -0.92387953f, -0.98078528f };
            int k = ((2 * x + 1) * u) % 32, sgn = 1; float cv;
            if (k >= 16) { k = 32 - k; }
            /* cos(k*pi/16) for k in 0..16 */
            if (k > 8) { k = 16 - k; sgn = -1; }
            cv = ct[k] * (float)sgn;
            c[x][u] = (u == 0 ? 0.35355339f : 0.5f) * cv;
        }
        init = 1;
    }
    for (y = 0; y < 8; y++) for (x = 0; x < 8; x++) { float s = 0; for (u = 0; u < 8; u++) s += c[x][u] * (float)in[y * 8 + u]; tmp[y * 8 + x] = s; }
    for (x = 0; x < 8; x++) for (y = 0; y < 8; y++) {
        float s = 0; int iv;
        for (v = 0; v < 8; v++) s += c[y][v] * tmp[v * 8 + x];
        iv = (int)(s + 128.5f); if (s + 128.5f < 0) iv = 0;
        out[y * stride + x] = (mu8)(iv < 0 ? 0 : iv > 255 ? 255 : iv);
    }
}
typedef struct { int id, h, v, tq, td, ta, pred, pw, ph; mu8 *pl; } MJComp;
static int mi_jpg_hdr(const mu8 *d, int n, int *w, int *h, int *prog) {
    int i = 2;
    if (n < 4 || d[0] != 0xFF || d[1] != 0xD8) return 0;
    *prog = 0;
    while (i + 4 <= n) {
        int m, len;
        if (d[i] != 0xFF) { i++; continue; }
        m = d[i + 1];
        if (m == 0xFF) { i++; continue; }
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { i += 2; continue; }
        len = (d[i + 2] << 8) | d[i + 3];
        if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
            if (i + 9 > n) return 0;
            *h = (d[i + 5] << 8) | d[i + 6]; *w = (d[i + 7] << 8) | d[i + 8];
            *prog = (m == 0xC2) ? 1 : (m == 0xC0 || m == 0xC1) ? 0 : 2;
            return *w > 0 && *h > 0 && *w <= MI_MAXDIM && *h <= MI_MAXDIM;
        }
        i += 2 + len;
    }
    return 0;
}
static int mi_jpg(const mu8 *d, int n, unsigned *out, int W, int H) {
    static MHuff hdc[4], hac[4];
    static int qt[4][64];
    MJComp cp[4]; int nc = 0, i = 2, w = 0, h = 0, prog, ri = 0, hmax = 1, vmax = 1, adobe = -1, mcux, mcuy, mx, my, c, k;
    long long used = 0;
    if (!mi_jpg_hdr(d, n, &w, &h, &prog) || w != W || h != H || prog) return 0;
    for (k = 0; k < 4; k++) { hdc[k].ok = hac[k].ok = 0; for (c = 0; c < 64; c++) qt[k][c] = 1; }
    for (;;) {
        int m, len;
        while (i < n && d[i] != 0xFF) i++;
        while (i < n && d[i] == 0xFF) i++;
        if (i >= n) return 0;
        m = d[i++];
        if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) continue;
        if (m == 0xD9) return 0;
        if (i + 2 > n) return 0;
        len = (d[i] << 8) | d[i + 1];
        if (len < 2 || i + len > n) return 0;
        if (m == 0xDB) {
            int p = i + 2;
            while (p < i + len) {
                int pq = d[p] >> 4, tq = d[p] & 15;
                if (tq > 3) return 0;
                p++;
                for (k = 0; k < 64; k++) {
                    if (pq) { if (p + 2 > i + len) return 0; qt[tq][mi_zz[k]] = (d[p] << 8) | d[p + 1]; p += 2; }
                    else { if (p + 1 > i + len) return 0; qt[tq][mi_zz[k]] = d[p]; p++; }
                }
            }
        } else if (m == 0xC4) {
            int p = i + 2;
            while (p + 17 <= i + len) {
                int tc = d[p] >> 4, th = d[p] & 15, total = 0; MHuff *hf;
                if (th > 3 || tc > 1) return 0;
                hf = tc ? &hac[th] : &hdc[th];
                for (k = 1; k <= 16; k++) { hf->bits[k] = d[p + k]; total += d[p + k]; }
                p += 17;
                if (total > 256 || p + total > i + len) return 0;
                for (k = 0; k < total; k++) hf->val[k] = d[p + k];
                p += total; mi_hbuild(hf);
            }
        } else if (m == 0xDD) { if (len < 4) return 0; ri = (d[i + 2] << 8) | d[i + 3]; }
        else if (m == 0xEE && len >= 14 && d[i + 2] == 'A' && d[i + 3] == 'd' && d[i + 4] == 'o') adobe = d[i + 13];
        else if (m == 0xC0 || m == 0xC1) {
            int p = i + 2;
            if (d[p] != 8) return 0;
            nc = d[p + 5]; p += 6;
            if ((nc != 1 && nc != 3) || len < 8 + nc * 3) return 0;
            for (k = 0; k < nc; k++) {
                cp[k].id = d[p]; cp[k].h = d[p + 1] >> 4; cp[k].v = d[p + 1] & 15; cp[k].tq = d[p + 2] & 3; p += 3;
                if (cp[k].h < 1 || cp[k].h > 4 || cp[k].v < 1 || cp[k].v > 4) return 0;
                if (cp[k].h > hmax) hmax = cp[k].h;
                if (cp[k].v > vmax) vmax = cp[k].v;
            }
        } else if (m == 0xDA) {
            int ns = d[i + 2], p = i + 3; MJBits b; int rst = 0, row, col;
            if (!nc || ns != nc) return 0;                 /* single interleaved scan only */
            for (k = 0; k < ns; k++) {
                for (c = 0; c < nc; c++) if (cp[c].id == d[p]) break;
                if (c == nc) return 0;
                cp[c].td = d[p + 1] >> 4; cp[c].ta = d[p + 1] & 15; p += 2;
                if (cp[c].td > 3 || cp[c].ta > 3 || !hdc[cp[c].td].ok || !hac[cp[c].ta].ok) return 0;
            }
            mcux = (w + 8 * hmax - 1) / (8 * hmax); mcuy = (h + 8 * vmax - 1) / (8 * vmax);
            for (k = 0; k < nc; k++) {
                cp[k].pw = mcux * cp[k].h * 8; cp[k].ph = mcuy * cp[k].v * 8; cp[k].pred = 0;
                if (used + (long long)cp[k].pw * cp[k].ph > MI_SCRATCH) return 0;
                cp[k].pl = mi_scr + used; used += (long long)cp[k].pw * cp[k].ph;
            }
            b.p = d; b.n = n; b.i = i + len; b.cnt = 0; b.eof = 0;
            for (my = 0; my < mcuy; my++) for (mx = 0; mx < mcux; mx++) {
                if (ri && rst == ri) {                      /* restart: realign, skip the RSTn marker */
                    b.cnt = 0; b.eof = 0;
                    while (b.i + 1 < n && !(d[b.i] == 0xFF && d[b.i + 1] >= 0xD0 && d[b.i + 1] <= 0xD7)) b.i++;
                    b.i += 2; rst = 0;
                    for (k = 0; k < nc; k++) cp[k].pred = 0;
                }
                for (k = 0; k < nc; k++) for (row = 0; row < cp[k].v; row++) for (col = 0; col < cp[k].h; col++) {
                    int blk[64], t, s, z, v;
                    for (z = 0; z < 64; z++) blk[z] = 0;
                    t = mi_jdec(&b, &hdc[cp[k].td]);
                    if (t < 0 || t > 11) return 0;
                    cp[k].pred += mi_jext(mi_jrecv(&b, t), t);
                    blk[0] = cp[k].pred * qt[cp[k].tq][0];
                    for (z = 1; z < 64;) {
                        int rs = mi_jdec(&b, &hac[cp[k].ta]);
                        if (rs < 0) return 0;
                        s = rs & 15; v = rs >> 4;
                        if (!s) { if (v == 15) { z += 16; continue; } break; }
                        z += v; if (z > 63) return 0;
                        blk[mi_zz[z]] = mi_jext(mi_jrecv(&b, s), s) * qt[cp[k].tq][mi_zz[z]];
                        z++;
                    }
                    if (b.eof && b.i >= n) return 0;
                    mi_idct(blk, cp[k].pl + (long long)(my * cp[k].v + row) * 8 * cp[k].pw + (long long)(mx * cp[k].h + col) * 8, cp[k].pw);
                }
                rst++;
            }
            {   /* upsample (nearest) + colour convert */
                int x, y;
                for (y = 0; y < h; y++) for (x = 0; x < w; x++) {
                    int comp[3];
                    for (k = 0; k < nc; k++) {
                        if (cp[k].h == hmax && cp[k].v == vmax) comp[k] = cp[k].pl[(long long)y * cp[k].pw + x];
                        else {   /* chroma is subsampled: bilinear ("fancy") upsampling, like libjpeg */
                            float fx = ((float)x + 0.5f) * (float)cp[k].h / (float)hmax - 0.5f, fy = ((float)y + 0.5f) * (float)cp[k].v / (float)vmax - 0.5f, wx, wy;
                            int x0 = (int)fx, y0 = (int)fy, x1, y1, cw = (w * cp[k].h + hmax - 1) / hmax, ch = (h * cp[k].v + vmax - 1) / vmax;
                            if (fx < 0) { x0 = 0; fx = 0; }
                            if (fy < 0) { y0 = 0; fy = 0; }
                            wx = fx - (float)x0; wy = fy - (float)y0; x1 = x0 + 1; y1 = y0 + 1;
                            if (x0 >= cw) x0 = cw - 1;
                            if (x1 >= cw) x1 = cw - 1;
                            if (y0 >= ch) y0 = ch - 1;
                            if (y1 >= ch) y1 = ch - 1;
                            comp[k] = (int)((1 - wy) * ((1 - wx) * (float)cp[k].pl[(long long)y0 * cp[k].pw + x0] + wx * (float)cp[k].pl[(long long)y0 * cp[k].pw + x1]) +
                                            wy * ((1 - wx) * (float)cp[k].pl[(long long)y1 * cp[k].pw + x0] + wx * (float)cp[k].pl[(long long)y1 * cp[k].pw + x1]) + 0.5f);
                        }
                    }
                    if (nc == 1) out[(long long)y * w + x] = 0xFF000000u | ((unsigned)comp[0] << 16) | ((unsigned)comp[0] << 8) | (unsigned)comp[0];
                    else {
                        float Y = (float)comp[0], cb = (float)comp[1] - 128.0f, cr = (float)comp[2] - 128.0f;
                        int r, g, bl;
                        if (adobe == 0) { r = comp[0]; g = comp[1]; bl = comp[2]; }     /* Adobe transform 0: plain RGB */
                        else { r = (int)(Y + 1.402f * cr + 0.5f); g = (int)(Y - 0.344136f * cb - 0.714136f * cr + 0.5f); bl = (int)(Y + 1.772f * cb + 0.5f); }
                        r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; bl = bl < 0 ? 0 : bl > 255 ? 255 : bl;
                        out[(long long)y * w + x] = 0xFF000000u | ((unsigned)r << 16) | ((unsigned)g << 8) | (unsigned)bl;
                    }
                }
            }
            return 1;
        }
        i += len;
    }
}

/* ---------------------------------------------------------------- front end */
/* 1 PNG, 2 JPEG, 3 GIF, 4 BMP; 0 = unknown/unsupported. Fills *w,*h. */
static int mi_probe(const mu8 *d, int n, int *w, int *h) {
    int dummy, dd, ct, il, prog;
    if (n >= 8 && d[0] == 137 && d[1] == 'P') return mi_png_hdr(d, n, w, h, &dd, &ct, &il) ? 1 : 0;
    if (n >= 3 && d[0] == 0xFF && d[1] == 0xD8) { if (!mi_jpg_hdr(d, n, w, h, &prog) || prog) return 0; return 2; }
    if (n >= 6 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F') return mi_gif_hdr(d, n, w, h) ? 3 : 0;
    if (n >= 2 && d[0] == 'B' && d[1] == 'M') return mi_bmp_hdr(d, n, w, h) ? 4 : 0;
    (void)dummy;
    return 0;
}
static int mi_decode(int fmt, const mu8 *d, int n, unsigned *out, int w, int h) {
    switch (fmt) {
    case 1: return mi_png(d, n, out, w, h);
    case 2: return mi_jpg(d, n, out, w, h);
    case 3: return mi_gif(d, n, out, w, h);
    case 4: return mi_bmp(d, n, out, w, h);
    }
    return 0;
}
#endif

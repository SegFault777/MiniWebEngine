/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine: text selection (new in v0.13).
 * Page text: drag to select, double-click = word, triple-click = line, Shift+arrows extend, Ctrl+A selects everything. The
 * selection is a pair of positions (text run number `ord`, byte offset in that run); it is drawn as a highlight behind the glyphs and
 * mwe_sel_text() returns the selected text as UTF-8 (line breaks where the text wraps onto a new line or a new block).
 * Form fields (<input>, <textarea>) have their own selection (anchor + caret, see mwe_forms.h): drag, Shift+arrows, Ctrl+A,
 * typing replaces it, mwe_field_text() returns it for copy/cut.
 * Selection follows what is displayed (collapsed white space, text-transform applied), not the raw HTML source.
 * Not done: selecting inside <svg>, across iframes, bidirectional text reordering, selection of images, drag-and-drop of selected text. */

static int sela_o = -1, sela_i, self_o = -1, self_i;   /* anchor / focus: run ord (-1 = none), byte offset */
static int selgran, selpa_o, selpa_i, selpb_o, selpb_i; /* drag granularity (0 char, 1 word, 2 line) and the initial word/line */
static char *selbuf;
#define SELBUF (1 << 18)

static int sel_isfield(Cmd *c) { return c->on && c->on->gen == 3; }
static Cmd *sel_run(int ord) {
    int i;
    for (i = 0; i < nc; i++) if (cm[i].k == 1 && cm[i].ord == ord && cm[i].n > 0 && cm[i].t) return &cm[i];
    return 0;
}
static int sel_maxord(void) { int i, m = 0; for (i = 0; i < nc; i++) if (cm[i].k == 1 && cm[i].ord > m) m = cm[i].ord; return m; }
static int sel_cmp(int ao, int ai, int bo, int bi) { return ao != bo ? (ao < bo ? -1 : 1) : (ai < bi ? -1 : ai > bi ? 1 : 0); }
static int sel_active(void) { return sela_o >= 0 && self_o >= 0 && sel_cmp(sela_o, sela_i, self_o, self_i) != 0; }
void mwe_sel_clear(void) { sela_o = self_o = -1; }
int mwe_sel_has_caret(void) { return self_o >= 0; }
int mwe_sel_active(void) { return sel_active(); }

/* the text run and byte offset under a document point (nearest run when the point is between lines) */
static int sel_hit(int x, int y, int *ord, int *idx) {
    int i, best = -1, bestd = 1 << 30;
    for (i = 0; i < nc; i++) {
        Cmd *c = &cm[i];
        int dy, dx, d;
        if (c->k != 1 || !c->ord || c->n <= 0 || !c->t || sel_isfield(c) || c->w <= 0) continue;
        dy = y < c->y ? c->y - y : y >= c->y + c->h ? y - (c->y + c->h - 1) : 0;
        dx = x < c->x ? c->x - x : x > c->x + c->w ? x - (c->x + c->w) : 0;
        d = dy * 4096 + dx;
        if (d < bestd || (d == bestd && c->ord > cm[best].ord)) { bestd = d; best = i; }
    }
    if (best < 0) return 0;
    {
        Cmd *c = &cm[best];
        int j = 0, prev = 0;
        *ord = c->ord;
        if (x <= c->x) { *idx = 0; return 1; }
        while (j < c->n) {
            int q = j, w0, w1;
            u8d(c->t, c->n, &q);
            w0 = wid(c->t, j, c->fs, c->tf, c->ls); w1 = wid(c->t, q, c->fs, c->tf, c->ls);
            if (c->x + (w0 + w1) / 2 > x) break;
            prev = q; j = q;
        }
        *idx = j < c->n ? j : c->n;
        (void)prev;
    }
    return 1;
}
static int sel_class(int cp) { return cp == ' ' || cp == '\t' || cp == '\n' || cp == 0xA0 ? 0 : (cp < 128 && !(isal(cp) || cp == '_')) ? 2 : 1; }
static void sel_word(int ord, int idx, int *so, int *si, int *eo, int *ei) {   /* word around a position, within one run */
    Cmd *c = sel_run(ord);
    int a, b, cls, q;
    *so = *eo = ord; *si = *ei = idx;
    if (!c) return;
    a = b = idx > c->n ? c->n : idx;
    if (b < c->n) { q = b; cls = sel_class(u8d(c->t, c->n, &q)); }
    else if (a > 0) { int p = a - 1; while (p > 0 && ((unsigned char)c->t[p] & 0xC0) == 0x80) p--; q = p; cls = sel_class(u8d(c->t, c->n, &q)); }
    else return;
    while (a > 0) { int p = a - 1, t2; while (p > 0 && ((unsigned char)c->t[p] & 0xC0) == 0x80) p--; t2 = p; if (sel_class(u8d(c->t, c->n, &t2)) != cls) break; a = p; }
    while (b < c->n) { q = b; if (sel_class(u8d(c->t, c->n, &q)) != cls) break; b = q; }
    *si = a; *ei = b;
}
static void sel_line(int ord, int *so, int *si, int *eo, int *ei) {   /* the visual line: every run overlapping this run's row */
    Cmd *c = sel_run(ord);
    int i, lo = ord, hi = ord;
    *so = *eo = ord; *si = 0; *ei = 0;
    if (!c) return;
    for (i = 0; i < nc; i++) {
        Cmd *d = &cm[i];
        if (d->k != 1 || !d->ord || d->n <= 0 || !d->t || sel_isfield(d)) continue;
        if (d->y < c->y + c->h - 1 && d->y + d->h - 1 > c->y) { if (d->ord < lo) lo = d->ord; if (d->ord > hi) hi = d->ord; }
    }
    *so = lo; *si = 0; *eo = hi; { Cmd *l = sel_run(hi); *ei = l ? l->n : 0; }
}
static void sel_extend_gran(int x, int y) {   /* drag: move the focus, keeping the initial word/line inside the selection */
    int o, i, so, si, eo, ei;
    if (!sel_hit(x, y, &o, &i)) return;
    if (selgran == 0) { self_o = o; self_i = i; return; }
    if (selgran == 1) sel_word(o, i, &so, &si, &eo, &ei); else { sel_line(o, &so, &si, &eo, &ei); }
    if (sel_cmp(o, i, selpa_o, selpa_i) < 0) { sela_o = selpb_o; sela_i = selpb_i; self_o = so; self_i = si; }
    else { sela_o = selpa_o; sela_i = selpa_i; self_o = eo; self_i = ei; }
}
/* mouse down at a document point: clicks 1 = place the caret (collapsed selection), 2 = word, 3 = line */
void mwe_sel_begin(int x, int y, int clicks) {
    int o, i, so, si, eo, ei;
    if (!sel_hit(x, y, &o, &i)) { mwe_sel_clear(); return; }
    selgran = clicks >= 3 ? 2 : clicks == 2 ? 1 : 0;
    if (selgran == 0) { sela_o = self_o = o; sela_i = self_i = i; selpa_o = selpb_o = o; selpa_i = selpb_i = i; return; }
    if (selgran == 1) sel_word(o, i, &so, &si, &eo, &ei); else sel_line(o, &so, &si, &eo, &ei);
    sela_o = selpa_o = so; sela_i = selpa_i = si; self_o = selpb_o = eo; self_i = selpb_i = ei;
}
void mwe_sel_drag(int x, int y) { if (sela_o >= 0) sel_extend_gran(x, y); }
void mwe_sel_all(void) {
    int last = sel_maxord(), i, first = 1 << 30;
    Cmd *l;
    for (i = 0; i < nc; i++) if (cm[i].k == 1 && cm[i].ord && cm[i].n > 0 && cm[i].t && !sel_isfield(&cm[i]) && cm[i].ord < first) first = cm[i].ord;
    if (first == 1 << 30) { mwe_sel_clear(); return; }
    while (last > first && (!(l = sel_run(last)) || sel_isfield(l))) last--;
    l = sel_run(last);
    sela_o = first; sela_i = 0; self_o = last; self_i = l ? l->n : 0;
}

/* selected page text as UTF-8; *len gets its length */
const char *mwe_sel_text(int *len) {
    int ao = sela_o, ai = sela_i, bo = self_o, bi = self_i, i, o = 0, pend = -1, pey = 0, peh = 0, pex = 0;
    *len = 0;
    if (!sel_active()) return "";
    if (!selbuf && !(selbuf = (char *)mem_get(SELBUF))) return "";
    if (sel_cmp(ao, ai, bo, bi) > 0) { int t = ao; ao = bo; bo = t; t = ai; ai = bi; bi = t; }
    for (i = ao; i <= bo; i++) {
        Cmd *c = sel_run(i);
        int s, e, k;
        if (!c || sel_isfield(c)) continue;
        s = i == ao ? ai : 0; e = i == bo ? bi : c->n;
        if (s > c->n) s = c->n;
        if (e > c->n) e = c->n;
        if (e <= s) continue;
        if (pend >= 0) {   /* between runs: a new line when this run sits below the previous one */
            if (c->y >= pey + peh - 2 && c->y + c->h > pey + peh + 1) { if (o < SELBUF - 2) selbuf[o++] = '\n'; if (c->y > pey + peh + peh / 2 + 2 && o < SELBUF - 2) selbuf[o++] = '\n'; }
            else if (c->x - pex >= 1 && o < SELBUF - 2 && selbuf[o - 1] != ' ' && selbuf[o - 1] != '\n') selbuf[o++] = ' ';   /* words are separate runs: the gap between them is a space */
        }
        for (k = s; k < e && o < SELBUF - 1; k++) selbuf[o++] = c->t[k];
        pend = i; pey = c->y; peh = c->h; pex = c->x + c->w;
    }
    selbuf[o] = 0; *len = o;
    return selbuf;
}

/* ---- keyboard on page text (no focused field): Shift+arrows extend, plain arrows collapse ---- */
static int sel_step(int *ord, int *idx, int dir) {   /* one character left (-1) / right (+1), across runs */
    Cmd *c = sel_run(*ord);
    int mo = sel_maxord();
    if (!c) return 0;
    if (dir > 0) {
        if (*idx < c->n) { int q = *idx; u8d(c->t, c->n, &q); *idx = q; return 1; }
        { int o = *ord + 1; Cmd *d = 0; while (o <= mo && !(d = sel_run(o))) o++; if (!d) return 0; *ord = o; *idx = 0; }
        return 1;
    }
    if (*idx > 0) { int p = *idx - 1; while (p > 0 && ((unsigned char)c->t[p] & 0xC0) == 0x80) p--; *idx = p; return 1; }
    { int o = *ord - 1; Cmd *d = 0; while (o >= 1 && !(d = sel_run(o))) o--; if (!d) return 0; *ord = o; *idx = d->n; }
    return 1;
}
static int sel_xpos(int ord, int idx) { Cmd *c = sel_run(ord); return c ? c->x + wid(c->t, idx > c->n ? c->n : idx, c->fs, c->tf, c->ls) : 0; }
/* mods: 1 shift, 2 ctrl. Returns 1 if the selection changed. */
int mwe_sel_key(const char *key, int mods) {
    int o, i, shift = mods & 1, moved = 0;
    if (self_o < 0) return 0;
    o = self_o; i = self_i;
    if (!strcmp_k(key, "ArrowLeft")) { if (!shift && sel_active()) { if (sel_cmp(sela_o, sela_i, self_o, self_i) < 0) { o = sela_o; i = sela_i; } moved = 1; } else moved = sel_step(&o, &i, -1); }
    else if (!strcmp_k(key, "ArrowRight")) { if (!shift && sel_active()) { if (sel_cmp(sela_o, sela_i, self_o, self_i) > 0) { o = sela_o; i = sela_i; } moved = 1; } else moved = sel_step(&o, &i, 1); }
    else if (!strcmp_k(key, "Home") || !strcmp_k(key, "End")) { int so, si, eo, ei; sel_line(o, &so, &si, &eo, &ei); if (key[0] == 'H') { o = so; i = si; } else { o = eo; i = ei; } moved = 1; }
    else if (!strcmp_k(key, "ArrowUp") || !strcmp_k(key, "ArrowDown")) {
        Cmd *c = sel_run(o);
        int x = sel_xpos(o, i), j, best = -1, bd = 1 << 30, up = key[5] == 'U', ho, hi_;
        if (c) {
            for (j = 0; j < nc; j++) {
                Cmd *d = &cm[j];
                int dist;
                if (d->k != 1 || !d->ord || d->n <= 0 || !d->t || sel_isfield(d)) continue;
                if (up ? d->y + d->h > c->y + 1 : d->y < c->y + c->h - 1) continue;
                dist = up ? c->y - d->y : d->y - c->y;
                if (dist < bd || (dist == bd && (d->x <= x && d->x + d->w >= x))) { bd = dist; best = j; }
            }
            if (best >= 0) {
                Cmd *d0 = &cm[best];
                int row_y = d0->y, bestj = -1, bx = 1 << 30;
                for (j = 0; j < nc; j++) {   /* among runs on that row, the one under x */
                    Cmd *d = &cm[j];
                    int dd;
                    if (d->k != 1 || !d->ord || d->n <= 0 || !d->t || sel_isfield(d) || d->y != row_y) continue;
                    dd = x < d->x ? d->x - x : x > d->x + d->w ? x - d->x - d->w : 0;
                    if (dd < bx) { bx = dd; bestj = j; }
                }
                if (bestj >= 0 && sel_hit(x, cm[bestj].y + cm[bestj].h / 2, &ho, &hi_)) { o = ho; i = hi_; moved = 1; }
            }
        }
    }
    if (!moved) return 0;
    self_o = o; self_i = i;
    if (!shift) { sela_o = o; sela_i = i; }
    return 1;
}
int mwe_sel_focus_y(void) { Cmd *c = sel_run(self_o); return c ? c->y : -1; }   /* document y of the focus (for scrolling it into view) */

/* ---- painting: highlight behind the glyphs of one text run ---- */
#define SELCOL 0xB3D4FC
static void sel_hl(Cmd *k) {
    int s = -1, e = -1, more = 0;
    if (k->n <= 0 || !k->t) return;
    if (k->ord && sel_active() && !sel_isfield(k)) {
        int ao = sela_o, ai = sela_i, bo = self_o, bi = self_i;
        if (sel_cmp(ao, ai, bo, bi) > 0) { int t = ao; ao = bo; bo = t; t = ai; ai = bi; bi = t; }
        if (k->ord < ao || k->ord > bo) return;
        s = k->ord == ao ? ai : 0; e = k->ord == bo ? bi : k->n;
        more = k->ord < bo;
    } else if (focn && k->on && k->on->gen == 3 && k->on->par == focn && fanc >= 0 && fanc != focpos) {   /* selection inside the focused field */
        S v = fget(focn);
        int a = fanc < focpos ? fanc : focpos, b = fanc < focpos ? focpos : fanc, off = fdisp_off(focn, k);
        if (fkind(focn) == 1 && seq(getattr(focn, "type"), "password")) { a = fcps(v, a) * 3; b = fcps(v, b) * 3; }
        if (off < 0) return;
        s = a - off; e = b - off;
        more = e > k->n;
        if (s < 0) s = 0;
        if (e > k->n) e = k->n;
    }
    if (s < 0 || e <= s) return;
    if (s > k->n) return;
    {
        int x0 = k->x + wid(k->t, s, k->fs, k->tf, k->ls), x1 = x0 + wid(k->t + s, e - s, k->fs, k->tf, k->ls);
        if (more && e >= k->n) {   /* the selection goes on in the next run of this line: cover the space between the words */
            int o2 = k->ord + 1, mo = sel_maxord();
            Cmd *nx = 0;
            while (o2 <= mo && !(nx = sel_run(o2))) o2++;
            if (nx && nx->y < k->y + k->h / 2 + 1 && nx->y > k->y - k->h / 2 - 1 && nx->x > x1 && nx->x - x1 < 3 * k->fs) x1 = nx->x;
        }
        fr(x0, k->y, x1 - x0, k->h, SELCOL);
    }
}

/* ---- mouse: one entry point for page text and form fields (document coordinates) ---- */
static int msel_mode;   /* 0 idle, 1 dragging inside a field, 2 dragging page text */
/* button pressed. clicks = 1, 2 (word) or 3 (line / whole field). Returns 1 if the point is in an editable field, else 0. */
int mwe_mouse_down(int x, int y, int clicks) {
    if (fat(x, y)) {
        mwe_sel_clear();
        mwe_field_mouse(x, y, 0);
        if (clicks == 2) mwe_field_mouse(x, y, 2);
        else if (clicks >= 3) mwe_field_select_all();
        msel_mode = 1;
        return 1;
    }
    if (focn) focus_set(0);
    msel_mode = 2;
    mwe_sel_begin(x, y, clicks);
    return 0;
}
int mwe_mouse_drag(int x, int y) { if (msel_mode == 1) { mwe_field_mouse(x, y, 1); return 1; } if (msel_mode == 2) mwe_sel_drag(x, y); return 0; }   /* 1: a field changed (needs a new layout) */
void mwe_mouse_up(void) { msel_mode = 0; }
/* the selected text to copy: the focused field's selection if it has one, else the page selection; UTF-8 */
const char *mwe_selection(int *len) {
    const char *t = mwe_field_text(len);
    if (*len) return t;
    return mwe_sel_text(len);
}

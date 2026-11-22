/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine JavaScript module: Promise, microtask queue, async/await, generators, Symbol.
 * Included by mwe_js.h (needs the interpreter's types and helpers).
 *
 * Semantics worth knowing (see README, "JavaScript"):
 *  - Promise follows the spec (then/catch/finally, all/allSettled/race/any, thenable adoption, microtask
 *    ordering). Microtasks run when a host entry (script, event, timer) finishes.
 *  - The interpreter is a tree walker without coroutines, so `await` and generators cannot suspend:
 *      * `await p` runs the microtask queue and the timers until p settles, then continues. Results and
 *        errors are right; the *interleaving* with code after the async call differs from browsers
 *        (the async function runs to completion before the call returns).
 *      * a generator body runs eagerly at the first next()/iteration and its yielded values are buffered;
 *        values sent with next(v) are ignored and infinite generators stop with a RangeError after 100000 values.
 *  - Symbols are unique strings that start with "\1@" (typeof says "symbol", they are hidden from enumeration). */
#ifndef MWE_JS_ASYNC_H
#define MWE_JS_ASYNC_H

#define JPF 32                 /* JOb.fl: promise */
#define JGF 64                 /* JOb.fl: generator object */
#define JPF_HANDLED 128        /* JOb.fl: a reaction was attached (unhandled-rejection report) */
#define JGENMAX 100000

static JOb *jmicroq, *jPromProto, *jGenProto, *junh;
static int jmhead, jsymn;

static JOb *jpnew(void) { JOb *p = jnewob(OK_PLAIN, jPromProto); p->fl |= JPF; p->pad = 0; p->ex = jundef(); return p; }
static int jispromise(JV v) { return v.t == JT_O && (v.u.o->fl & JPF); }
static void jmicropush(JV h, JV v, JV child, int mode) { jarrpush(jmicroq, h); jarrpush(jmicroq, v); jarrpush(jmicroq, child); jarrpush(jmicroq, jnum(mode)); }

static JV jpresolve(JOb *p, JV x);
static void jpsettle(JOb *p, int st, JV v) {
    int i;
    if (p->pad) return;
    p->pad = (short)st; p->ex = v;
    for (i = 0; i + 2 < p->na; i += 3) jmicropush(st == 1 ? p->it[i] : p->it[i + 1], v, p->it[i + 2], st == 1 ? 0 : 1);
    p->na = 0;
    if (st == 2 && !(p->fl & JPF_HANDLED)) jarrpush(junh, jobj(p));
}

/* resolving functions: native function objects with env = promise, ex = shared "already resolved" flag object, pad = 0 resolve / 1 reject */
JNAT(jf_pres) {
    JOb *self = jcurnat, *p, *fl;
    if (!self) return jundef();
    p = self->env; fl = self->ex.u.o;
    if (fl->pad) return jundef();
    fl->pad = 1;
    if (self->pad) jpsettle(p, 2, JARG(0)); else jpresolve(p, JARG(0));
    return jundef();
}
static JOb *jmkres(JOb *p, JOb *fl, int which) { JOb *f = jmknat(0, jf_pres); f->env = p; f->ex = jobj(fl); f->pad = (short)which; return f; }

static JV jpresolve(JOb *p, JV x) {
    if (p->pad) return jundef();
    if (x.t == JT_O) {
        JV th;
        if (x.u.o == p) { JV e; jthrowerr(jErrT, "Chaining cycle detected for promise"); e = jexv; jthrow = 0; jpsettle(p, 2, e); return jundef(); }
        th = jget(x, jintc("then"));
        if (jthrow) { JV e = jexv; jthrow = 0; jpsettle(p, 2, e); return jundef(); }
        if (jiscallable(th)) { jmicropush(x, th, jobj(p), 2); return jundef(); }   /* PromiseResolveThenableJob */
    }
    jpsettle(p, 1, x);
    return jundef();
}

static int jruntimers(j64 now);
static void jreportexc(const char *what) { (void)what; jthrow = 1; jreport(); }

static int jmicrostep(void) {
    JV h, v, child;
    int mode;
    if (jmhead + 3 >= jmicroq->na) { jmicroq->na = 0; jmhead = 0; return 0; }
    h = jmicroq->it[jmhead]; v = jmicroq->it[jmhead + 1]; child = jmicroq->it[jmhead + 2]; mode = (int)jmicroq->it[jmhead + 3].u.d;
    jmhead += 4;
    if (mode <= 1) {                                   /* then-reaction */
        JOb *c = child.t == JT_O ? child.u.o : 0;
        if (jiscallable(h)) {
            JV r = jcall(h, jundef(), &v, 1);
            if (jthrow) { JV e = jexv; jthrow = 0; if (c) jpsettle(c, 2, e); else { jexv = e; jreportexc("Uncaught (in promise): "); } }
            else if (c) jpresolve(c, r);
        } else if (c) { if (mode == 0) jpresolve(c, v); else jpsettle(c, 2, v); }
    } else if (mode == 2) {                            /* adopt a thenable */
        JOb *c = child.u.o, *fl = jnewob(OK_PLAIN, 0);
        JV a[2];
        a[0] = jobj(jmkres(c, fl, 0)); a[1] = jobj(jmkres(c, fl, 1));
        jcall(v, h, a, 2);
        if (jthrow) { JV e = jexv; jthrow = 0; if (!fl->pad) { fl->pad = 1; jpsettle(c, 2, e); } }
    } else {                                           /* queueMicrotask */
        jcall(h, jundef(), 0, 0);
        if (jthrow) jreportexc("Uncaught: ");
    }
    return 1;
}
static void jmicrorun(void) {
    int i;
    if (!jmicroq) return;
    while (!jabort && jmicrostep()) {}
    for (i = 0; i < junh->na; i++) {                   /* rejections nobody handled */
        JOb *p = junh->it[i].u.o;
        if (!(p->fl & JPF_HANDLED)) { jexv = p->ex; jthrow = 1; jreport(); }
    }
    junh->na = 0;
}

/* run the earliest pending timer (virtual time), used while awaiting */
static int jtimerstep(void) {
    int i, best = -1;
    JTm t;
    for (i = 0; i < jnt; i++) if (jtm[i].live && (best < 0 || jtm[i].due < jtm[best].due)) best = i;
    if (best < 0) return 0;
    t = jtm[best];
    if (t.due > jnowmono) jnowmono = t.due;
    if (t.iv) jtm[best].due = jnowmono + t.iv; else jtm[best].live = 0;
    {
        JV av[3]; int k;
        for (k = 0; k < 3; k++) av[k] = t.a[k];
        jcall(t.fn, jobj(jGlob), av, t.na);
        if (jthrow && !jabort) jreportexc("Uncaught: ");
    }
    return 1;
}

static JV jawait(JV v) {
    JOb *p;
    int spins = 0;
    if (jispromise(v)) p = v.u.o;
    else if (v.t == JT_O) { p = jpnew(); jpresolve(p, v); if (jthrow) return jundef(); }
    else return v;
    p->fl |= JPF_HANDLED;
    {
        long long t0 = jnowmono;
        while (p->pad == 0 && !jabort) {
            if (jmicrostep()) continue;
            if (jpollcb) {   /* async host: let the network make progress (and nothing else) while this await waits */
                long long now = jnowmono;
                int out = jpollcb(&now);
                jnowmono = now;
                jruntimers(now);
                if (p->pad != 0 || jmhead + 3 < jmicroq->na) continue;
                if (out > 0) {
                    if (now - t0 > 120000) return jthrowerr(jErrT, "await: gave up waiting for the network");
                    continue;
                }
            }
            if (!jtimerstep()) return jthrowerr(jErrT, "await: the promise can never settle (no pending timers or tasks)");
            if (++spins > 20000) return jthrowerr(jErrT, "await: gave up waiting for a promise");
        }
    }
    if (jabort) return jundef();
    if (p->pad == 2) { jexv = p->ex; jthrow = 1; return jundef(); }
    return p->ex;
}

/* ---- Promise API ---- */
JNAT(jf_Promise) {
    JOb *p, *fl;
    JV ex = JARG(0), a[2];
    if (!jnewcall || th.t != JT_O) return jthrowerr(jErrT, "Promise constructor cannot be invoked without 'new'");
    if (!jiscallable(ex)) return jthrowerr(jErrT, "Promise resolver is not a function");
    p = th.u.o; p->fl |= JPF; p->pad = 0; p->ex = jundef();
    fl = jnewob(OK_PLAIN, 0);
    a[0] = jobj(jmkres(p, fl, 0)); a[1] = jobj(jmkres(p, fl, 1));
    jcall(ex, jundef(), a, 2);
    if (jthrow) { JV e = jexv; jthrow = 0; if (!fl->pad) { fl->pad = 1; jpsettle(p, 2, e); } }
    return th;
}
static JV jthen(JOb *p, JV onF, JV onR) {
    JOb *child = jpnew();
    p->fl |= JPF_HANDLED;
    if (p->pad == 0) { jarrpush(p, onF); jarrpush(p, onR); jarrpush(p, jobj(child)); }
    else jmicropush(p->pad == 1 ? onF : onR, p->ex, jobj(child), p->pad == 1 ? 0 : 1);
    return jobj(child);
}
JNAT(jp_then) {
    if (!jispromise(th)) return jthrowerr(jErrT, "Promise.prototype.then called on a non-promise");
    return jthen(th.u.o, JARG(0), JARG(1));
}
JNAT(jp_catch) {
    if (!jispromise(th)) return jthrowerr(jErrT, "Promise.prototype.catch called on a non-promise");
    return jthen(th.u.o, jundef(), JARG(0));
}
JNAT(jf_finok) { JOb *self = jcurnat; JV v = JARG(0); jcall(self->ex, jundef(), 0, 0); if (jthrow) return jundef(); return v; }
JNAT(jf_finerr) { JOb *self = jcurnat; JV v = JARG(0); jcall(self->ex, jundef(), 0, 0); if (jthrow) return jundef(); jexv = v; jthrow = 1; return jundef(); }
JNAT(jp_finally) {
    JV f = JARG(0);
    JOb *a, *b;
    if (!jispromise(th)) return jthrowerr(jErrT, "Promise.prototype.finally called on a non-promise");
    if (!jiscallable(f)) return jthen(th.u.o, f, f);
    a = jmknat(0, jf_finok); a->ex = f; b = jmknat(0, jf_finerr); b->ex = f;
    return jthen(th.u.o, jobj(a), jobj(b));
}
static JOb *jtopromise(JV v) {
    JOb *p;
    if (jispromise(v)) return v.u.o;
    p = jpnew(); jpresolve(p, v); return p;
}
JNAT(jpf_resolve) { return jobj(jtopromise(JARG(0))); }
JNAT(jpf_reject) { JOb *p = jpnew(); jpsettle(p, 2, JARG(0)); return jobj(p); }

/* all / allSettled / any / race: state object s: env = results array, ex = result promise, pad = remaining; handlers carry their index in pad */
JNAT(jf_allok) {
    JOb *self = jcurnat, *s = self->env, *arr = s->env;
    arr->it[self->pad] = JARG(0);
    if (--s->pad == 0) jpresolve(s->ex.u.o, jobj(arr));
    return jundef();
}
JNAT(jf_allrej) { JOb *self = jcurnat; jpsettle(self->env, 2, JARG(0)); return jundef(); }
static JOb *jsettledobj(int ok, JV v) {
    JOb *o = jnewob(OK_PLAIN, jObjProto);
    jaddpr(o, jintc("status"), jcv(ok ? "fulfilled" : "rejected"), 0);
    jaddpr(o, jintc(ok ? "value" : "reason"), v, 0);
    return o;
}
JNAT(jf_setok) {
    JOb *self = jcurnat, *s = self->env, *arr = s->env;
    arr->it[self->pad] = jobj(jsettledobj(1, JARG(0)));
    if (--s->pad == 0) jpresolve(s->ex.u.o, jobj(arr));
    return jundef();
}
JNAT(jf_setrej) {
    JOb *self = jcurnat, *s = self->env, *arr = s->env;
    arr->it[self->pad] = jobj(jsettledobj(0, JARG(0)));
    if (--s->pad == 0) jpresolve(s->ex.u.o, jobj(arr));
    return jundef();
}
JNAT(jf_anyrej) {
    JOb *self = jcurnat, *s = self->env, *arr = s->env;
    arr->it[self->pad] = JARG(0);
    if (--s->pad == 0) {
        JV e;
        jthrowerr(jErrT, "All promises were rejected");
        e = jexv; jthrow = 0;
        jaddpr(e.u.o, jintc("errors"), jobj(arr), 1);
        jpsettle(s->ex.u.o, 2, e);
    }
    return jundef();
}
JNAT(jf_resolveres) { JOb *self = jcurnat; jpresolve(self->env, JARG(0)); return jundef(); }
/* kind: 0 all, 1 allSettled, 2 any, 3 race */
static JV jcombinator(JV iter, int kind) {
    JOb *src = jiterable(iter), *res = jpnew(), *s, *arr;
    int i, n;
    if (!src) { jpsettle(res, 2, jcv("object is not iterable")); return jobj(res); }
    n = src->na;
    s = jnewob(OK_PLAIN, 0); arr = jnewarr(n > 0 ? n : 1);
    for (i = 0; i < n; i++) jarrpush(arr, jundef());
    s->env = arr; s->ex = jobj(res); s->pad = (short)n;
    if (n == 0 && kind != 3) {
        if (kind == 2) { JV e; jthrowerr(jErrT, "All promises were rejected"); e = jexv; jthrow = 0; jpsettle(res, 2, e); }
        else jpresolve(res, jobj(arr));
        return jobj(res);
    }
    for (i = 0; i < n; i++) {
        JOb *pe = jtopromise(src->it[i]), *h, *r;
        JV hf, hr;
        switch (kind) {
        case 0: h = jmknat(0, jf_allok); h->pad = (short)i; h->env = s; r = jmknat(0, jf_allrej); r->env = res; break;
        case 1: h = jmknat(0, jf_setok); h->pad = (short)i; h->env = s; r = jmknat(0, jf_setrej); r->env = s; r->pad = (short)i; break;
        case 2: h = jmknat(0, jf_resolveres); h->env = res; r = jmknat(0, jf_anyrej); r->pad = (short)i; r->env = s; break;
        default: h = jmknat(0, jf_resolveres); h->env = res; r = jmknat(0, jf_allrej); r->env = res; break;
        }
        hf = jobj(h); hr = jobj(r);
        jthen(pe, hf, hr);
    }
    return jobj(res);
}
JNAT(jpf_all) { return jcombinator(JARG(0), 0); }
JNAT(jpf_allSettled) { return jcombinator(JARG(0), 1); }
JNAT(jpf_any) { return jcombinator(JARG(0), 2); }
JNAT(jpf_race) { return jcombinator(JARG(0), 3); }
JNAT(jw_queueMicrotask) { if (!jiscallable(JARG(0))) return jthrowerr(jErrT, "queueMicrotask: argument is not a function"); jmicropush(JARG(0), jundef(), jundef(), 3); return jundef(); }

/* ---- generators (eager) ---- */
static JOb *jgencur;
static JV jiterres(JV value, int done) {
    JOb *o = jnewob(OK_PLAIN, jObjProto);
    jaddpr(o, jintc("value"), value, 0);
    jaddpr(o, jintc("done"), jbool(done), 0);
    return jobj(o);
}
static JV jmkgen(JOb *fo, JV thisv, JV *args, int argc) {
    JOb *g = jnewob(OK_PLAIN, jGenProto), *av = jnewarr(argc > 0 ? argc : 1);
    int i;
    g->fl |= JGF; g->pad = 0; g->env = fo;
    for (i = 0; i < argc; i++) jarrpush(av, args[i]);
    jaddpr(g, jintc("%this"), thisv, 1);
    jaddpr(g, jintc("%args"), jobj(av), 1);
    jaddpr(g, jintc("%pos"), jnum(0), 1);
    jaddpr(g, jintc("%ret"), jundef(), 1);
    return jobj(g);
}
static JV jcallfn0(JOb *fo, JV thisv, JV *args, int argc, int isnew);
static void jgenrun(JOb *g) {
    JOb *saved, *av;
    JV thisv, r;
    if (g->pad) return;
    g->pad = 1;
    thisv = jget(jobj(g), jintc("%this")); av = jget(jobj(g), jintc("%args")).u.o;
    saved = jgencur; jgencur = g;
    r = jcallfn0(g->env, thisv, av->it, av->na, 0);
    jgencur = saved;
    if (!jthrow) jput(jobj(g), jintc("%ret"), r);
}
static JV jgennext(JOb *g) {
    int pos;
    jgenrun(g);
    if (jthrow) return jundef();
    pos = (int)jtonum(jget(jobj(g), jintc("%pos")));
    if (pos < g->na) { jput(jobj(g), jintc("%pos"), jnum(pos + 1)); return jiterres(g->it[pos], 0); }
    jput(jobj(g), jintc("%pos"), jnum(g->na + 1));                 /* the return value is delivered once */
    return jiterres(pos == g->na ? jget(jobj(g), jintc("%ret")) : jundef(), 1);
}
JNAT(jg_next) { if (th.t != JT_O || !(th.u.o->fl & JGF)) return jthrowerr(jErrT, "next method called on incompatible receiver"); return jgennext(th.u.o); }
JNAT(jg_return) {
    if (th.t != JT_O || !(th.u.o->fl & JGF)) return jthrowerr(jErrT, "return method called on incompatible receiver");
    th.u.o->pad = 1; jput(th, jintc("%pos"), jnum(th.u.o->na));
    return jiterres(JARG(0), 1);
}
JNAT(jg_throw) { jexv = JARG(0); jthrow = 1; return jundef(); }
JNAT(jg_iter) { return th; }
static JV jyieldeval(JNd *n, JOb *env) {
    JV v = n->a ? jev(n->a, env) : jundef();
    if (jthrow) return jundef();
    if (!jgencur) return jthrowerr(jErrT, "yield outside of a generator");
    if (n->x) {                                         /* yield* */
        JOb *arr = jiterable(v);
        int i;
        if (jthrow) return jundef();
        if (!arr) return jthrowerr(jErrT, "yield* operand is not iterable");
        for (i = 0; i < arr->na; i++) jarrpush(jgencur, arr->it[i]);
        return jundef();
    }
    if (jgencur->na >= JGENMAX) return jthrowerr(jErrR, "generator produced too many values (generators run eagerly in this engine)");
    jarrpush(jgencur, v);
    return jundef();
}

/* custom iterables: [Symbol.iterator]() / [Symbol.asyncIterator]() protocols, collected eagerly */
static JOb *jiterfrom(JV v) {
    JOb *r;
    JV f, it, nx, res;
    int async = 0, n = 0;
    if (v.t != JT_O) return 0;
    if (v.u.o->fl & JGF) { jgenrun(v.u.o); if (jthrow) return 0; return v.u.o; }
    f = jget(v, jintc("\1@iterator"));
    if (jthrow) return 0;
    if (!jiscallable(f)) { f = jget(v, jintc("\1@asyncIterator")); async = 1; if (!jiscallable(f)) return 0; }
    it = jcall(f, v, 0, 0);
    if (jthrow) return 0;
    nx = jget(it, jintc("next"));
    if (jthrow || !jiscallable(nx)) return 0;
    r = jnewarr(8);
    for (;;) {
        res = jcall(nx, it, 0, 0);
        if (jthrow) return 0;
        if (async) { res = jawait(res); if (jthrow) return 0; }
        if (res.t != JT_O) break;
        if (jtruthy(jget(res, jintc("done")))) break;
        jarrpush(r, jget(res, jintc("value")));
        if (++n >= JGENMAX) { jthrowerr(jErrR, "iterator produced too many values"); return 0; }
    }
    return r;
}

/* ---- Symbol: unique strings starting with "\1@" ---- */
JNAT(jf_Symbol) {
    char b[200];
    int n = 0, i;
    JStr *d = argc > 0 && JARG(0).t != JT_U ? jtos(JARG(0)) : &jemptys;
    if (jnewcall) return jthrowerr(jErrT, "Symbol is not a constructor");
    b[n++] = 1; b[n++] = '@';
    { int k = ++jsymn; char t[12]; int m = 0; while (k) { t[m++] = (char)('0' + k % 10); k /= 10; } while (m) b[n++] = t[--m]; }
    b[n++] = ':';
    for (i = 0; i < d->len && n < 190; i++) b[n++] = d->d[i];
    return jstrv(jnewstr(b, n));
}
JNAT(jsy_for) {
    char b[200];
    int n = 0, i;
    JStr *d = jtos(JARG(0));
    b[n++] = 1; b[n++] = '@'; b[n++] = 'f'; b[n++] = ':';
    for (i = 0; i < d->len && n < 190; i++) b[n++] = d->d[i];
    return jstrv(jnewstr(b, n));
}
static int jissym(JV v) { return v.t == JT_S && v.u.s->len > 1 && v.u.s->d[0] == 1 && v.u.s->d[1] == '@'; }

static void jinit_async(void) {
    JOb *f, *o;
    jmicroq = jnewarr(32); jmhead = 0; junh = jnewarr(4); jsymn = 0; jgencur = 0; jcurnat = 0;
    jPromProto = jnewob(OK_PLAIN, jObjProto);
    jGenProto = jnewob(OK_PLAIN, jObjProto);
    f = jctor("Promise", jf_Promise, jPromProto);
    jdef(f, "resolve", jpf_resolve); jdef(f, "reject", jpf_reject); jdef(f, "all", jpf_all); jdef(f, "allSettled", jpf_allSettled);
    jdef(f, "any", jpf_any); jdef(f, "race", jpf_race);
    jdef(jPromProto, "then", jp_then); jdef(jPromProto, "catch", jp_catch); jdef(jPromProto, "finally", jp_finally);
    jdef(jGenProto, "next", jg_next); jdef(jGenProto, "return", jg_return); jdef(jGenProto, "throw", jg_throw);
    jaddpr(jGenProto, jintc("\1@iterator"), jobj(jmknat("[Symbol.iterator]", jg_iter)), 1);
    jgset("queueMicrotask", jobj(jmknat("queueMicrotask", jw_queueMicrotask)));
    f = jmknat("Symbol", jf_Symbol); jgset("Symbol", jobj(f));
    jdef(f, "for", jsy_for);
    o = f;
    jaddpr(o, jintc("iterator"), jcv("\1@iterator"), 1); jaddpr(o, jintc("asyncIterator"), jcv("\1@asyncIterator"), 1);
    jaddpr(o, jintc("toPrimitive"), jcv("\1@toPrimitive"), 1); jaddpr(o, jintc("hasInstance"), jcv("\1@hasInstance"), 1);
    jaddpr(o, jintc("toStringTag"), jcv("\1@toStringTag"), 1); jaddpr(o, jintc("species"), jcv("\1@species"), 1);
}
#endif

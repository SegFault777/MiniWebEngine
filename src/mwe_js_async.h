/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine JavaScript module: Promise, microtask queue, async/await, generators, Symbol.
 * Included by mwe_js.h (needs the interpreter's types and helpers).
 *
 * Semantics worth knowing (see README, "JavaScript"):
 *  - Promise follows the spec (then/catch/finally, all/allSettled/race/any, thenable adoption, microtask
 *    ordering). Microtasks run when a host entry (script, event, timer) finishes.
 *  - v0.13: async functions and generators are coroutines (own C stack, see the coroutine section below), so `await`
 *    really suspends and generators are lazy, with browser-like ordering. Where coroutines are unavailable (not x86-64 System V,
 *    or more than 128 live at once) the v0.12 behaviour remains: await pumps the event loop, generators run eagerly.
 *  - Symbols are unique strings that start with "\1@" (typeof says "symbol", they are hidden from enumeration). */
#ifndef MWE_JS_ASYNC_H
#define MWE_JS_ASYNC_H

#if defined(__x86_64__) && !defined(_WIN32) && !defined(MWE_NO_COROUTINES)
#define JCO_ASM 1
#else
#define JCO_ASM 0
#endif
#define JPF 32                 /* JOb.fl: promise */
#define JGF 64                 /* JOb.fl: generator object */
#define JPF_HANDLED 128        /* JOb.fl: a reaction was attached (unhandled-rejection report) */
#define JGENMAX 100000

typedef struct JCo JCo;
static JOb *jmicroq, *jPromProto, *jGenProto, *junh;
static int jmhead, jsymn;

#define JCO_MAX 128
#define JCO_STACK (1 << 20)            /* per coroutine; committed lazily by the OS */
#define JCO_GUARD (64 << 10)           /* "Maximum call stack size exceeded" this close to the end of a stack */
enum { JCO_FREE, JCO_NEW, JCO_RUN, JCO_SUSP, JCO_DONE };
struct JCo {
    int st, kind, mode, din, wait, gcs;     /* kind: 0 async function, 1 generator, 2 async generator; mode: how it was resumed (0 value, 1 throw, 2 return) */
    void *sp;                          /* saved stack pointer while another stack runs */
    char *stk, *lo, *hi;
    JCo *res;                          /* the stack that resumed us (we switch back to it) */
    JV val, out;                       /* value passed in / value yielded or returned out */
    JOb *box, *obj;                    /* [function, this, args...] ; the promise (async fn) or generator object */
};
static JCo jcoro[JCO_MAX], jcomain;
static JCo *jcocur = &jcomain;

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

static JV jco_await(JV v);
static JV jawait_block(JV v) {
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

static JV jawait(JV v) { return jcocur != &jcomain ? jco_await(v) : jawait_block(v); }

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

/* ---- coroutines: async functions and generators run on their own C stacks (v0.13) ----
 * A coroutine is a separate C stack plus saved registers. `await` registers a reaction on the promise and switches back to
 * whoever resumed the coroutine (the caller of the async function the first time, the microtask loop later); the reaction
 * switches back in, so the code after the await continues exactly where it stopped, as a microtask, while timers, rendering and
 * input keep running in between. Generators use the same mechanism: next() resumes the body, yield suspends it.
 * The switch is a few instructions of assembly (x86-64 System V: Linux, BSD, macOS/Intel). On other CPUs / Windows the engine
 * falls back to the v0.12 behaviour (await pumps the event loop; generators run eagerly). The GC scans every coroutine stack. */

#if JCO_ASM
void mwe_jco_swap(void **save, void *to);
void mwe_jco_boot(void);
#if defined(__APPLE__)
#define JCO_SYM(n) "_" n
#else
#define JCO_SYM(n) n
#endif
__asm__(".text\n"
        ".globl " JCO_SYM("mwe_jco_swap") "\n" JCO_SYM("mwe_jco_swap") ":\n"
        "\tpushq %rbp\n\tpushq %rbx\n\tpushq %r12\n\tpushq %r13\n\tpushq %r14\n\tpushq %r15\n"
        "\tmovq %rsp, (%rdi)\n\tmovq %rsi, %rsp\n"
        "\tpopq %r15\n\tpopq %r14\n\tpopq %r13\n\tpopq %r12\n\tpopq %rbx\n\tpopq %rbp\n\tret\n"
        ".globl " JCO_SYM("mwe_jco_boot") "\n" JCO_SYM("mwe_jco_boot") ":\n"
        "\tsubq $8, %rsp\n\tmovq %r12, %rdi\n\tcall " JCO_SYM("mwe_jco_main") "\n\tud2\n");
void mwe_jco_main(JCo *c);
#endif

static void jco_release(JCo *c) {
    c->st = JCO_FREE; c->box = 0; c->obj = 0; c->val = c->out = jundef(); c->res = 0;
}
static void jco_reset(void) {          /* new page: every suspended coroutine is simply dropped */
    int i;
    jcocur = &jcomain; jstklim = 0;
    for (i = 0; i < JCO_MAX; i++) { jco_release(&jcoro[i]); mem_rel(jcoro[i].stk); jcoro[i].stk = 0; }
}
static int jco_spawn(int kind, JOb *fo, JV thisv, JV *args, int argc, JOb *obj) {
#if JCO_ASM
    int i, k;
    JCo *c;
    JOb *box;
    void **sp;
    for (i = 0; i < JCO_MAX && jcoro[i].st != JCO_FREE; i++) {}
    if (i == JCO_MAX) return -1;
    c = &jcoro[i];
    if (!c->stk) { c->stk = (char *)mem_get(JCO_STACK); if (!c->stk) return -1; }
    box = jnewarr(argc + 2);
    jarrpush(box, jobj(fo)); jarrpush(box, thisv);
    for (k = 0; k < argc; k++) jarrpush(box, args[k]);
    c->st = JCO_NEW; c->kind = kind; c->mode = 0; c->din = 0; c->wait = 0; c->box = box; c->obj = obj; c->res = 0;
    c->val = c->out = jundef(); c->lo = c->stk; c->hi = c->stk + JCO_STACK;
    sp = (void **)(((unsigned long)c->hi) & ~15UL);
    *--sp = 0;                                         /* fake return address of the boot code */
    *--sp = (void *)mwe_jco_boot;                      /* where the first switch "returns" to */
    *--sp = 0; *--sp = 0;                              /* rbp, rbx */
    *--sp = (void *)c;                                 /* r12: the coroutine, passed on to mwe_jco_main */
    *--sp = 0; *--sp = 0; *--sp = 0;                   /* r13 r14 r15 */
    c->sp = (void *)sp;
    return i;
#else
    (void)kind; (void)fo; (void)thisv; (void)args; (void)argc; (void)obj;
    return -1;
#endif
}
/* run c until it suspends or finishes */
static void jco_resume(JCo *c, int mode, JV val) {
#if JCO_ASM
    JCo *me = jcocur;
    int outer = jdepth;
    char *sl = jstklim;
    c->res = me; c->mode = mode; c->val = val;
    jdepth = c->din;
    jstklim = c->lo + JCO_GUARD;
    jcocur = c; c->st = JCO_RUN;
    mwe_jco_swap(&me->sp, c->sp);
    jcocur = me; jstklim = sl;
    c->din = jdepth; jdepth = outer;
#else
    (void)c; (void)mode; (void)val;
#endif
}
static void jco_suspend(JCo *c) {
#if JCO_ASM
    c->st = JCO_SUSP;
    mwe_jco_swap(&c->sp, c->res->sp);
#else
    (void)c;
#endif
}
static void jag_settle(JCo *c);
static void jag_after_fwd(JOb *g, JCo *c);
#if JCO_ASM
__attribute__((used, noinline)) void mwe_jco_main(JCo *c) {
    JV r, fo, th;
    JOb *b = c->box;
    fo = b->it[0]; th = b->it[1];
    r = jcallfn0(fo.u.o, th, b->it + 2, b->na - 2, 0);
    if (c->kind == 0) {                                /* async function: settle its promise */
        JOb *p = c->obj;
        if (jabort) { jthrow = 0; }
        else if (jthrow) { JV e = jexv; jthrow = 0; jpsettle(p, 2, e); }
        else jpresolve(p, r);
    } else {                                           /* generator: the final value, or the exception for the caller of next() */
        if (jthrow && jgenret) { jgenret = 0; jthrow = 0; c->mode = 0; c->out = jexv; }
        else if (jthrow) { c->mode = 1; c->out = jexv; jthrow = 0; }
        else { c->mode = 0; c->out = r; }
        jexv = jundef();
    }
    c->st = JCO_DONE;
    if (c->kind == 2) jag_settle(c);
    mwe_jco_swap(&c->sp, c->res->sp);
    for (;;) {}
}
#endif

/* ---- garbage collector support ---- */
static int jco_cur_hi(char **hi) { if (jcocur == &jcomain) return 0; *hi = jcocur->hi; return 1; }
static void jco_scan_others(void) {
    int i;
    if (jcocur != &jcomain && jstackbase && jcomain.sp) jscanrange((char *)jcomain.sp, jstackbase);
    for (i = 0; i < JCO_MAX; i++) {
        JCo *c = &jcoro[i];
        c->gcs = 0;
        if (c->st == JCO_FREE) continue;
        if (c->kind != 1 || c->st == JCO_RUN) {         /* async functions are always alive; a generator that is running is on the call chain */
            jmarkp(c->box); jmarkp(c->obj); jmarkv(c->val); jmarkv(c->out);
            if (c != jcocur && c->st != JCO_NEW && c->sp) jscanrange((char *)c->sp, c->hi);
            c->gcs = 1;
        }
    }
}
static int jco_gc_gens(void) {
    int i, ch = 0;
    for (i = 0; i < JCO_MAX; i++) {
        JCo *c = &jcoro[i];
        if (c->st == JCO_SUSP && c->kind == 1 && !c->gcs && c->obj) {
            JHd *h = jfindblk(c->obj);
            if (h && h->mk) {
                c->gcs = 1; ch = 1;
                jmarkp(c->box); jmarkv(c->val); jmarkv(c->out);
                jscanrange((char *)c->sp, c->hi);
                jdrain();
            }
        }
    }
    return ch;
}
static void jco_gc_free_dead(void) {
    int i;
    for (i = 0; i < JCO_MAX; i++) { JCo *c = &jcoro[i]; if (c->st == JCO_SUSP && c->kind == 1 && !c->gcs) jco_release(c); }
}

/* ---- await inside a coroutine ---- */
static void jco_after(JCo *c) {
    if (c->kind == 0) { if (c->st == JCO_DONE) jco_release(c); }
    else if (c->kind == 2) { JOb *g = c->obj; jag_after_fwd(g, c); }
}
JNAT(jco_nf_ok) { JCo *c = &jcoro[jcurnat->pad]; JV v = JARG(0); jco_resume(c, 0, v); jco_after(c); return jundef(); }
JNAT(jco_nf_err) { JCo *c = &jcoro[jcurnat->pad]; JV v = JARG(0); jco_resume(c, 1, v); jco_after(c); return jundef(); }
static int jco_start_async(JOb *fo, JV thisv, JV *args, int argc, JOb *p) {
    int idx = jco_spawn(0, fo, thisv, args, argc, p);
    if (idx < 0) return 0;
    jco_resume(&jcoro[idx], 0, jundef());
    jco_after(&jcoro[idx]);
    return 1;
}
static JV jco_await(JV v) {
    JCo *c = jcocur;
    JOb *p, *ok, *er;
    if (jispromise(v)) p = v.u.o;
    else { p = jpnew(); jpresolve(p, v); if (jthrow) return jundef(); }
    ok = jmknat(0, jco_nf_ok); ok->pad = (short)(c - jcoro);
    er = jmknat(0, jco_nf_err); er->pad = (short)(c - jcoro);
    jthen(p, jobj(ok), jobj(er));
    c->wait = 1;
    jco_suspend(c);
    c->wait = 0;
    if (c->mode == 1) { jexv = c->val; jthrow = 1; return jundef(); }
    return c->val;
}

/* ---- generators ---- */
static JV jiterres(JV value, int done) {
    JOb *o = jnewob(OK_PLAIN, jObjProto);
    jaddpr(o, jintc("value"), value, 0);
    jaddpr(o, jintc("done"), jbool(done), 0);
    return jobj(o);
}
/* generator object: ex = [function, this, args...]; pad = (state << 8) | (coroutine slot + 1), state: 0 not started, 1 suspended at a yield, 2 running, 3 finished */
#define JG_ST(g) ((g)->pad >> 8)
#define JG_SLOT(g) ((g)->pad & 255)
#define JG_SET(g, st, slot1) ((g)->pad = (short)(((st) << 8) | (slot1)))
static JV jmkgen(JOb *fo, JV thisv, JV *args, int argc) {
    JOb *g = jnewob(OK_PLAIN, jGenProto), *box = jnewarr(argc + 2);
    int i;
    jarrpush(box, jobj(fo)); jarrpush(box, thisv);
    for (i = 0; i < argc; i++) jarrpush(box, args[i]);
    g->fl |= JGF; JG_SET(g, 0, 0); g->env = fo; g->ex = jobj(box);
    return jobj(g);
}
/* mode 0 next(val), 1 throw(val), 2 return(val); returns { value, done } */
static JV jgen_step(JOb *g, int mode, JV val) {
    JCo *c;
    int idx, st = JG_ST(g);
    if (st == 2) return jthrowerr(jErrT, "Generator is already running");
    if (st == 3) {
        if (mode == 1) { jexv = val; jthrow = 1; return jundef(); }
        return jiterres(mode == 2 ? val : jundef(), 1);
    }
    if (st == 0) {
        if (mode == 1) { JG_SET(g, 3, 0); jexv = val; jthrow = 1; return jundef(); }
        if (mode == 2) { JG_SET(g, 3, 0); return jiterres(val, 1); }
        idx = jco_spawn(1, g->env, jundef(), 0, 0, g);
        if (idx < 0) return jthrowerr(jErrR, "too many active generators");
        jcoro[idx].box = g->ex.u.o;                     /* the call saved by jmkgen */
        c = &jcoro[idx];
        JG_SET(g, 2, idx + 1);
        jco_resume(c, 0, jundef());
    } else {
        c = &jcoro[JG_SLOT(g) - 1];
        JG_SET(g, 2, JG_SLOT(g));
        jco_resume(c, mode, val);
    }
    if (c->st == JCO_DONE) {
        JV out = c->out; int m = c->mode;
        JG_SET(g, 3, 0); jco_release(c);
        if (m == 1) { jexv = out; jthrow = 1; return jundef(); }
        return jiterres(out, 1);
    }
    JG_SET(g, 1, JG_SLOT(g));
    return jiterres(c->out, 0);
}
JNAT(jg_next) { if (th.t != JT_O || !(th.u.o->fl & JGF)) return jthrowerr(jErrT, "next method called on incompatible receiver"); return jgen_step(th.u.o, 0, JARG(0)); }
JNAT(jg_return) { if (th.t != JT_O || !(th.u.o->fl & JGF)) return jthrowerr(jErrT, "return method called on incompatible receiver"); return jgen_step(th.u.o, 2, JARG(0)); }
JNAT(jg_throw) { if (th.t != JT_O || !(th.u.o->fl & JGF)) return jthrowerr(jErrT, "throw method called on incompatible receiver"); return jgen_step(th.u.o, 1, JARG(0)); }
JNAT(jg_iter) { return th; }
/* yield v: hand v to the caller of next(), continue when it is called again */
static JV jco_yield(JV v) {
    JCo *c = jcocur;
    c->out = v;
    jco_suspend(c);
    if (c->mode == 1) { jexv = c->val; jthrow = 1; return jundef(); }
    if (c->mode == 2) { jgenret = 1; jexv = c->val; jthrow = 1; return jundef(); }
    return c->val;
}
static JV jyieldeval(JNd *n, JOb *env) {
    JV v = n->a ? jev(n->a, env) : jundef();
    if (jthrow) return jundef();
    if (jcocur == &jcomain || jcocur->kind == 0) return jthrowerr(jErrT, "yield outside of a generator");
    if (n->x) {                                         /* yield*: delegate */
        if (v.t == JT_O && (v.u.o->fl & JGF)) {
            JV sent = jundef();
            for (;;) {
                JV r = jgen_step(v.u.o, 0, sent), d;
                if (jthrow) return jundef();
                d = jget(r, jintc("done"));
                if (jtruthy(d)) return jget(r, jintc("value"));
                sent = jco_yield(jget(r, jintc("value")));
                if (jthrow) return jundef();
            }
        } else {
            JOb *arr = jiterable(v);
            int i;
            if (jthrow) return jundef();
            if (!arr) return jthrowerr(jErrT, "yield* operand is not iterable");
            for (i = 0; i < arr->na; i++) { jco_yield(arr->it[i]); if (jthrow) return jundef(); }
            return jundef();
        }
    }
    return jco_yield(v);
}

/* ---- async generators (async function*): next() returns a promise; `await` and `yield` may be mixed ----
 * obj = the generator object; its `ex` holds the call box, `na/it` is unused. Requests wait in a queue (array stored in the property %q);
 * the head request's promise is settled when the body yields, returns or throws. */
static void jag_pump(JOb *g);
static void jag_after(JOb *g, JCo *c) {   /* a kind-2 coroutine just stopped (yield, return, throw or await): settle the request being served */
    JV pv = jget(jobj(g), jintc("%cur"));
    JOb *p;
    if (pv.t != JT_O) return;
    p = pv.u.o;
    if (c->st == JCO_DONE) {
        JV out = c->out; int m = c->mode;
        JG_SET(g, 3, 0); jco_release(c);
        if (m == 1) jpsettle(p, 2, out); else jpresolve(p, jiterres(out, 1));
        jput(jobj(g), jintc("%cur"), jundef());
    } else if (!c->wait) {                              /* suspended at a yield */
        JG_SET(g, 1, (int)(c - jcoro) + 1);
        jput(jobj(g), jintc("%cur"), jundef());
        jpresolve(p, jiterres(c->out, 0));
    }
}
static void jag_settle(JCo *c) { (void)c; }
static JV jag_request(JOb *g, int mode, JV val) {
    JOb *q = jget(jobj(g), jintc("%q")).u.o, *p = jpnew(), *rq = jnewarr(3);
    jarrpush(rq, jnum(mode)); jarrpush(rq, val); jarrpush(rq, jobj(p));
    jarrpush(q, jobj(rq));
    jag_pump(g);
    return jobj(p);
}
static void jag_pump(JOb *g) {
    JOb *q = jget(jobj(g), jintc("%q")).u.o;
    while (q->na > 0 && JG_ST(g) != 2 && jget(jobj(g), jintc("%cur")).t != JT_O) {
        JOb *rq = q->it[0].u.o, *p = rq->it[2].u.o;
        int mode = (int)rq->it[0].u.d, i, st = JG_ST(g);
        JV val = rq->it[1];
        JCo *c;
        for (i = 1; i < q->na; i++) q->it[i - 1] = q->it[i];
        q->na--;
        if (st == 3 || (st == 0 && mode != 0)) {
            JG_SET(g, 3, 0);
            if (mode == 1) jpsettle(p, 2, val); else jpresolve(p, jiterres(mode == 2 ? val : jundef(), 1));
            continue;
        }
        jput(jobj(g), jintc("%cur"), jobj(p));
        if (st == 0) {
            int idx = jco_spawn(2, g->env, jundef(), 0, 0, g);
            if (idx < 0) { jput(jobj(g), jintc("%cur"), jundef()); jpsettle(p, 2, jcv("too many active generators")); continue; }
            jcoro[idx].box = g->ex.u.o; c = &jcoro[idx];
            JG_SET(g, 2, idx + 1); jco_resume(c, 0, jundef());
        } else { c = &jcoro[JG_SLOT(g) - 1]; JG_SET(g, 2, JG_SLOT(g)); jco_resume(c, mode, val); }
        jag_after(g, c);
        if (c->st == JCO_SUSP && c->wait) { JG_SET(g, 2, (int)(c - jcoro) + 1); break; }   /* waiting for an await: the resume handler pumps again */
    }
}
JNAT(jag_next) { if (th.t != JT_O || !(th.u.o->fl & JGF)) return jthrowerr(jErrT, "next method called on incompatible receiver"); return jag_request(th.u.o, 0, JARG(0)); }
JNAT(jag_return) { if (th.t != JT_O || !(th.u.o->fl & JGF)) return jthrowerr(jErrT, "return method called on incompatible receiver"); return jag_request(th.u.o, 2, JARG(0)); }
JNAT(jag_throw) { if (th.t != JT_O || !(th.u.o->fl & JGF)) return jthrowerr(jErrT, "throw method called on incompatible receiver"); return jag_request(th.u.o, 1, JARG(0)); }
static void jag_after_fwd(JOb *g, JCo *c) { jag_after(g, c); jag_pump(g); }
static JV jmkagen(JOb *fo, JV thisv, JV *args, int argc) {
    JV g = jmkgen(fo, thisv, args, argc);
    g.u.o->proto = jAGenProto;
    jaddpr(g.u.o, jintc("%q"), jobj(jnewarr(2)), 1);
    jaddpr(g.u.o, jintc("%cur"), jundef(), 1);
    return g;
}

/* lazy iteration for for...of / for await: generators and custom iterators are stepped one value at a time, and closed with return() when the loop exits early */
static int jlazy(JV coll, int async, JV *it, JV *nx, int *gen) {
    JOb *o;
    JV f;
    if (coll.t != JT_O) return 0;
    o = coll.u.o;
    *gen = 0; *nx = jundef();
    if (o->fl & JGF) { *it = coll; *gen = 1; *nx = jget(coll, jintc("next")); return 1; }
    if (o->k == OK_ARR || (o->fl & 12) || o->k == OK_PROXY) return 0;
    f = jget(coll, jintc(async ? "\1@asyncIterator" : "\1@iterator"));
    if (jthrow) return 1;
    if (!jiscallable(f)) return 0;
    *it = jcall(f, coll, 0, 0);
    if (jthrow) return 1;
    if (it->t != JT_O) { jthrowerr(jErrT, "iterator is not an object"); return 1; }
    *nx = jget(*it, jintc("next"));
    if (jthrow) return 1;
    if (!jiscallable(*nx)) { jthrowerr(jErrT, "iterator.next is not a function"); return 1; }
    *gen = (it->u.o->fl & JGF) != 0;
    return 1;
}
static void jiterclose(JV it, int gen, int async) {   /* call it.return() without disturbing a pending exception / return value */
    int st = jthrow, ab = jabort;
    JV sv = jexv, rv = jrv, r;
    JStr *lb = jlabel;
    JV f;
    if (ab) return;
    jthrow = 0;
    if (gen && !async && it.t == JT_O) { r = jgen_step(it.u.o, 2, jundef()); (void)r; }
    else {
        f = jget(it, jintc("return"));
        if (!jthrow && jiscallable(f)) { r = jcall(f, it, 0, 0); if (!jthrow && async) jawait(r); }
    }
    if (jthrow && !st) return;                        /* a failing return() reports its error when nothing else is pending */
    jthrow = st; jexv = sv; jrv = rv; jlabel = lb;
}

/* custom iterables: [Symbol.iterator]() / [Symbol.asyncIterator]() protocols, collected eagerly (for...of itself iterates lazily) */
static JOb *jiterfrom(JV v) {
    JOb *r;
    JV f, it, nx, res;
    int async = 0, n = 0;
    if (v.t != JT_O) return 0;
    if (v.u.o->fl & JGF) {
        r = jnewarr(8);
        for (;;) {
            res = jgen_step(v.u.o, 0, jundef());
            if (jthrow) return 0;
            if (jtruthy(jget(res, jintc("done")))) break;
            jarrpush(r, jget(res, jintc("value")));
            if (++n >= JGENMAX) { jthrowerr(jErrR, "generator produced too many values"); return 0; }
        }
        return r;
    }
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
    jmicroq = jnewarr(32); jmhead = 0; junh = jnewarr(4); jsymn = 0; jgencur = 0; jcurnat = 0; jgenret = 0;
    jco_reset();
    jPromProto = jnewob(OK_PLAIN, jObjProto);
    jGenProto = jnewob(OK_PLAIN, jObjProto);
    f = jctor("Promise", jf_Promise, jPromProto);
    jdef(f, "resolve", jpf_resolve); jdef(f, "reject", jpf_reject); jdef(f, "all", jpf_all); jdef(f, "allSettled", jpf_allSettled);
    jdef(f, "any", jpf_any); jdef(f, "race", jpf_race);
    jdef(jPromProto, "then", jp_then); jdef(jPromProto, "catch", jp_catch); jdef(jPromProto, "finally", jp_finally);
    jdef(jGenProto, "next", jg_next); jdef(jGenProto, "return", jg_return); jdef(jGenProto, "throw", jg_throw);
    jaddpr(jGenProto, jintc("\1@iterator"), jobj(jmknat("[Symbol.iterator]", jg_iter)), 1);
    jAGenProto = jnewob(OK_PLAIN, jObjProto);
    jdef(jAGenProto, "next", jag_next); jdef(jAGenProto, "return", jag_return); jdef(jAGenProto, "throw", jag_throw);
    jaddpr(jAGenProto, jintc("\1@asyncIterator"), jobj(jmknat("[Symbol.asyncIterator]", jg_iter)), 1);
    jgset("queueMicrotask", jobj(jmknat("queueMicrotask", jw_queueMicrotask)));
    f = jmknat("Symbol", jf_Symbol); jgset("Symbol", jobj(f));
    jdef(f, "for", jsy_for);
    o = f;
    jaddpr(o, jintc("iterator"), jcv("\1@iterator"), 1); jaddpr(o, jintc("asyncIterator"), jcv("\1@asyncIterator"), 1);
    jaddpr(o, jintc("toPrimitive"), jcv("\1@toPrimitive"), 1); jaddpr(o, jintc("hasInstance"), jcv("\1@hasInstance"), 1);
    jaddpr(o, jintc("toStringTag"), jcv("\1@toStringTag"), 1); jaddpr(o, jintc("species"), jcv("\1@species"), 1);
}
#endif

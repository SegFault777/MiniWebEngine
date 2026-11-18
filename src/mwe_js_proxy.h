/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine JavaScript module: Proxy. Supported traps: get, set, has, deleteProperty, ownKeys.
 * Not supported: apply/construct (function targets), getOwnPropertyDescriptor/defineProperty/getPrototypeOf traps,
 * Proxy.revocable. A proxy of an array reports Array.isArray() true. */
#ifndef MWE_JS_PROXY_H
#define MWE_JS_PROXY_H

static JV jproxytrap(JOb *px, const char *name) {
    JV f = jget(px->ex, jintc(name));
    if (jthrow) return jundef();
    return jiscallable(f) ? f : jundef();
}
static JV jproxyget(JOb *px, JStr *key) {
    JV f = jproxytrap(px, "get"), a[3];
    if (jthrow) return jundef();
    if (f.t == JT_U) return jget(jobj(px->env), key);
    a[0] = jobj(px->env); a[1] = jstrv(key); a[2] = jobj(px);
    return jcall(f, px->ex, a, 3);
}
static void jproxyset(JOb *px, JStr *key, JV v) {
    JV f = jproxytrap(px, "set"), a[4];
    if (jthrow) return;
    if (f.t == JT_U) { jput(jobj(px->env), key, v); return; }
    a[0] = jobj(px->env); a[1] = jstrv(key); a[2] = v; a[3] = jobj(px);
    jcall(f, px->ex, a, 4);
}
static int jproxyhas(JOb *px, JStr *key) {
    JV f = jproxytrap(px, "has"), a[2], r;
    if (jthrow) return 0;
    if (f.t == JT_U) return jhasprop(jobj(px->env), key);
    a[0] = jobj(px->env); a[1] = jstrv(key);
    r = jcall(f, px->ex, a, 2);
    return !jthrow && jtruthy(r);
}
static int jproxydel(JOb *px, JStr *key) {
    JV f = jproxytrap(px, "deleteProperty"), a[2], r;
    if (jthrow) return 0;
    if (f.t == JT_U) { if (px->env->k != OK_ARR) jdelpr(px->env, key); return 1; }
    a[0] = jobj(px->env); a[1] = jstrv(key);
    r = jcall(f, px->ex, a, 2);
    return !jthrow && jtruthy(r);
}
static void jproxykeys(JOb *px, JOb *out) {
    JV f = jproxytrap(px, "ownKeys"), r;
    int i;
    if (jthrow) return;
    if (f.t == JT_U) { jkeysof(jobj(px->env), out); return; }
    r = jcall(f, px->ex, &(JV){0}, 0);
    if (jthrow || r.t != JT_O || r.u.o->k != OK_ARR) return;
    for (i = 0; i < r.u.o->na; i++) if (!jissym(r.u.o->it[i])) jarrpush(out, r.u.o->it[i]);
}
/* a plain copy of a proxy's visible properties (used by JSON.stringify, spread and Object.assign) */
static JV jproxysnap(JV v) {
    JOb *k = jnewarr(8), *cp, *tg = v.u.o;
    int i;
    while (tg && tg->k == OK_PROXY) tg = tg->env;
    cp = tg && tg->k == OK_ARR ? jnewarr(8) : jnewob(OK_PLAIN, jObjProto);
    jkeysof(v, k);
    for (i = 0; i < k->na; i++) {
        JStr *ks = k->it[i].t == JT_S ? k->it[i].u.s : jtos(k->it[i]);
        JV x = jget(v, ks);
        if (jthrow) return jundef();
        if (cp->k == OK_ARR && jidx(ks) >= 0) jarrpush(cp, x); else jsetown(cp, ks, x);
    }
    return jobj(cp);
}
JNAT(jf_Proxy) {
    JV t = JARG(0), h = JARG(1);
    if (!jnewcall) return jthrowerr(jErrT, "Constructor Proxy requires 'new'");
    if (t.t != JT_O || h.t != JT_O) return jthrowerr(jErrT, "Cannot create proxy with a non-object as target or handler");
    th.u.o->k = OK_PROXY; th.u.o->env = t.u.o; th.u.o->ex = h;
    return th;
}
static void jinit_proxy(void) { jctor("Proxy", jf_Proxy, jnewob(OK_PLAIN, jObjProto)); }
#endif

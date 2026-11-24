/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine JavaScript module: web platform glue. The APIs themselves (URL, fetch, XMLHttpRequest,
 * AbortController, observers, matchMedia, structuredClone, Intl, typed arrays, ...) are written in JavaScript
 * (mwe_js_prelude.js, embedded as mwe_js_prelude.h); this file adds the one native piece they need:
 * the synchronous network callback behind fetch() and XMLHttpRequest. */
#ifndef MWE_JS_WEB_H
#define MWE_JS_WEB_H
#include "mwe_js_prelude.h"

typedef int (*JFetchCb)(const char *method, const char *url, const char *body, int blen, const char *reqheaders,
                        char *out, int cap, int *status, char *rh, int rhcap);
static JFetchCb jfetchcb;
#define JFETCHSZ (1 << 22)
static char *jfetchbuf, jfetchrh[2048];   /* jfetchbuf: only the synchronous host callback (CLI/tests) needs it; lent on first use */

static void jcopyc(char *d, int cap, JStr *s) { int i; for (i = 0; i + 1 < cap && i < s->len; i++) d[i] = s->d[i]; d[i] = 0; }

/* __fetch(method, url, body|null, "k: v\nk: v") -> { status, statusText, body, headers, url, error } */
JNAT(jw___fetch) {
    char m[16], u[2048], hd[4096];
    JStr *ms = jtos(JARG(0)), *us = jtos(JARG(1)), *hs = jtos(JARG(3));
    JV b = JARG(2);
    JOb *r = jnewob(OK_PLAIN, jObjProto);
    const char *bp = 0;
    int bl = 0, st = 0, n;
    jcopyc(m, sizeof m, ms); jcopyc(u, sizeof u, us); jcopyc(hd, sizeof hd, hs);
    if (b.t == JT_S) { bp = b.u.s->d; bl = b.u.s->len; }
    jfetchrh[0] = 0;
    if (!jfetchcb) { jaddpr(r, jintc("status"), jnum(0), 0); jaddpr(r, jintc("error"), jcv("fetch is not available in this host"), 0); return jobj(r); }
    if (!jfetchbuf) jfetchbuf = (char *)mem_get(JFETCHSZ);
    if (!jfetchbuf) return jthrowerr(jErrT, "out of memory");
    n = jfetchcb(m, u, bp, bl, hd, jfetchbuf, JFETCHSZ - 1, &st, jfetchrh, (int)sizeof jfetchrh);
    jaddpr(r, jintc("status"), jnum(n >= 0 ? st : 0), 0);
    jaddpr(r, jintc("body"), jstrv(n > 0 ? jnewstr(jfetchbuf, n) : &jemptys), 0);
    jaddpr(r, jintc("headers"), jstrv(jcstr(jfetchrh)), 0);
    jaddpr(r, jintc("url"), jstrv(jcstr(u)), 0);
    if (n < 0) jaddpr(r, jintc("error"), jcv(jfetchbuf[0] ? jfetchbuf : "network error"), 0);
    return jobj(r);
}
/* ---- asynchronous requests (v0.10): the host starts the request and reports back later ---- */
typedef int (*JFetchStartCb)(int id, const char *method, const char *url, const char *body, int blen, const char *reqheaders);
typedef void (*JFetchAbortCb)(int id);
static JFetchStartCb jfstart;
static JFetchAbortCb jfabort;
static char jfbusy[JFCB];

/* __fetchStart(method, url, body|null, "k: v\nk: v", callback) -> id (> 0), or 0 if the request could not be started.
 * The callback gets { status, statusText, body, headers, url, error } once the host calls mwe_js_fetch_done(). */
JNAT(jw___fetchStart) {
    char m[16], u[2048], hd[4096];
    JStr *ms = jtos(JARG(0)), *us = jtos(JARG(1)), *hs = jtos(JARG(3));
    JV b = JARG(2), cb = JARG(4);
    const char *bp = 0;
    int bl = 0, id = 0, k;
    if (!jfstart || !jiscallable(cb)) return jnum(0);
    for (k = 0; k < JFCB - 1; k++) { int c = 1 + (jfnext + k) % (JFCB - 1); if (!jfbusy[c]) { id = c; break; } }
    if (!id) return jnum(0);
    jfnext = id;
    jcopyc(m, sizeof m, ms); jcopyc(u, sizeof u, us); jcopyc(hd, sizeof hd, hs);
    if (b.t == JT_S) { bp = b.u.s->d; bl = b.u.s->len; }
    jfcb[id] = cb; jfbusy[id] = 1;
    if (jfstart(id, m, u, bp, bl, hd) < 0) { jfbusy[id] = 0; jfcb[id] = jundef(); return jnum(0); }
    return jnum(id);
}
JNAT(jw___fetchAbort) {
    int id = (int)jtonum(JARG(0));
    if (id > 0 && id < JFCB && jfbusy[id]) { jfbusy[id] = 0; jfcb[id] = jundef(); if (jfabort) jfabort(id); }
    return jundef();
}
static void jinit_web(void) {
    int i;
    for (i = 0; i < JFCB; i++) { jfbusy[i] = 0; jfcb[i] = jundef(); }
    jgset("__fetch", jobj(jmknat("__fetch", jw___fetch)));
    if (jfstart) {
        jgset("__fetchStart", jobj(jmknat("__fetchStart", jw___fetchStart)));
        jgset("__fetchAbort", jobj(jmknat("__fetchAbort", jw___fetchAbort)));
    }
}
#endif

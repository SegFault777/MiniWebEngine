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
static char jfetchbuf[1 << 22], jfetchrh[2048];

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
    n = jfetchcb(m, u, bp, bl, hd, jfetchbuf, (int)sizeof jfetchbuf - 1, &st, jfetchrh, (int)sizeof jfetchrh);
    jaddpr(r, jintc("status"), jnum(n >= 0 ? st : 0), 0);
    jaddpr(r, jintc("body"), jstrv(n > 0 ? jnewstr(jfetchbuf, n) : &jemptys), 0);
    jaddpr(r, jintc("headers"), jstrv(jcstr(jfetchrh)), 0);
    jaddpr(r, jintc("url"), jstrv(jcstr(u)), 0);
    if (n < 0) jaddpr(r, jintc("error"), jcv(jfetchbuf[0] ? jfetchbuf : "network error"), 0);
    return jobj(r);
}
static void jinit_web(void) { jgset("__fetch", jobj(jmknat("__fetch", jw___fetch))); }
#endif

/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.5 public API (build mwe.c with -DMWE_LIB to embed it). */
#ifndef MWE_H
#define MWE_H
#define MWE_VERSION "0.5"

/* Parse html (modified in place; the buffer must stay valid while the page is
 * used) and compute styles. Returns nonzero if the document was truncated. */
int  mwe_load(char *html, int len);
/* Lay the loaded page out for a viewport width. Returns document height (px). */
int  mwe_layout(int width);
/* Render document rect (x,y,w,h) into buf (0x00RRGGBB, stride in pixels). */
void mwe_paint(unsigned *buf, int stride, int x, int y, int w, int h);
/* href under a document point (not NUL-terminated), or 0. */
const char *mwe_link(int x, int y, int *len);
const char *mwe_title(int *len);
/* Viewport height in px (for vh units and fixed boxes); width comes from mwe_layout(). */
void mwe_viewport_h(int h);
/* Toggle a <details> if the point hits its <summary>. Returns 1 if the page changed (re-layout). */
int  mwe_click(int x, int y);
/* Linked stylesheets (<link rel=stylesheet href=...>): the host fetches them and passes the text
 * back with mwe_add_css() (the buffer must stay valid); then call mwe_layout() again. */
const char *mwe_stylesheet(int i, int *len);
void mwe_add_css(char *css, int len);
unsigned    mwe_bg(void);
const char *mwe_font_name(void);
/* Copyright and licence notices of the engine and the embedded fonts (UTF-8, static string). */
const char *mwe_licenses(void);

/* Text/rect helpers for host UI chrome (same font renderer). x-clip is [clx0,clx1). */
int  mwe_textw(const char *s, int n, int fs, int bold);
void mwe_text(unsigned *buf, int stride, int bw, int bh, int x, int baseline,
              const char *s, int n, int fs, int bold, unsigned col, int clx0, int clx1);
void mwe_rect(unsigned *buf, int stride, int bw, int bh, int x, int y, int w, int h, unsigned col);

/* ---- JavaScript (subset; see README) ----
 * Host order: mwe_load -> stylesheets (mwe_add_css) -> external scripts (mwe_script / mwe_add_js)
 * -> mwe_js_time + mwe_js_url -> mwe_js_run -> mwe_layout. Scripts run once, after the whole
 * document was parsed (like defer). */
const char *mwe_script(int i, int *len);              /* src of the i-th external <script src>, or 0 */
void mwe_add_js(int i, char *text, int len);          /* script text of external script i (buffer must stay valid) */
void mwe_js_time(long long unix_ms, long long mono_ms);
void mwe_js_url(const char *url);                     /* value of location.href */
int  mwe_js_run(void);                                /* runs scripts, DOMContentLoaded, load; 1 = page changed */
int  mwe_js_tick(long long mono_ms);                  /* runs due timers; 1 = page changed (re-layout) */
int  mwe_js_active(void);                             /* nonzero while timers are pending */
/* Click at a document point: fires mousedown/mouseup/click. bit0 = re-layout needed,
 * bit1 = preventDefault() was called (the host must not follow the link). */
int  mwe_event_click(int x, int y);
int  mwe_event_key(const char *type, const char *key); /* "keydown"/"keyup"; same return bits */
int  mwe_js_eval(const char *src, int len);           /* run extra script text (debug consoles/tests); 1 = page changed */
const char *mwe_js_log(int *len);                     /* console output (UTF-8) */
void mwe_js_log_clear(void);
const char *mwe_js_alert(int *len);                   /* pops one pending alert() text, or 0 */
const char *mwe_js_nav(int *len);                     /* pending location change (returned once), or 0 */
#endif

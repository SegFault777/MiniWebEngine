/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.10 public API (build mwe.c with -DMWE_LIB to embed it). */
#ifndef MWE_H
#define MWE_H
#define MWE_VERSION "0.11"

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
/* Images (<img src>): PNG, JPEG (baseline), GIF (first frame) and BMP; data: URIs (base64) are decoded by the
 * engine itself. For other sources the host lists the pending ones after mwe_load()/mwe_layout(), fetches the
 * bytes and passes them with mwe_image_set() (decoded immediately; the bytes need not stay valid), then calls
 * mwe_layout() again. State: 0 pending, 1 decoded, 2 failed. */
int  mwe_image_count(void);
const char *mwe_image_src(int i, int *len);
int  mwe_image_state(int i);
int  mwe_image_set(int i, const unsigned char *data, int len);   /* 1 = decoded */
void mwe_image_fail(int i);
/* Network access for JavaScript fetch() / XMLHttpRequest. The host performs the request synchronously: it fills out[]
 * (at most cap bytes) with the response body and returns its length, sets *status and writes the response headers
 * ("name: value\n" lines) to rh; on failure it returns -1 and may put a message in out. */
typedef int (*mwe_fetch_cb)(const char *method, const char *url, const char *body, int blen, const char *reqheaders,
                            char *out, int cap, int *status, char *rh, int rhcap);
void mwe_js_set_fetch(mwe_fetch_cb cb);
/* Asynchronous network access (v0.10; when set, fetch()/XMLHttpRequest use it instead of the synchronous callback).
 * start() is called with a request id and must return at once (0 = started, <0 = cannot start); the host reports the
 * result later with mwe_js_fetch_done(), from any point where no script is running or from inside the poll callback.
 * That resolves/rejects the script's Promise (or fires the XHR events); microtasks run right after. abort() is called when a
 * script cancels the request (AbortController, xhr.abort(), timeout) - the host should stop it and not report back. */
typedef int  (*mwe_fetch_start_cb)(int id, const char *method, const char *url, const char *body, int blen, const char *reqheaders);
typedef void (*mwe_fetch_abort_cb)(int id);
void mwe_js_set_fetch_async(mwe_fetch_start_cb start, mwe_fetch_abort_cb abort);
int  mwe_js_fetch_done(int id, int status, const char *headers, const char *body, int blen, const char *error, const char *url);   /* 1 = page changed */
/* `await` cannot suspend (the interpreter is a tree walker), so while a script awaits a pending Promise the engine calls
 * this: advance the network (nothing else: no navigation, no painting), store the current monotonic time in *now_ms and
 * return the number of operations still pending (0 lets the engine give up with an error instead of waiting forever). */
/* document.cookie: get() returns the "a=1; b=2" string for the page (HttpOnly cookies left out), set(line) receives what a script
 * assigned ("name=value; Path=/; Max-Age=60"). Without these hooks document.cookie is empty and read-only. */
void mwe_js_set_cookies(const char *(*get)(void), void (*set)(const char *));
typedef int  (*mwe_poll_cb)(long long *now_ms);
void mwe_js_set_poll(mwe_poll_cb cb);
/* Is external script i marked async? (host: fetch it at low priority, run it later with mwe_js_eval) */
int  mwe_script_async(int i);
/* Document y of the top of the first box that shows image i, or -1 (not laid out yet). */
int  mwe_image_y(int i);
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
/* Form controls (v0.11): the engine keeps the focus and edits values itself. After a click, ask mwe_focus_kind(): 0 nothing
 * focused, 1 text field, 2 textarea, 3 select -- then send typed text with mwe_input_text() and editing keys by name
 * with mwe_input_key() ("Backspace" "Delete" "ArrowLeft" "ArrowRight" "ArrowUp" "ArrowDown" "Home" "End" "Enter" "Tab"
 * "ShiftTab"). Both return bit 1 when the page needs a new layout (bit 8: Tab moved the focus out of the page). Call
 * mwe_form_submit() after any click/key: it returns 1 once per submitted form with the method, the action as written
 * in the HTML (resolve it against the page address) and the urlencoded fields. mwe_blur() removes the focus. */
int  mwe_focus_kind(void);
int  mwe_input_text(const char *utf8, int n);
int  mwe_input_key(const char *key);
void mwe_blur(void);
int  mwe_form_submit(const char **method, const char **action, const char **data, int *dlen);
int  mwe_event_key(const char *type, const char *key); /* "keydown"/"keyup"; same return bits */
int  mwe_js_eval(const char *src, int len);           /* run extra script text (debug consoles/tests); 1 = page changed */
const char *mwe_js_log(int *len);                     /* console output (UTF-8) */
void mwe_js_log_clear(void);
const char *mwe_js_alert(int *len);                   /* pops one pending alert() text, or 0 */
const char *mwe_js_nav(int *len);                     /* pending location change (returned once), or 0 */
#endif

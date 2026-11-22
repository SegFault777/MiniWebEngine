/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser core <-> platform interface. The core is portable C with no OS calls. */
#ifndef MB_H
#define MB_H
#define MB_VERSION "0.11"

/* ---- implemented by the platform layer (mb_win32.c, mb_shot.c) ---- */
/* Non-blocking TCP sockets (v0.10). Nothing in the core ever waits on the network; the platform layer must
 * not block either: name resolution and connect() run in the background (a worker thread is the simple way). */
int  mb_plat_connect(const char *host, int port);       /* start connecting; handle >= 0 at once (even before DNS finished), or -1 */
int  mb_plat_connect_poll(int h);                       /* 1 connected, 0 still connecting, -1 failed */
int  mb_plat_send(int h, const void *buf, int n);       /* bytes sent (>0), 0 would block, -1 error */
int  mb_plat_recv(int h, void *buf, int n);             /* >0 bytes, -2 nothing yet, 0 closed by the peer, -1 error */
void mb_plat_close(int h);                              /* safe at any time, also while connecting */
void mb_plat_idle(int ms);                              /* sleep up to ms (only used while a script awaits the network) */
void mb_plat_random(unsigned char *buf, int n);         /* cryptographically secure bytes */
long long mb_plat_time(void);                           /* seconds since 1970-01-01 UTC */
long long mb_plat_ms(void);                             /* monotonic milliseconds (timers) */
void mb_plat_alert(const char *utf8, int n);            /* alert() from a script (may block) */
void mb_plat_log(const char *utf8, int n);              /* console.log output (a debugger/stderr is fine) */
void mb_plat_title(const char *utf8, int n);
void mb_plat_cursor(int hand);

/* ---- TLS 1.3 / 1.2 client, implemented by mb_tls.c on top of the platform socket ----
 * Up to MB_NTLS connections are driven at the same time; every call names one by its id (from mb_tls_begin). */
#define MB_NTLS 6
int  mb_tls_begin(const char *host, int port);          /* start connecting + handshaking; id >= 0, or -1 if all slots are busy */
int  mb_tls_step(int c);                                /* 1 = handshake done and certificate verified, 0 = poll again, -1 = failed */
int  mb_tls_write(int c, const void *buf, int n);       /* encrypt and queue (after step() returned 1); 1 ok, 0 failed */
int  mb_tls_read(int c, void *buf, int cap);            /* >0 bytes; 0 nothing yet; -1 error; -2 closed by the server */
void mb_tls_end(int c);
const char *mb_tls_error(int c);                        /* human-readable reason after a failure */
int  mb_tls_net_failed(int c);                          /* 1 if the failure was at TCP level (nothing listens / no TLS) */
typedef struct {                                        /* what was negotiated and verified on the last successful open */
    int  tls13;                 /* 1 = TLS 1.3, 0 = TLS 1.2 */
    char suite[40];             /* e.g. TLS_AES_256_GCM_SHA384 */
    char cn[128];               /* certificate common name (leaf) */
    char issuer[128];           /* common name of the next certificate in the chain, "" if the server sent none */
    char key[24];               /* "ECDSA P-256", "RSA 2048-bit", ... */
    long long nb, na;           /* validity, packed YYYYMMDDHHMMSS */
    int  chain;                 /* certificates sent by the server */
} mb_tls_info_t;
const mb_tls_info_t *mb_tls_info(int c);
int  mb_root_count(void);                               /* trusted root certificates compiled in */
const char *mb_root_name(int i, int *legacy, const char **sha256hex);   /* name; legacy=1 for the legacy tier */

/* ---- implemented by the core ---- */
enum { MBK_BACKSPACE = 1, MBK_DELETE, MBK_ENTER, MBK_LEFT, MBK_RIGHT, MBK_HOME, MBK_END,
       MBK_UP, MBK_DOWN, MBK_PGUP, MBK_PGDN, MBK_ESC, MBK_F5, MBK_FOCUS_URL, MBK_BACK, MBK_FORWARD, MBK_TAB, MBK_SHIFT_TAB };
void mb_init(const char *start_url, int w, int h);
void mb_resize(int w, int h);
void mb_draw(unsigned *px);                 /* full window, w*h pixels, 0x00RRGGBB */
void mb_click(int x, int y);
void mb_move(int x, int y);
void mb_wheel(int notches);                 /* >0 scrolls up */
void mb_key(int k);
void mb_char(int codepoint);
void mb_paste(const char *utf8, int n);
int  mb_busy(void);                         /* 1 while a page, resource or request is still in flight */
const char *mb_summary(void);               /* one line of timing statistics (tests) */
int  mb_tick(void);                         /* call every ~16 ms: runs script timers; 1 = redraw needed */
#endif

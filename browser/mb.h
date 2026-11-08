/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser core <-> platform interface. The core is portable C with no OS calls. */
#ifndef MB_H
#define MB_H
#define MB_VERSION "0.8"

/* ---- implemented by the platform layer (mb_win32.c, mb_shot.c) ---- */
/* A blocking TCP socket with a receive timeout (about 15 s). */
int  mb_plat_open(const char *host, int port);          /* handle >= 0, or -1 */
int  mb_plat_send(int h, const void *buf, int n);       /* bytes sent, or -1 */
int  mb_plat_recv(int h, void *buf, int n);             /* >0 bytes; 0 closed; -1 error/timeout */
void mb_plat_close(int h);
void mb_plat_random(unsigned char *buf, int n);         /* cryptographically secure bytes */
long long mb_plat_time(void);                           /* seconds since 1970-01-01 UTC */
long long mb_plat_ms(void);                             /* monotonic milliseconds (timers) */
void mb_plat_alert(const char *utf8, int n);            /* alert() from a script (may block) */
void mb_plat_log(const char *utf8, int n);              /* console.log output (a debugger/stderr is fine) */
void mb_plat_title(const char *utf8, int n);
void mb_plat_cursor(int hand);

/* ---- TLS 1.3 / 1.2 client, implemented by mb_tls.c on top of the platform socket ---- */
int  mb_tls_open(const char *host, int port);           /* 0 = handshake done and certificate verified */
int  mb_tls_send(const void *buf, int n);               /* 1 ok, 0 failed */
int  mb_tls_recv(void *buf, int cap);                   /* >0 bytes; 0 end of data; -1 error */
void mb_tls_close(void);
const char *mb_tls_error(void);                         /* human-readable reason after a failure */
int  mb_tls_net_failed(void);                           /* 1 if the last open failed at TCP level (nothing listens / no TLS) */
typedef struct {                                        /* what was negotiated and verified on the last successful open */
    int  tls13;                 /* 1 = TLS 1.3, 0 = TLS 1.2 */
    char suite[40];             /* e.g. TLS_AES_256_GCM_SHA384 */
    char cn[128];               /* certificate common name (leaf) */
    char issuer[128];           /* common name of the next certificate in the chain, "" if the server sent none */
    char key[24];               /* "ECDSA P-256", "RSA 2048-bit", ... */
    long long nb, na;           /* validity, packed YYYYMMDDHHMMSS */
    int  chain;                 /* certificates sent by the server */
} mb_tls_info_t;
const mb_tls_info_t *mb_tls_info(void);
int  mb_root_count(void);                               /* trusted root certificates compiled in */
const char *mb_root_name(int i, int *legacy, const char **sha256hex);   /* name; legacy=1 for the legacy tier */

/* ---- implemented by the core ---- */
enum { MBK_BACKSPACE = 1, MBK_DELETE, MBK_ENTER, MBK_LEFT, MBK_RIGHT, MBK_HOME, MBK_END,
       MBK_UP, MBK_DOWN, MBK_PGUP, MBK_PGDN, MBK_ESC, MBK_F5, MBK_FOCUS_URL, MBK_BACK, MBK_FORWARD };
void mb_init(const char *start_url, int w, int h);
void mb_resize(int w, int h);
void mb_draw(unsigned *px);                 /* full window, w*h pixels, 0x00RRGGBB */
void mb_click(int x, int y);
void mb_move(int x, int y);
void mb_wheel(int notches);                 /* >0 scrolls up */
void mb_key(int k);
void mb_char(int codepoint);
void mb_paste(const char *utf8, int n);
int  mb_tick(void);                         /* call every ~16 ms: runs script timers; 1 = redraw needed */
#endif

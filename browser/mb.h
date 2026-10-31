/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser core <-> platform interface. The core is portable C with no OS calls. */
#ifndef MB_H
#define MB_H
#define MB_VERSION "0.3-dev1"

/* ---- implemented by the platform layer (mb_win32.c, mb_shot.c) ---- */
/* A blocking TCP socket with a receive timeout (about 15 s). */
int  mb_plat_open(const char *host, int port);          /* handle >= 0, or -1 */
int  mb_plat_send(int h, const void *buf, int n);       /* bytes sent, or -1 */
int  mb_plat_recv(int h, void *buf, int n);             /* >0 bytes; 0 closed; -1 error/timeout */
void mb_plat_close(int h);
void mb_plat_random(unsigned char *buf, int n);         /* cryptographically secure bytes */
long long mb_plat_time(void);                           /* seconds since 1970-01-01 UTC */
void mb_plat_title(const char *utf8, int n);
void mb_plat_cursor(int hand);

/* ---- TLS 1.2 client, implemented by mb_tls.c on top of the platform socket ---- */
int  mb_tls_open(const char *host, int port);           /* 0 = handshake done and certificate verified */
int  mb_tls_send(const void *buf, int n);               /* 1 ok, 0 failed */
int  mb_tls_recv(void *buf, int cap);                   /* >0 bytes; 0 end of data; -1 error */
void mb_tls_close(void);
const char *mb_tls_error(void);                         /* human-readable reason after a failure */

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
#endif

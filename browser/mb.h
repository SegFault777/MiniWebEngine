/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser core <-> platform interface. The core is portable C with no OS calls. */
#ifndef MB_H
#define MB_H
#define MB_VERSION "0.2"

/* ---- implemented by the platform layer (mb_win32.c, mb_shot.c) ---- */
/* Send req over TCP to host:port and read the whole reply into out.
 * Returns bytes received (>0) or -1 on failure. */
int  mb_plat_http(const char *host, int port, const char *req, int reqlen, char *out, int cap);
void mb_plat_title(const char *utf8, int n);
void mb_plat_cursor(int hand);

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

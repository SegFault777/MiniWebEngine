/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniWebEngine v0.2 public API (build mwe.c with -DMWE_LIB to embed it). */
#ifndef MWE_H
#define MWE_H
#define MWE_VERSION "0.2"

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
unsigned    mwe_bg(void);
const char *mwe_font_name(void);

/* Text/rect helpers for host UI chrome (same font renderer). x-clip is [clx0,clx1). */
int  mwe_textw(const char *s, int n, int fs, int bold);
void mwe_text(unsigned *buf, int stride, int bw, int bh, int x, int baseline,
              const char *s, int n, int fs, int bold, unsigned col, int clx0, int clx1);
void mwe_rect(unsigned *buf, int stride, int bw, int bh, int x, int y, int w, int h, unsigned col);
#endif

/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef SERIAL_H
#define SERIAL_H
#include "io.h"
/* Diagnostics: silent unless built with -DMB_TLS_DEBUG (then they go to stderr). */
#ifdef MB_TLS_DEBUG
extern void mb_plat_log(const char *utf8, int n);   /* platform log (mb.h), no libc */
static inline void serial_puts(const char *s) { int n = 0; while (s[n]) n++; mb_plat_log(s, n); }
static inline void serial_putc(char c) { mb_plat_log(&c, 1); }
static inline void serial_put_dec(u32 v) { char t[12]; int k = 0, i; if (!v) t[k++] = '0'; while (v) { t[k++] = (char)('0' + v % 10); v /= 10; } for (i = k - 1; i >= 0; i--) mb_plat_log(&t[i], 1); }
static inline void serial_put_hex16(u16 v) { static const char h[] = "0123456789abcdef"; char t[4]; t[0] = h[v >> 12 & 15]; t[1] = h[v >> 8 & 15]; t[2] = h[v >> 4 & 15]; t[3] = h[v & 15]; mb_plat_log(t, 4); }
#else
static inline void serial_puts(const char *s) { (void)s; }
static inline void serial_putc(char c) { (void)c; }
static inline void serial_put_dec(u32 v) { (void)v; }
static inline void serial_put_hex16(u16 v) { (void)v; }
#endif
#endif

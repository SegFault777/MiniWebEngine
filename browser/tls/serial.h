/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef SERIAL_H
#define SERIAL_H
#include "io.h"
/* Diagnostics: silent unless built with -DMB_TLS_DEBUG (then they go to stderr). */
#ifdef MB_TLS_DEBUG
#include <stdio.h>
static inline void serial_puts(const char *s) { fputs(s, stderr); }
static inline void serial_putc(char c) { fputc(c, stderr); }
static inline void serial_put_dec(u32 v) { fprintf(stderr, "%u", v); }
static inline void serial_put_hex16(u16 v) { fprintf(stderr, "%04x", v); }
#else
static inline void serial_puts(const char *s) { (void)s; }
static inline void serial_putc(char c) { (void)c; }
static inline void serial_put_dec(u32 v) { (void)v; }
static inline void serial_put_hex16(u16 v) { (void)v; }
#endif
#endif

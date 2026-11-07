/* SPDX-License-Identifier: GPL-3.0-only */
/* Regression guard: mwe.c is #included by tests/jt.c BEFORE the C library headers, so none of its
 * file-scope names may collide with libc / POSIX / BSD declarations (v0.5 failed on macOS because of a
 * variable named `fls`, which <string.h> declares as a function there). CI compiles this with -fsyntax-only. */
#include "../mwe.c"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <stdint.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <inttypes.h>
#include <locale.h>
#include <float.h>
/* BSD names declared by the macOS SDK that glibc lacks (identical prototypes, so redeclaring is harmless) */
int fls(int); int flsl(long); int ffs(int); int ffsl(long); int ffsll(long long);
int clash_unused(void) { return 0; }

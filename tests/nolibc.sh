#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# "No C library" guarantee (v0.12). Run from the repo root.
#  1. the engine (mwe.c), the TLS stack and the browser core compile with -ffreestanding -nostdlib and leave NO undefined symbol
#     except the host interface (mb_plat_*, mb_tls_*, mwe_*) - at every optimization level;
#  2. no core source includes a C library header;
#  3. the command-line tool builds as a static executable that links against nothing (Linux x86-64: raw system calls) and
#     renders every test page exactly like the libc build.
set -e
D=${TMPDIR:-/tmp}/mwe_nolibc_chk; mkdir -p "$D"; fail=0
for o in 0 1 2 3 s; do
  for f in "mwe.c" "browser/minibrowser.c" "browser/mb_tls.c"; do
    cc -std=gnu11 -O$o -ffreestanding -nostdlib -DMWE_LIB -c "$f" -o "$D/x.o"
    bad=$(nm -u "$D/x.o" | grep -v -E ' (mb_|mwe_)|_GLOBAL_OFFSET_TABLE_' || true)
    [ -z "$bad" ] || { echo "FAIL  -O$o $f needs: $bad"; fail=1; }
  done
done
[ $fail -eq 0 ] && echo "ok   engine, TLS and browser core: no libc symbols at -O0 -O1 -O2 -O3 -Os"
inc=$(grep -n '^[[:space:]]*#[[:space:]]*include[[:space:]]*<' mwe.c src/*.inc src/*.h browser/minibrowser.c browser/mb_*.inc browser/mb_tls.c browser/tls/*.h | grep -v -E 'mwe_api.inc:[0-9]+:#include <(stdio|stdlib)\.h>|mwe_img.h:[0-9]+:#include <stdlib.h>' || true)
[ -z "$inc" ] && echo "ok   no libc headers in the core (only the libc CLI variant and the standalone image-decoder fallback)" || { echo "FAIL  libc headers: $inc"; fail=1; }
if [ "$(uname -s)-$(uname -m)" = "Linux-x86_64" ]; then
  cc -std=c99 -Os -DMWE_NOLIBC -ffreestanding -fno-builtin -nostdlib -static mwe.c -o "$D/mwe_nolibc"
  cc -std=c99 -Os mwe.c -o "$D/mwe_libc"
  [ -z "$(nm -u "$D/mwe_nolibc")" ] && echo "ok   mwe CLI is a static executable with no undefined symbols" || { echo "FAIL  mwe_nolibc has undefined symbols"; fail=1; }
  n=0; bad=0
  for p in test*.html tests/layout/*.html; do
    "$D/mwe_nolibc" "$p" "$D/a.bmp" 800 2>/dev/null; "$D/mwe_libc" "$p" "$D/b.bmp" 800 2>/dev/null
    cmp -s "$D/a.bmp" "$D/b.bmp" || { bad=$((bad+1)); echo "FAIL  $p differs"; }; n=$((n+1)); done
  [ $bad -eq 0 ] && echo "ok   no-libc CLI renders $n pages identically to the libc build" || fail=1
else echo "skip  no-libc CLI (needs Linux x86-64)"; fi
[ $fail -eq 0 ] && echo "no-libc checks passed" || exit 1

#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# v0.10 asynchronous network tests: MiniBrowser (headless, short timeouts) against tests/net/server.py.
#   bash tests/net/run.sh      needs: cc, python3 + Pillow, a free port (PORT, default 18081)
set -u
cd "$(dirname "$0")/../.."
PORT=${PORT:-18081}; D=$(mktemp -d); pass=0; fail=0
cc -std=gnu11 -O1 -DMB_FAST_TIMEOUTS -DMWE_LIB mwe.c browser/minibrowser.c browser/mb_tls.c browser/mb_shot.c -lpthread -o "$D/mb" || exit 1
python3 tests/net/server.py "$PORT" >/dev/null 2>&1 & SP=$!
trap 'kill $SP 2>/dev/null; rm -rf "$D"' EXIT
for _ in 1 2 3 4 5 6 7 8 9 10; do (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break; sleep 0.3; done
U=http://127.0.0.1:$PORT
t() { # t <label> <expected-substring-in-output> <args...>
  local label=$1 want=$2; shift 2
  local out; out=$(timeout 60 "$D/mb" "$@" 2>&1)
  if printf '%s' "$out" | grep -qF -- "$want"; then pass=$((pass+1)); echo "PASS  $label"; else fail=$((fail+1)); echo "FAIL  $label (wanted: $want)"; printf '%s\n' "$out" | head -5; fi
}
num() { printf '%s' "$1" | sed -n "s/.*$2=\([0-9-]*\).*/\1/p"; }
# 1. first render does not wait for slow resources; everything settles; the hanging image fails by timeout
out=$("$D/mb" $U/index.html $D/i.bmp 800 400 --trace 2>&1)
fr=$(num "$out" first-render); st=$(num "$out" settled)
[ "${fr:--1}" -ge 0 ] && [ "$fr" -lt 500 ] && { pass=$((pass+1)); echo "PASS  first render in ${fr}ms (css 1.5s, js 1s, images slower)"; } || { fail=$((fail+1)); echo "FAIL  first render ${fr}"; }
[ "${st:-0}" -gt 1400 ] && [ "$st" -lt 9000 ] && { pass=$((pass+1)); echo "PASS  settled after ${st}ms, hanging image timed out"; } || { fail=$((fail+1)); echo "FAIL  settled ${st}"; }
echo "$out" | grep -q "failed=1" && { pass=$((pass+1)); echo "PASS  exactly the hanging image failed"; } || { fail=$((fail+1)); echo "FAIL  failed count"; echo "$out" | grep trace; }
mt=$(num "$out" maxtick); [ "${mt:-999}" -lt 100 ] && { pass=$((pass+1)); echo "PASS  longest mb_tick ${mt}ms (UI never blocked)"; } || { fail=$((fail+1)); echo "FAIL  maxtick ${mt}"; }
python3 - "$D" "$U" <<'PY' || { echo "FAIL  staged rendering"; exit 1; }
import sys,subprocess
from PIL import Image
D=sys.argv[1]; U=sys.argv[2]
def shot(ms):
    subprocess.run([D+"/mb",U+"/index.html",D+"/e.bmp","800","300","--early",str(ms)],capture_output=True)
    return Image.open(D+"/e.bmp").convert("RGB")
a=shot(250); b=shot(2400)
ok=a.getpixel((5,60))==(255,255,255) and b.getpixel((5,60))==(238,238,255)
print(("PASS" if ok else "FAIL")+"  unstyled text first (%s), stylesheet applied later (%s)"%(a.getpixel((5,60)),b.getpixel((5,60))))
sys.exit(0 if ok else 1)
PY
[ $? -eq 0 ] && pass=$((pass+1)) || fail=$((fail+1))
t "script from a slow <script src> ran after css+js arrived" "title: net test" $U/index.html $D/o.bmp 800 400
t "redirect followed"                 "title: redirected" $U/redir1 $D/o.bmp 800 300
t "redirect loop stops at the limit"  "title: Too many redirects" $U/redir $D/o.bmp 800 300
t "chunked response"                  "title: chunked" $U/chunked $D/o.bmp 800 300
t "server that never answers times out" "title: Timed out" $U/hang $D/o.bmp 800 300
t "headers that never finish time out" "title: Timed out" $U/trickle $D/o.bmp 800 300
t "nothing listening -> clear error"   "title: Could not connect" http://127.0.0.1:1/ $D/o.bmp 800 300
t "fetch().then() and AbortController (aborted request cancelled)" "aborted:AbortError" $U/fetch.html $D/o.bmp 800 300 --trace
t "abort cancels the connection"       "cancelled=1" $U/fetch.html $D/o.bmp 800 300 --trace
t "await fetch() continues with the result" "await done 42" $U/await.html $D/o.bmp 800 300
t "navigating away cancels pending requests" "cancelled=" $U/index.html $D/o.bmp 800 300 --early 200 --goto $U/ok.html --trace
t "...and the new page is shown"       "title: redirected" $U/index.html $D/o.bmp 800 300 --early 200 --goto $U/redir1
# size limit: a page whose stylesheet is bigger than 256 KB
cat > "$D/big.html" <<HTML
<html><head><title>big</title><link rel=stylesheet href="$U/huge.css"></head><body>x</body></html>
HTML
t "stylesheet over the size limit is refused" "failed=1" "file://$D/big.html" $D/o.bmp 800 300 --trace
t "repeat visit is served from the cache (css, js, 2 images)" "cache-hits=4" $U/index.html $D/o.bmp 800 300 --trace --goto $U/index.html
out=$("$D/mb" $U/fetch.html $D/o.bmp 800 300 --goto $U/fetch.html 2>&1)
[ "$(printf '%s' "$out" | grep -c 'aborted:AbortError')" = 2 ] && { pass=$((pass+1)); echo "PASS  scripts still work on the second page of a session (v0.9 bug: stale AST arena)"; } || { fail=$((fail+1)); echo "FAIL  second scripted page"; printf '%s\n' "$out" | head -4; }
echo "net tests: $pass passed, $fail failed"; [ $fail -eq 0 ]

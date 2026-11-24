#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Layout/paint checks for v0.11-v0.12: z-index stacking, transform: translate, position: sticky, inline svg.  Run from the repo root.
set -e
D=${TMPDIR:-/tmp}/mwe_layout; mkdir -p "$D"; fail=0
cc -std=c99 -O1 -o "$D/mwe" mwe.c
cc -std=gnu11 -O1 -DMWE_LIB -I. -o "$D/sticky" tests/layout/sticky.c
python3 - "$D" <<'PY' || fail=1
import sys,subprocess
from PIL import Image
D=sys.argv[1]
def px(page,w,pts):
    subprocess.run([D+"/mwe","tests/layout/"+page,D+"/o.bmp",str(w)],check=True,capture_output=True)
    im=Image.open(D+"/o.bmp").convert("RGB");return [im.getpixel(p) for p in pts]
ok=True
def chk(name,got,want):
    global ok
    good=all(abs(g[i]-w[i])<3 for g,w in zip(got,want) for i in range(3))
    print(("ok   " if good else "FAIL ")+name,"" if good else (got,want));ok&=good
chk("z-index: z1 above z0",px("zindex.html",400,[(60,50)])[:1],[(255,0,0)])
chk("z-index: nested context (z2 inside z5) above z3",px("zindex.html",400,[(260,100),(215,40)]),[(255,170,0),(0,0,255)])
chk("transform: translate px, %, X/Y",px("transform.html",500,[(50,25),(150,130),(50,130),(60,200)]),[(204,204,204),(0,170,0),(255,255,255),(0,0,255)])
chk("svg: rounded rect, circle, star path, currentColor",px("svg.html",420,[(30,70),(98,73),(180,60),(61,134)]),[(204,0,0),(0,0,255),(255,170,0),(0,0,0)])
sys.exit(0 if ok else 1)
PY
"$D/sticky" > "$D/sticky.got"
cat > "$D/sticky.want" <<'OUT'
doc height 1000
scroll   0 -> red bar top at viewport y=60 (doc y=60)
scroll  30 -> red bar top at viewport y=30 (doc y=60)
scroll 100 -> red bar top at viewport y=10 (doc y=110)
scroll 300 -> red bar top at viewport y=10 (doc y=310)
scroll 640 -> red bar top at viewport y=10 (doc y=650)
scroll 700 -> red bar top at viewport y=-1 (doc y=-1)
OUT
if cmp -s "$D/sticky.got" "$D/sticky.want"; then echo "ok   position: sticky sticks, then leaves with its container"; else echo "FAIL sticky"; diff "$D/sticky.got" "$D/sticky.want"; fail=1; fi
[ $fail -eq 0 ] && echo "layout tests passed" || exit 1

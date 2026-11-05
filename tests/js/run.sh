#!/bin/sh
# JS regression tests.  Usage: sh tests/js/run.sh   (from the repository root)
# lang/numbers/accessors/regex: *.js are wrapped into an HTML page, console.log output must equal *.out
# (the .out files were produced by Node.js, so they double as conformance checks).
# dom.html / events.html: output checked against the recorded *.out (events use the jt driver).
set -e
CC=${CC:-cc}
T=tests/js
OUT=${TMPDIR:-/tmp}/mwe_jstest
mkdir -p "$OUT"
$CC -std=c99 -O1 -o "$OUT/mwe" mwe.c
$CC -std=c99 -O1 -o "$OUT/jt" tests/jt.c
fail=0
for n in lang numbers accessors regex iter; do
  { echo '<html><body><script>'; cat $T/$n.js; echo '</script></body></html>'; } > "$OUT/$n.html"
  "$OUT/mwe" "$OUT/$n.html" "$OUT/x.bmp" > "$OUT/$n.got" 2>&1 || true
  if cmp -s "$OUT/$n.got" $T/$n.out; then echo "ok   $n"; else echo "FAIL $n"; diff "$OUT/$n.got" $T/$n.out | head -10; fail=1; fi
done
"$OUT/mwe" $T/dom.html "$OUT/x.bmp" > "$OUT/dom.got" 2>&1 || true
if cmp -s "$OUT/dom.got" $T/dom.out; then echo "ok   dom"; else echo "FAIL dom"; diff "$OUT/dom.got" $T/dom.out | head -10; fail=1; fi
"$OUT/jt" $T/events.html "c:10,10" "c:10,10" "c:10,80" "c:5,100" "t:120" "k:ArrowLeft" "e:console.log(document.getElementById('o').textContent,document.getElementById('cb').checked)" > "$OUT/events.got" 2>&1 || true
if cmp -s "$OUT/events.got" $T/events.out; then echo "ok   events"; else echo "FAIL events"; diff "$OUT/events.got" $T/events.out | head -10; fail=1; fi
exit $fail

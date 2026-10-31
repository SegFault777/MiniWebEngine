#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# End-to-end TLS test: the real MiniBrowser TLS stack (headless build) against a local
# `openssl s_server`, with a throw-away test CA compiled in as the only trust anchor.
#   browser/tests/tls_e2e.sh           needs: bash, cc, openssl (1.1.1+/3.x), python3, a free port
# Each positive case must load the page; each negative case must be REFUSED with the error page.
set -u
cd "$(dirname "$0")/../.."
ROOT=$PWD; D=$(mktemp -d); PORT=${PORT:-18443}; CC=${CC:-cc}
mkdir -p "$D/www" && cp test.html "$D/www/"
fail=0; pass=0
SPID=""
cleanup() { [ -n "$SPID" ] && kill "$SPID" 2>/dev/null; rm -rf "$D"; }
trap cleanup EXIT

mkpki() {   # leaf: rsa|ec256|ec384   ca: rsa|ec384   wrong: 0|1 (trust a different CA than the signer)
  local leaf=$1 ca=$2 wrong=${3:-0}
  ( cd "$D" && rm -f ca.* leaf.* other.* ext.cnf test_roots.h
    if [ "$ca" = rsa ]; then
      openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -out ca.pem -days 2 -subj "/CN=MB Test Root" -sha256 \
        -addext basicConstraints=critical,CA:TRUE -addext keyUsage=critical,keyCertSign,cRLSign 2>/dev/null
    else
      openssl req -x509 -newkey ec -pkeyopt ec_paramgen_curve:secp384r1 -nodes -keyout ca.key -out ca.pem -days 2 -subj "/CN=MB Test Root" -sha384 \
        -addext basicConstraints=critical,CA:TRUE -addext keyUsage=critical,keyCertSign,cRLSign 2>/dev/null
    fi
    case $leaf in
      rsa)   openssl req -newkey rsa:2048 -nodes -keyout leaf.key -out leaf.csr -subj "/CN=localhost" 2>/dev/null; ku=digitalSignature,keyEncipherment;;
      ec256) openssl req -newkey ec -pkeyopt ec_paramgen_curve:prime256v1 -nodes -keyout leaf.key -out leaf.csr -subj "/CN=localhost" 2>/dev/null; ku=digitalSignature;;
      ec384) openssl req -newkey ec -pkeyopt ec_paramgen_curve:secp384r1 -nodes -keyout leaf.key -out leaf.csr -subj "/CN=localhost" 2>/dev/null; ku=digitalSignature;;
    esac
    printf 'subjectAltName=DNS:localhost\nbasicConstraints=CA:FALSE\nkeyUsage=%s\nextendedKeyUsage=serverAuth\n' "$ku" > ext.cnf
    openssl x509 -req -in leaf.csr -CA ca.pem -CAkey ca.key -CAcreateserial -out leaf.pem -days 2 -extfile ext.cnf 2>/dev/null
    trust=ca.pem
    if [ "$wrong" = 1 ]; then
      openssl req -x509 -newkey rsa:2048 -nodes -keyout other.key -out other.pem -days 2 -subj "/CN=Someone Else" \
        -addext basicConstraints=critical,CA:TRUE 2>/dev/null; trust=other.pem
    fi
    openssl x509 -in $trust -outform DER -out trust.der
    python3 - <<'PY'
d = open("trust.der", "rb").read()
body = ",".join("0x%02x" % b for b in d)
open("test_roots.h", "w").write(
    "static const unsigned char mw_test_root0[] = {%s};\n"
    "static const unsigned char *const mw_test_roots[] = {mw_test_root0};\n"
    "static const unsigned int mw_test_root_lens[] = {%d};\n#define MW_TEST_ROOT_COUNT 1\n" % (body, len(d)))
PY
  )
  $CC -std=gnu11 -Os -DMWE_LIB -DMW_TLS_TEST_ROOTS="\"$D/test_roots.h\"" \
      mwe.c browser/minibrowser.c browser/mb_tls.c browser/mb_shot.c -o "$D/mb_t" || { echo "BUILD FAILED"; exit 1; }
}

run() {   # run <label> <expect: ok|refuse> <url-host> -- <s_server args...>
  local label=$1 expect=$2 host=$3; shift 4
  ( cd "$D/www" && exec openssl s_server -accept "$PORT" -cert "$D/leaf.pem" -key "$D/leaf.key" -WWW "$@" >"$D/server.log" 2>&1 ) &
  SPID=$!
  for _ in 1 2 3 4 5 6 7 8 9 10; do (exec 3<>/dev/tcp/127.0.0.1/$PORT) 2>/dev/null && break; sleep 0.3; done
  local out; out=$(timeout 60 "$D/mb_t" "https://$host:$PORT/test.html" "$D/shot.bmp" 800 500 2>&1)
  kill "$SPID" 2>/dev/null; wait "$SPID" 2>/dev/null; SPID=""
  local ok=0
  case $expect in
    ok)     [[ "$out" == *"title: MiniWebEngine test"* ]] && ok=1;;
    refuse) [[ "$out" == *"title: Secure connection failed"* ]] && ok=1;;
  esac
  if [ $ok = 1 ]; then pass=$((pass+1)); echo "PASS  $label"; else fail=$((fail+1)); echo "FAIL  $label :: $out"; fi
}

mkpki rsa rsa
run "rsa cert  / ECDHE-RSA-AES128-GCM / X25519" ok localhost -- -tls1_2 -cipher ECDHE-RSA-AES128-GCM-SHA256 -groups X25519
run "rsa cert  / ECDHE-RSA-AES256-GCM / X25519" ok localhost -- -tls1_2 -cipher ECDHE-RSA-AES256-GCM-SHA384 -groups X25519
run "rsa cert  / ECDHE-RSA-AES128-GCM / P-256"  ok localhost -- -tls1_2 -cipher ECDHE-RSA-AES128-GCM-SHA256 -groups P-256
run "rsa cert  / ECDHE-RSA-AES256-GCM / P-384"  ok localhost -- -tls1_2 -cipher ECDHE-RSA-AES256-GCM-SHA384 -groups P-384
run "rsa cert, server also offers TLS 1.3 (must fall back to 1.2)" ok localhost -- -cipher ECDHE-RSA-AES128-GCM-SHA256
run "REFUSE hostname mismatch (127.0.0.1 vs localhost)" refuse 127.0.0.1 -- -tls1_2 -cipher ECDHE-RSA-AES128-GCM-SHA256 -groups X25519
run "REFUSE TLS 1.3-only server" refuse localhost -- -tls1_3
run "REFUSE no common cipher (ChaCha20 only)" refuse localhost -- -tls1_2 -cipher ECDHE-RSA-CHACHA20-POLY1305 -groups X25519
mkpki ec256 rsa
run "ecdsa-p256 cert / ECDHE-ECDSA-AES128-GCM" ok localhost -- -tls1_2 -cipher ECDHE-ECDSA-AES128-GCM-SHA256 -groups X25519
mkpki ec384 ec384
run "ecdsa-p384 cert (P-384 CA) / ECDHE-ECDSA-AES256-GCM" ok localhost -- -tls1_2 -cipher ECDHE-ECDSA-AES256-GCM-SHA384 -groups X25519
mkpki rsa rsa 1
run "REFUSE certificate signed by an untrusted CA" refuse localhost -- -tls1_2 -cipher ECDHE-RSA-AES128-GCM-SHA256 -groups X25519

echo "TLS e2e: $pass passed, $fail failed"
[ $fail = 0 ]

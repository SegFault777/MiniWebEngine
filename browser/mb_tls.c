/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser TLS driver: runs the polled TLS 1.3 / 1.2 state machine (tls/tls.h) to completion
 * over a blocking platform socket. Certificates are always verified (chain, validity window,
 * hostname, key-exchange signature); there is no "continue anyway" path. */
#include "tls/tls.h"
#include "mb.h"

static const char *terr = "";
static mb_tls_info_t tinfo;
static int tnet;
static void sset(char *d, int cap, const u8 *s, u32 n) { u32 i; for (i = 0; i + 1 < (u32)cap && i < n; i++) d[i] = (char)s[i]; d[i] = 0; }
static void put(char *d, int cap, const char *s) { int i = 0; while (s[i] && i + 1 < cap) { d[i] = s[i]; i++; } d[i] = 0; }
static const char *suite_name(u16 s) {
    switch (s) {
    case 0x1301: return "TLS_AES_128_GCM_SHA256";
    case 0x1302: return "TLS_AES_256_GCM_SHA384";
    case TLS_SUITE_ECDHE_ECDSA_AES128_GCM_SHA256: return "ECDHE_ECDSA_AES_128_GCM_SHA256";
    case TLS_SUITE_ECDHE_ECDSA_AES256_GCM_SHA384: return "ECDHE_ECDSA_AES_256_GCM_SHA384";
    case TLS_SUITE_ECDHE_RSA_AES128_GCM_SHA256:   return "ECDHE_RSA_AES_128_GCM_SHA256";
    case TLS_SUITE_ECDHE_RSA_AES256_GCM_SHA384:   return "ECDHE_RSA_AES_256_GCM_SHA384";
    default: return "unknown";
    }
}
static void capture_info(void) {
    x509_cert_t *leaf = &tls_conn.chain[0];
    int bits, n;
    char *k = tinfo.key;
    tinfo.tls13 = tls_conn.tls13;
    put(tinfo.suite, sizeof tinfo.suite, suite_name(tls_conn.cipher_suite));
    sset(tinfo.cn, sizeof tinfo.cn, leaf->cn, leaf->cn_len);
    tinfo.issuer[0] = 0;
    if (tls_conn.chain_len > 1) sset(tinfo.issuer, sizeof tinfo.issuer, tls_conn.chain[1].cn, tls_conn.chain[1].cn_len);
    tinfo.nb = (long long)leaf->not_before; tinfo.na = (long long)leaf->not_after;
    tinfo.chain = tls_conn.chain_len;
    if (leaf->ec_group == TLS_GROUP_SECP256R1) put(k, sizeof tinfo.key, "ECDSA P-256");
    else if (leaf->ec_group == TLS_GROUP_SECP384R1) put(k, sizeof tinfo.key, "ECDSA P-384");
    else if (leaf->pubkey_valid) {
        bits = (int)bn_bit_length(&leaf->pubkey_modulus);
        put(k, sizeof tinfo.key, "RSA ");
        n = 4; if (bits >= 1000) k[n++] = (char)('0' + bits / 1000 % 10);
        k[n++] = (char)('0' + bits / 100 % 10); k[n++] = (char)('0' + bits / 10 % 10); k[n++] = (char)('0' + bits % 10);
        put(k + n, (int)sizeof tinfo.key - n, "-bit");
    } else put(k, sizeof tinfo.key, "unknown");
}
const mb_tls_info_t *mb_tls_info(void) { return &tinfo; }
int mb_root_count(void) { return TRUSTED_ROOT_COUNT; }
const char *mb_root_name(int i, int *legacy, const char **sha) {
    if (i < 0 || i >= TRUSTED_ROOT_COUNT) { *legacy = 0; *sha = ""; return ""; }
    *legacy = trusted_root_legacy[i]; *sha = trusted_root_sha256[i];
    return trusted_root_names[i];
}
int mb_tls_net_failed(void) { return tnet; }

/* civil date from days since 1970-01-01 (proleptic Gregorian) */
static u64 now_packed(void) {
    long long t = mb_plat_time(), d, z, era, doe, yoe, y, doy, mp, dd, m, sec;
    sec = t % 86400; d = t / 86400;
    if (sec < 0) { sec += 86400; d--; }
    z = d + 719468; era = (z >= 0 ? z : z - 146096) / 146097; doe = z - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365; y = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100); mp = (5 * doy + 2) / 153;
    dd = doy - (153 * mp + 2) / 5 + 1; m = mp < 10 ? mp + 3 : mp - 9; if (m <= 2) y++;
    return tls_pack_datetime((u32)y, (u32)m, (u32)dd, (u32)(sec / 3600), (u32)(sec % 3600 / 60), (u32)(sec % 60));
}
static const char *why(void) {
    static char b[96];
    int i, n;
    const char *p;
    switch (tls_conn.fail_reason) {
    case TLS_FAIL_TCP: return "The connection was closed during the TLS handshake.";
    case TLS_FAIL_CERT_UNTRUSTED: return "The server certificate is not trusted (it does not chain to a known root).";
    case TLS_FAIL_CERT_CHAIN: return "The certificate chain is invalid or expired.";
    case TLS_FAIL_CERT_PARSE: return "The server certificate could not be read.";
    case TLS_FAIL_CERT_EXTENSION: return "The server certificate has unsupported or disallowed extensions.";
    case TLS_FAIL_HOSTNAME_MISMATCH: return "The certificate is not valid for this host name.";
    case TLS_FAIL_SKE_SIGNATURE: return "The server's key-exchange signature is invalid.";
    case TLS_FAIL_DECRYPT: return "A record failed authentication (possible tampering).";
    case TLS_FAIL_SERVER_FINISHED: return "The handshake could not be verified.";
    case TLS_FAIL_UNSUPPORTED_CIPHER_SUITE: return "The server chose a cipher suite this browser does not support.";
    case TLS_FAIL_UNSUPPORTED_CURVE: return "The server chose an unsupported key-exchange curve (TLS 1.3 supports X25519 only; TLS 1.2 also P-256 and P-384).";
    case TLS_FAIL_PROTOCOL_VERSION: return "The server does not speak TLS 1.2 or 1.3 (or a protocol downgrade was detected).";
    case TLS_FAIL_HELLO_RETRY: return "The server requires a key-exchange group this browser did not offer first (HelloRetryRequest is not supported).";
    case TLS_FAIL_CERT_VERIFY: return "The server's TLS 1.3 CertificateVerify signature is invalid.";
    case TLS_FAIL_BUFFER_OVERFLOW: return "The server sent more handshake data than this browser can hold.";
    case TLS_FAIL_BAD_RECORD: return "The server sent malformed TLS data.";
    case TLS_FAIL_PEER_ALERT:
        p = "The server refused the handshake (TLS alert ";
        for (n = 0; p[n]; n++) b[n] = p[n];
        i = tls_conn.alert_desc;
        if (i >= 100) b[n++] = '0' + i / 100;
        if (i >= 10) b[n++] = '0' + i / 10 % 10;
        b[n++] = '0' + i % 10; b[n++] = ')'; b[n++] = '.'; b[n] = 0;
        return b;
    default: return "The TLS handshake failed.";
    }
}
const char *mb_tls_error(void) { return terr; }

int mb_tls_open(const char *host, int port) {
    u8 seed[16] = {0};
    tls_state_t st;
    tcp_host = host;
    tnet = 0;
    tls_connect(0, (u16)port, host, seed);
    for (;;) {
        st = tls_poll(now_packed());
        if (st == TLS_ESTABLISHED) { capture_info(); return 0; }
        if (st == TLS_FAILED) { terr = why(); tnet = tls_conn.fail_reason == TLS_FAIL_TCP; tcp_close(); return -1; }
        if (tcp_conn.state == TCP_CLOSED) {
            tnet = 1;
            terr = st == TLS_WAIT_TCP || tcp_h < 0 ? "Could not connect to the server." : "The connection closed during the TLS handshake (or timed out).";
            tcp_close(); return -1;
        }
    }
}
int mb_tls_send(const void *buf, int n) { return tls_send_app_data((const u8 *)buf, (u32)n); }
int mb_tls_recv(void *buf, int cap) {
    u32 r0, a0, h0;
    tls_state_t s0;
    for (;;) {
        if (tls_conn.app_recv_len) return tls_poll_recv_app_data((u8 *)buf, (u16)(cap > 65535 ? 65535 : cap));
        if (tls_conn.state == TLS_FAILED) { terr = why(); return -1; }
        if (tls_conn.peer_close_notify || tcp_conn.state == TCP_CLOSED) {
            if (tls_conn.rx_raw_len < 5) return 0;   /* nothing left to decode */
        }
        r0 = tls_conn.rx_raw_len; a0 = tls_conn.app_recv_len; h0 = tls_conn.hs_buf_len; s0 = tls_conn.state;
        /* a whole record may already be buffered: decode it before blocking on the socket again */
        if (r0 >= 5 && r0 >= 5u + (((u32)TLS_RX_RAW[3] << 8) | TLS_RX_RAW[4])) {
            tls_process_raw_buffer(now_packed());
            if (tls_conn.rx_raw_len != r0 || tls_conn.app_recv_len != a0 || tls_conn.hs_buf_len != h0 || tls_conn.state != s0) continue;
        }
        if (tcp_conn.state == TCP_CLOSED) return 0;
        tls_poll(now_packed());
    }
}
void mb_tls_close(void) { tls_close(); }

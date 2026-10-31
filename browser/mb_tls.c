/* SPDX-License-Identifier: GPL-3.0-only */
/* MiniBrowser TLS driver: runs the polled TLS 1.2 state machine (tls/tls.h) to completion
 * over a blocking platform socket. Certificates are always verified (chain, validity window,
 * hostname, key-exchange signature); there is no "continue anyway" path. */
#include "tls/tls.h"
#include "mb.h"

static const char *terr = "";

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
    case TLS_FAIL_UNSUPPORTED_CURVE: return "The server chose an unsupported key-exchange curve (only X25519, P-256 and P-384 are supported).";
    case TLS_FAIL_PROTOCOL_VERSION: return "The server does not speak TLS 1.2.";
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
    tls_connect(0, (u16)port, host, seed);
    for (;;) {
        st = tls_poll(now_packed());
        if (st == TLS_ESTABLISHED) return 0;
        if (st == TLS_FAILED) { terr = why(); tcp_close(); return -1; }
        if (tcp_conn.state == TCP_CLOSED) {
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

/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef TCP_H
#define TCP_H
#include "io.h"
#include "../mb.h"
/* The TLS state machine talks to "tcp_*"; here that is one blocking socket supplied by the
 * platform layer (mb_plat_*). tcp_host is the name tls_connect() should reach. */
typedef enum { TCP_CLOSED = 0, TCP_ESTABLISHED = 1 } tcp_state_t;
typedef struct { tcp_state_t state; u32 recv_len; } tcp_conn_t;
static tcp_conn_t tcp_conn;
static const char *tcp_host;
static int tcp_h = -1;

static inline void tcp_connect(u32 ip, u16 port) {
    (void)ip;
    tcp_h = mb_plat_open(tcp_host, port);
    tcp_conn.state = tcp_h >= 0 ? TCP_ESTABLISHED : TCP_CLOSED;
}
static inline u32 tcp_send_space(void) { return 1u << 20; }
static inline int tcp_send_data(const u8 *d, u16 n) {
    int off = 0;
    while (off < n) {
        int k = mb_plat_send(tcp_h, d + off, n - off);
        if (k <= 0) { tcp_conn.state = TCP_CLOSED; return 0; }
        off += k;
    }
    return 1;
}
/* Blocks (platform timeout) until bytes arrive; 0 means closed, failed or timed out. */
static inline u16 tcp_poll_recv(u8 *out, u16 max) {
    int n = tcp_h < 0 ? -1 : mb_plat_recv(tcp_h, out, max);
    if (n <= 0) { tcp_conn.state = TCP_CLOSED; return 0; }
    return (u16)n;
}
static inline void tcp_close(void) {
    if (tcp_h >= 0) mb_plat_close(tcp_h);
    tcp_h = -1;
    tcp_conn.state = TCP_CLOSED;
}
#endif

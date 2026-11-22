/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef TCP_H
#define TCP_H
#include "io.h"
#include "../mb.h"
/* The TLS state machine talks to "tcp_*". Since v0.10 that is a NON-BLOCKING socket from the platform
 * layer (mb_plat_connect / connect_poll / send / recv): nothing in here ever waits. Every call works on the
 * connection tcp_cur points at (tls_select() sets it), so any number of connections can be driven in turn.
 * Outgoing bytes are queued and flushed by tcp_pump(); incoming bytes are pulled by tcp_poll_recv(). */
typedef enum { TCP_CLOSED = 0, TCP_ESTABLISHED = 1, TCP_CONNECTING = 2 } tcp_state_t;
#define MB_TXQ 70000
typedef struct {
    tcp_state_t state; u32 recv_len;
    int h, eof;                 /* platform handle; eof=1 when the peer closed cleanly */
    u32 txn, txo;               /* queued / already flushed bytes */
    u8 txq[MB_TXQ];
} tcp_conn_t;
static tcp_conn_t *tcp_cur;
#define tcp_conn (*tcp_cur)
static const char *tcp_host;

static inline void tcp_connect(u32 ip, u16 port) {
    (void)ip;
    tcp_conn.txn = tcp_conn.txo = 0; tcp_conn.eof = 0; tcp_conn.recv_len = 0;
    tcp_conn.h = mb_plat_connect(tcp_host, port);
    tcp_conn.state = tcp_conn.h >= 0 ? TCP_CONNECTING : TCP_CLOSED;
}
static inline void tcp_flush(void) {
    while (tcp_conn.state == TCP_ESTABLISHED && tcp_conn.txo < tcp_conn.txn) {
        int k = mb_plat_send(tcp_conn.h, tcp_conn.txq + tcp_conn.txo, (int)(tcp_conn.txn - tcp_conn.txo));
        if (k < 0) { tcp_conn.state = TCP_CLOSED; return; }
        if (k == 0) return;                                 /* would block: try again on the next tick */
        tcp_conn.txo += (u32)k;
    }
    if (tcp_conn.txo >= tcp_conn.txn) tcp_conn.txo = tcp_conn.txn = 0;
}
/* advances connect-in-progress and flushes queued output; cheap, called at the top of every poll */
static inline void tcp_pump(void) {
    if (tcp_conn.state == TCP_CONNECTING) {
        int r = mb_plat_connect_poll(tcp_conn.h);
        if (r > 0) tcp_conn.state = TCP_ESTABLISHED;
        else if (r < 0) tcp_conn.state = TCP_CLOSED;
    }
    tcp_flush();
}
static inline u32 tcp_send_space(void) { return MB_TXQ - tcp_conn.txn; }
static inline int tcp_send_data(const u8 *d, u16 n) {
    u32 i;
    if (tcp_conn.state == TCP_CLOSED || n > tcp_send_space()) return 0;
    for (i = 0; i < n; i++) tcp_conn.txq[tcp_conn.txn + i] = d[i];
    tcp_conn.txn += n;
    tcp_flush();
    return tcp_conn.state != TCP_CLOSED;
}
/* Returns what is available right now (0 = nothing yet). A closed or failed connection flips to TCP_CLOSED. */
static inline u16 tcp_poll_recv(u8 *out, u16 max) {
    int n;
    if (tcp_conn.state != TCP_ESTABLISHED) return 0;
    n = mb_plat_recv(tcp_conn.h, out, max);
    if (n > 0) return (u16)n;
    if (n == -2) return 0;                                  /* would block */
    tcp_conn.eof = n == 0;
    tcp_conn.state = TCP_CLOSED;
    return 0;
}
static inline void tcp_close(void) {
    if (tcp_conn.h >= 0) mb_plat_close(tcp_conn.h);
    tcp_conn.h = -1;
    tcp_conn.state = TCP_CLOSED;
}
#endif

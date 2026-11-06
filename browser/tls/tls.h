/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef TLS_H
#define TLS_H
#include "io.h"
#include "sha256.h"
#include "hmac_sha256.h"
#include "aes.h"
#include "gcm.h"
#include "bignum.h"
#include "x25519.h"
#include "x509.h"
#include "pkcs1.h"
#include "sha512.h"
#include "hmac_sha384.h"
#include "ecc.h"
#include "trusted_roots.h"
#ifdef MW_TLS_TEST_ROOTS
#include MW_TLS_TEST_ROOTS   /* defines MW_TEST_ROOT_COUNT, mw_test_roots[], mw_test_root_lens[] */
#endif
#include "tcp.h"
#include "serial.h"

#ifndef MW_TLS_RAW_SIZE
#define MW_TLS_RAW_SIZE 20480   /* one max-size record (16413) + headroom */
#define MW_TLS_HS_SIZE  65536   /* reassembled handshake bytes: certificate chains */
#define MW_TLS_APP_SIZE 65536   /* decrypted application data waiting for the reader */
#endif

/* ============================================================
 * tls.h -- TLS 1.2, one cipher suite only:
 * TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256 (0xC02F). X25519 for key
 * exchange, RSA-PKCS1v1.5-SHA256 for authenticating the exchange (and
 * the certificate chain that backs it), AES-128-GCM for the record
 * cipher, SHA-256 for the handshake PRF.
 *
 * This is NOT the cipher suite this client started development with --
 * see the earlier design notes for TLS_RSA_WITH_AES_128_CBC_SHA256 (no
 * forward secrecy, RSA key transport instead of ECDHE). That suite was
 * abandoned after a real, security-conscious HTTPS endpoint flatly
 * rejected it with a handshake_failure alert during testing: static RSA
 * key exchange has fallen out of favor industry-wide for lacking
 * forward secrecy, and enough of the real web has dropped it that a
 * TLS client without ECDHE genuinely can't reach much of it anymore.
 * ECDHE_RSA with AES-GCM is the suite that actually works against real
 * servers today, so that's what got built, cipher-math complexity be
 * damned.
 *
 * Everything this client does NOT support -- and simply refuses,
 * rather than pretends to handle -- follows from that one cipher
 * suite: no TLS 1.3 (a different-enough protocol that "TLS 1.2 client,
 * pick one suite" doesn't extend to it for free), no ECDSA
 * certificates, no session resumption, no renegotiation, no client
 * certificates, no ALPN. A server that requires any of those fails
 * this connection cleanly (TLS_STATE_FAILED, a specific reason logged)
 * rather than silently misbehaving.
 * ============================================================ */

#define TLS_VERSION_1_2 0x0303
#define TLS_VERSION_1_3 0x0304

#define TLS_CONTENT_CHANGE_CIPHER_SPEC 20
#define TLS_CONTENT_ALERT              21
#define TLS_CONTENT_HANDSHAKE          22
#define TLS_CONTENT_APPLICATION_DATA   23

#define TLS_HS_HELLO_REQUEST        0
#define TLS_HS_CLIENT_HELLO         1
#define TLS_HS_SERVER_HELLO         2
#define TLS_HS_NEW_SESSION_TICKET   4
#define TLS_HS_ENCRYPTED_EXTENSIONS 8
#define TLS_HS_CERTIFICATE          11
#define TLS_HS_SERVER_KEY_EXCHANGE  12
#define TLS_HS_CERTIFICATE_REQUEST  13
#define TLS_HS_SERVER_HELLO_DONE    14
#define TLS_HS_CERTIFICATE_VERIFY   15
#define TLS_HS_CLIENT_KEY_EXCHANGE  16
#define TLS_HS_FINISHED             20
#define TLS_HS_KEY_UPDATE           24

/* Cipher suites this client offers, in preference order. All four are TLS 1.2 ECDHE AEAD suites
 * -- forward secret, GCM -- differing in (a) what signs the key exchange, the server certificate's
 * key type (ECDSA vs RSA) and (b) the AES key size / hash (128+SHA-256 vs 256+SHA-384). Before
 * pre-24 only the last-but-one (0xC02F) was offered, so any server whose certificate is ECDSA --
 * most Google/Cloudflare-fronted sites -- had no suite in common with us and answered
 * handshake_failure. */
#define TLS_SUITE_ECDHE_ECDSA_AES128_GCM_SHA256 0xC02B
#define TLS_SUITE_ECDHE_ECDSA_AES256_GCM_SHA384 0xC02C
#define TLS_SUITE_ECDHE_RSA_AES128_GCM_SHA256   0xC02F
#define TLS_SUITE_ECDHE_RSA_AES256_GCM_SHA384   0xC030

static inline int tls_suite_supported(u16 s) {
    return s == TLS_SUITE_ECDHE_ECDSA_AES128_GCM_SHA256 || s == TLS_SUITE_ECDHE_ECDSA_AES256_GCM_SHA384 ||
           s == TLS_SUITE_ECDHE_RSA_AES128_GCM_SHA256   || s == TLS_SUITE_ECDHE_RSA_AES256_GCM_SHA384;
}
static inline int tls_suite_is_ecdsa(u16 s)  { return s == TLS_SUITE_ECDHE_ECDSA_AES128_GCM_SHA256 || s == TLS_SUITE_ECDHE_ECDSA_AES256_GCM_SHA384; }
#define TLS_SUITE_13_AES128_GCM_SHA256 0x1301   /* TLS 1.3 suites (RFC 8446 B.4) */
#define TLS_SUITE_13_AES256_GCM_SHA384 0x1302
static inline int tls_suite_is_sha384(u16 s) { return s == TLS_SUITE_ECDHE_ECDSA_AES256_GCM_SHA384 || s == TLS_SUITE_ECDHE_RSA_AES256_GCM_SHA384 || s == TLS_SUITE_13_AES256_GCM_SHA384; }
static inline int tls_suite_key_len(u16 s)   { return tls_suite_is_sha384(s) ? 32 : 16; }   /* AES-256 vs AES-128 */

/* Named groups (RFC 8422): the elliptic curves the ephemeral key exchange may run on. */
#define TLS_GROUP_SECP256R1 0x0017
#define TLS_GROUP_SECP384R1 0x0018
#define TLS_GROUP_X25519    0x001D

#define TLS_MAX_RECORD_PLAINTEXT 16384   /* TLS spec's own per-record cap */
/* RFC 5246 6.2.3: a ciphertext fragment may exceed the plaintext cap by at most 2048 bytes (MAC/padding/nonce/tag
 * expansion). Anything longer than this is not a TLS 1.2 record, whatever its header claims. */
#define TLS_MAX_RECORD_CIPHERTEXT (TLS_MAX_RECORD_PLAINTEXT + 2048)
/* rx_raw MUST be able to hold one complete record even at the maximum
 * legal size -- 5-byte header + 8-byte explicit nonce + up to 16384
 * bytes of plaintext + 16-byte GCM tag = 16413 bytes worst case. A
 * buffer smaller than that isn't just slow for large responses, it's
 * an outright deadlock: tls_process_raw_buffer() correctly waits for a
 * record's declared length to fully arrive before processing it (this
 * is *correct* stream-reassembly behavior, not a bug), but if that
 * length can never fit in the buffer at all, "wait for more data" never
 * resolves -- discovered exactly this way, testing against a real
 * server that (reasonably) sent its response as one large record. Sized
 * with a little headroom above the bare minimum for whatever of the
 * next record's bytes TCP happens to deliver in the same read. */
#define TLS_RX_RAW_BUF_SIZE   MW_TLS_RAW_SIZE   /* 20KB: 16413-byte max record + headroom */
#define TLS_HS_BUF_SIZE       MW_TLS_HS_SIZE  /* 32KB (was 8KB): reassembled handshake-content bytes,
                                       * not yet split into messages -- real
                                       * certificate chains (a leaf plus one
                                       * or two intermediates) run a few KB;
                                       * this has comfortable headroom above
                                       * anything actually observed */
#define TLS_APP_RECV_BUF_SIZE MW_TLS_APP_SIZE /* 32KB (was 4KB) -- NOT sized for a worst-case 16384-byte
                                      * record -- see tls_process_raw_buffer()'s
                                      * application_data handling, which
                                      * truncates (keeps the first
                                      * TLS_APP_RECV_BUF_SIZE bytes, discards
                                      * the rest) rather than treating an
                                      * oversized response as fatal. This
                                      * matches https.h's own response
                                      * buffer size, which matches http.h's
                                      * -- this client only ever previews a
                                      * page's opening bytes, so keeping
                                      * app_recv sized for that instead of
                                      * for "the biggest single record TLS
                                      * legally allows" saves real BSS
                                      * budget for a limit this client
                                      * already imposes one layer up anyway. */
#define TLS_CHAIN_MAX 5              /* (was 3) leaf + up to four intermediates/root. Some servers send a cross-signed extra.
                                      * Real chains are overwhelmingly 2-3
                                      * certificates (a server sending
                                      * leaf+intermediate and trusting the
                                      * client already has the root is the
                                      * common case; leaf+intermediate+root
                                      * explicitly, as this client's own
                                      * test network happened to send, is
                                      * the other common case) -- a server
                                      * sending a longer chain than that
                                      * fails cleanly here rather than
                                      * following it. */

typedef enum {
    TLS_CLOSED = 0,
    TLS_WAIT_TCP,             /* underlying tcp_connect() not ESTABLISHED yet */
    TLS_CLIENT_HELLO_SENT,    /* waiting for ServerHello..ServerHelloDone */
    TLS_CLIENT_FINISHED_SENT, /* waiting for server's ChangeCipherSpec+Finished */
    TLS_ESTABLISHED,
    TLS_FAILED,
} tls_state_t;

/* Why a handshake failed -- kept as plain reason codes rather than only
 * a serial-log line, so a caller (the browser) can
 * show something more specific than "it didn't work." */
typedef enum {
    TLS_FAIL_NONE = 0,
    TLS_FAIL_TCP,
    TLS_FAIL_UNEXPECTED_MESSAGE,
    TLS_FAIL_UNSUPPORTED_CIPHER_SUITE,   /* server picked something other
                                          * than the one suite we offered --
                                          * shouldn't happen since we only
                                          * offer one, but a server that
                                          * echoes back nonsense gets
                                          * caught here rather than trusted */
    TLS_FAIL_UNSUPPORTED_CURVE,          /* ServerKeyExchange named a curve
                                          * other than x25519 */
    TLS_FAIL_CERT_PARSE,
    TLS_FAIL_CERT_CHAIN,                 /* a link in the chain didn't
                                          * verify (wrong issuer, bad
                                          * signature, expired, ...) */
    TLS_FAIL_CERT_UNTRUSTED,             /* chain is internally consistent
                                          * but doesn't reach any of our
                                          * embedded trusted roots */
    TLS_FAIL_HOSTNAME_MISMATCH,
    TLS_FAIL_SKE_SIGNATURE,              /* ServerKeyExchange's signature
                                          * didn't verify against the leaf
                                          * cert's key */
    TLS_FAIL_DECRYPT,                    /* a GCM tag check failed --
                                          * treated as tampering, full stop */
    TLS_FAIL_SERVER_FINISHED,            /* server's Finished verify_data
                                          * didn't match what we computed */
    TLS_FAIL_BUFFER_OVERFLOW,            /* a message or chain was bigger
                                          * than this client's fixed buffers --
                                          * refuses to overrun them rather
                                          * than silently truncate */
    TLS_FAIL_PEER_ALERT,                 /* the server sent a fatal alert;
                                          * tls_conn.alert_desc says which
                                          * (40 = handshake_failure, 70 =
                                          * protocol_version, 71 =
                                          * insufficient_security, ...) --
                                          * appended at the END of this enum
                                          * so every older reason code keeps
                                          * its number */
    TLS_FAIL_BAD_RECORD,                 /* a record header that no honest TLS 1.2 peer sends: unknown
                                          * content type, impossible version, or a length that cannot fit
                                          * the receive buffer (also appended at the end, same reason) */
    TLS_FAIL_PROTOCOL_VERSION,           /* ServerHello chose a protocol version this client never
                                          * offered (it offers TLS 1.2 and nothing else) */
    TLS_FAIL_HELLO_RETRY,                /* TLS 1.3 HelloRetryRequest: the server wants a key share we did not send */
    TLS_FAIL_CERT_VERIFY,                /* TLS 1.3 CertificateVerify did not verify against the leaf certificate */
    TLS_FAIL_CERT_EXTENSION,             /* a certificate in the chain has a critical extension this
                                          * client does not understand, or a keyUsage / basicConstraints
                                          * combination that does not allow its role in the chain */
} tls_fail_reason_t;

typedef struct {
    tls_state_t state;
    tls_fail_reason_t fail_reason;

    char hostname[128];

    u8 alert_level, alert_desc;   /* the last alert record the peer sent (0/0 = none yet) */
    int peer_close_notify;        /* the peer sent close_notify: it will send no more application data,
                                   * whether or not its TCP FIN has arrived yet (some servers dawdle) */

    u8 client_random[32];
    u8 server_random[32];

    u16 cipher_suite;    /* the suite the server picked (one of TLS_SUITE_*) */
    u16 kx_curve;        /* the named group of the ephemeral key exchange (TLS_GROUP_*) */

    u8 my_private[48];   /* our ephemeral private key: 32 bytes for X25519/P-256, 48 for P-384 */
    u8 my_public[97];    /* ...and its public half: 32 bytes (X25519) or 0x04||X||Y (65/97) */
    u32 my_public_len;
    u8 peer_public[97];  /* the server's ephemeral public key, from ServerKeyExchange */
    u32 peer_public_len;

    u8 master_secret[48];

    /* ---- TLS 1.3 (RFC 8446) ---- */
    int tls13;              /* 1 once the ServerHello selected TLS 1.3 */
    int t13_stage;          /* 0 EncryptedExtensions, 1 Certificate, 2 CertificateVerify, 3 Finished */
    int t13_certreq;        /* the server asked for a client certificate (we answer with an empty one) */
    int t13_folded;         /* the Finished handler already hashed the server Finished into the transcript */
    u8 session_id[32];      /* legacy_session_id we sent (the server must echo it) */
    u8 c_hs[48], s_hs[48];  /* client/server handshake traffic secrets */
    u8 master13[48];        /* master secret (after the handshake secret was used) */
    u8 c_ap[48], s_ap[48];  /* client/server application traffic secrets */
    u8 c_iv13[12], s_iv13[12];

    aes128_ctx_t client_write_aes, server_write_aes;
    u8 client_write_iv[4], server_write_iv[4];   /* the GCM "salt" half of
                                                  * each direction's nonce */
    u64 client_seq, server_seq;
    int write_cipher_active, read_cipher_active;

    sha512_ctx_t transcript384; /* the same running hash, but SHA-384: the *_SHA384 suites hash the
                                 * handshake with it, and the suite isn't known until ServerHello --
                                 * after the ClientHello is already hashed -- so both are kept. */
    sha256_ctx_t transcript;   /* running hash over every handshake message,
                               * sent or received, in order -- see
                               * tls_transcript_update() */

    x509_cert_t chain[TLS_CHAIN_MAX];
    int chain_len;

    /* The three big byte buffers (raw record bytes, reassembled handshake bytes, decrypted
     * application data) are NOT members any more: they sit in the net arena (memmap.h) as
     * TLS_RX_RAW / TLS_HS_BUF / TLS_APP_RECV, which took 29KB out of .bss. Only their fill
     * levels live here. */
    u32 rx_raw_len;
    u32 hs_buf_len;
    u32 app_recv_len;

    /* A tiny xorshift-like PRNG state, seeded once by whatever entropy
     * kernel/tls.h's caller provides (see tls_connect()'s seed
     * parameter) -- used only for ClientHello's client_random and our
     * ephemeral X25519 private key (this cipher suite needs no PKCS#1
     * *encryption* padding at all -- ECDHE carries the secret, not RSA).
     * Not a hardened CSPRNG -- see tls_connect()'s own comment for the
     * honest caveat on that. */
    u8 rng_state[32];   /* hash-based generator state -- see tls_rng_* */
    u32 rng_counter;
} tls_conn_t;

/* The one TLS connection and its three big buffers are plain statics in a hosted build. */
static tls_conn_t tls_conn_s;
#define tls_conn tls_conn_s
static u8 tls_rx_raw_buf[MW_TLS_RAW_SIZE];
static u8 tls_hs_buf_mem[MW_TLS_HS_SIZE];
static u8 tls_app_recv_buf[MW_TLS_APP_SIZE];
#define TLS_RX_RAW   tls_rx_raw_buf
#define TLS_HS_BUF   tls_hs_buf_mem
#define TLS_APP_RECV tls_app_recv_buf

/* ---- random numbers: the operating system's CSPRNG, via the platform layer ----
 * Every secret this client creates (client_random, ephemeral private keys) comes straight from
 * mb_plat_random(). A hosted program has a real entropy source, so no home-grown generator and
 * no x86-only instructions are needed. tls_rng_seed() is kept so callers stay unchanged. */
static inline void tls_rng_seed(const u8 seed[16]) { (void)seed; }
static inline void tls_rng_bytes(u8 *out, u32 len) { mb_plat_random(out, (int)len); }

/* ---- TLS 1.2's PRF: P_SHA256(secret, label||seed), per RFC 5246 5 ----
 * A(0) = seed; A(i) = HMAC(secret, A(i-1))
 * P_hash = HMAC(secret, A(1)||seed) || HMAC(secret, A(2)||seed) || ...
 * truncated to out_len bytes. Every session key this connection ever
 * uses (master secret, then the whole key_block) comes out of this one
 * function, just with different (secret, label, seed, out_len). */
static inline void tls_hmac(u8 *out, const u8 *key, u32 key_len, const u8 *msg, u32 msg_len) {
    if (tls_suite_is_sha384(tls_conn.cipher_suite)) hmac_sha384(key, key_len, msg, msg_len, out);
    else hmac_sha256(key, key_len, msg, msg_len, out);
}

/* The PRF's hash follows the negotiated suite: SHA-256 for the *_SHA256 suites, SHA-384 for the
 * *_SHA384 ones (RFC 5288 / 5289). h is its output size (32 or 48) -- the A(i) values and the
 * blocks are h bytes each. */
static inline void tls_prf(u8 *out, u32 out_len,
                            const u8 *secret, u32 secret_len,
                            const char *label,
                            const u8 *seed, u32 seed_len) {
    u32 h = tls_suite_is_sha384(tls_conn.cipher_suite) ? 48 : 32;
    u8 label_seed[256];
    u32 label_len = 0;
    while (label[label_len]) label_len++;
    u32 ls_len = label_len + seed_len;
    for (u32 i = 0; i < label_len; i++) label_seed[i] = (u8)label[i];
    for (u32 i = 0; i < seed_len; i++) label_seed[label_len + i] = seed[i];

    u8 a[48];
    tls_hmac(a, secret, secret_len, label_seed, ls_len); /* A(1) */

    u32 produced = 0;
    while (produced < out_len) {
        u8 a_plus_seed[48 + 256];
        for (u32 i = 0; i < h; i++) a_plus_seed[i] = a[i];
        for (u32 i = 0; i < ls_len; i++) a_plus_seed[h + i] = label_seed[i];

        u8 block[48];
        tls_hmac(block, secret, secret_len, a_plus_seed, h + ls_len);

        u32 take = out_len - produced;
        if (take > h) take = h;
        for (u32 i = 0; i < take; i++) out[produced + i] = block[i];
        produced += take;

        u8 next_a[48];
        tls_hmac(next_a, secret, secret_len, a, h);
        for (u32 i = 0; i < h; i++) a[i] = next_a[i];
    }
}

/* Feeds one complete handshake message's raw bytes (4-byte type+length
 * header, then body) into the running transcript hash. Called exactly
 * once per message, at the point it's sent or fully received/dispatched
 * -- see the file header for why message order here has to exactly
 * match both sides' actual send/receive order. */
static inline void tls_transcript_update(const u8 *msg, u32 msg_len) {
#ifdef TLS_DEBUG_TRANSCRIPT
    fprintf(stderr, "[transcript] type=%d len=%u bytes=", msg[0], msg_len);
    for (u32 i = 0; i < msg_len; i++) fprintf(stderr, "%02x", msg[i]);
    fprintf(stderr, "\n");
#endif
    sha256_update(&tls_conn.transcript, msg, msg_len);
    sha512_update(&tls_conn.transcript384, msg, msg_len);
}


/* ======================= TLS 1.3 (RFC 8446) =======================
 * Key schedule (HKDF), record protection (AEAD with per-record nonce = IV xor sequence number,
 * inner content type), CertificateVerify (ECDSA P-256/P-384 and RSA-PSS), Finished and KeyUpdate.
 * Not supported: HelloRetryRequest, PSK / session tickets / 0-RTT, client certificates (an empty
 * Certificate is sent if the server asks), ChaCha20-Poly1305 (AES-GCM suites only). */
static inline u32 tls13_hlen(void) { return tls_suite_is_sha384(tls_conn.cipher_suite) ? 48 : 32; }

/* Hash of everything in the transcript so far (the suite's hash). */
static inline void tls13_thash(u8 *out) {
    if (tls13_hlen() == 48) { sha512_ctx_t c = tls_conn.transcript384; sha384_final(&c, out); }
    else { sha256_ctx_t c = tls_conn.transcript; sha256_final(&c, out); }
}
static inline void tls13_empty_hash(u8 *out) {
    if (tls13_hlen() == 48) sha384((const u8 *)"", 0, out); else sha256((const u8 *)"", 0, out);
}
/* HKDF-Extract(salt, ikm) = HMAC(salt, ikm) */
static inline void tls13_extract(u8 *out, const u8 *salt, u32 sl, const u8 *ikm, u32 il) { tls_hmac(out, salt, sl, ikm, il); }

/* HKDF-Expand-Label(secret, label, context, olen). Every use here asks for at most one hash
 * block of output, so HKDF-Expand is a single HMAC: T(1) = HMAC(secret, info || 0x01). */
static inline void tls13_expand_label(u8 *out, u32 olen, const u8 *secret, const char *label, const u8 *ctx, u32 cl) {
    u8 info[2 + 1 + 6 + 16 + 1 + 48 + 1];
    u8 blk[48];
    u32 p = 0, ll = 0, i;
    while (label[ll]) ll++;
    info[p++] = (u8)(olen >> 8); info[p++] = (u8)olen;
    info[p++] = (u8)(6 + ll);
    for (i = 0; i < 6; i++) info[p++] = (u8)"tls13 "[i];
    for (i = 0; i < ll; i++) info[p++] = (u8)label[i];
    info[p++] = (u8)cl;
    for (i = 0; i < cl; i++) info[p++] = ctx[i];
    info[p++] = 1;
    tls_hmac(blk, secret, tls13_hlen(), info, p);
    for (i = 0; i < olen; i++) out[i] = blk[i];
}
/* Derive-Secret(secret, label, transcript_hash) */
static inline void tls13_derive(u8 *out, const u8 *secret, const char *label, const u8 *thash) {
    tls13_expand_label(out, tls13_hlen(), secret, label, thash, tls13_hlen());
}
static inline void tls13_traffic_keys(const u8 *secret, aes128_ctx_t *aes, u8 iv[12]) {
    u8 key[32];
    u32 kl = (u32)tls_suite_key_len(tls_conn.cipher_suite);
    tls13_expand_label(key, kl, secret, "key", (const u8 *)"", 0);
    aes_set_key(aes, key, (int)kl);
    tls13_expand_label(iv, 12, secret, "iv", (const u8 *)"", 0);
}
static inline int tls13_ct_equal(const u8 *a, const u8 *b, u32 n) {
    u8 d = 0; u32 i;
    for (i = 0; i < n; i++) d |= (u8)(a[i] ^ b[i]);
    return d == 0;
}
static inline void tls13_finished_mac(u8 *out, const u8 *base_secret, const u8 *thash) {
    u8 fk[48];
    u32 h = tls13_hlen();
    tls13_expand_label(fk, h, base_secret, "finished", (const u8 *)"", 0);
    tls_hmac(out, fk, h, thash, h);
}

/* Called while handling ServerHello (the transcript already holds ClientHello + ServerHello):
 * ECDHE with the server's X25519 share, then handshake secret -> traffic keys for both directions. */
static inline int tls13_after_server_hello(const u8 *server_share) {
    u8 shared[32], zero[48], early[48], derived[48], eh[48], th[48], hs[48];
    u32 h = tls13_hlen(), i;
    u8 nz = 0;
    for (i = 0; i < 48; i++) zero[i] = 0;
    x25519_scalarmult(shared, tls_conn.my_private, server_share);
    for (i = 0; i < 32; i++) nz |= shared[i];
    if (!nz) { tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CURVE; return 0; }   /* low-order point */
    tls13_empty_hash(eh);
    tls13_extract(early, zero, h, zero, h);
    tls13_derive(derived, early, "derived", eh);
    tls13_extract(hs, derived, h, shared, 32);
    tls13_thash(th);                                   /* ClientHello..ServerHello */
    tls13_derive(tls_conn.c_hs, hs, "c hs traffic", th);
    tls13_derive(tls_conn.s_hs, hs, "s hs traffic", th);
    tls13_derive(derived, hs, "derived", eh);
    tls13_extract(tls_conn.master13, derived, h, zero, h);
    tls13_traffic_keys(tls_conn.c_hs, &tls_conn.client_write_aes, tls_conn.c_iv13);
    tls13_traffic_keys(tls_conn.s_hs, &tls_conn.server_write_aes, tls_conn.s_iv13);
    tls_conn.client_seq = 0; tls_conn.server_seq = 0;
    tls_conn.read_cipher_active = 1; tls_conn.write_cipher_active = 1;
    return 1;
}

/* RSASSA-PSS verification (RFC 8017 9.1.2) with MGF1 and a salt as long as the hash, as TLS 1.3 requires. */
static inline int tls13_rsa_pss_verify(int hash_id, const u8 *msg, u32 ml, const u8 *sig, u32 sl,
                                       const bignum_t *n, u32 e) {
    u32 modbits = (u32)bn_bit_length(n), k = (modbits + 7) / 8, embits, emlen, hl, dblen, topbits, pad, i, c;
    bignum_t sb, mb;
    u8 emfull[512], mh[64], db[512], hh[64], mp[8 + 64 + 64], buf[64 + 4], mk[64];
    const u8 *em, *H;
    u8 topmask;
    if (k == 0 || k > 512 || sl != k || modbits < 2) return 0;
    bn_from_bytes_be(&sb, sig, sl);
    bn_modpow_u32exp(&mb, &sb, e, n);
    bn_to_bytes_be(&mb, emfull, k);
    embits = modbits - 1; emlen = (embits + 7) / 8;
    if (k > emlen && emfull[0] != 0) return 0;
    em = emfull + (k - emlen);
    hl = pkcs1_hash(hash_id, msg, ml, mh);
    if (!hl || emlen < 2 * hl + 2 || em[emlen - 1] != 0xBC) return 0;
    dblen = emlen - hl - 1; H = em + dblen;
    topbits = 8 * emlen - embits;
    topmask = (u8)(0xFF << (8 - topbits));
    if (topbits && (em[0] & topmask)) return 0;
    for (i = 0; i < hl; i++) buf[i] = H[i];
    for (c = 0; c * hl < dblen; c++) {
        u32 take;
        buf[hl] = (u8)(c >> 24); buf[hl + 1] = (u8)(c >> 16); buf[hl + 2] = (u8)(c >> 8); buf[hl + 3] = (u8)c;
        pkcs1_hash(hash_id, buf, hl + 4, mk);
        take = dblen - c * hl; if (take > hl) take = hl;
        for (i = 0; i < take; i++) db[c * hl + i] = (u8)(em[c * hl + i] ^ mk[i]);
    }
    if (topbits) db[0] &= (u8)(0xFF >> topbits);
    pad = dblen - hl - 1;
    for (i = 0; i < pad; i++) if (db[i]) return 0;
    if (db[pad] != 0x01) return 0;
    for (i = 0; i < 8; i++) mp[i] = 0;
    for (i = 0; i < hl; i++) mp[8 + i] = mh[i];
    for (i = 0; i < hl; i++) mp[8 + hl + i] = db[pad + 1 + i];
    pkcs1_hash(hash_id, mp, 8 + 2 * hl, hh);
    return tls13_ct_equal(hh, H, hl);
}

/* Writes one TLS record: header (content type, version, length) plus
 * payload, encrypting the payload first if write_cipher_active. This is
 * the ONLY function in this file that calls tcp_send_data() -- every
 * handshake message and every byte of application data funnels through
 * here, so the encryption decision lives in exactly one place. */
static inline int tls_write_record(u8 content_type, const u8 *plaintext, u32 len) {
    u8 record[5 + TLS_MAX_RECORD_PLAINTEXT + 8 + 16];
    u32 payload_len;

    /* Check the TCP send queue has room BEFORE touching anything. The
     * encrypt path below bumps client_seq, and a sequence number that
     * advanced for a record that never left the building desynchronizes
     * the whole connection (every later record would fail its GCM tag
     * on the server). All or nothing. */
    if (5 + len + (tls_conn.write_cipher_active ? (tls_conn.tls13 ? 1 + 16 : 8 + 16) : 0) > tcp_send_space()) return 0;

    if (tls_conn.write_cipher_active && tls_conn.tls13) {
        /* TLS 1.3: inner plaintext = content || content_type; nonce = IV xor 64-bit sequence number;
         * the AAD is the record header itself; the outer type is always application_data. */
        u8 nonce[12], aad[5], tag[16];
        u8 inner[TLS_MAX_RECORD_PLAINTEXT + 1];
        u32 ctl = len + 1 + 16, i;
        for (i = 0; i < 12; i++) nonce[i] = tls_conn.c_iv13[i];
        for (i = 0; i < 8; i++) nonce[4 + i] ^= (u8)(tls_conn.client_seq >> (56 - i * 8));
        aad[0] = TLS_CONTENT_APPLICATION_DATA; aad[1] = 3; aad[2] = 3; aad[3] = (u8)(ctl >> 8); aad[4] = (u8)ctl;
        for (i = 0; i < len; i++) inner[i] = plaintext[i];
        inner[len] = content_type;
        gcm_encrypt(&tls_conn.client_write_aes, nonce, aad, 5, inner, len + 1, tag);
        for (i = 0; i < 5; i++) record[i] = aad[i];
        for (i = 0; i < len + 1; i++) record[5 + i] = inner[i];
        for (i = 0; i < 16; i++) record[5 + len + 1 + i] = tag[i];
        tls_conn.client_seq++;
        return tcp_send_data(record, (u16)(5 + ctl));
    }

    if (tls_conn.write_cipher_active) {
        u8 nonce[12];
        for (int i = 0; i < 4; i++) nonce[i] = tls_conn.client_write_iv[i];
        for (int i = 0; i < 8; i++) nonce[4 + i] = (u8)(tls_conn.client_seq >> (56 - i * 8));

        u8 aad[13];
        for (int i = 0; i < 8; i++) aad[i] = (u8)(tls_conn.client_seq >> (56 - i * 8));
        aad[8] = content_type;
        aad[9] = (u8)(TLS_VERSION_1_2 >> 8);
        aad[10] = (u8)(TLS_VERSION_1_2 & 0xFF);
        aad[11] = (u8)(len >> 8);
        aad[12] = (u8)(len & 0xFF);

        u8 ciphertext[TLS_MAX_RECORD_PLAINTEXT];
        for (u32 i = 0; i < len; i++) ciphertext[i] = plaintext[i];
        u8 tag[16];
        gcm_encrypt(&tls_conn.client_write_aes, nonce, aad, 13, ciphertext, len, tag);

        for (int i = 0; i < 8; i++) record[5 + i] = nonce[4 + i]; /* explicit nonce = seq num */
        for (u32 i = 0; i < len; i++) record[5 + 8 + i] = ciphertext[i];
        for (int i = 0; i < 16; i++) record[5 + 8 + len + i] = tag[i];
        payload_len = 8 + len + 16;
        tls_conn.client_seq++;
    } else {
        for (u32 i = 0; i < len; i++) record[5 + i] = plaintext[i];
        payload_len = len;
    }

    record[0] = content_type;
    record[1] = (u8)(TLS_VERSION_1_2 >> 8);
    record[2] = (u8)(TLS_VERSION_1_2 & 0xFF);
    record[3] = (u8)(payload_len >> 8);
    record[4] = (u8)(payload_len & 0xFF);

    return tcp_send_data(record, (u16)(5 + payload_len));
}

/* Sends one handshake message: builds the 4-byte type+length header,
 * writes header+body as one record (tls_write_record handles
 * encryption once that's active), and folds the same bytes into the
 * transcript hash. Every handshake message this client sends
 * (ClientHello, ClientKeyExchange, Finished) goes through this one
 * function so those two things can never accidentally drift apart. */
static inline int tls_send_handshake(u8 msg_type, const u8 *body, u32 body_len) {
    u8 msg[4 + 4096];
    msg[0] = msg_type;
    msg[1] = (u8)(body_len >> 16);
    msg[2] = (u8)(body_len >> 8);
    msg[3] = (u8)(body_len & 0xFF);
    for (u32 i = 0; i < body_len; i++) msg[4 + i] = body[i];

    tls_transcript_update(msg, 4 + body_len);
    return tls_write_record(TLS_CONTENT_HANDSHAKE, msg, 4 + body_len);
}

/* Builds and sends ClientHello: version, client_random, empty session
 * ID, our one cipher suite, null compression, and the three extensions
 * a modern ECDHE_RSA handshake actually needs (SNI so the server picks
 * the right certificate; supported_groups=x25519 and
 * ec_point_formats=uncompressed so the server knows we can do the key
 * exchange we're about to demand; signature_algorithms so it knows
 * rsa_pkcs1_sha256 is an acceptable way to sign ServerKeyExchange). */
static inline void tls_send_client_hello(void) {
    tls_rng_bytes(tls_conn.client_random, 32);

    u32 host_len = 0;
    while (tls_conn.hostname[host_len]) host_len++;

    u8 body[768];
    u32 pos = 0;
    body[pos++] = (u8)(TLS_VERSION_1_2 >> 8);
    body[pos++] = (u8)(TLS_VERSION_1_2 & 0xFF);
    for (int i = 0; i < 32; i++) body[pos++] = tls_conn.client_random[i];
    /* legacy_session_id: 32 random bytes (RFC 8446 D.4 middlebox compatibility); a TLS 1.3 server must echo it */
    tls_rng_bytes(tls_conn.session_id, 32);
    body[pos++] = 32;
    for (int i = 0; i < 32; i++) body[pos++] = tls_conn.session_id[i];
    /* our ephemeral X25519 key share for TLS 1.3 */
    tls_rng_bytes(tls_conn.my_private, 32);
    x25519_derive_public(tls_conn.my_public, tls_conn.my_private);
    tls_conn.my_public_len = 32;

    body[pos++] = 0x00; body[pos++] = 0x0C; /* cipher_suites length: 12 bytes = 6 suites */
    static const u16 offered[6] = { TLS_SUITE_13_AES128_GCM_SHA256, TLS_SUITE_13_AES256_GCM_SHA384,
                                    TLS_SUITE_ECDHE_ECDSA_AES128_GCM_SHA256, TLS_SUITE_ECDHE_RSA_AES128_GCM_SHA256,
                                    TLS_SUITE_ECDHE_ECDSA_AES256_GCM_SHA384, TLS_SUITE_ECDHE_RSA_AES256_GCM_SHA384 };
    for (int k = 0; k < 6; k++) { body[pos++] = (u8)(offered[k] >> 8); body[pos++] = (u8)(offered[k] & 0xFF); }

    body[pos++] = 0x01; body[pos++] = 0x00; /* compression_methods: 1 method, null (0) */

    u32 ext_len_pos = pos; pos += 2; /* extensions length, filled in after */
    u32 ext_start = pos;

    /* server_name (SNI): extension_type=0, list of {name_type=0(host_name), name} */
    /* server_name (SNI) -- but never for a bare IP literal: RFC 6066 forbids it, and some servers
     * answer an IP in SNI with an unrecognized_name alert. */
    int host_is_ip = host_len > 0;
    for (u32 i = 0; i < host_len; i++) {
        char hc = tls_conn.hostname[i];
        if (!((hc >= '0' && hc <= '9') || hc == '.')) { host_is_ip = 0; break; }
    }
    if (!host_is_ip) {
        body[pos++] = 0x00; body[pos++] = 0x00;
        u32 ext_body_len_pos = pos; pos += 2;
        u32 list_len_pos = pos; pos += 2;
        body[pos++] = 0x00; /* name_type: host_name */
        body[pos++] = (u8)(host_len >> 8); body[pos++] = (u8)(host_len & 0xFF);
        for (u32 i = 0; i < host_len; i++) body[pos++] = (u8)tls_conn.hostname[i];
        u32 list_len = pos - list_len_pos - 2;
        body[list_len_pos] = (u8)(list_len >> 8); body[list_len_pos+1] = (u8)(list_len & 0xFF);
        u32 ext_body_len = pos - ext_body_len_pos - 2;
        body[ext_body_len_pos] = (u8)(ext_body_len >> 8); body[ext_body_len_pos+1] = (u8)(ext_body_len & 0xFF);
    }
    /* supported_groups: extension_type=10, list of named groups -- x25519 (0x001D) only */
    {
        body[pos++] = 0x00; body[pos++] = 0x0A;          /* supported_groups: which curves we can do */
        body[pos++] = 0x00; body[pos++] = 0x08;          /* ext body len */
        body[pos++] = 0x00; body[pos++] = 0x06;          /* list len: 3 groups */
        body[pos++] = 0x00; body[pos++] = 0x1D;          /* x25519 */
        body[pos++] = 0x00; body[pos++] = 0x17;          /* secp256r1 (P-256) */
        body[pos++] = 0x00; body[pos++] = 0x18;          /* secp384r1 (P-384) */
    }
    /* ec_point_formats: extension_type=11 -- uncompressed(0) only.
     * Not actually meaningful for X25519 (which has no point-compression
     * concept the way NIST curves do), but servers/middleboxes from the
     * ECDHE-over-NIST-curves era sometimes still expect this extension
     * to be present at all, so it's included for compatibility. */
    {
        body[pos++] = 0x00; body[pos++] = 0x0B;
        body[pos++] = 0x00; body[pos++] = 0x02;
        body[pos++] = 0x01; body[pos++] = 0x00; /* list len=1, format=uncompressed */
    }
    /* signature_algorithms: extension_type=13 -- rsa_pkcs1_sha256 (0x0401) only */
    {
        body[pos++] = 0x00; body[pos++] = 0x0D;          /* signature_algorithms: what we can VERIFY */
        body[pos++] = 0x00; body[pos++] = 0x12;          /* ext body len */
        body[pos++] = 0x00; body[pos++] = 0x10;          /* list len: 8 pairs of (hash, signature) */
        body[pos++] = 0x04; body[pos++] = 0x03;          /* ecdsa_secp256r1_sha256 */
        body[pos++] = 0x05; body[pos++] = 0x03;          /* ecdsa_secp384r1_sha384 */
        body[pos++] = 0x08; body[pos++] = 0x04;          /* rsa_pss_rsae_sha256 (TLS 1.3 CertificateVerify) */
        body[pos++] = 0x08; body[pos++] = 0x05;          /* rsa_pss_rsae_sha384 */
        body[pos++] = 0x08; body[pos++] = 0x06;          /* rsa_pss_rsae_sha512 */
        body[pos++] = 0x04; body[pos++] = 0x01;          /* rsa_pkcs1_sha256 */
        body[pos++] = 0x05; body[pos++] = 0x01;          /* rsa_pkcs1_sha384 */
        body[pos++] = 0x06; body[pos++] = 0x01;          /* rsa_pkcs1_sha512 */
    }

    /* supported_versions: TLS 1.3 first, TLS 1.2 as the fallback */
    body[pos++] = 0x00; body[pos++] = 0x2B; body[pos++] = 0x00; body[pos++] = 0x05; body[pos++] = 0x04;
    body[pos++] = 0x03; body[pos++] = 0x04; body[pos++] = 0x03; body[pos++] = 0x03;
    /* key_share: one X25519 share (a server that insists on another group sends HelloRetryRequest, which we reject) */
    body[pos++] = 0x00; body[pos++] = 0x33; body[pos++] = 0x00; body[pos++] = 0x26; body[pos++] = 0x00; body[pos++] = 0x24;
    body[pos++] = 0x00; body[pos++] = 0x1D; body[pos++] = 0x00; body[pos++] = 0x20;
    for (int i = 0; i < 32; i++) body[pos++] = tls_conn.my_public[i];

    u32 ext_total_len = pos - ext_start;
    body[ext_len_pos] = (u8)(ext_total_len >> 8);
    body[ext_len_pos+1] = (u8)(ext_total_len & 0xFF);

    tls_send_handshake(TLS_HS_CLIENT_HELLO, body, pos);
}

/* Parses ServerHello's body (already stripped of its 4-byte handshake
 * header by the caller). Only reads what this client needs: version
 * (logged but not enforced beyond "did the server even try TLS 1.2"),
 * server_random, and the cipher suite (which MUST echo the one suite we
 * offered, or this connection fails rather than silently accepting a
 * server picking something we never offered). Session ID and any
 * extensions are present but unread -- this client sent no extensions
 * whose response it needs to act on (no ALPN, no session tickets). */
static inline int tls_parse_server_hello(const u8 *body, u32 len) {
    static const u8 hrr_random[32] = {   /* SHA-256("HelloRetryRequest"), RFC 8446 4.1.3 */
        0xCF,0x21,0xAD,0x74,0xE5,0x9A,0x61,0x11,0xBE,0x1D,0x8C,0x02,0x1E,0x65,0xB8,0x91,
        0xC2,0xA2,0x11,0x16,0x7A,0xBB,0x8C,0x5E,0x07,0x9E,0x09,0xE2,0xC8,0xA8,0x33,0x9C };
    if (len < 2 + 32 + 1) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }

    /* legacy_version is 0x0303 in both TLS 1.2 and TLS 1.3 ServerHellos; the real TLS 1.3 answer is
     * the supported_versions extension below. Anything else is a protocol we never offered. */
    u16 version = (u16)((body[0] << 8) | body[1]);
    if (version != TLS_VERSION_1_2) {
        serial_puts("[TLS] server chose protocol version 0x");
        serial_put_hex16(version);
        serial_puts(", we offered only TLS 1.3 and 1.2\n");
        tls_conn.fail_reason = TLS_FAIL_PROTOCOL_VERSION;
        return 0;
    }
    u32 pos = 2;
    for (int i = 0; i < 32; i++) tls_conn.server_random[i] = body[pos + i];
    pos += 32;
    {   /* HelloRetryRequest is a ServerHello with this fixed random */
        u8 d = 0;
        for (int i = 0; i < 32; i++) d |= (u8)(tls_conn.server_random[i] ^ hrr_random[i]);
        if (d == 0) { tls_conn.fail_reason = TLS_FAIL_HELLO_RETRY; return 0; }
    }
    u8 session_id_len = body[pos++];
    if (session_id_len > 32) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }   /* the spec's own cap */
    const u8 *sid = body + pos;
    pos += session_id_len;
    if (pos + 3 > len) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }       /* suite + compression must fit */
    u16 cipher_suite = (u16)((body[pos] << 8) | body[pos+1]);
    pos += 2;
    u8 compression = body[pos++];
    /* We offered null compression only; anything else cannot be decoded (and TLS compression is a
     * known attack surface -- CRIME). */
    if (compression != 0) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }

    /* Extensions: the framing must be exactly right (2-byte total, then {type, 2-byte length, body}
     * entries ending precisely at the end of the message). We read two: supported_versions (0x002B,
     * the TLS 1.3 marker) and key_share (0x0033, the server's X25519 share). */
    u16 sel_version = 0, ks_group = 0;
    const u8 *ks_key = 0;
    u32 ks_len = 0;
    if (pos < len) {
        if (pos + 2 > len) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
        u32 ext_total = ((u32)body[pos] << 8) | body[pos + 1];
        pos += 2;
        if (pos + ext_total != len) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
        while (pos < len) {
            if (pos + 4 > len) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
            u16 etype = (u16)((body[pos] << 8) | body[pos + 1]);
            u32 elen = ((u32)body[pos + 2] << 8) | body[pos + 3];
            pos += 4;
            if (pos + elen > len) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
            if (etype == 0x002B && elen == 2) sel_version = (u16)((body[pos] << 8) | body[pos + 1]);
            else if (etype == 0x0033 && elen >= 4) {
                ks_group = (u16)((body[pos] << 8) | body[pos + 1]);
                ks_len = ((u32)body[pos + 2] << 8) | body[pos + 3];
                if (4 + ks_len != elen) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
                ks_key = body + pos + 4;
            }
            pos += elen;
        }
    }

    if (sel_version == TLS_VERSION_1_3) {
        if (cipher_suite != TLS_SUITE_13_AES128_GCM_SHA256 && cipher_suite != TLS_SUITE_13_AES256_GCM_SHA384) {
            tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CIPHER_SUITE; return 0;
        }
        if (session_id_len != 32) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
        { u8 d = 0; for (int i = 0; i < 32; i++) d |= (u8)(sid[i] ^ tls_conn.session_id[i]);
          if (d) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; } }   /* must echo our session id */
        if (!ks_key || ks_group != TLS_GROUP_X25519 || ks_len != 32) { tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CURVE; return 0; }
        tls_conn.cipher_suite = cipher_suite;
        tls_conn.tls13 = 1;
        tls_conn.t13_stage = 0;
        return tls13_after_server_hello(ks_key);
    }
    if (sel_version != 0) { tls_conn.fail_reason = TLS_FAIL_PROTOCOL_VERSION; return 0; }

    /* ---- TLS 1.2 ---- */
    /* Downgrade protection (RFC 8446 4.1.3): a TLS 1.3-capable server that is forced down to 1.2 puts
     * "DOWNGRD" + 0x01 in the last 8 bytes of its random. Seeing it means somebody interfered. */
    {
        static const u8 dg[7] = { 0x44,0x4F,0x57,0x4E,0x47,0x52,0x44 };
        int hit = 1;
        for (int i = 0; i < 7; i++) if (tls_conn.server_random[24 + i] != dg[i]) hit = 0;
        if (hit && (tls_conn.server_random[31] == 0x00 || tls_conn.server_random[31] == 0x01)) {
            tls_conn.fail_reason = TLS_FAIL_PROTOCOL_VERSION; return 0;
        }
    }
    /* The suite must be one of the four TLS 1.2 suites we offered. */
    if (!tls_suite_supported(cipher_suite)) {
        serial_puts("[TLS] server picked unsupported cipher suite 0x");
        serial_put_hex16(cipher_suite);
        serial_putc('\n');
        tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CIPHER_SUITE;
        return 0;
    }
    tls_conn.cipher_suite = cipher_suite;
    return 1;
}

/* Parses the Certificate message body: a 3-byte total-length prefix
 * followed by a sequence of {3-byte cert length, cert DER bytes}.
 * Parses each into tls_conn.chain[] via x509_parse() -- cryptographic
 * verification of the chain happens separately, in
 * tls_verify_certificate_chain(), once every certificate is at least
 * syntactically parsed. */
static inline int tls_parse_certificate_message(const u8 *body, u32 len) {
    if (len < 3) return 0;
    u32 total_len = ((u32)body[0] << 16) | ((u32)body[1] << 8) | body[2];
    if (3 + total_len > len) return 0;

    u32 pos = 3;
    u32 end = 3 + total_len;
    int count = 0;
    while (pos < end) {
        if (pos + 3 > end) return 0;
        u32 cert_len = ((u32)body[pos] << 16) | ((u32)body[pos+1] << 8) | body[pos+2];
        pos += 3;
        if (pos + cert_len > end) return 0;
        if (count >= TLS_CHAIN_MAX) {
            tls_conn.fail_reason = TLS_FAIL_BUFFER_OVERFLOW;
            return 0;
        }
        if (!x509_parse(body + pos, cert_len, &tls_conn.chain[count])) {
            tls_conn.fail_reason = TLS_FAIL_CERT_PARSE;
            return 0;
        }
        count++;
        pos += cert_len;
    }
    tls_conn.chain_len = count;
    return count > 0;
}

/* Packs the current RTC-supplied date into the same YYYYMMDDHHMMSS u64
 * format x509.h's validity fields use. Takes the fields directly
 * (rather than an rtc_time_t, so this header doesn't need to depend on
 * kernel/rtc.h) -- kernel.c's integration code is what actually reads
 * the CMOS clock and calls this. */
static inline u64 tls_pack_datetime(u32 year, u32 month, u32 day, u32 hour, u32 minute, u32 second) {
    return (u64)year * 10000000000ull + (u64)month * 100000000ull + (u64)day * 1000000ull
         + (u64)hour * 10000ull + (u64)minute * 100ull + (u64)second;
}

/* Serial diagnostics for a rejected chain. TLS_FAIL_CERT_UNTRUSTED on
 * its own only says "the top of the chain isn't any root we carry" --
 * useless for figuring out WHICH root is missing. This pulls the first
 * commonName (OID 2.5.4.3) out of a raw DER Name and prints it, so the
 * serial log names exactly the issuer we'd need to add to
 * trusted_roots.h. Diagnostic only: never affects the verdict. */
static inline void tls_dbg_print_cn(const char *label, const u8 *name, u32 len) {
    serial_puts(label);
    for (u32 i = 0; i + 5 < len; i++) {
        if (name[i] == 0x55 && name[i + 1] == 0x04 && name[i + 2] == 0x03) {
            u32 n = name[i + 4];           /* tag at i+3, short-form length at i+4 */
            if (n > len - (i + 5)) n = len - (i + 5);
            for (u32 j = 0; j < n; j++) {
                u8 c = name[i + 5 + j];
                serial_putc((c >= 32 && c < 127) ? (char)c : '?');
            }
            serial_puts("\n");
            return;
        }
    }
    serial_puts("(no CN)\n");
}

/* Does the trust anchor at `der` (a DER certificate) vouch for `cert`?
 * True when the anchor's subject is cert's issuer AND its key really
 * verifies cert's signature. An anchor's OWN signature is never checked
 * (it's trusted because it's in the list, not because it says so). */
static inline int tls_anchored_by(const x509_cert_t *cert, const u8 *der, u32 der_len) {
    x509_cert_t root;
    if (!x509_parse(der, der_len, &root)) return 0;
    return x509_issuer_matches_subject(cert, &root) && x509_verify_signed_by(cert, &root);
}

/* Walks tls_conn.chain[] (as parsed by tls_parse_certificate_message())
 * and checks: each certificate's issuer matches the next one's subject,
 * each certificate's signature verifies against the next one's public
 * key, every certificate is within its validity window, the leaf
 * matches `hostname`, and finally that the chain's last certificate was
 * signed by one of this client's embedded trusted roots. That last
 * check alone covers both ways a server's chain can reach a trusted
 * root: sending leaf+intermediate only (the common case -- the last
 * cert in THAT chain is the intermediate, verified here against our own
 * copy of the root's public key) and sending the root explicitly too
 * (then the "last cert" IS the root itself, self-signed, and
 * "verify its signature against our embedded copy of the same root's
 * key" succeeds precisely because it's the same key signing itself). A
 * server whose chain doesn't reach any embedded root fails with
 * TLS_FAIL_CERT_UNTRUSTED -- this client never falls back to trusting
 * an unrecognized self-signed certificate just because the chain is
 * otherwise internally consistent (that would make any attacker's own
 * self-signed cert as good as a real one). */
static inline int tls_verify_certificate_chain(u64 now_packed) {
    if (tls_conn.chain_len == 0) { tls_conn.fail_reason = TLS_FAIL_CERT_CHAIN; return 0; }

    for (int i = 0; i < tls_conn.chain_len; i++) {
        x509_cert_t *cert = &tls_conn.chain[i];
        if (!x509_has_usable_key(cert) || !cert->sig_alg_supported) {
            tls_conn.fail_reason = TLS_FAIL_CERT_CHAIN;
            return 0;
        }
        if (!x509_is_valid_now(cert, now_packed)) {
            tls_conn.fail_reason = TLS_FAIL_CERT_CHAIN;
            return 0;
        }
        if (cert->has_unknown_critical) {            /* RFC 5280 4.2: a critical extension we do not understand means REJECT */
            tls_conn.fail_reason = TLS_FAIL_CERT_EXTENSION;
            return 0;
        }
    }
    if (!x509_leaf_extensions_ok(&tls_conn.chain[0])) {   /* keyUsage must allow signing the key exchange */
        tls_conn.fail_reason = TLS_FAIL_CERT_EXTENSION;
        return 0;
    }

    if (!x509_matches_hostname(&tls_conn.chain[0], tls_conn.hostname)) {
        tls_conn.fail_reason = TLS_FAIL_HOSTNAME_MISMATCH;
        return 0;
    }

    for (int i = 0; i < tls_conn.chain_len - 1; i++) {
        x509_cert_t *child = &tls_conn.chain[i];
        x509_cert_t *parent = &tls_conn.chain[i + 1];
        if (!x509_issuer_matches_subject(child, parent)) {
            tls_conn.fail_reason = TLS_FAIL_CERT_CHAIN;
            return 0;
        }
        if (!parent->is_ca) {
            tls_conn.fail_reason = TLS_FAIL_CERT_CHAIN;
            return 0;
        }
        if (!x509_ca_extensions_ok(parent)) {         /* CA:TRUE but keyUsage without keyCertSign, or an unknown critical extension */
            tls_conn.fail_reason = TLS_FAIL_CERT_EXTENSION;
            return 0;
        }
        if (!x509_verify_signed_by(child, parent)) {
            tls_conn.fail_reason = TLS_FAIL_CERT_CHAIN;
            return 0;
        }
    }

    x509_cert_t *last = &tls_conn.chain[tls_conn.chain_len - 1];
    for (int r = 0; r < TRUSTED_ROOT_COUNT; r++) {
        if (tls_anchored_by(last, trusted_roots[r], trusted_root_lens[r])) return 1;
    }
#ifdef MW_TLS_TEST_ROOTS
    /* TEST BUILDS ONLY (compiled in with -DMW_TLS_TEST_ROOTS=\"file.h\",
     * never in a committed build): extra trust anchors for a throwaway
     * local CA, so the handshake can be exercised end to end against an
     * `openssl s_server` on the host -- this sandbox's own network
     * re-signs every real site's TLS with a private CA, so the real
     * internet can't be used to test certificate paths here. */
    for (int r = 0; r < MW_TEST_ROOT_COUNT; r++) {
        if (tls_anchored_by(last, mw_test_roots[r], mw_test_root_lens[r])) return 1;
    }
#endif

    serial_puts("[TLS] chain rejected as untrusted; host=");
    serial_puts(tls_conn.hostname);
    serial_puts(" certs=");
    serial_put_dec((u32)tls_conn.chain_len);
    serial_puts("\n");
    tls_dbg_print_cn("[TLS]   top cert subject CN: ", last->subject, last->subject_len);
    tls_dbg_print_cn("[TLS]   top cert issuer  CN: ", last->issuer, last->issuer_len);
    tls_conn.fail_reason = TLS_FAIL_CERT_UNTRUSTED;
    return 0;
}

/* Parses ServerKeyExchange for the ECDHE_RSA case: ECParameters
 * (curve_type=named_curve(3), named_curve -- MUST be x25519/0x001D, or
 * this client can't do the exchange and fails cleanly), the server's
 * ephemeral public key (length-prefixed opaque), and a signature
 * (SignatureAndHashAlgorithm + length-prefixed signature bytes) over
 * (client_random || server_random || the ECParameters and public key
 * bytes exactly as they appeared on the wire). Verifies that signature
 * against the LEAF certificate's RSA public key -- this is what
 * actually proves the party we're key-exchanging with is the same one
 * that holds the certificate's private key, tying the cryptographic
 * channel to the identity the chain vouches for. Without this check, an
 * attacker with no certificate at all could still complete a key
 * exchange; this is the step that closes that gap. */
static inline int tls_parse_server_key_exchange(const u8 *body, u32 len) {
    if (len < 4) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
    if (body[0] != 3) { tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CURVE; return 0; } /* curve_type: named_curve */
    u16 named_curve = (u16)((body[1] << 8) | body[2]);
    u32 want_len;
    if (named_curve == TLS_GROUP_X25519) want_len = 32;
    else if (named_curve == TLS_GROUP_SECP256R1) want_len = 65;
    else if (named_curve == TLS_GROUP_SECP384R1) want_len = 97;
    else {
        serial_puts("[TLS] server chose unsupported curve 0x");
        serial_put_hex16(named_curve);
        serial_putc('\n');
        tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CURVE;
        return 0;
    }
    u8 pubkey_len = body[3];
    if (pubkey_len != want_len || 4u + pubkey_len > len) { tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CURVE; return 0; }

    u32 params_end = 4 + pubkey_len;
    tls_conn.kx_curve = named_curve;
    tls_conn.peer_public_len = pubkey_len;
    for (u32 i = 0; i < pubkey_len; i++) tls_conn.peer_public[i] = body[4 + i];

    /* Then the signature over those params: a SignatureAndHashAlgorithm pair, a length, the bytes. */
    if (params_end + 4 > len) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
    u8 hash_byte = body[params_end], sig_byte = body[params_end + 1];
    /* 0x0804..0x0806 = rsa_pss_rsae_sha256/384/512: hash_byte is 8 and sig_byte names the hash */
    int is_pss = (hash_byte == 8 && sig_byte >= 4 && sig_byte <= 6);
    int hash_id = is_pss ? (sig_byte == 4 ? PKCS1_HASH_SHA256 : sig_byte == 5 ? PKCS1_HASH_SHA384 : PKCS1_HASH_SHA512)
                : (hash_byte == 4) ? PKCS1_HASH_SHA256 : (hash_byte == 5) ? PKCS1_HASH_SHA384
                : (hash_byte == 6) ? PKCS1_HASH_SHA512 : PKCS1_HASH_NONE;
    int is_ecdsa_sig = (!is_pss && sig_byte == 3);
    if (hash_id == PKCS1_HASH_NONE || (!is_pss && sig_byte != 1 && sig_byte != 3)) {
        serial_puts("[TLS] server signed with unsupported algorithm 0x");
        serial_put_hex16((u16)((hash_byte << 8) | sig_byte));
        serial_putc('\n');
        tls_conn.fail_reason = TLS_FAIL_SKE_SIGNATURE;
        return 0;
    }
    /* The signature scheme has to agree with the suite the server itself picked... */
    if (is_ecdsa_sig != tls_suite_is_ecdsa(tls_conn.cipher_suite)) { tls_conn.fail_reason = TLS_FAIL_SKE_SIGNATURE; return 0; }
    u16 sig_len = (u16)((body[params_end + 2] << 8) | body[params_end + 3]);
    u32 sig_start = params_end + 4;
    if (sig_start + sig_len > len) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }

    /* signed = client_random || server_random || the ECDH params exactly as sent */
    u8 signed_data[64 + 4 + 97];
    u32 sd_pos = 0;
    for (int i = 0; i < 32; i++) signed_data[sd_pos++] = tls_conn.client_random[i];
    for (int i = 0; i < 32; i++) signed_data[sd_pos++] = tls_conn.server_random[i];
    for (u32 i = 0; i < params_end; i++) signed_data[sd_pos++] = body[i];

    /* ...and with the certificate's own key type -- which is what actually vouches for these params. */
    x509_cert_t *leaf = &tls_conn.chain[0];
    if (is_ecdsa_sig) {
        ecc_curve_t *curve = ecc_curve_by_group(leaf->ec_group);
        u8 digest[64];
        u32 dl = pkcs1_hash(hash_id, signed_data, sd_pos, digest);
        if (!curve || leaf->ec_point_len == 0 || !dl ||
            !ecdsa_verify_der(curve, leaf->ec_point, leaf->ec_point_len, digest, dl, body + sig_start, sig_len)) {
            tls_conn.fail_reason = TLS_FAIL_SKE_SIGNATURE;
            return 0;
        }
    } else {
        if (!leaf->pubkey_valid ||
            !(is_pss ? tls13_rsa_pss_verify(hash_id, signed_data, sd_pos, body + sig_start, sig_len, &leaf->pubkey_modulus, leaf->pubkey_exponent)
                     : pkcs1_verify(hash_id, signed_data, sd_pos, body + sig_start, sig_len, &leaf->pubkey_modulus, leaf->pubkey_exponent))) {
            tls_conn.fail_reason = TLS_FAIL_SKE_SIGNATURE;
            return 0;
        }
    }
    return 1;
}

static inline void tls_send_client_key_exchange(void) {
    u8 body[1 + 97];
    body[0] = (u8)tls_conn.my_public_len;
    for (u32 i = 0; i < tls_conn.my_public_len; i++) body[1 + i] = tls_conn.my_public[i];
    tls_send_handshake(TLS_HS_CLIENT_KEY_EXCHANGE, body, 1 + tls_conn.my_public_len);
}

/* Derives master_secret from the X25519 shared secret (the "premaster
 * secret" in TLS's terminology, same PRF-based derivation regardless of
 * which key exchange produced it) and, from master_secret, the full
 * key_block this cipher suite needs: client/server write keys (16 bytes
 * each) and client/server write IVs (4-byte GCM salts each) -- no MAC
 * keys, since GCM is AEAD and authenticates via the tag, not a separate
 * HMAC the way the CBC suites this client doesn't support would need. */
static inline void tls_derive_keys(const u8 *premaster, u32 premaster_len) {
    u8 seed[64];
    for (int i = 0; i < 32; i++) seed[i] = tls_conn.client_random[i];
    for (int i = 0; i < 32; i++) seed[32 + i] = tls_conn.server_random[i];
    tls_prf(tls_conn.master_secret, 48, premaster, premaster_len, "master secret", seed, 64);

    /* key_block = client_write_key | server_write_key | client_write_IV | server_write_IV
     * (no MAC keys: GCM authenticates itself). 16-byte keys for AES-128, 32 for AES-256. */
    u32 klen = (u32)tls_suite_key_len(tls_conn.cipher_suite);
    u8 key_block[2 * 32 + 2 * 4];
    u8 seed2[64];
    for (int i = 0; i < 32; i++) seed2[i] = tls_conn.server_random[i];
    for (int i = 0; i < 32; i++) seed2[32 + i] = tls_conn.client_random[i];
    tls_prf(key_block, 2 * klen + 8, tls_conn.master_secret, 48, "key expansion", seed2, 64);

    u32 pos = 0;
    aes_set_key(&tls_conn.client_write_aes, key_block + pos, (int)klen); pos += klen;
    aes_set_key(&tls_conn.server_write_aes, key_block + pos, (int)klen); pos += klen;
    for (int i = 0; i < 4; i++) { tls_conn.client_write_iv[i] = key_block[pos + i]; }
    pos += 4;
    for (int i = 0; i < 4; i++) { tls_conn.server_write_iv[i] = key_block[pos + i]; }
    pos += 4;
}

static inline void tls_compute_finished(u8 out[12], const char *label) {
    /* verify_data = PRF(master_secret, label, Hash(all handshake messages so far))[0..11], where
     * Hash is the suite's PRF hash. Snapshot copies: *_final() consumes its context, and the
     * transcript must keep growing afterwards (the server's Finished is hashed in later). */
    u8 transcript_hash[48];
    u32 hl;
    if (tls_suite_is_sha384(tls_conn.cipher_suite)) {
        sha512_ctx_t snap = tls_conn.transcript384;
        sha384_final(&snap, transcript_hash);
        hl = 48;
    } else {
        sha256_ctx_t snap = tls_conn.transcript;
        sha256_final(&snap, transcript_hash);
        hl = 32;
    }
    u8 full[12];
    tls_prf(full, 12, tls_conn.master_secret, 48, label, transcript_hash, hl);
    for (int i = 0; i < 12; i++) out[i] = full[i];
}

static inline void tls_send_finished(void) {
    u8 verify_data[12];
    tls_compute_finished(verify_data, "client finished");
    /* ChangeCipherSpec is its own content type, not a handshake message
     * -- one byte (0x01), unencrypted (we're not yet writing encrypted,
     * since write_cipher_active flips to 1 right after this, not
     * before), and NOT folded into the transcript hash (only actual
     * handshake-content-type messages are). */
    u8 ccs = 0x01;
    tls_write_record(TLS_CONTENT_CHANGE_CIPHER_SPEC, &ccs, 1);
    tls_conn.write_cipher_active = 1;
    tls_conn.client_seq = 0;
    tls_send_handshake(TLS_HS_FINISHED, verify_data, 12);
}

/* Called once we've received and decrypted the server's Finished
 * message: recomputes what its verify_data SHOULD be (from our own
 * accumulated transcript, which by this point includes our ClientHello
 * through our own Finished, but not yet the server's) and compares.
 * Mismatch here means either a transcript-hash bug on one side or -- the
 * entire point of this check existing -- active tampering with the
 * handshake; either way, this connection cannot be trusted and fails. */
static inline int tls_verify_server_finished(const u8 *received_verify_data, u32 len) {
    if (len != 12) return 0;
    u8 expected[12];
    tls_compute_finished(expected, "server finished");
    u8 diff = 0;
    for (int i = 0; i < 12; i++) diff |= (u8)(expected[i] ^ received_verify_data[i]);
    return diff == 0;
}

/* Decrypts one already-extracted record's payload in place (for
 * content types handshake/application_data, once read_cipher_active).
 * `payload` is the record's full payload (explicit nonce + ciphertext +
 * tag); on success, the first 8 bytes are consumed as the nonce and the
 * ciphertext portion is overwritten with plaintext, and this returns a
 * pointer to that plaintext plus its length via out params. Returns 0
 * (and sets TLS_FAIL_DECRYPT) on a bad tag -- callers must not use
 * anything from `payload` if this returns 0. */
static inline int tls_decrypt_record(u8 content_type, u8 *payload, u32 payload_len,
                                       u8 **out_plain, u32 *out_len) {
    if (payload_len < 8 + 16) { tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE; return 0; }
    u8 *explicit_nonce = payload;
    u8 *ciphertext = payload + 8;
    u32 ct_len = payload_len - 8 - 16;
    u8 *tag = payload + 8 + ct_len;

    u8 nonce[12];
    for (int i = 0; i < 4; i++) nonce[i] = tls_conn.server_write_iv[i];
    for (int i = 0; i < 8; i++) nonce[4 + i] = explicit_nonce[i];

    u8 aad[13];
    for (int i = 0; i < 8; i++) aad[i] = (u8)(tls_conn.server_seq >> (56 - i * 8));
    aad[8] = content_type;
    aad[9] = (u8)(TLS_VERSION_1_2 >> 8);
    aad[10] = (u8)(TLS_VERSION_1_2 & 0xFF);
    aad[11] = (u8)(ct_len >> 8);
    aad[12] = (u8)(ct_len & 0xFF);

    if (!gcm_decrypt(&tls_conn.server_write_aes, nonce, aad, 13, ciphertext, ct_len, tag)) {
        serial_puts("[TLS] record failed authentication: seq=");
        serial_put_dec((u32)tls_conn.server_seq);
        serial_puts(" type=");
        serial_put_dec(content_type);
        serial_puts(" len=");
        serial_put_dec(ct_len);
        serial_puts(" tcp.recv_len=");
        serial_put_dec(tcp_conn.recv_len);
        serial_putc('\n');
        tls_conn.fail_reason = TLS_FAIL_DECRYPT;
        return 0;
    }
    tls_conn.server_seq++;
    *out_plain = ciphertext;
    *out_len = ct_len;
    return 1;
}

/* ---- TLS 1.3 handshake messages (everything after ServerHello is encrypted) ---- */

/* Certificate: opaque context<0..255>, then certificate_list<0..2^24-1> of
 * { cert_data<1..2^24-1>, extensions<0..2^16-1> } entries. */
static inline int tls13_parse_certificate(const u8 *body, u32 len) {
    if (len < 4) return 0;
    u32 ctx = body[0];
    if (1 + ctx + 3 > len) return 0;
    u32 pos = 1 + ctx;
    u32 total = ((u32)body[pos] << 16) | ((u32)body[pos + 1] << 8) | body[pos + 2];
    pos += 3;
    if (pos + total != len) return 0;
    int count = 0;
    while (pos < len) {
        if (pos + 3 > len) return 0;
        u32 cl = ((u32)body[pos] << 16) | ((u32)body[pos + 1] << 8) | body[pos + 2];
        pos += 3;
        if (cl == 0 || pos + cl + 2 > len) return 0;
        if (count >= TLS_CHAIN_MAX) { tls_conn.fail_reason = TLS_FAIL_BUFFER_OVERFLOW; return 0; }
        if (!x509_parse(body + pos, cl, &tls_conn.chain[count])) { tls_conn.fail_reason = TLS_FAIL_CERT_PARSE; return 0; }
        count++;
        pos += cl;
        u32 el = ((u32)body[pos] << 8) | body[pos + 1];
        pos += 2;
        if (pos + el > len) return 0;
        pos += el;                                       /* per-certificate extensions (OCSP, SCT): ignored */
    }
    tls_conn.chain_len = count;
    return count > 0;
}

/* CertificateVerify: the server signs 64 spaces || "TLS 1.3, server CertificateVerify" || 0x00 ||
 * Hash(transcript up to and including Certificate) with the key of the leaf certificate. */
static inline int tls13_verify_certificate_verify(const u8 *body, u32 len) {
    static const char ctxs[] = "TLS 1.3, server CertificateVerify";
    u8 content[64 + 33 + 1 + 48];
    u32 n = 0, hl = tls13_hlen(), i;
    if (len < 4) return 0;
    u16 scheme = (u16)((body[0] << 8) | body[1]);
    u32 sl = ((u32)body[2] << 8) | body[3];
    if (4 + sl != len) return 0;
    const u8 *sig = body + 4;
    for (i = 0; i < 64; i++) content[n++] = 0x20;
    for (i = 0; i < 33; i++) content[n++] = (u8)ctxs[i];       /* the 33-character context string */
    content[n++] = 0x00;                                       /* ...then the zero separator */
    tls13_thash(content + n); n += hl;
    x509_cert_t *leaf = &tls_conn.chain[0];
    if (scheme == 0x0403 || scheme == 0x0503) {
        u16 want = scheme == 0x0403 ? TLS_GROUP_SECP256R1 : TLS_GROUP_SECP384R1;
        ecc_curve_t *curve = ecc_curve_by_group(leaf->ec_group);
        u8 digest[64];
        u32 dl = pkcs1_hash(scheme == 0x0403 ? PKCS1_HASH_SHA256 : PKCS1_HASH_SHA384, content, n, digest);
        if (leaf->ec_group != want || !curve || leaf->ec_point_len == 0 || !dl) return 0;
        return ecdsa_verify_der(curve, leaf->ec_point, leaf->ec_point_len, digest, dl, sig, sl);
    }
    if (scheme == 0x0804 || scheme == 0x0805 || scheme == 0x0806) {
        int h = scheme == 0x0804 ? PKCS1_HASH_SHA256 : scheme == 0x0805 ? PKCS1_HASH_SHA384 : PKCS1_HASH_SHA512;
        if (!leaf->pubkey_valid) return 0;
        return tls13_rsa_pss_verify(h, content, n, sig, sl, &leaf->pubkey_modulus, leaf->pubkey_exponent);
    }
    return 0;      /* rsa_pkcs1 and anything else are not allowed to sign a TLS 1.3 CertificateVerify */
}

static inline void tls13_fail(tls_fail_reason_t r) { tls_conn.fail_reason = r; tls_conn.state = TLS_FAILED; }

/* The server's Finished verified: derive the application keys, answer with our Finished. */
static inline void tls13_finish_handshake(const u8 *finished_msg, u32 msg_len) {
    u32 h = tls13_hlen();
    u8 th[48], mac[48], hdr[4];
    /* fold the server Finished into the transcript now: the application secrets cover it */
    tls_transcript_update(finished_msg, msg_len);
    tls_conn.t13_folded = 1;
    tls13_thash(th);                                    /* ClientHello..server Finished */
    tls13_derive(tls_conn.c_ap, tls_conn.master13, "c ap traffic", th);
    tls13_derive(tls_conn.s_ap, tls_conn.master13, "s ap traffic", th);

    /* middlebox-compatibility ChangeCipherSpec (RFC 8446 D.4) -- a plain record, not encrypted */
    { u8 ccs[6] = { TLS_CONTENT_CHANGE_CIPHER_SPEC, 3, 3, 0, 1, 1 }; tcp_send_data(ccs, 6); }
    if (tls_conn.t13_certreq) {                         /* we have no client certificate: say so with an empty list */
        static const u8 empty_cert[4] = { 0, 0, 0, 0 };
        if (!tls_send_handshake(TLS_HS_CERTIFICATE, empty_cert, 4)) { tls13_fail(TLS_FAIL_TCP); return; }
    }
    tls13_thash(th);                                    /* includes our own Certificate, if any */
    tls13_finished_mac(mac, tls_conn.c_hs, th);
    (void)hdr;
    if (!tls_send_handshake(TLS_HS_FINISHED, mac, h)) { tls13_fail(TLS_FAIL_TCP); return; }

    /* switch both directions to the application traffic keys */
    tls13_traffic_keys(tls_conn.c_ap, &tls_conn.client_write_aes, tls_conn.c_iv13);
    tls13_traffic_keys(tls_conn.s_ap, &tls_conn.server_write_aes, tls_conn.s_iv13);
    tls_conn.client_seq = 0; tls_conn.server_seq = 0;
    tls_conn.state = TLS_ESTABLISHED;
}

static inline void tls13_dispatch(u8 msg_type, const u8 *body, u32 body_len, u64 now_packed) {
    u32 h = tls13_hlen();
    if (tls_conn.state == TLS_ESTABLISHED) {
        if (msg_type == TLS_HS_KEY_UPDATE && body_len == 1 && body[0] <= 1) {
            u8 z = 0;
            tls13_expand_label(tls_conn.s_ap, h, tls_conn.s_ap, "traffic upd", (const u8 *)"", 0);
            tls13_traffic_keys(tls_conn.s_ap, &tls_conn.server_write_aes, tls_conn.s_iv13);
            tls_conn.server_seq = 0;
            if (body[0] == 1) {                         /* update_requested: update our sending keys too */
                if (!tls_send_handshake(TLS_HS_KEY_UPDATE, &z, 1)) { tls13_fail(TLS_FAIL_TCP); return; }
                tls13_expand_label(tls_conn.c_ap, h, tls_conn.c_ap, "traffic upd", (const u8 *)"", 0);
                tls13_traffic_keys(tls_conn.c_ap, &tls_conn.client_write_aes, tls_conn.c_iv13);
                tls_conn.client_seq = 0;
            }
        } else if (msg_type == TLS_HS_KEY_UPDATE) {
            tls13_fail(TLS_FAIL_UNEXPECTED_MESSAGE);
        }
        return;                                         /* NewSessionTicket and friends: ignored */
    }
    switch (tls_conn.t13_stage) {
    case 0:                                             /* EncryptedExtensions */
        if (msg_type != TLS_HS_ENCRYPTED_EXTENSIONS || body_len < 2 ||
            (((u32)body[0] << 8) | body[1]) + 2 != body_len) { tls13_fail(TLS_FAIL_UNEXPECTED_MESSAGE); return; }
        tls_conn.t13_stage = 1;
        break;
    case 1:                                             /* [CertificateRequest] then Certificate */
        if (msg_type == TLS_HS_CERTIFICATE_REQUEST) { tls_conn.t13_certreq = 1; break; }
        if (msg_type != TLS_HS_CERTIFICATE) { tls13_fail(TLS_FAIL_UNEXPECTED_MESSAGE); return; }
        if (!tls13_parse_certificate(body, body_len)) {
            if (tls_conn.fail_reason == TLS_FAIL_NONE) tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE;
            tls_conn.state = TLS_FAILED; return;
        }
        if (!tls_verify_certificate_chain(now_packed)) { tls_conn.state = TLS_FAILED; return; }
        tls_conn.t13_stage = 2;
        break;
    case 2:                                             /* CertificateVerify */
        if (msg_type != TLS_HS_CERTIFICATE_VERIFY) { tls13_fail(TLS_FAIL_UNEXPECTED_MESSAGE); return; }
        if (!tls13_verify_certificate_verify(body, body_len)) { tls13_fail(TLS_FAIL_CERT_VERIFY); return; }
        tls_conn.t13_stage = 3;
        break;
    case 3: {                                           /* Finished */
        u8 th[48], mac[48];
        if (msg_type != TLS_HS_FINISHED || body_len != h) { tls13_fail(TLS_FAIL_SERVER_FINISHED); return; }
        tls13_thash(th);                                /* through CertificateVerify */
        tls13_finished_mac(mac, tls_conn.s_hs, th);
        if (!tls13_ct_equal(mac, body, h)) { tls13_fail(TLS_FAIL_SERVER_FINISHED); return; }
        {   /* rebuild the 4-byte header for the transcript: the caller still holds the full message */
            u8 msg[4 + 48];
            msg[0] = TLS_HS_FINISHED; msg[1] = 0; msg[2] = 0; msg[3] = (u8)h;
            for (u32 i = 0; i < h; i++) msg[4 + i] = body[i];
            tls13_finish_handshake(msg, 4 + h);
        }
        break; }
    default:
        tls13_fail(TLS_FAIL_UNEXPECTED_MESSAGE);
    }
}

/* Decrypts one TLS 1.3 record in place. The 5 header bytes sit right before `payload`. Returns the
 * inner content type (last non-zero byte of the plaintext) and the content length. */
static inline int tls13_decrypt_record(u8 *payload, u32 payload_len, u8 **out_plain, u32 *out_len, u8 *inner_type) {
    if (payload_len < 16 + 1) { tls_conn.fail_reason = TLS_FAIL_BAD_RECORD; return 0; }
    u32 ct_len = payload_len - 16, i;
    u8 nonce[12];
    for (i = 0; i < 12; i++) nonce[i] = tls_conn.s_iv13[i];
    for (i = 0; i < 8; i++) nonce[4 + i] ^= (u8)(tls_conn.server_seq >> (56 - i * 8));
    if (!gcm_decrypt(&tls_conn.server_write_aes, nonce, payload - 5, 5, payload, ct_len, payload + ct_len)) {
        tls_conn.fail_reason = TLS_FAIL_DECRYPT;
        return 0;
    }
    tls_conn.server_seq++;
    while (ct_len > 0 && payload[ct_len - 1] == 0) ct_len--;       /* strip the zero padding */
    if (ct_len == 0) { tls_conn.fail_reason = TLS_FAIL_BAD_RECORD; return 0; }
    *inner_type = payload[ct_len - 1];
    *out_plain = payload;
    *out_len = ct_len - 1;
    if (*out_len > TLS_MAX_RECORD_PLAINTEXT) { tls_conn.fail_reason = TLS_FAIL_BAD_RECORD; return 0; }
    return 1;
}

/* Dispatches exactly one complete handshake message (already reassembled
 * in TLS_HS_BUF) according to the current state. Advances
 * tls_conn.state on success; sets TLS_FAILED + a reason on anything
 * unexpected. This is the heart of the handshake's actual protocol
 * logic -- everything above this function is plumbing (framing,
 * crypto primitives) this function calls in the right order.
 *
 * Takes `now_packed` (the current YYYYMMDDHHMMSS date, from RTC) purely
 * to hand to tls_verify_certificate_chain() the moment the Certificate
 * message finishes parsing -- verifying immediately, rather than
 * waiting for ServerHelloDone, means a chain that fails validation
 * stops this handshake right there instead of after also processing
 * whatever ServerKeyExchange the server sent for a certificate we're
 * about to reject anyway. */
static inline void tls_dispatch_handshake_message(u8 msg_type, const u8 *body, u32 body_len, u64 now_packed) {
    if (tls_conn.tls13) { tls13_dispatch(msg_type, body, body_len, now_packed); return; }
    switch (tls_conn.state) {
        case TLS_CLIENT_HELLO_SENT:
            if (msg_type == TLS_HS_SERVER_HELLO) {
                if (!tls_parse_server_hello(body, body_len)) { tls_conn.state = TLS_FAILED; return; }
            } else if (msg_type == TLS_HS_CERTIFICATE) {
                if (!tls_parse_certificate_message(body, body_len)) { tls_conn.state = TLS_FAILED; return; }
                if (!tls_verify_certificate_chain(now_packed)) { tls_conn.state = TLS_FAILED; return; }
            } else if (msg_type == TLS_HS_SERVER_KEY_EXCHANGE) {
                if (!tls_parse_server_key_exchange(body, body_len)) { tls_conn.state = TLS_FAILED; return; }
            } else if (msg_type == TLS_HS_CERTIFICATE_REQUEST) {
                /* Client certificate auth: not supported, but also not
                 * fatal by itself -- a server asking doesn't mean it
                 * requires one. We simply never send a Certificate
                 * message of our own; most servers accept that as "no
                 * client cert offered" and proceed. */
            } else if (msg_type == TLS_HS_SERVER_HELLO_DONE) {
                /* Full server flight received, chain and SKE signature
                 * already verified as each message arrived above -- now
                 * do the key exchange: generate our ephemeral X25519
                 * keypair, compute the shared secret against the
                 * server's ephemeral public key, derive the session
                 * keys, and respond. */
                u8 premaster[48];
                u32 premaster_len;
                if (tls_conn.kx_curve == TLS_GROUP_X25519) {
                    tls_rng_bytes(tls_conn.my_private, 32);
                    x25519_derive_public(tls_conn.my_public, tls_conn.my_private);
                    tls_conn.my_public_len = 32;
                    x25519_scalarmult(premaster, tls_conn.my_private, tls_conn.peer_public);
                    premaster_len = 32;
                } else {
                    ecc_curve_t *ec = ecc_curve_by_group(tls_conn.kx_curve);
                    u8 rnd[48];
                    if (!ec) { tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CURVE; tls_conn.state = TLS_FAILED; return; }
                    tls_rng_bytes(rnd, (u32)ec->bytes);
                    if (!ecdh_keygen(ec, rnd, tls_conn.my_private, tls_conn.my_public)) {
                        tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CURVE; tls_conn.state = TLS_FAILED; return;
                    }
                    tls_conn.my_public_len = (u32)(1 + 2 * ec->bytes);
                    /* ecdh_shared() refuses a point that isn't on the curve: a malicious or
                     * broken server can't feed us an invalid-curve point to leak our key. */
                    if (!ecdh_shared(ec, tls_conn.my_private, tls_conn.peer_public, tls_conn.peer_public_len, premaster)) {
                        tls_conn.fail_reason = TLS_FAIL_UNSUPPORTED_CURVE; tls_conn.state = TLS_FAILED; return;
                    }
                    premaster_len = (u32)ec->bytes;
                }
                tls_derive_keys(premaster, premaster_len);

                tls_send_client_key_exchange();
                tls_send_finished();
                tls_conn.state = TLS_CLIENT_FINISHED_SENT;
            } else {
                tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE;
                tls_conn.state = TLS_FAILED;
            }
            break;

        case TLS_CLIENT_FINISHED_SENT:
            if (msg_type == TLS_HS_FINISHED) {
                if (!tls_verify_server_finished(body, body_len)) {
                    tls_conn.fail_reason = TLS_FAIL_SERVER_FINISHED;
                    tls_conn.state = TLS_FAILED;
                    return;
                }
                tls_conn.state = TLS_ESTABLISHED;
            } else {
                tls_conn.fail_reason = TLS_FAIL_UNEXPECTED_MESSAGE;
                tls_conn.state = TLS_FAILED;
            }
            break;

        default:
            /* a handshake message in ESTABLISHED (e.g. a renegotiation
             * request) or any other state this client doesn't expect
             * one in -- ignored rather than failing the connection,
             * since post-handshake HelloRequest messages are legal
             * (if rare) and this client simply never renegotiates */
            break;
    }
}

/* Splits TLS_HS_BUF into complete handshake messages and dispatches
 * each in order, folding received messages into the transcript hash as
 * they're consumed -- mirroring tls_send_handshake()'s hashing on the
 * send side, so both directions' messages end up hashed in true wire
 * order regardless of how tcp_poll_recv() happened to chunk the
 * underlying byte stream. */
static inline void tls_process_handshake_buffer(u64 now_packed) {
    while (tls_conn.hs_buf_len >= 4) {
        u32 body_len = ((u32)TLS_HS_BUF[1] << 16) | ((u32)TLS_HS_BUF[2] << 8) | TLS_HS_BUF[3];
        u32 total = 4 + body_len;
        if (tls_conn.hs_buf_len < total) break; /* message not fully arrived yet */

        u8 msg_type = TLS_HS_BUF[0];

        /* Special case: the server's own Finished message. Per RFC 5246
         * 7.4.9, a Finished message's verify_data covers every handshake
         * message *up to but not including* the Finished message itself
         * -- so the transcript hash used to check it must NOT yet
         * include these exact bytes. Every other message gets folded in
         * before dispatch (the normal, simpler order); this one gets
         * dispatched (verified) first and folded in after, so
         * tls_verify_server_finished()'s snapshot is the correct
         * "everything except this" hash instead of a self-referential
         * one that could never match. */
        int is_server_finished = (tls_conn.state == TLS_CLIENT_FINISHED_SENT && msg_type == TLS_HS_FINISHED);
        /* TLS 1.3: CertificateVerify and Finished are verified against the transcript BEFORE them, so they
         * are hashed after dispatch (the Finished handler hashes itself: it needs the result). */
        int late13 = tls_conn.tls13 && tls_conn.state == TLS_CLIENT_HELLO_SENT &&
                     (msg_type == TLS_HS_CERTIFICATE_VERIFY || msg_type == TLS_HS_FINISHED);
        if (!is_server_finished && !late13) {
            tls_transcript_update(TLS_HS_BUF, total);
        }
        tls_dispatch_handshake_message(msg_type, TLS_HS_BUF + 4, body_len, now_packed);
        if (late13 && !tls_conn.t13_folded && tls_conn.state != TLS_FAILED) {
            tls_transcript_update(TLS_HS_BUF, total);
        }
        tls_conn.t13_folded = 0;
        if (is_server_finished && tls_conn.state != TLS_FAILED) {
            tls_transcript_update(TLS_HS_BUF, total);
        }

        for (u32 i = total; i < tls_conn.hs_buf_len; i++) TLS_HS_BUF[i - total] = TLS_HS_BUF[i];
        tls_conn.hs_buf_len -= total;

        if (tls_conn.state == TLS_FAILED) return;
    }
}

/* Splits TLS_RX_RAW into complete TLS records and processes each:
 * change_cipher_spec flips read_cipher_active and resets server_seq;
 * alert is inspected (a fatal alert fails the connection; close_notify
 * is treated as a clean EOF signal, same spirit as TCP's own FIN);
 * handshake content gets decrypted if needed and appended to hs_buf for
 * tls_process_handshake_buffer() to split into messages; application_data
 * gets decrypted and appended to app_recv for http.h to read. */
static inline void tls_process_raw_buffer(u64 now_packed) {
    while (tls_conn.rx_raw_len >= 5) {
        u8 content_type = TLS_RX_RAW[0];
        u32 rec_len = ((u32)TLS_RX_RAW[3] << 8) | TLS_RX_RAW[4];
        u32 total = 5 + rec_len;

        /* Judge the HEADER the moment its five bytes are here, before waiting for the body. A header
         * that declares a record this connection can never hold (bigger than the spec allows, or than
         * TLS_RX_RAW can buffer) used to just make us wait for "the rest of the record" -- which can
         * never fit, so the raw buffer filled up, TCP's window closed, and the page hung. Likewise a
         * content type / version no TLS 1.2 peer would send is a broken or hostile stream, not
         * something to keep parsing. All of these end the connection now, with a reason. */
        if (!(content_type >= TLS_CONTENT_CHANGE_CIPHER_SPEC && content_type <= TLS_CONTENT_APPLICATION_DATA) ||
            TLS_RX_RAW[1] != 3 || TLS_RX_RAW[2] < 1 || TLS_RX_RAW[2] > 3 ||
            rec_len > TLS_MAX_RECORD_CIPHERTEXT || total > TLS_RX_RAW_BUF_SIZE) {
            serial_puts("[TLS] rejecting record header: type=");
            serial_put_dec(content_type);
            serial_puts(" len=");
            serial_put_dec(rec_len);
            serial_putc('\n');
            tls_conn.fail_reason = TLS_FAIL_BAD_RECORD;
            tls_conn.state = TLS_FAILED;
            return;
        }
        if (tls_conn.rx_raw_len < total) break;

        /* BACKPRESSURE: if this is application data and the reader hasn't yet drained enough of
         * app_recv to hold it, leave the record sitting in the raw buffer and try again next poll.
         * The full raw buffer then stops tls_poll() reading from TCP, TCP's advertised window
         * shrinks, and the server slows down -- flow control end to end, instead of the old
         * behaviour of silently dropping whatever didn't fit (which, once pages could be larger
         * than one buffer, would have quietly corrupted every big download). */
        if (content_type == TLS_CONTENT_APPLICATION_DATA &&
            tls_conn.app_recv_len + rec_len > TLS_APP_RECV_BUF_SIZE) break;

        u8 *payload = TLS_RX_RAW + 5;
        u8 *plain = payload;
        u32 plain_len = rec_len;

        if (tls_conn.tls13 && tls_conn.read_cipher_active && content_type != TLS_CONTENT_CHANGE_CIPHER_SPEC) {
            /* TLS 1.3: every record after the ServerHello is application_data on the outside; the real
             * type is the last non-zero byte of the decrypted plaintext. A plaintext alert/handshake
             * record at this point is not something a TLS 1.3 server sends. */
            u8 inner;
            if (content_type != TLS_CONTENT_APPLICATION_DATA) { tls_conn.fail_reason = TLS_FAIL_BAD_RECORD; tls_conn.state = TLS_FAILED; return; }
            if (!tls13_decrypt_record(payload, rec_len, &plain, &plain_len, &inner)) { tls_conn.state = TLS_FAILED; return; }
            content_type = inner;
        }

        if (content_type == TLS_CONTENT_CHANGE_CIPHER_SPEC) {
            if (!tls_conn.tls13) {                      /* in TLS 1.3 the CCS record is a meaningless compatibility relic */
                tls_conn.read_cipher_active = 1;
                tls_conn.server_seq = 0;
            }
        } else if (content_type == TLS_CONTENT_ALERT) {
            if (tls_conn.read_cipher_active && !tls_conn.tls13) {
                if (!tls_decrypt_record(content_type, payload, rec_len, &plain, &plain_len)) {
                    tls_conn.state = TLS_FAILED;
                    return;
                }
            }
            if (plain_len >= 2) {
                tls_conn.alert_level = plain[0];
                tls_conn.alert_desc = plain[1];
                /* Say what the server said. An alert's description code is
                 * the single most useful clue when a handshake dies: 40 =
                 * handshake_failure (no cipher/curve/sig-alg in common),
                 * 70 = protocol_version (wants TLS 1.3), 71 = insufficient
                 * security, 80 = internal_error, 112 = unrecognized_name
                 * (SNI). Before this, a fatal alert was filed as a bland
                 * "unexpected message" and the code was thrown away. */
                if (plain[0] == 1 && plain[1] == 0) tls_conn.peer_close_notify = 1;
                serial_puts("[TLS] peer alert: level=");
                serial_put_dec(plain[0]);
                serial_puts(" desc=");
                serial_put_dec(plain[1]);
                serial_putc('\n');
            }
            if (plain_len >= 2 && plain[0] == 2 /* fatal */) {
                tls_conn.fail_reason = TLS_FAIL_PEER_ALERT;
                tls_conn.state = TLS_FAILED;
                return;
            }
            /* a warning-level alert (most commonly close_notify) --
             * noted, not fatal; the peer closing their write side is
             * handled the same way TCP's own FIN is, elsewhere */
        } else if (content_type == TLS_CONTENT_HANDSHAKE || content_type == TLS_CONTENT_APPLICATION_DATA) {
            if (tls_conn.read_cipher_active && !tls_conn.tls13) {
                if (!tls_decrypt_record(content_type, payload, rec_len, &plain, &plain_len)) {
                    tls_conn.state = TLS_FAILED;
                    return;
                }
            }
            if (content_type == TLS_CONTENT_HANDSHAKE) {
                if (tls_conn.hs_buf_len + plain_len > TLS_HS_BUF_SIZE) {
                    tls_conn.fail_reason = TLS_FAIL_BUFFER_OVERFLOW;
                    tls_conn.state = TLS_FAILED;
                    return;
                }
                for (u32 i = 0; i < plain_len; i++) TLS_HS_BUF[tls_conn.hs_buf_len + i] = plain[i];
                tls_conn.hs_buf_len += plain_len;
            } else {
                /* (The backpressure check at the top of the loop guarantees this fits; the
                 * clamp below is belt and braces, never a normal path.) */
                u32 room = TLS_APP_RECV_BUF_SIZE - tls_conn.app_recv_len;
                u32 take = plain_len < room ? plain_len : room;
                for (u32 i = 0; i < take; i++) TLS_APP_RECV[tls_conn.app_recv_len + i] = plain[i];
                tls_conn.app_recv_len += take;
            }
        }

        for (u32 i = total; i < tls_conn.rx_raw_len; i++) TLS_RX_RAW[i - total] = TLS_RX_RAW[i];
        tls_conn.rx_raw_len -= total;

        tls_process_handshake_buffer(now_packed);
        if (tls_conn.state == TLS_FAILED) return;
    }
}



/* ---- Public API -- the only functions kernel.c (via https.h) should
 * ever call directly. Everything above this point is this file's own
 * internal machinery. ---- */

/* Starts a new TLS connection: resets all per-connection state, seeds
 * this connection's PRNG (see the caveat on tls_rng_seed()'s own
 * comment -- `entropy_seed` should be whatever this kernel's best
 * available entropy is, e.g. a mix of RTC fields and the tick counter;
 * it does not need to be, and currently cannot be, cryptographically
 * strong), and kicks off the underlying TCP connection. Call tls_poll()
 * every main-loop iteration afterward to actually drive the handshake
 * forward; nothing here blocks. */
static inline void tls_connect(u32 ip, u16 port, const char *hostname, const u8 entropy_seed[16]) {
    tls_conn.state = TLS_WAIT_TCP;
    tls_conn.fail_reason = TLS_FAIL_NONE;
    tls_conn.chain_len = 0;
    tls_conn.rx_raw_len = 0;
    tls_conn.hs_buf_len = 0;
    tls_conn.app_recv_len = 0;
    tls_conn.client_seq = 0;
    tls_conn.server_seq = 0;
    tls_conn.write_cipher_active = 0;
    tls_conn.read_cipher_active = 0;
    tls_conn.alert_level = 0;
    tls_conn.alert_desc = 0;
    tls_conn.peer_close_notify = 0;
    tls_conn.tls13 = 0;
    tls_conn.t13_stage = 0;
    tls_conn.t13_certreq = 0;
    tls_conn.t13_folded = 0;

    u32 i = 0;
    for (; hostname[i] && i < sizeof(tls_conn.hostname) - 1; i++) tls_conn.hostname[i] = hostname[i];
    tls_conn.hostname[i] = 0;

    tls_rng_seed(entropy_seed);
    sha256_init(&tls_conn.transcript);
    sha384_init(&tls_conn.transcript384);
    tls_conn.cipher_suite = 0;
    tls_conn.kx_curve = 0;

    tcp_connect(ip, port);
}

/* Advances the connection: checks on the underlying TCP handshake if
 * still waiting on it, otherwise drains whatever TCP bytes are
 * available into rx_raw and processes as many complete records/
 * messages as have arrived. Returns the current state, same
 * poll()-style contract as tcp_poll_retransmit()/dns_poll()/
 * http_poll() elsewhere in this stack. `now_packed` is only actually
 * used at one specific moment (right when the Certificate message
 * finishes parsing, for validity-date checking) but is threaded through
 * every call for simplicity -- reading RTC is cheap, and this avoids a
 * more complex "only pass it in sometimes" calling convention. */
static inline tls_state_t tls_poll(u64 now_packed) {
    if (tls_conn.state == TLS_WAIT_TCP) {
        if (tcp_conn.state == TCP_ESTABLISHED) {
            tls_send_client_hello();
            tls_conn.state = TLS_CLIENT_HELLO_SENT;
        } else if (tcp_conn.state == TCP_CLOSED) {
            tls_conn.fail_reason = TLS_FAIL_TCP;
            tls_conn.state = TLS_FAILED;
        }
        return tls_conn.state;
    }

    if (tls_conn.state == TLS_CLIENT_HELLO_SENT || tls_conn.state == TLS_CLIENT_FINISHED_SENT ||
        tls_conn.state == TLS_ESTABLISHED) {
        u32 room = TLS_RX_RAW_BUF_SIZE - tls_conn.rx_raw_len;
        if (room > 0) {
            u16 n = tcp_poll_recv(TLS_RX_RAW + tls_conn.rx_raw_len, (u16)(room > 0xFFFF ? 0xFFFF : room));
            tls_conn.rx_raw_len += n;
        }
        tls_process_raw_buffer(now_packed);
    }
    return tls_conn.state;
}

/* Sends `len` bytes of application data (an HTTP request, in this
 * client's actual use) as one or more encrypted records. Only valid
 * once TLS_ESTABLISHED. Splits into TLS_MAX_RECORD_PLAINTEXT-sized
 * chunks if necessary, though this client's own HTTP requests are
 * always far smaller than that in practice. */
static inline int tls_send_app_data(const u8 *data, u32 len) {
    if (tls_conn.state != TLS_ESTABLISHED) return 0;
    u32 off = 0;
    while (off < len) {
        u32 chunk = len - off;
        if (chunk > TLS_MAX_RECORD_PLAINTEXT) chunk = TLS_MAX_RECORD_PLAINTEXT;
        if (!tls_write_record(TLS_CONTENT_APPLICATION_DATA, data + off, chunk)) return 0;
        off += chunk;
    }
    return 1;
}

/* Drains up to `maxlen` bytes of decrypted application data into `out`,
 * sliding any remainder down -- identical contract to tcp_poll_recv()
 * and http.h's own response buffer draining, so https.h (the eventual
 * http.h counterpart that speaks TLS) can reuse the same polling
 * pattern http.h already established. */
static inline u16 tls_poll_recv_app_data(u8 *out, u16 maxlen) {
    u16 n = tls_conn.app_recv_len < maxlen ? (u16)tls_conn.app_recv_len : maxlen;
    for (u16 i = 0; i < n; i++) out[i] = TLS_APP_RECV[i];
    for (u32 i = n; i < tls_conn.app_recv_len; i++) TLS_APP_RECV[i - n] = TLS_APP_RECV[i];
    tls_conn.app_recv_len -= n;
    return n;
}

/* Closes the connection: sends a close_notify alert (the polite way to
 * end a TLS connection, per RFC 5246 7.2.1 -- lets the peer distinguish
 * "connection ended cleanly" from "connection was cut off mid-stream,
 * possibly by an attacker truncating the response") if we ever finished
 * the handshake, then closes the underlying TCP connection the same way
 * http.h already does via tcp_close(). */
static inline void tls_close(void) {
    if (tls_conn.write_cipher_active) {
        u8 alert[2] = { 0x01, 0x00 }; /* level=warning, description=close_notify */
        tls_write_record(TLS_CONTENT_ALERT, alert, 2);
    }
    tcp_close();
    tls_conn.state = TLS_CLOSED;
}

#endif

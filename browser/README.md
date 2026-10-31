# MiniBrowser

A small standalone desktop web browser built on [MiniWebEngine](../README.md).

> **MiniBrowser is not MiniWeb.**
> MiniWeb is the browser built into the **MiniWin** operating system. MiniBrowser
> is a separate project that runs as an ordinary desktop application and only
> shares the "Mini" naming. The two have different code bases and different goals.

**Version 0.3-dev1** (v0.3 in progress: HTTPS added; matches MiniWebEngine 0.3-dev1)

## Features

- Address bar with caret, selection, UTF-8 editing and Ctrl+V paste
- Back / forward / reload, per-entry scroll position, 32-entry history
- `http://` and **`https://`** (HTTP/1.1, redirects, chunked transfer decoding)
- Address-bar security badge: **Secure** (verified TLS), **Not secure** (plain HTTP), none for local pages
- `file://` URLs and plain paths, `about:home`, `about:blank`
- Clickable links with relative URL resolution, hover URL in the status bar
- Mouse wheel, scrollbar, keyboard scrolling, page title in the window title

## HTTPS / TLS

A built-in TLS 1.2 client (`tls/`, no external library):

- Cipher suites: `ECDHE-{RSA,ECDSA}-AES{128,256}-GCM-{SHA256,SHA384}`
- Key exchange: X25519, P-256, P-384
- Certificates are **always verified**: signature chain to an embedded root store (Mozilla CA
  bundle subset, `tls/trusted_roots.h`), validity dates, CA / key-usage / critical-extension
  checks, host name, and the server's key-exchange signature. There is no "proceed anyway" option.
- Randomness and the clock come from the operating system (`mb_plat_random`, `mb_plat_time`).

Not supported: TLS 1.3 (the browser offers TLS 1.2 only, so TLS-1.3-only servers are refused),
session resumption, client certificates, ChaCha20-Poly1305, revocation checking (OCSP/CRL),
and roots that are not in `tls/trusted_roots.h` (update that file to add or retire roots).

## Not supported (yet)

Images, JavaScript, forms, cookies, `#fragment` scrolling, non-Latin text, DPI scaling.
Pages load synchronously, so the window is unresponsive while a request is in flight.

## Layout

| File | Role |
|------|------|
| `minibrowser.c` | Portable core: URLs, HTTP parsing, history, UI drawing, input handling |
| `mb.h` | Interface between the core and the platform layer |
| `mb_tls.c` | Drives the TLS 1.2 state machine to completion over the platform socket |
| `tls/` | TLS 1.2 client, X.509, RSA/ECDSA, AES-GCM, SHA-2, X25519, P-256/P-384, root store |
| `tests/tls_e2e.sh` | End-to-end TLS test against a local `openssl s_server` (11 cases) |
| `mb_win32.c` | Win32 layer: window, GDI blit, input, Winsock, system RNG (user32, gdi32, ws2_32, advapi32) |
| `mb_shot.c` | Headless driver that renders the whole window to a BMP (tests, screenshots) |

## Build

Windows (MinGW-w64):

```sh
gcc -std=gnu11 -Os -s -DMWE_LIB -mwindows ../mwe.c minibrowser.c mb_tls.c mb_win32.c \
    -o minibrowser.exe -lws2_32 -lgdi32 -luser32 -ladvapi32
minibrowser.exe [url-or-path]
```

Headless screenshot (any platform):

```sh
cc -std=gnu11 -Os -DMWE_LIB ../mwe.c minibrowser.c mb_tls.c mb_shot.c -o mb_shot
./mb_shot ../test.html shot.bmp 1000 640        # optional: --click X Y
./mb_shot https://example.org/ shot.bmp         # Unix builds have real sockets
bash tests/tls_e2e.sh                            # TLS test suite (needs openssl, python3)
```

## Keys

Ctrl+L / F6 focus the address bar, Enter go, Esc cancel, F5 / Ctrl+R reload,
Alt+Left / Alt+Right back / forward, Up/Down/PgUp/PgDn/Home/End scroll.

## Status of this release

**0.3-dev1** adds HTTPS. The TLS stack passes `tests/tls_e2e.sh` (RSA and ECDSA certificates,
AES-128/256-GCM, X25519/P-256/P-384, TLS 1.3 fallback, and five must-refuse cases: wrong host name,
untrusted CA, TLS-1.3-only server, no common cipher) and a 150 KB / 400 KB / chunked page run under
ASan/UBSan. It has not been tried against public web sites from the build environment (no network there).
`mb_win32.c` has been syntax-checked against a stub of the Win32 API but has not been run on Windows yet;
CI builds it on a Windows runner.

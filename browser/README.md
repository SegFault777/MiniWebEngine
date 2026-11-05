# MiniBrowser

A small standalone desktop web browser built on [MiniWebEngine](../README.md).

> **MiniBrowser is not MiniWeb.**
> MiniWeb is the browser built into the **MiniWin** operating system. MiniBrowser
> is a separate project that runs as an ordinary desktop application and only
> shares the "Mini" naming. The two have different code bases and different goals.

**Version 0.3-dev3** (v0.3 in progress; matches MiniWebEngine 0.3-dev3)

## Features

- Address bar with caret, selection, UTF-8 editing and Ctrl+V paste
- Back / forward / reload, per-entry scroll position, 32-entry history
- `http://` and **`https://`** (HTTP/1.1, redirects, chunked transfer decoding)
- Address-bar security badge: **Secure** (verified TLS), **Not secure** (plain HTTP), none for local pages
- `file://` URLs and plain paths, `about:home`, `about:blank`
- Clickable links with relative URL resolution, hover URL in the status bar
- External stylesheets (`<link rel=stylesheet>`, up to 8 / 256 KB) over file, HTTP or HTTPS
- HTML5 / CSS3 rendering from the engine (flexbox, grid, tables, `@media` that follows the window size, ...)
- `<details>` toggles on click
- **JavaScript** (subset, see the engine README): inline and external `<script>`, `onclick="..."` attributes, DOM
  events (click, key), timers, `alert()`, `console.log` (debugger / stderr), `location` changes, checkbox and radio toggling
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

Images, text input in forms (controls are drawn but not editable), `fetch` / `XMLHttpRequest`, cookies, `#fragment` scrolling, non-Latin text, DPI scaling.
Pages load synchronously, so the window is unresponsive while a request is in flight. Scripts run to completion
(a step budget aborts runaway loops), so a heavy script also blocks the window.

## Layout

| File | Role |
|------|------|
| `minibrowser.c` | Portable core: URLs, HTTP parsing, history, UI drawing, input handling |
| `mb.h` | Interface between the core and the platform layer |
| `mb_tls.c` | Drives the TLS 1.2 state machine to completion over the platform socket |
| `tls/` | TLS 1.2 client, X.509, RSA/ECDSA, AES-GCM, SHA-2, X25519, P-256/P-384, root store |
| `tests/tls_e2e.sh` | End-to-end TLS test against a local `openssl s_server` (11 cases) |
| `mb_win32.c` | Win32 layer: window, GDI blit, input, a 16 ms timer for script timers, Winsock, system RNG, `alert()` (user32, gdi32, ws2_32, advapi32) |
| `mb_shot.c` | Headless driver that renders the whole window to a BMP (tests, screenshots) |

## Build

Windows (MinGW-w64):

```sh
gcc -std=gnu11 -Os -s -DMWE_LIB -mwindows ../mwe.c minibrowser.c mb_tls.c mb_win32.c \
    -Wl,--stack,8388608 -o minibrowser.exe -lws2_32 -lgdi32 -luser32 -ladvapi32
minibrowser.exe [url-or-path]
```

Headless screenshot (any platform):

```sh
cc -std=gnu11 -Os -DMWE_LIB ../mwe.c minibrowser.c mb_tls.c mb_shot.c -o mb_shot
./mb_shot ../test.html shot.bmp 1000 640        # options, run in order: --click X Y, --wait MS, --key C
./mb_shot https://example.org/ shot.bmp         # Unix builds have real sockets
bash tests/tls_e2e.sh                            # TLS test suite (needs openssl, python3)
```

## Keys

Ctrl+L / F6 focus the address bar, Enter go, Esc cancel, F5 / Ctrl+R reload,
Alt+Left / Alt+Right back / forward, Up/Down/PgUp/PgDn/Home/End scroll.

## Platform interface (mb.h)

dev3 adds `mb_plat_ms()` (monotonic milliseconds), `mb_plat_alert()` and `mb_plat_log()`; the platform
layer must call `mb_tick()` about every 16 ms (it returns 1 when the page changed and must be redrawn).
The Win32 layer needs a larger stack than the default 1 MB for deep script recursion: link with
`-Wl,--stack,8388608`.

## Status of this release

**0.3-dev3** adds the JavaScript subset to the engine and wires it into MiniBrowser (external scripts,
clicks, keys, timers, `alert`, `location`). Verified headlessly (`mb_shot` with `--click`, `--wait`),
by `tests/js/run.sh` (language and number formatting checked against Node.js output) and by ASan/UBSan
fuzzing including a GC-stress build. `mb_win32.c` was extended (timer, `GetTickCount64`, `MessageBoxW`)
but still has not been compiled or run on Windows; the CI Windows runner is the first real build.
No public web site has been tried from the build environment (no network there).

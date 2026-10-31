# MiniBrowser

A small standalone desktop web browser built on [MiniWebEngine](../README.md).

> **MiniBrowser is not MiniWeb.**
> MiniWeb is the browser built into the **MiniWin** operating system. MiniBrowser
> is a separate project that runs as an ordinary desktop application and only
> shares the "Mini" naming. The two have different code bases and different goals.

**Version 0.2** (first release, matching MiniWebEngine v0.2)

## Features

- Address bar with caret, selection, UTF-8 editing and Ctrl+V paste
- Back / forward / reload, per-entry scroll position, 32-entry history
- `http://` over plain TCP (HTTP/1.1, redirects, chunked transfer decoding)
- `file://` URLs and plain paths, `about:home`, `about:blank`
- Clickable links with relative URL resolution, hover URL in the status bar
- Mouse wheel, scrollbar, keyboard scrolling, page title in the window title

## Not supported (yet)

HTTPS (no TLS: the project stays dependency-free), images, JavaScript, forms,
cookies, `#fragment` scrolling, non-Latin text, DPI scaling. Pages load
synchronously, so the window is unresponsive while a request is in flight.

## Layout

| File | Role |
|------|------|
| `minibrowser.c` | Portable core: URLs, HTTP parsing, history, UI drawing, input handling |
| `mb.h` | Interface between the core and the platform layer |
| `mb_win32.c` | Win32 layer: window, GDI blit, input, Winsock (user32, gdi32, ws2_32 only) |
| `mb_shot.c` | Headless driver that renders the whole window to a BMP (tests, screenshots) |

## Build

Windows (MinGW-w64):

```sh
gcc -std=gnu99 -Os -s -DMWE_LIB -mwindows ../mwe.c minibrowser.c mb_win32.c \
    -o minibrowser.exe -lws2_32 -lgdi32 -luser32
minibrowser.exe [url-or-path]
```

Headless screenshot (any platform):

```sh
cc -std=gnu99 -Os -DMWE_LIB ../mwe.c minibrowser.c mb_shot.c -o mb_shot
./mb_shot ../test.html shot.bmp 1000 640        # optional: --click X Y
```

## Keys

Ctrl+L / F6 focus the address bar, Enter go, Esc cancel, F5 / Ctrl+R reload,
Alt+Left / Alt+Right back / forward, Up/Down/PgUp/PgDn/Home/End scroll.

## Status of this release

**v0.2_1** fixes the Windows build of v0.2: the key-code enum used the prefix
`MB_`, which collides with Win32 message-box macros (`MB_RIGHT`, `MB_HELP`, ...)
from `winuser.h`; it is now `MBK_`. All `-Wmisleading-indentation` warnings were fixed too
(the code builds with `-Wall -Wextra` warning-free).

The portable core is exercised by the headless driver and a fake-network test
(redirects, chunked bodies, link resolution) under ASan/UBSan. The Win32 layer
compiled on a real MinGW-w64 toolchain only up to the enum error above; after
the fix it has been checked against a stub of the Win32 API, but it has not been
run on Windows yet.

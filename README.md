# MiniWebEngine

[![Build](../../actions/workflows/Build.yml/badge.svg)](../../actions/workflows/Build.yml)
[![License: GPL-3.0-only](https://img.shields.io/badge/license-GPL--3.0--only-blue.svg)](LICENSE)

A tiny, zero-dependency HTML + CSS rendering engine written in a single C99 file.

**Current version: v0.1**

MiniWebEngine parses HTML and CSS, computes layout, rasterizes the page with a
built-in bitmap font, and writes a BMP image. It is built around one goal:
stay as small and as lean as possible.

- **~22 KB** stripped binary (Linux, GCC `-Os`)
- **No external dependencies**, no `malloc`, no `string.h`
- libc surface: `fopen`, `fread`, `fwrite`, `fclose`, `exit`, `stderr`
- One source file (`mwe.c`, ~375 lines)
- Streaming output: the page is rendered in 32-row bands, so no full-size
  framebuffer is ever allocated

## Build

Any C99 compiler works.

```sh
# Linux / MinGW
cc -std=c99 -Os -s -ffunction-sections -fdata-sections -Wl,--gc-sections \
   -fno-asynchronous-unwind-tables -fno-ident mwe.c -o mwe

# macOS
cc -std=c99 -Os -ffunction-sections -fdata-sections -Wl,-dead_strip mwe.c -o mwe && strip mwe
```

## Usage

```sh
./mwe input.html output.bmp [width]
```

| Argument     | Description                                   |
|--------------|-----------------------------------------------|
| `input.html` | HTML file to render (max 1 MB)                |
| `output.bmp` | Output 24-bit BMP                             |
| `width`      | Viewport width in pixels (64-2048, default 800) |

Try it with the bundled sample:

```sh
./mwe test.html out.bmp 800
```

## Supported features (v0.1)

**HTML**
- Tags, attributes (`id`, `class`, `style`), comments, doctype
- Void and self-closing elements, implicit closing of `<p>` / `<li>`
- Entities: `&amp; &lt; &gt; &quot; &apos; &nbsp;` and numeric (ASCII range)
- `<style>` blocks (`<script>` is skipped)

**CSS**
- Selectors: type, `.class`, `#id`, descendant chains (up to 3 levels), comma lists
- Cascade by specificity and source order, then inline `style`
- Properties: `color`, `background` / `background-color`, `font-size`,
  `font-weight`, `text-decoration`, `text-align`, `display`,
  `margin` / `padding` (shorthand and per-side, `margin: auto`),
  `width`, `max-width`, `height`, `border`, `border-width`, `border-color`
- Units: `px`, `em`, `%` (width); colors: `#rgb`, `#rrggbb`, `rgb()`, 17 named colors

**Layout and painting**
- Block layout with margin collapsing, inline flow with word wrapping
- `<br>`, `<hr>`, bulleted lists, centered/right-aligned text
- Built-in 5x7 bitmap font (integer scaling), bold, underline, inline backgrounds

## Limitations

- ASCII only: non-ASCII characters (including CJK) are drawn as `?`
- No images, tables, `pre` whitespace preservation, italics, or JavaScript
- No networking and no window output (BMP file only)
- Pseudo-classes and attribute selectors are ignored (the rule is dropped);
  `>`, `+`, `~` are treated like a descendant combinator
- Very long unbreakable words overflow the line

## Roadmap

- Unicode / CJK font support
- `<img>`, `pre`, tables
- Native window output (Win32 / X11) behind an optional build flag
- Inline box model (padding and margins on inline elements)

## Project layout

```
mwe.c                         engine (single file)
test.html                     sample page for smoke tests
LICENSE                       GNU GPL v3
.github/workflows/Build.yml   CI: build + smoke test on Linux, Windows, macOS
```

## License

Copyright (C) MiniWebEngine contributors.

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU General Public License version 3 **only**, as published by
the Free Software Foundation. See [LICENSE](LICENSE) for the full text.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

SPDX-License-Identifier: `GPL-3.0-only`

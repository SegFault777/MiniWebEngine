# Third-party notices

MiniWebEngine itself is licensed under **GPL-3.0-only** (see `LICENSE`).
Copyright (C) 2026 the MiniWebEngine authors.

The glyph outlines embedded in `mwe_font.h` are derived from the following font, licensed under the
**SIL Open Font License, Version 1.1**; the complete license text is in `fonts/OFL-NotoSansCJK.txt`.
This notice must stay with every copy of the source or binary distribution (the engine also exposes
it at run time: `mwe --licenses`, `mwe_licenses()`, and `about:licenses` in MiniBrowser).

## Noto Sans CJK SC and KR (Latin, Greek, Cyrillic, Simplified Chinese, Hangul)

- Fonts: Noto Sans CJK SC Regular and Bold, and Noto Sans CJK KR Regular, version 2.004, https://github.com/notofonts/noto-cjk
- Copyright 2014-2021 Adobe (http://www.adobe.com/). Noto is a trademark of Google Inc.
- License: SIL Open Font License 1.1 - `fonts/OFL-NotoSansCJK.txt`
- Subset: the Latin, Greek and Cyrillic repertoire the font offers (Regular + Bold) and, from the
  Regular weight, the GB2312 repertoire (6763 hanzi plus symbols, kana, pinyin) with CJK punctuation
  and full-width forms; from the KR face, the KS X 1001 Hangul repertoire (2350 syllables), compatibility
  jamo and KS X 1001 symbols. Bold CJK and Hangul are synthesized by double-striking.

## Mozilla CA certificate bundle (trusted root certificates)

- `browser/tls/trusted_roots.h` embeds 116 TLS server root certificates taken from the Mozilla root
  program as distributed in python-certifi 2026.10.19 (`roots/ROOTS.txt` lists each with its SHA-256;
  `roots/SKIPPED.txt` and `roots/REMOVED.txt` list what was left out and why).
- The bundle is derived from Mozilla's `certdata.txt`, licensed under the **Mozilla Public License 2.0**
  (https://www.mozilla.org/MPL/2.0/). The certificates themselves are public data of their issuers.
- Regenerate with `tools/gen_roots.py`; the file and its list must be updated whenever Mozilla changes
  its root program, otherwise removed or distrusted roots stay trusted.

## Modification statement (OFL 1.1)

The font software (both faces) was **converted and subsetted**: outlines were reduced to the listed code points,
scaled to a 1024-unit em, converted to quadratic curves and compressed into the C arrays in
`mwe_font.h` by `tools/genfont.py`. The result is a Modified Version embedded in the engine; it is
not distributed as a font file, not sold by itself, and not presented under the original font name.
The upstream copyright holders do not endorse this software. If you publish the extracted outlines
as a font, you must follow the OFL (including any Reserved Font Names).

`fonts/OFL-NotoSansCJK.txt` uses the standard OFL 1.1 body with the copyright line embedded in the font
(name table). Compare it with the `OFL.txt` shipped by the upstream project if you need the
authoritative copy.

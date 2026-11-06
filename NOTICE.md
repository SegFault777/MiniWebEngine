# Third-party notices

MiniWebEngine itself is licensed under **GPL-3.0-only** (see `LICENSE`).
Copyright (C) 2026 the MiniWebEngine authors.

The glyph outlines embedded in `mwe_font.h` are derived from the following fonts. Both are
licensed under the **SIL Open Font License, Version 1.1**; the complete license texts are in
`fonts/OFL-Inter.txt` and `fonts/OFL-NotoSansCJK.txt`. These notices must stay with every copy of
the source or binary distribution (the engine also exposes them at run time: `mwe --licenses`,
`mwe_licenses()`, and `about:licenses` in MiniBrowser).

## Inter (Latin, Greek, Cyrillic)

- Font: Inter 4.000, Regular and Bold (static OTF), https://github.com/rsms/inter
- Copyright 2016 The Inter Project Authors (https://github.com/rsms/inter)
- License: SIL Open Font License 1.1 - `fonts/OFL-Inter.txt`

## Noto Sans CJK SC (Simplified Chinese)

- Font: Noto Sans CJK SC Regular 2.004, https://github.com/notofonts/noto-cjk
- Copyright 2014-2021 Adobe (http://www.adobe.com/). Noto is a trademark of Google Inc.
- License: SIL Open Font License 1.1 - `fonts/OFL-NotoSansCJK.txt`
- Subset: the GB2312 repertoire (6763 hanzi plus symbols, kana, pinyin) and CJK punctuation /
  full-width forms. Bold is synthesized by double-striking.

## Modification statement (OFL 1.1, condition 2/3)

The font software was **converted and subsetted**: outlines were reduced to the listed code
points, scaled to a 1024-unit em, converted to quadratic curves and compressed into the C arrays
in `mwe_font.h` by `tools/genfont.py`. The result is a Modified Version embedded in the engine
and is not distributed as a font file, is not sold by itself, and is not presented under the
original font names. Upstream copyright holders do not endorse this software. If you publish the
extracted font data as a font, you must follow the OFL (including Reserved Font Names, if any).

The license texts in `fonts/` use the standard OFL 1.1 body with the copyright line of the font
as embedded in its name table; compare with the `OFL.txt` shipped by the upstream projects if you
need the authoritative copy.
